// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/nau_animation_clip_editor.hpp"

#include "nau/scene/nau_scene_editor_interface.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau/nau_editor_plugin_manager.hpp"
#include "nau/nau_usd_scene_editor.hpp"
#include "nau/asset_tools/db_manager.h"

#include "nau/NauAnimationClipAsset/nauAnimationClip.h"
#include "nau/NauAnimationClipAsset/nauAnimationController.h"
#include "nau/NauAnimationClipAsset/nauAnimationSkeleton.h"
#include "nau/NauAnimationClipAsset/nauAnimationTrack.h"
#include "nau/NauAnimationClipAsset/tokens.h"

#include "nau/animation/components/animation_component.h"
#include "nau/animation/components/skeleton_component.h"
#include "nau/animation/playback/animation_instance.h"
#include "nau/animation/controller/animation_controller.h"

#include "nau_assert.hpp"
#include "browser/nau_file_operations.hpp"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/bindingAPI.h>
#include <pxr/usd/usdSkel/cache.h>
#include <pxr/usd/usdSkel/root.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/skeletonQuery.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usd/variantSets.h>

#include <QFileDialog>


namespace nau
{
    template<typename Component, typename ...Other>
    void FindComponentsForObject(scene::SceneObject::WeakRef sceneObject, Component*& component, Other*&... other)
    {
        component = sceneObject->findFirstComponent<Component>();
        if constexpr (sizeof...(Other) > 0) {
            FindComponentsForObject(sceneObject, other...);
        }
    }

    template<typename ...Components>
    static void FindComponentsByPrim(NauUsdSceneEditorInterface& usdSceneEditor, const pxr::UsdPrim& prim, Components*&... components)
    {
        if constexpr (sizeof...(Components) > 0) {
            auto&& sceneObject = usdSceneEditor.sceneSynchronizer().sceneObjectByPrimPath(prim.GetPrimPath());
            if (!sceneObject) {
                return;
            }
            FindComponentsForObject(sceneObject, components...);
        }
    }

    static std::string GetPrimPathFromAssetPath(const std::string& assetPath)
    {
        constexpr size_t EXT_SIZE = 5; // ".usda"

        auto beginIt = std::find_if(assetPath.rbegin(), assetPath.rend(), [](char c) noexcept {
            return (c == '/') || (c == '\\');
        });
        const size_t beginIndex = beginIt.base() - assetPath.begin();
        std::string primPath = std::string("/") + assetPath.substr(beginIndex, assetPath.size() - beginIndex - EXT_SIZE);

        auto it = std::remove_if(primPath.begin(), primPath.end(), [](char& c) {
            return (c == '(') || (c == ')');
        });
        primPath.erase(it, primPath.end());

        return primPath;
    }
}

// ** NauAnimationClipEditor

NauAnimationClipEditor::NauAnimationClipEditor()
    : QObject(nullptr)
    , m_editorDockManger(nullptr)
    , m_editorDockWidget(nullptr)
    , m_timelineWindow(nullptr)
    , m_usdSceneEditor(nullptr)
    , m_animationState(AnimationState::Stop)
    , m_controllerPtr(nullptr)
    , m_creationComponentTimer(nullptr)
    , m_updateComponentTimer(nullptr)
    , m_propertyListPtr(std::make_shared<NauAnimationPropertyList>())
    , m_keyframeToken("keyframes")
    , m_hackToken("hack")
{
}

void NauAnimationClipEditor::initialize(NauEditorInterface* mainEditor)
{
    m_mainEditor = mainEditor;
    const auto& mainWindow = mainEditor->mainWindow();
    m_editorDockManger = mainWindow.dockManager();

    m_usdSceneEditor = &Nau::EditorServiceProvider().get<NauUsdSceneEditorInterface>();
    auto&& selectionContainer = m_usdSceneEditor->selectionContainer();
    selectionContainer->selectionChangedDelegate.addCallback([this](const NauUsdPrimsSelection& selection) {
        m_creationComponentTimer->stop();
        pxr::UsdPrim selectedPrim;
        if (selection.size() == 1ull) {
            const auto& prim = selection.front();
            if (!NauUsdPrimUtils::isPrimComponent(prim)) {
                selectedPrim = prim;
            }
        }
        setSelectedObject(selectedPrim, true);
    });
}

void NauAnimationClipEditor::terminate()
{
    m_updateComponentTimer->stop();
    m_creationComponentTimer->stop();
}

void NauAnimationClipEditor::postInitialize()
{
    if (m_editorDockManger == nullptr) {
        return;
    }

    auto assetManager = m_mainEditor->assetManager();
    assetManager->addClient({ NauEditorFileType::Animation }, this);

    m_editorDockWidget = std::make_unique<NauDockWidget>(QObject::tr("Animation Editor"), nullptr);
    m_editorDockWidget->setObjectName("DockAnimationEditor");
    m_timelineWindow = std::make_unique<NauTimelineWindow>();

    auto* projectBrowser = m_editorDockManger->projectBrowser();
    auto* area = m_editorDockManger->addDockWidgetTabToArea(m_editorDockWidget.get(), projectBrowser->dockAreaWidget());
    area->setTabsPosition(ads::CDockAreaWidget::South);
    area->setCurrentDockWidget(projectBrowser);
    m_editorDockWidget->setWidget(m_timelineWindow.get());

    m_creationComponentTimer = std::make_unique<QTimer>();
    connect(m_creationComponentTimer.get(), &QTimer::timeout, [this]() {
        tryGetControllerComponent();
    });
    m_updateComponentTimer = std::make_unique<QTimer>();
    connect(m_updateComponentTimer.get(), &QTimer::timeout, [this]() {
        updateControllerComponent();
    });

    connect(m_timelineWindow.get(), &NauTimelineWindow::eventPlayButtonPressed, [this]() {
        setAnimationState(AnimationState::Play);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventPauseButtonPressed, [this]() {
        setAnimationState(AnimationState::Pause);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventStopButtonPressed, [this]() {
        setAnimationState(AnimationState::Stop);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventCreateControllerButtonPressed, [this]() { 
        createClipAssetFromDialog();
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventManualTimeChanged, [this](float time) {
        setControllerComponentTime(time);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventClipSwitched, [this](int clipIndex) {
        switchClip(clipIndex);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventTrackAdded, [this](int propertyIndex) {
        addTrack(propertyIndex);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventTrackDeleted, [this](int propertyIndex) {
        deleteTrack(propertyIndex);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventKeyframeDeleted, [this](int propertyIndex, float time) {
        deleteKeyframe(propertyIndex, time);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventKeyframeChanged, [this](int propertyIndex, float timeOld, float timeNew) {
        changeKeyframeTime(propertyIndex, timeOld, timeNew);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventAddKeyframe, [this](int propertyIndex, float time) {
        NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex);
        addKeyframe(propertyIndex, property->dataForTime(time), time);
    });
    connect(m_timelineWindow.get(), &NauTimelineWindow::eventPropertyChanged, [this](int propertyIndex, const NauAnimationPropertyData& data, float time) {
        addKeyframe(propertyIndex, data, time);
    });
}

void NauAnimationClipEditor::preTerminate()
{
    m_timelineWindow->setParent(nullptr);
    m_editorDockManger->removeDockWidget(m_editorDockWidget.get());
}

void NauAnimationClipEditor::createAsset(const std::string& assetPath)
{
    createClipAsset(assetPath);
}

bool NauAnimationClipEditor::openAsset(const std::string& assetPath)
{
    return true;
}

bool NauAnimationClipEditor::saveAsset(const std::string& assetPath)
{
    return true;
}

std::string NauAnimationClipEditor::editorName() const
{
    return "Animation Editor";
}

NauDockWidget& NauAnimationClipEditor::dockWidget() noexcept
{
    return *m_editorDockWidget;
}

NauEditorFileType NauAnimationClipEditor::assetType() const
{
    return NauEditorFileType::Animation;
}

void NauAnimationClipEditor::handleSourceAdded(const std::string& sourcePath)
{
}

void NauAnimationClipEditor::handleSourceRemoved(const std::string& sourcePath)
{
}

NauAnimationNameList NauAnimationClipEditor::createNameList()
{
    if (!m_controllerPrim || !m_assetStage) {
        return {};
    }

    NauAnimationNameList nameList;

    const std::string& path = (m_skeletonPrim ? m_skeletonPrim : m_animationPrim).GetPrimPath().GetString();
    auto it = std::find(path.rbegin(), path.rend(), '/').base();
    nameList.emplace_back(path.substr(it - path.begin()));

    return nameList;
}

void NauAnimationClipEditor::switchClip(int clipIndex)
{
}

void NauAnimationClipEditor::addTrack(int propertyIndex)
{
    NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex);
    if (property == nullptr) {
        return;
    }
    if (m_skeletonPrim) {
        // TODO: Support skeleton modification
        return;
    }

    property->setSelected(true);

    const pxr::TfToken dataTypeToken{ property->name()};
    const auto& reference = m_animationPrim;
    property->setPrim(m_assetStage->DefinePrim(reference.GetPrimPath().AppendChild(dataTypeToken), pxr::UsdTokens.Get()->AnimationTrack));
    property->setKeyframesAttribute(property->prim().CreateAttribute(m_keyframeToken, property->typeName()));

    pxr::UsdNauAnimationTrack track{ property->prim() };
    track.GetDataTypeAttr().Set(dataTypeToken);

    m_assetStage->Save();

    updateControllerPrim();
    setSelectedObject(m_selectedPrim, false);
}

void NauAnimationClipEditor::deleteTrack(int propertyIndex)
{
    NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex);
    if (property == nullptr) {
        return;
    }
    if (m_skeletonPrim) {
        // TODO: Support skeleton modification
        return;
    }

    m_assetStage->RemovePrim(property->prim().GetPath());
    m_assetStage->Save();
    property->reset();
    updateControllerPrim();
    updateAnimationDuration();
    setSelectedObject(m_selectedPrim, true);
}

void NauAnimationClipEditor::addKeyframe(int propertyIndex, const NauAnimationPropertyData& data, float time)
{
    NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex);
    if (property == nullptr) {
        return;
    }
    if (m_skeletonPrim) {
        // TODO: Support skeleton modification
        return;
    }

    property->setKeyframeData(time, data);
    m_assetStage->Save();
    updateControllerPrim();
    updateAnimationDuration();
    setSelectedObject(m_selectedPrim, false);
}

void NauAnimationClipEditor::deleteKeyframe(int propertyIndex, float time)
{
    if (m_skeletonPrim) {
        // TODO: Support skeleton modification
        return;
    }
    if (propertyIndex == -1) {
        m_propertyListPtr->forEach([time](NauAnimationProperty& property) {
            property.deleteKeyframe(time);
        });
    } else if (NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex)) {
        property->deleteKeyframe(time);
    }

    m_assetStage->Save();
    updateControllerPrim();
    updateAnimationDuration();
    setSelectedObject(m_selectedPrim, false);
}

void NauAnimationClipEditor::changeKeyframeTime(int propertyIndex, float timeOld, float timeNew)
{
    if (m_skeletonPrim) {
        // TODO: Support skeleton modification
        return;
    }
    if (propertyIndex == -1) {
        m_propertyListPtr->forEach([timeOld, timeNew](NauAnimationProperty& property) {
            property.changeKeyframeTime(timeOld, timeNew);
        });
    } else if (NauAnimationProperty* property = m_propertyListPtr->propertyByIndex(propertyIndex)) {
        property->changeKeyframeTime(timeOld, timeNew);
    }

    m_assetStage->Save();
    updateControllerPrim();
    updateAnimationDuration();
    setSelectedObject(m_selectedPrim, false);
}

void NauAnimationClipEditor::loadPathList()
{
    m_assetStage.Reset();

    if (!m_controllerPrim) {
        return;
    }

    pxr::SdfAssetPath assetPath;
    pxr::UsdNauAnimationController controller{ m_controllerPrim };
    controller.GetAnimationAssetAttr().Get(&assetPath);
    if (assetPath.GetAssetPath().empty()) {
        return;
    }

    const auto* customTokens = pxr::UsdTokens.Get();
    const auto* skeletonTokens = pxr::UsdSkelTokens.Get();

    const std::string_view assetPathStr = assetPath.GetAssetPath();
    auto&& uid = nau::Uid::parseString(assetPathStr.starts_with("uid:") ? assetPathStr.substr(4) : assetPathStr);
    if (uid.isError()) {
        return;
    }
    auto&& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();
    std::string path = assetDb.getSourcePathFromUid(*uid).c_str();
    path = nau::Paths::instance().getAssetsPath() + '/' + path.substr(0, path.find('+'));

    m_assetStage = pxr::UsdStage::Open(path.ends_with(".usda") ? path : path + ".usda");
    if (!m_assetStage) {
        return;
    }

    for (const pxr::UsdPrim& prim : m_assetStage->Traverse()) {
        const auto& primTypeName = prim.GetTypeName();
        if (primTypeName == customTokens->AnimationClip) {
            m_animationPrim = prim;
            break;
        }
        if (primTypeName == skeletonTokens->SkelRoot) {
            m_skeletonPrim = prim;
            break;
        }
    }
}

void NauAnimationClipEditor::loadSkelProperties()
{
    pxr::UsdSkelRoot skelRoot{ m_skeletonPrim };
    if (!skelRoot) {
        return;
    }

    pxr::UsdSkelCache skelCache;
    skelCache.Populate(skelRoot, pxr::UsdTraverseInstanceProxies());

    std::vector<pxr::UsdSkelBinding> bindings;
    skelCache.ComputeSkelBindings(skelRoot, &bindings, pxr::UsdTraverseInstanceProxies());
    if (bindings.empty()) {
        return;
    }

    pxr::UsdSkelBindingAPI bindingAPI{ bindings.front().GetSkeleton().GetPrim() };
    pxr::SdfPathVector animPrimPaths;
    bindingAPI.GetAnimationSourceRel().GetTargets(&animPrimPaths);
    if (animPrimPaths.empty()) {
        return;
    }

    pxr::UsdPrim skelAnimPrim = m_assetStage->GetPrimAtPath(animPrimPaths.front());
    pxr::UsdSkelAnimation skelAnimation{ skelAnimPrim };
    if (!skelAnimation) {
        return;
    }

    std::vector<pxr::UsdAttribute> existsAttributes;
    existsAttributes.reserve(3);

    if (pxr::UsdAttribute attribute = skelAnimation.GetTranslationsAttr()) {
        existsAttributes.emplace_back(attribute);
    }
    if (pxr::UsdAttribute attribute = skelAnimation.GetRotationsAttr()) {
        existsAttributes.emplace_back(attribute);
    }
    if (pxr::UsdAttribute attribute = skelAnimation.GetScalesAttr()) {
        existsAttributes.emplace_back(attribute);
    }
    if (existsAttributes.empty()) {
        return;
    }

    pxr::VtArray<pxr::TfToken> jointNameList;
    skelAnimation.GetJointsAttr().Get(&jointNameList);

    const auto& vec3TypeName = pxr::SdfValueTypeNames.Get()->Float3Array;
    const auto& quatTypeName = pxr::SdfValueTypeNames.Get()->QuatfArray;

    size_t propertyIndex = 0;
    auto& propertyList = m_propertyListPtr->skelList();
    propertyList.reserve(jointNameList.size() * existsAttributes.size());
    
    for (const pxr::TfToken& jointName : jointNameList) {
        for (const pxr::UsdAttribute& attribute : existsAttributes) {
            const std::string& name = attribute.GetName();
            const auto& attributeTypeName = attribute.GetTypeName();
            NauAnimationTrackDataType dataType{};
            if (attributeTypeName == vec3TypeName) {
                dataType = NauAnimationTrackDataType::Vec3;
            } else if (attributeTypeName == quatTypeName) {
                dataType = NauAnimationTrackDataType::Quat;
            } else {
                continue;
            }
            propertyList.emplace_back(jointName.GetString() + "." + name, dataType, propertyIndex++, true);
            propertyList.back().setKeyframesAttribute(attribute);
            propertyList.back().setPrim(skelAnimPrim);
        }
    }
}

void NauAnimationClipEditor::loadClipProperties()
{
    auto& clipList = m_propertyListPtr->clipList();
    clipList.reserve(3);
    clipList.emplace_back("Translation", NauAnimationTrackDataType::Vec3, false);
    clipList.emplace_back("Rotation", NauAnimationTrackDataType::Vec3, false);
    clipList.emplace_back("Scale", NauAnimationTrackDataType::Vec3, false);

    pxr::TfToken dataTypeToken;
    for (auto&& trackPrim : m_animationPrim.GetAllChildren()) {
        pxr::UsdNauAnimationTrack track{ trackPrim };
        if (!track) {
            continue;
        }
        track.GetDataTypeAttr().Get(&dataTypeToken);
        auto propertyIt = std::find_if(clipList.begin(), clipList.end(), [&dataTypeToken](const NauAnimationClipProperty& property) {
            return property.name() == dataTypeToken.GetString();
        });
        if (propertyIt == clipList.end()) {
            /// todo: notify window about missing property
            continue;
        }

        propertyIt->setSelected(true);
        propertyIt->setPrim(trackPrim);
        propertyIt->setKeyframesAttribute(trackPrim.CreateAttribute(m_keyframeToken, propertyIt->typeName()));
        m_keyIndex = propertyIt->timeSamples().size();
    }
}

void NauAnimationClipEditor::loadPropertyList()
{
    m_keyIndex = 0;
    nau::SkeletonComponent::drawDebugSkeletons = false;

    if (!m_controllerPrim || !m_assetStage) {
        return;
    }

    if (m_skeletonPrim) {
        loadSkelProperties();
        m_propertyListPtr->setReadOnly(true);
        nau::SkeletonComponent::drawDebugSkeletons = true;
    } else {
        loadClipProperties();
    }
    updateAnimationDuration();
}

void NauAnimationClipEditor::resetComponents()
{
    m_controllerPrim = {};
    m_skeletonPrim = {};
    m_animationPrim = {};
    if (m_selectedPrim) {
        for (auto&& child : m_selectedPrim.GetAllChildren()) {
            if (pxr::UsdNauAnimationController controller{ child }; controller && !m_controllerPrim) {
                m_controllerPrim = child;
                updateControllerPrim();
                break;
            }
        }
    }
    nau::FindComponentsByPrim(*m_usdSceneEditor, m_selectedPrim, m_controllerPtr);
    if (m_controllerPtr) {
        auto* controller = m_controllerPtr->getController();
        const float frameTime = 1000.f / (std::max(1.f, controller->getFrameRate()));
        m_updateComponentTimer->start(static_cast<int>(frameTime));
    } else {
        m_updateComponentTimer->stop();
        m_controllerPrim = {};
    }
}

void NauAnimationClipEditor::updateAnimationDuration()
{
    auto* controller = m_controllerPtr->getController();
    const float frameTime = 1.f / controller->getFrameRate();
    float startTime = FLT_MAX;
    float endTime = 0.f;
    for (int trackIndex = 0; controller && (trackIndex < controller->getAnimationInstancesCount()); ++trackIndex) {
        auto* animInstance = controller->getAnimationInstanceAt(trackIndex);
        const float currentTime = animInstance->getCurrentTime();

        auto* player = animInstance->getPlayer();
        if (player == nullptr) {
            continue;
        }
        const float duration = static_cast<float>(player->getDurationInFrames()) * frameTime;

        player->jumpToFirstFrame();
        m_controllerPtr->updateComponent(0.f);
        startTime = std::min(startTime, currentTime);
        endTime = std::max(endTime, currentTime + duration);
    }
    m_propertyListPtr->setTimeAnimation(startTime, endTime);
    m_propertyListPtr->setFrameDuration(frameTime);
}

void NauAnimationClipEditor::setControllerComponentTime(float time)
{
    if (m_controllerPtr == nullptr) {
        return;
    }
    setAnimationState(AnimationState::Stop);
    auto* controller = m_controllerPtr->getController();
    const float frameTime = 1.f / (std::max(1.f, controller->getFrameRate()));
    const int frame = static_cast<int>(time / frameTime);
    for (int trackIndex = 0; controller && (trackIndex < controller->getAnimationInstancesCount()); ++trackIndex) {
        auto* animInstance = controller->getAnimationInstanceAt(trackIndex);
        auto* player = animInstance->getPlayer();
        const int lastFrame = player->getDurationInFrames() - 1;
        const int currentFrame = std::min(lastFrame, frame);
        player->jumpToFrame(currentFrame);
        player->play();
        player->pause(true);
    }
    m_controllerPtr->updateComponent(frameTime);
    m_timelineWindow->setCurrentTime(time);
    m_timelineWindow->stopPlayback();
}

void NauAnimationClipEditor::setAnimationState(AnimationState state)
{
    if (m_controllerPtr == nullptr) {
        return;
    }
    if ((m_animationState == state) && (m_animationState != AnimationState::Pause)) {
        return;
    }
    
    auto* controller = m_controllerPtr->getController();
    for (int trackIndex = 0; trackIndex < controller->getAnimationInstancesCount(); ++trackIndex) {
        if (auto* animInstance = controller->getAnimationInstanceAt(trackIndex)) {
            if (!animInstance->getPlayer()) {
                animInstance->load();
            }

            if (auto* player = animInstance->getPlayer()) {
                switch (state) {
                case AnimationState::Play:
                    player->play();
                    break;
                case AnimationState::Pause:
                    player->pause(true);
                    break;
                case AnimationState::Stop:
                    player->stop();
                    break;
                }
            }
        }
    }
    m_animationState = state;
    m_controllerPtr->updateComponent(.0f);
}

void NauAnimationClipEditor::createClipAssetFromDialog()
{
    if (!m_selectedPrim) {
        return;
    }

    const QString projectPath = NauEditorInterface::currentProject()->dir().absolutePath();
    const QString defaultPath = NauFileOperations::generateFileNameIfExists(projectPath + "/content/NewAnimation" + NAU_ANIMATION_FILE_EXTENSION);
    auto&& pp = pxr::UsdStage::CreateInMemory((defaultPath + "1").toUtf8().constData());

    QString filePath;
    do {
        QString file_ext;
        QTextStream out(&file_ext);
        out << "Animations (" << NAU_ANIMATION_FILE_FILTER << ")";
        filePath = QFileDialog::getSaveFileName(m_timelineWindow.get(), tr("Create animation"), defaultPath, tr(file_ext.toUtf8().constData()));
    } while (!filePath.isEmpty() && !filePath.startsWith(projectPath));
    
    if (filePath.isEmpty()) {
        return;
    }
    const std::string assetPath = filePath.toUtf8().constData();
    if (createClipAsset(assetPath)) {
        const std::string primPath = nau::GetPrimPathFromAssetPath(assetPath);
        addAnimationAsset(assetPath, pxr::SdfPath{ primPath });
    }
}

bool NauAnimationClipEditor::createClipAsset(const std::string& assetPath)
{
    auto&& assetStage = pxr::UsdStage::CreateNew(assetPath);
    if (assetStage == nullptr) {
        NED_ERROR("Failed to create animation clip asset.");
        return false;
    }

    const std::string primPathStr = nau::GetPrimPathFromAssetPath(assetPath);
    const pxr::SdfPath primPath{ primPathStr };
    if (auto&& animationPrim = pxr::UsdNauAnimationClip::Define(assetStage, primPath)) {
        assetStage->Save();
        return true;
    }

    NED_ERROR("Failed to create animation clip prim.");
    return false;
}

void NauAnimationClipEditor::updateControllerPrim()
{
    if (!m_controllerPrim) {
        return;
    }

    auto rootAdapter = m_usdSceneEditor->sceneSynchronizer().translator()->getRootAdapter();
    if (!rootAdapter) {
        return;
    }

    std::function<UsdTranslator::IPrimAdapter::Ptr(UsdTranslator::IPrimAdapter::Ptr)> findAdapter;
    findAdapter = [this, &findAdapter, path = m_controllerPrim.GetPath()](UsdTranslator::IPrimAdapter::Ptr adapter) {
        if (path == adapter->getPrimPath()) {
            return adapter;
        }

        UsdTranslator::IPrimAdapter::Ptr result{};
        for (auto child : adapter->getChildren()) {
            result = findAdapter(child.second);
            if (result) {
                break;
            }
        }

        return result;
    };

    if (auto controllerAdapter = findAdapter(rootAdapter)) {
        controllerAdapter->update();
    }
}

void NauAnimationClipEditor::tryGetControllerComponent()
{
    nau::FindComponentsByPrim(*m_usdSceneEditor, m_selectedPrim, m_controllerPtr);
    if (m_controllerPtr == nullptr) {
        return;
    }
    m_creationComponentTimer->stop();
    setSelectedObject(m_selectedPrim, true);
}

void NauAnimationClipEditor::updateControllerComponent()
{
    if (m_controllerPtr == nullptr) {
        return;
    }
    if (m_animationState != AnimationState::Play) {
        return;
    }

    const float dt = 1.f / static_cast<float>(std::max(1, m_updateComponentTimer->interval()));
    m_controllerPtr->updateComponent(dt);

    bool needForceStop = true;
    float currentTime = 0.f;
    auto* controller = m_controllerPtr->getController();
    for (int trackIndex = 0; trackIndex < controller->getAnimationInstancesCount(); ++trackIndex) {
        auto* animInstance = controller->getAnimationInstanceAt(trackIndex);
        const int currentFrame = animInstance->getCurrentFrame();
        const int frameAmount = animInstance->getPlayer()->getDurationInFrames();
        needForceStop &= (currentFrame >= frameAmount);
        currentTime = std::max(currentTime, animInstance->getCurrentTime());
    }
    m_timelineWindow->setCurrentTime(currentTime);

    if (needForceStop) {
        m_timelineWindow->stopPlayback();
    }
}

void NauAnimationClipEditor::addAnimationAsset(const std::string& assetPath, const pxr::SdfPath& primPath)
{
    const QString projectPath = NauEditorInterface::currentProject()->dir().absolutePath();
    const auto* tokens = pxr::UsdTokens.Get();
    const auto& controllerToken = tokens->AnimationController;
    if (!m_controllerPrim) {
        for (const pxr::UsdPrim& prim : m_selectedPrim.GetAllChildren()) {
            if (prim.GetTypeName() == controllerToken) {
                m_controllerPrim = prim;
                break;
            }
        }
    }
    if (!m_controllerPrim) {
        std::string uniquePath = "";
        m_controllerPrim = m_usdSceneEditor->createPrim(m_selectedPrim.GetPrimPath(), controllerToken, controllerToken, true, uniquePath);
    }

    const auto& primPathStr = primPath.GetString();
    pxr::UsdNauAnimationController controller{ m_controllerPrim };
    std::string sourceTrackAssetPath = std::format(
        "{}+[kfanimation]", 
        !assetPath.empty() ? nau::FileSystemExtensions::getRelativeAssetPath(assetPath).string() : std::string{}
    );

    controller.CreateAnimationAssetAttr().Set(pxr::SdfAssetPath{ sourceTrackAssetPath });

    m_creationComponentTimer->start(10);
}

void NauAnimationClipEditor::setSelectedObject(const pxr::UsdPrim& prim, bool needReset)
{
    m_propertyListPtr->setRefillFlag(needReset);
    if (m_selectedPrim != prim || needReset) {
        m_selectedPrim = prim;
        resetComponents();
        loadPathList();
        loadPropertyList();
    }
    m_timelineWindow->setClipProperties(m_propertyListPtr);
    m_timelineWindow->setCreationAvailable(prim.IsValid());

    if (needReset) {
        const NauAnimationNameList nameList = std::move(createNameList());
        m_timelineWindow->setClipNameList(nameList, 0);
    }
}

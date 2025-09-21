// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/inspector/nau_usd_inspector_client.hpp"
#include "nau/inspector/nau_usd_property_factory.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"


#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/sdf/listEditorProxy.h"
#include <pxr/usd/usdGeom/xformCache.h>

#include "nau/assets/nau_asset_manager.hpp"
#include "nau/app/nau_editor_services.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/prim-factory/nau_usd_prim_creator.hpp"

#include <memory>
#include "nau/shared/file_system.h"


//TODO: Refine the api architecture of client-widget interaction in the future
// ** NauUsdInspectorClient

NauUsdInspectorClient::NauUsdInspectorClient(NauInspectorPage* inspector)
    : m_inspector(inspector)
    , m_needUpdate(false)
    , m_needRebuild(false)
{
    connect(&m_updateTimer, &QTimer::timeout, this, &NauUsdInspectorClient::tick);
}

void NauUsdInspectorClient::handleNotice(NauUITranslatorProxyNotice const& notice)
{
    if (m_currentPrimPath.IsEmpty()) {
        return;
    }

    // TODO: Refactor inspector update
    bool needClear = false;

    // Check resynced paths
    for (const pxr::SdfPath& path : notice.resyncedPaths()) {
        pxr::UsdPrim prim = m_currentScene->GetPrimAtPath(path);
        // Need to clear inspector, if updated prim path equals current inspector prim path and it is invalid (prim deleted)
        needClear = (m_currentPrimPath == path) && !prim.IsValid();
        if (needClear) {
            break;
        }

        m_needUpdate = m_currentPrimPath == path;
        m_needRebuild = m_currentPrimPath == path.GetParentPath();
        if (m_needUpdate || m_needRebuild) {
            break;
        }
    }

    if (needClear) {
        clear();
        return;
    }

    // Check info changes
    if (!(m_needUpdate || m_needRebuild)) {
        for (const pxr::SdfPath& path : notice.infoChanges()) {
            
            m_needUpdate = m_currentPrimPath == path.GetParentPath();
            if (m_needUpdate) {
                break;
            }
        }
    }
}

void NauUsdInspectorClient::buildFromMaterial(PXR_NS::UsdPrim prim)
{
    m_inspector->clear();
    m_inspector->mainLayout()->addStretch(1);

    m_currentPrimPath = prim.GetPath();
    m_currentScene = prim.GetStage();

    std::string displayName = m_currentScene->GetRootLayer()->GetDisplayName();
    std::string typeName = "Material";

    const bool hideAddButton = true;
    auto header = m_inspector->addHeader(displayName, typeName, hideAddButton);

    // Build for pipelines
    PXR_NS::VtArray<PXR_NS::TfToken> transformTokens;
    for (auto component : prim.GetAllChildren()) {
        buildProperties(component, component.GetName().GetString(), transformTokens);
    }

    m_updateTimer.start(16);
}

void NauUsdInspectorClient::buildFromPrim(PXR_NS::UsdPrim prim, bool hideAddButton)
{
    m_hideAddButton = hideAddButton;
    buildFromPrimInternal(prim);
}

void NauUsdInspectorClient::buildFromPrimInternal(PXR_NS::UsdPrim prim)
{
    m_inspector->clear();
    m_inspector->mainLayout()->addStretch(1);

    m_currentPrimPath = prim.GetPath();
    m_currentScene = prim.GetStage();

    std::string displayName = prim.GetDisplayName();
    if (displayName.empty()) {
        displayName = prim.GetName().GetString();
    }

    std::string typeName = prim.GetTypeName().GetString();

    // TODO: What to do with primas without a specific type?
    if (typeName.empty()) {
        typeName = "Prim";
    } 

    // Add header
    auto header = m_inspector->addHeader(displayName, typeName, m_hideAddButton);

    std::vector<std::string> excludeList = {
        "AudioComponentEmitter",
        "AudioComponentListener",
        "Mesh",
        "VFXInstance",
        "SphereLight",
        "Xform",
        "nau::SkinnedMeshComponent",
        //"nau::scene::DirectionalLightComponent",
        "nau::scene::EnvironmentComponent",
        //"nau::scene::OmnilightComponent",
        "nau::scene::SceneComponent",
        //"nau::scene::SpotlightComponent",
        "nau::scene::StaticMeshComponent",
        "nau::scene::CameraComponent",
        "nau::scene::BillboardComponent"
    };

    auto allComponets = NauUsdPrimFactory::instance().registeredPrimCreatorsWithDisplayNames([](const std::string& value) {
        return value.find("NauGui") == std::string::npos;
    });
    
    std::map<std::string, std::string> resultTypes;

    for (auto existedType : allComponets) {
        auto it = std::find_if(excludeList.begin(), excludeList.end(), [&existedType](const std::string& exc) {
            return existedType.second == exc;
        });

        if (it != excludeList.end()) {
            continue;
        }

        resultTypes[existedType.first] = existedType.second;
    }

    header->creationList()->initTypesList(resultTypes);

    header->creationList()->connect(header->creationList(), &NauObjectCreationList::eventCreateObject, [this](const std::string& typeName) {
        emit eventComponentAdded(m_currentPrimPath, pxr::TfToken(typeName));
    });

    // Build the transform separately
    // TODO: Do it inside the factory?
    PXR_NS::VtArray<PXR_NS::TfToken> transformTokens;
    buildTransformProperty(prim, transformTokens);
    
    // Build other prim properties
    buildProperties(prim, typeName, transformTokens);

    // Build prim components properties
    for (auto component : prim.GetAllChildren()) {
        // If we built from asset prim, skip component checking
        if (!NauUsdPrimUtils::isPrimComponent(component)) {
            continue;
        }
        
        const std::string componentTypeName = component.GetTypeName().GetString();
        //We are forced to postpone the construction of the UI so that the component has time to be created,
        //as its creation happens in asynchronous mode.
        QTimer* buildTimer = new QTimer(this);
        buildTimer->setSingleShot(true);
        connect(buildTimer, &QTimer::timeout, [this, component, componentTypeName, transformTokens]() {
            buildProperties(component, componentTypeName, transformTokens);
        });
        m_componentBuildTimers.push_back(buildTimer);
        buildTimer->start();
    }

    m_updateTimer.start(16);
}

void NauUsdInspectorClient::buildTransformProperty(PXR_NS::UsdPrim prim, PXR_NS::VtArray<PXR_NS::TfToken>& transformTokens)
{
    if (auto xform = PXR_NS::UsdGeomXformable(prim)) {

        PXR_NS::UsdGeomXformCache cache;
        bool resetsStack = false;
        PXR_NS::VtValue transformVal(cache.GetLocalTransformation(prim, &resetsStack));

        // TODO: Make it so that the tranform attribute is also created through the factory in the future
        
        auto transformOp = xform.GetTransformOp();
        if (!transformOp)
        {
            transformOp = xform.AddTransformOp();
        }

        auto transfromPropertyWidget = createPropertyWidget(prim.GetPath(), transformVal, "matrix4d", "", transformOp.GetName());
        if (transfromPropertyWidget) {
            auto transfromSpoiler = m_inspector->addComponent(PXR_NS::UsdGeomTokens->Xformable.GetString());
            transfromSpoiler->addWidget(transfromPropertyWidget);
            m_inspector->addSpoiler(transfromSpoiler);
        }
    }
}

void NauUsdInspectorClient::buildProperties(const UsdProxy::UsdProxyPrim& proxyPrim, const std::string& typeName, const PXR_NS::VtArray<PXR_NS::TfToken>& transformTokensToSkip)
{
    NauComponentSpoiler* componentSpoiler = nullptr;

    // Trying to get resource information from prim references, if any
    for (auto stack : proxyPrim.getPrim().GetPrimStack()) {
        for (auto ref : stack->GetReferenceList().GetPrependedItems()) {

            // Get the path to an asset with meta information about the resource
            PXR_NS::SdfReference sdfRef = ref;

            // TODO: Delete!
            //const auto assetPath = sdfRef.GetAssetPath();
            //const auto primPath = proxyPrim.getPrim().GetStage()->GetRootLayer()->ComputeAbsolutePath(assetPath);
            //sdfRef.SetAssetPath(primPath);

            PXR_NS::VtValue val = PXR_NS::VtValue(sdfRef);

            // TODO: Store asset manager as module and get it from EditorServiceProvider
            // Or add asset manager getter to NauEditor interface
            auto assetManager = Nau::Editor().assetManager();

            // If we have resolved path to asset file - get it

            std::string newAssetPath = sdfRef.GetAssetPath();
            if (sdfRef.GetAssetPath().starts_with("../")) {
                newAssetPath = sdfRef.GetAssetPath().substr(3);
            }

            auto& assetProcessor = Nau::EditorServiceProvider().get<NauAssetFileProcessorInterface>();
            std::string sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(newAssetPath.c_str());

            auto& assetDb = nau::getServiceProvider().get<nau::IAssetDB>();
            if (sourcePath.empty()) {

                std::string uidStr = "";
                if (newAssetPath.starts_with("uid:")) {
                    uidStr = newAssetPath.substr(4);
                }

                auto uid = nau::Uid::parseString(uidStr);

                sourcePath = nau::FileSystemExtensions::resolveToNativePathContentFolder(assetDb.getNausdPathFromUid(*uid).c_str());
            }

            auto m_assetType = assetManager->typeResolver()->resolve(sourcePath.c_str()).type;

            // Create a resource widget
            // TODO: But with a fake token
            auto propertyWidget = createReferenceWidget(proxyPrim.getPrim().GetPath(), val, "reference", magic_enum::enum_name(m_assetType).data());

            if (propertyWidget) {
                if (!componentSpoiler) componentSpoiler = m_inspector->addComponent("asset");
                componentSpoiler->addWidget(propertyWidget);
            }
        }
    }

    for (auto prop : proxyPrim.getProperties()) {
        // Skip invisible properties
        pxr::VtValue visibilityValue;
        if (prop.second->getMetadata(pxr::TfToken("visible"), visibilityValue)) {
            if (visibilityValue.IsHolding<bool>() && !visibilityValue.Get<bool>()) {
                continue;
            }
        }

        // Skip transform properties operations
        if (std::find(transformTokensToSkip.begin(), transformTokensToSkip.end(), prop.second->getName()) != transformTokensToSkip.end()) {
            continue;
        }

        // Getting information about usd data type
        auto valueTypeName = prop.second->getTypeName().GetAsToken().GetString();

        std::string valueNamespace = prop.second->getNamespace().GetString();

        PXR_NS::VtValue val;

        // TODO: The value may not be initialized, you need to check this condition in widgets
        prop.second->getValue(&val);
        auto propertyName = prop.second->getName();
        
        // Form a key, by which we further select the necessary widget view constructor
        auto usdType = valueTypeName == "asset" || valueNamespace == "" ? valueTypeName : prop.second->getName().GetString();

        // TODO: Fix me later
        if (propertyName.GetString() == "physics:collisionChannelSettings") {
            usdType = prop.second->getName().GetString();
        }

        if (propertyName.GetString() == "physics:collisionChannel") {
            usdType = prop.second->getName().GetString();
        }

        if (propertyName.GetString() == "PhysicsMaterial:material") {
            usdType = prop.second->getName().GetString();
        }

        // TODO: Delete!
        //if (valueTypeName == "asset") {
        //    const std::string assetPath = val.Get<pxr::SdfAssetPath>().GetAssetPath();
        //    val = pxr::SdfAssetPath(proxyPrim.getPrim().GetStage()->GetRootLayer()->ComputeAbsolutePath(assetPath));
        //}

        auto propertyWidget = createPropertyWidget(proxyPrim.getPrim().GetPath(), val, usdType, valueNamespace, propertyName);

        if (propertyWidget) {
            if (!componentSpoiler) componentSpoiler = m_inspector->addComponent(typeName);
            componentSpoiler->addWidget(propertyWidget);
        }
    }

    if (componentSpoiler) {
        m_inspector->addSpoiler(componentSpoiler);
        const pxr::SdfPath primPath = proxyPrim.getPrim().GetPath();
        m_inspector->connect(componentSpoiler, &NauComponentSpoiler::eventComponentRemoved, m_inspector, [this, primPath]() {
            emit eventComponentRemoved(primPath);
        });
    }
}

void NauUsdInspectorClient::stopComponentBuildTimers() {
    for (QTimer* timer : m_componentBuildTimers) {
        timer->stop();
        timer->deleteLater();
    }
    m_componentBuildTimers.clear();
}

void NauUsdInspectorClient::updateFromPrim(PXR_NS::UsdPrim prim)
{
    QSignalBlocker blocker(this);
    UsdProxy::UsdProxyPrim proxy(prim);
    
    // Now we only update if the transform has changed
    // TODO: Update all properties
    if (!m_needRebuild && m_needUpdate) {
        auto transformOpToken = pxr::UsdGeomXformOp::GetOpName(pxr::UsdGeomXformOp::TypeTransform);
        if (auto transformProperty = m_propertyMap.find(transformOpToken.GetString()); transformProperty != m_propertyMap.end()) {
            const auto transformWidget = transformProperty->second;
            if (!transformWidget) {
                return;
            }
            auto transformOld = transformWidget->getValue().Get<pxr::GfMatrix4d>();;

            PXR_NS::UsdGeomXformCache cache;
            [[maybe_unused]] bool resetsStack = false;
            auto transformNew = cache.GetLocalTransformation(prim, &resetsStack);

            if (transformOld != transformNew) {
                PXR_NS::VtValue transformValueNew(transformNew);
                transformWidget->setValue(transformValueNew);
                return;
            }
        }
    }

    if (m_needRebuild) {
        buildFromPrimInternal(prim);
    }
}

void NauUsdInspectorClient::tick()
{
    if (!m_currentPrimPath.IsEmpty() && (m_needUpdate || m_needRebuild)) {
        auto prim = m_currentScene->GetPrimAtPath(m_currentPrimPath);
        updateFromPrim(prim);
        m_needUpdate = false;
        m_needRebuild = false;
    }
}

void NauUsdInspectorClient::clear()
{
    m_updateTimer.stop();
    m_needUpdate = false;
    m_needRebuild = false;
    m_propertyMap.clear();
    m_currentPrimPath = pxr::SdfPath::EmptyPath();
    m_currentScene = nullptr;
    m_inspector->clear();
    stopComponentBuildTimers();
}

NauUsdPropertyAbstract* NauUsdInspectorClient::createPropertyWidget(const PXR_NS::SdfPath& primPath, const PXR_NS::VtValue& value, const std::string& rawTypeName, const std::string& metaInfo, PXR_NS::TfToken propertyName)
{
    // TODO: Get type from val (VtValue)
    auto typeName = rawTypeName;

    auto propertyWidget = NauUsdPropertyFactory::createPropertyWidget(typeName, propertyName.GetString(), metaInfo);

    if (propertyWidget != nullptr) {

        // TODO: Create property with value directly?
        propertyWidget->setValue(value);

        m_inspector->connect(propertyWidget, &NauUsdPropertyAbstract::eventValueChanged, m_inspector, [this, primPath, propertyName, propertyWidget]() {
            emit eventPropertyChanged(primPath, pxr::TfToken(propertyName), propertyWidget->getValue());
        });

        m_propertyMap[propertyName.GetString()] = propertyWidget;
    }

    return propertyWidget;
}

NauUsdPropertyAbstract* NauUsdInspectorClient::createReferenceWidget(const PXR_NS::SdfPath& primPath, const PXR_NS::VtValue& value, const std::string& rawTypeName, const std::string& metaInfo)
{
    auto propertyWidget = NauUsdPropertyFactory::createPropertyWidget(rawTypeName, "asset", metaInfo);

    if (propertyWidget != nullptr) {

        // TODO: Create property with value directly?
        propertyWidget->setValue(value);

        m_inspector->connect(propertyWidget, &NauUsdPropertyAbstract::eventValueChanged, m_inspector, [this, primPath, propertyWidget]() {
            emit eventAssetReferenceChanged(primPath, propertyWidget->getValue());
        });
    }

    return propertyWidget;
}

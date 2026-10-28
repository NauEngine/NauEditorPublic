// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/nau_usd_scene_synchronizer.hpp"
#include "nau_camera_visuals.hpp"
#include "nau/io/virtual_file_system.h"
#include "usd_translator/usd_component_adapter.h"
#include "usd_proxy/usd_stage_watcher.h"
#include <QCoreApplication>
#include "nau/scene/scene_manager.h"
#include "nau/scene/scene_factory.h"
#include "nau/service/service_provider.h"

#include "nau/assets/asset_ref.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/camera/camera_manager.h"

#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"

#include "pxr/usd/usd/primRange.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "nau/assets/asset_manager.h"

#include "nau_log.hpp"

#include "nau/scene/components/billboard_component.h"
#include "nau/scene/scene_factory.h"
#include "magic_enum/magic_enum_containers.hpp"

namespace
{
    const UsdTranslator::ComponentAdapter* componentAdapter(const UsdTranslator::IPrimAdapter& adapter)
    {
        // UsdTranslator is built without C++ RTTI. Its registered adapter type
        // identifies the two ComponentAdapter implementations used by cameras.
        const std::string_view type = adapter.getType();
        return type == "NauComponent" || type == "NauSceneComponent" ? static_cast<const UsdTranslator::ComponentAdapter*>(&adapter) : nullptr;
    }

    template <typename TPredicate>
    UsdTranslator::IPrimAdapter::Ptr findAdapter(UsdTranslator::IPrimAdapter::Ptr adapter, const TPredicate& matches)
    {
        if (!adapter || matches(adapter))
        {
            return adapter;
        }
        for (const auto& [name, child] : adapter->getChildren())
        {
            if (UsdTranslator::IPrimAdapter::Ptr found = findAdapter(child, matches))
            {
                return found;
            }
        }
        return nullptr;
    }

    nau::math::Transform cameraTransformFromMatrix(const pxr::GfMatrix4d& matrix)
    {
        const pxr::GfVec3d translation = matrix.ExtractTranslation();
        pxr::GfVec3d scale;
        pxr::GfMatrix4d rotationMatrix(1.0);
        int axisCount = 0;
        int lastAxis = 0;
        for (int axis = 0; axis < 3; ++axis)
        {
            const pxr::GfVec3d row = matrix.GetRow3(axis);
            scale[axis] = row.GetLength();
            if (scale[axis] > 0.0)
            {
                rotationMatrix.SetRow3(axis, row / scale[axis]);
                ++axisCount;
                lastAxis = axis;
            }
        }

        pxr::GfQuatd rotation(1.0);
        if (axisCount >= 2)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                if (scale[axis] == 0.0)
                {
                    rotationMatrix.SetRow3(axis, pxr::GfCross(rotationMatrix.GetRow3((axis + 1) % 3), rotationMatrix.GetRow3((axis + 2) % 3)).GetNormalized());
                }
            }
            if (rotationMatrix.GetDeterminant3() < 0.0)
            {
                rotationMatrix.SetRow3(0, -rotationMatrix.GetRow3(0));
                scale[0] = -scale[0];
            }
            rotationMatrix.Orthonormalize(false);
            rotation = rotationMatrix.ExtractRotationQuat().GetNormalized();
        }
        else if (axisCount == 1)
        {
            rotation = pxr::GfRotation(pxr::GfVec3d::Axis(lastAxis), rotationMatrix.GetRow3(lastAxis)).GetQuat();
        }

        const pxr::GfVec3d imaginary = rotation.GetImaginary();
        return nau::math::Transform(
            {static_cast<float>(imaginary[0]), static_cast<float>(imaginary[1]), static_cast<float>(imaginary[2]), static_cast<float>(rotation.GetReal())},
            {static_cast<float>(translation[0]), static_cast<float>(translation[1]), static_cast<float>(translation[2])},
            {static_cast<float>(scale[0]), static_cast<float>(scale[1]), static_cast<float>(scale[2])});
    }

    nau::math::Transform cameraWorldTransform(const UsdTranslator::IPrimAdapter::Ptr& rootAdapter, const nau::scene::CameraComponent& camera)
    {
        const UsdTranslator::IPrimAdapter::Ptr adapter = findAdapter(rootAdapter, [&camera](const UsdTranslator::IPrimAdapter::Ptr& candidate)
        {
            const UsdTranslator::ComponentAdapter* adapter = componentAdapter(*candidate);
            return adapter && adapter->getComponent().get() == &camera;
        });
        if (!adapter || !adapter->getPrim())
        {
            return camera.getWorldTransform();
        }
        pxr::UsdGeomXformCache cache;
        return cameraTransformFromMatrix(cache.GetLocalToWorldTransform(adapter->getPrim()));
    }

    bool cameraTransformsChanged(const pxr::UsdNotice::ObjectsChanged& notice)
    {
        if (!notice.GetResyncedPaths().empty())
        {
            return true;
        }
        for (const pxr::SdfPath& path : notice.GetChangedInfoOnlyPaths())
        {
            if (path.IsPropertyPath() && pxr::UsdGeomXformable::IsTransformationAffectedByAttrNamed(path.GetNameToken()))
            {
                return true;
            }
        }
        return false;
    }
}

// ** NauEngineSceneListener

NauEngineSceneListener::NauEngineSceneListener(pxr::UsdStageRefPtr editorScene, nau::scene::IScene::WeakRef engineScene)
    : m_editorCurrentScene(editorScene)
    , m_engineCurrentScene(engineScene)
{
}

void NauEngineSceneListener::onSceneBegin()
{
}

void NauEngineSceneListener::onSceneEnd()
{
}

void NauEngineSceneListener::onAfterActivatingObjects(eastl::span<const nau::scene::SceneObject*> objects)
{
    for (auto object : objects) {
        onActivateObject(object);
    }
}

void NauEngineSceneListener::onActivateObject(const nau::scene::SceneObject* object)
{
    if (m_objectsMap.contains(object->getUid())) {
        return;
    }

    const pxr::TfToken objectType(object->getRootComponent().getClassDescriptor()->getClassName().c_str());
    const std::string displayName = object->getName() == "/" || object->getName().empty() ? "Prim" : object->getName().data();

    const pxr::GfMatrix4d initialTransform = NauUsdEditorMathUtils::nauMatrixToGfMatrix(object->getTransform().getMatrix());
        
    std::function<std::string(nau::scene::SceneObject::WeakRef parentObject)> constructPath;

    std::string parentPath = "";
    if (object->getParentObject()) {
        constructPath = [&constructPath](nau::scene::SceneObject::WeakRef parentObject) {
            std::string result;
            const std::string name = parentObject->getName() == "/" || parentObject->getName().empty() ? "Prim" : parentObject->getName().data();
            result = std::format("{}", name);

            if (parentObject->getParentObject() == nullptr) {
                return result;
            }

            return std::format("{}/{}", constructPath(parentObject->getParentObject()), result);
        };
        parentPath = std::format("/{}", constructPath(object->getParentObject()));
    }

    const pxr::SdfPath primPath(NauUsdSceneUtils::generateUniquePrimPath(m_editorCurrentScene, parentPath, displayName));

    const bool isComponent = false;
    auto prim = NauUsdPrimFactory::instance().createPrim(m_editorCurrentScene, primPath, objectType, displayName, initialTransform, isComponent);

    m_objectsMap[object->getUid()] = prim.GetPrimPath();

    for (auto child : const_cast<nau::scene::SceneObject*>(object)->getChildObjects(false)) {
        onActivateObject(child);
    }
}

void NauEngineSceneListener::onBeforeDeletingObjects(eastl::span<const nau::scene::SceneObject*> objects)
{
    for (auto object : objects) {
        if (auto primPath = m_objectsMap.find(object->getUid()); primPath != m_objectsMap.end()) {
            m_editorCurrentScene->RemovePrim(primPath->second);
            m_objectsMap.erase(object->getUid());
        }
    }
}

void NauEngineSceneListener::onAfterActivatingComponents(eastl::span<const nau::scene::Component*> components)
{
    for (auto component : components) {
        if (m_objectsMap.contains(component->getUid())) {
            continue;
        }

        const pxr::TfToken objectType(component->getClassDescriptor()->getClassName().c_str());
        const std::string displayName = objectType.GetString();

        pxr::GfMatrix4d initialTransform;
        if (component->is<nau::scene::SceneComponent>()) {
            initialTransform = NauUsdEditorMathUtils::nauMatrixToGfMatrix(component->as<const nau::scene::SceneComponent*>()->getTransform().getMatrix());
        }

        std::function<std::string(nau::scene::SceneObject::WeakRef parentObject)> constructPath;
        constructPath = [&constructPath](nau::scene::SceneObject::WeakRef parentObject) {
            std::string result;
            result = std::format("/{}", parentObject->getName().data());

            if (parentObject->getParentObject() == nullptr) {
                return result;
            }

            return std::format("{}/{}", result, constructPath(parentObject->getParentObject()));
        };

        const std::string parentPath = std::format("{}/{}", constructPath(component->getParentObject()), displayName);

        const pxr::SdfPath primPath(NauUsdSceneUtils::generateUniquePrimPath(m_editorCurrentScene, parentPath, displayName));

        const bool isComponent = true;
        auto prim = NauUsdPrimFactory::instance().createPrim(m_editorCurrentScene, primPath, objectType, displayName, initialTransform, isComponent);

        m_objectsMap[component->getUid()] = prim.GetPrimPath();
    }
}

void NauEngineSceneListener::onBeforeDeletingComponents(eastl::span<const nau::scene::Component*> components)
{
    for (auto component : components) {
        if (auto primPath = m_objectsMap.find(component->getUid()); primPath != m_objectsMap.end()) {
            m_editorCurrentScene->RemovePrim(primPath->second);
            m_objectsMap.erase(component->getUid());
        }
    }
}

void NauEngineSceneListener::onComponentsChange(eastl::span<const nau::scene::Component*> components)
{

}

// ** NauUsdSceneSynchronizer

NauUsdSceneSynchronizer::NauUsdSceneSynchronizer()
    : m_sceneListener(nullptr)
    , m_syncMode(SyncMode::None)
{
    m_billboardIcons[BillboardTypes::SphereLight] = "file:/res/Images/billbords/Banner_Light_Bulb.png";
    m_billboardIcons[BillboardTypes::DirectionalLightComponent] = "file:/res/Images/billbords/Banner_Light_Direct.png";
    m_billboardIcons[BillboardTypes::VFXInstance] = "file:/res/Images/billbords/Banner_VFX.png";
    m_billboardIcons[BillboardTypes::AudioComponentEmitter] = "file:/res/Images/billbords/Banner_Sound_Emitter.png";
    m_billboardIcons[BillboardTypes::AudioComponentListener] = "file:/res/Images/billbords/Banner_Sound_Listener.png";
    m_billboardIcons[BillboardTypes::AudioEmitter] = "file:/res/Images/billbords/Banner_Sound_Emitter.png";
}

NauUsdSceneSynchronizer::~NauUsdSceneSynchronizer()
{
}

void NauUsdSceneSynchronizer::startSyncFromEditorScene(pxr::UsdStageRefPtr scene)
{
    if (m_syncMode == SyncMode::FromEngine) {
        stopSyncFromEngineSceneInternal();
    } else if (!scene) {
        stopSyncFromEditorSceneInternal();
        return;
    }

    startSyncFromEditorSceneInternal(scene);
}

void NauUsdSceneSynchronizer::startSyncFromEditorSceneInternal(pxr::UsdStageRefPtr editorScene)
{
    if (m_engineCurrentScene) {
        NED_ERROR("Error loading a new scene. Current engine scene has not been unloaded.");
        return;
    }

    m_editorCurrentScene = editorScene;
    auto sceneCreateTask = [this]() -> nau::async::Task<>
    {
        auto& sceneFactory = nau::getServiceProvider().get<nau::scene::ISceneFactory>();
        auto newEngineScene = sceneFactory.createEmptyScene();
      
        m_sceneTranslator = std::make_unique<UsdTranslator::StageTranslator>();
        m_sceneTranslator->setSource(m_editorCurrentScene);
        m_sceneTranslator->setTarget(*newEngineScene);
        co_await m_sceneTranslator->initScene();
        m_sceneTranslator->follow();

        // Create camera
        auto& cameraManager = nau::getServiceProvider().get<nau::scene::ICameraManager>();

        // Create detached camera in default world
        auto cameraControl = cameraManager.createDetachedCamera();
        cameraManager.setMainCamera(cameraControl->getCameraUid());

        // Set initial camera params
        // Will be overwritten from user settings
        cameraControl->setClipNearPlane(0.1);
        cameraControl->setClipFarPlane(1000);
        cameraControl->setFov(90);
        cameraControl->setCameraName(NauEditorCameraManager::MainCameraName);
            
        // Set active camera to editor camera manager 
        Nau::EditorEngine().cameraManager()->setActiveCamera(cameraControl);

        auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();
        m_engineCurrentScene = co_await sceneManager.getDefaultWorld().addScene(std::move(newEngineScene));
        // Mount only camera resources, once per VFS lifetime: unmount is not implemented
        nau::io::IVirtualFileSystem& fileSystem = nau::getServiceProvider().get<nau::io::IVirtualFileSystem>();
        if (!fileSystem.exists("/editor_camera", nau::io::FsEntryKind::Directory))
        {
            const QString meshDirectory = QCoreApplication::applicationDirPath() + "/editor_resources/meshes";
            co_await fileSystem.mount("/editor_camera", nau::io::createNativeFileSystem(meshDirectory.toUtf8().toStdString()));
        }
        m_cameraVisuals = eastl::make_unique<nau::editor::CameraVisuals>(*m_engineCurrentScene, [this](const nau::scene::CameraComponent& camera)
        {
            return cameraWorldTransform(m_sceneTranslator->getRootAdapter(), camera);
        });
        co_await m_cameraVisuals->activate();
        m_cameraTransformWatcher = std::make_unique<UsdProxy::StageObjectChangedWatcher>(m_editorCurrentScene, [this](const pxr::UsdNotice::ObjectsChanged& notice)
        {
            if (cameraTransformsChanged(notice))
            {
                m_cameraVisuals->updateTransforms();
            }
        });
    };

    Nau::EditorEngine().runTaskSync(sceneCreateTask().detach());
    m_syncMode = SyncMode::FromEditor;
}

void NauUsdSceneSynchronizer::stopCameraVisuals()
{
    m_cameraTransformWatcher.reset();
    if (m_cameraVisuals)
    {
        Nau::EditorEngine().runTaskSync(m_cameraVisuals->shutdown().detach());
        m_cameraVisuals.reset();
    }
}

void NauUsdSceneSynchronizer::stopSyncFromEditorSceneInternal()
{
    stopCameraVisuals();
    if (!m_engineCurrentScene) {
        return;
    }

    m_sceneTranslator.reset();

    nau::getServiceProvider().get<nau::scene::ISceneManager>().deactivateScene(m_engineCurrentScene);
    m_engineCurrentScene = nullptr;

    auto& cameraManager = nau::getServiceProvider().get<nau::scene::ICameraManager>();
    cameraManager.resetWorldMainCamera();

    m_syncMode = SyncMode::None;
}

void NauUsdSceneSynchronizer::startSyncFromEngineScene(pxr::UsdStageRefPtr emptyEditorScene, const std::string& scenePath)
{
    if (m_syncMode == SyncMode::FromEditor) {
        stopSyncFromEditorSceneInternal();
    } else if (scenePath.empty()) {
        stopSyncFromEngineSceneInternal();
        return;
    }

    startSyncFromEngineSceneInternal(emptyEditorScene, scenePath);
}

void NauUsdSceneSynchronizer::startSyncFromEngineSceneInternal(pxr::UsdStageRefPtr emptyEditorScene, const std::string& scenePath)
{
    if (m_engineCurrentScene) {
        NED_ERROR("Error loading a new scene. Current engine scene has not been unloaded.");
        return;
    }

    m_editorCurrentScene = emptyEditorScene;
    auto sceneCreateTask = [this, &scenePath]() -> nau::async::Task<>
    {          
        nau::IAssetDescriptor::Ptr asset = nau::getServiceProvider().get<nau::IAssetManager>().openAsset(nau::strings::toStringView(scenePath));
        asset->unload();
        asset->load();
        nau::SceneAsset::Ptr sceneAsset = co_await asset->getAssetView();

        auto scene = nau::getServiceProvider().get<nau::scene::ISceneFactory>().createSceneFromAsset(*sceneAsset);
        auto& sceneManager = nau::getServiceProvider().get<nau::scene::ISceneManager>();

        // TODO: Uncomment when the broadcast from the engine scene to usd is completed 
        //m_sceneListener = std::make_shared<NauEngineSceneListener>(m_editorCurrentScene, m_engineCurrentScene);
        //m_sceneListenerReg = nau::getServiceProvider().get<nau::scene::ISceneManagerInternal>().addSceneListener(*m_sceneListener);
        m_engineCurrentScene = co_await sceneManager.getDefaultWorld().addScene(std::move(scene));
    };

    Nau::EditorEngine().runTaskSync(sceneCreateTask().detach());
    m_syncMode = SyncMode::FromEngine;
}

void NauUsdSceneSynchronizer::stopSyncFromEngineSceneInternal()
{
    if (!m_engineCurrentScene) {
        return;
    }
    
    // TODO: Uncomment when the broadcast from the engine scene to usd is completed 
    //m_sceneListenerReg.reset();
    //m_sceneListener.reset();

    nau::getServiceProvider().get<nau::scene::ISceneManager>().deactivateScene(m_engineCurrentScene);
    m_engineCurrentScene = nullptr;

    m_syncMode = SyncMode::None;
}

UsdTranslator::StageTranslator* NauUsdSceneSynchronizer::translator() const
{
    return m_sceneTranslator.get();
}

pxr::SdfPath NauUsdSceneSynchronizer::primFromCameraMesh(const nau::Uid& uid) const
{
    if (!m_cameraVisuals || !m_sceneTranslator)
    {
        return {};
    }
    const nau::Uid cameraUid = m_cameraVisuals->cameraUidForMesh(uid);
    if (!cameraUid)
    {
        return {};
    }
    const UsdTranslator::IPrimAdapter::Ptr adapter = findAdapter(m_sceneTranslator->getRootAdapter(), [&cameraUid](const UsdTranslator::IPrimAdapter::Ptr& candidate)
    {
        const UsdTranslator::ComponentAdapter* adapter = componentAdapter(*candidate);
        return adapter && adapter->getComponent()
            && adapter->getComponent()->getUid() == cameraUid;
    });
    return adapter ? adapter->getPrimPath() : pxr::SdfPath{};
}

std::vector<nau::Uid> NauUsdSceneSynchronizer::cameraMeshUidsForPrim(const pxr::SdfPath& path) const
{
    std::vector<nau::Uid> result;
    if (!m_cameraVisuals || !m_sceneTranslator)
    {
        return result;
    }
    const UsdTranslator::IPrimAdapter::Ptr adapter = findAdapter(m_sceneTranslator->getRootAdapter(), [&path](const UsdTranslator::IPrimAdapter::Ptr& candidate)
    {
        return candidate->getPrimPath() == path;
    });
    if (!adapter)
    {
        return result;
    }
    if (const UsdTranslator::ComponentAdapter* cameraAdapter = componentAdapter(*adapter))
    {
        if (nau::scene::ObjectWeakRef<nau::scene::Component> component = cameraAdapter->getComponent(); component && component->is<nau::scene::CameraComponent>())
        {
            if (nau::Uid uid = m_cameraVisuals->meshUidForCamera(component->getUid()))
            {
                result.push_back(uid);
            }
            return result;
        }
    }
    if (nau::scene::SceneObject::WeakRef object = adapter->getSceneObject())
    {
        for (const nau::scene::Component* camera : object->getDirectComponents<nau::scene::CameraComponent>())
        {
            if (nau::Uid uid = m_cameraVisuals->meshUidForCamera(camera->getUid()))
            {
                result.push_back(uid);
            }
        }
    }
    return result;
}

nau::scene::SceneObject::WeakRef NauUsdSceneSynchronizer::sceneObjectByPrimPath(const pxr::SdfPath& path) const
{
    if (!m_sceneTranslator) {
        return nullptr;
    }

    auto rootAdapter = m_sceneTranslator->getRootAdapter();
    if (!rootAdapter) {
        return nullptr;
    }

    std::function<nau::scene::SceneObject::WeakRef(UsdTranslator::IPrimAdapter::Ptr)> findSceneObject;
    findSceneObject = [this, &findSceneObject, path](UsdTranslator::IPrimAdapter::Ptr adapter) {
        if (path == adapter->getPrimPath()) {
            return adapter->getSceneObject();
        }

        nau::scene::SceneObject::WeakRef sceneObject = nullptr;
        for (auto child : adapter->getChildren()) {
            sceneObject = findSceneObject(child.second);
            if (sceneObject) {
                break;
            }
        }

        return sceneObject;
    };

    return findSceneObject(rootAdapter);
}

void NauUsdSceneSynchronizer::focusOnObject(const pxr::SdfPath& primPath) const
{
    nau::scene::SceneObject::WeakRef object = sceneObjectByPrimPath(primPath);
    if (object) {
        Nau::EditorEngine().viewportManager()->mainViewport()->controller()->focusOnObject(object);
    } else {
        NED_ERROR("Focus operation failed");
    }
}

void NauUsdSceneSynchronizer::addBillboard(const pxr::SdfPath& primPath, const std::string& primType) const
{
    std::string str = "nau::scene::";
    auto normalizePrimType = primType;
    if (primType.starts_with(str)) {
        normalizePrimType = primType.substr(str.size());
    }

    auto type = magic_enum::enum_cast<BillboardTypes>(normalizePrimType).value_or(BillboardTypes::Unrecognized);
    if (!m_billboardIcons.contains(type)) {
        return;
    }

    QTimer::singleShot(200, [this, type, primPath]() {
        nau::scene::SceneObject* object = sceneObjectByPrimPath(primPath).get();
        auto& factory = nau::getServiceProvider().get<nau::scene::ISceneFactory>();

        auto& billboardObj = object->addComponent<nau::scene::BillboardComponent>();
        billboardObj.setScale({ 1.0f, 1.0f, 1.0f });
        billboardObj.setTextureRef(eastl::string_view(m_billboardIcons.at(type).c_str()));
        billboardObj.setScreenPercentageSize(0.025f);
    });
}

// TODO: Use fast access containers from UsdTranslator with SdfPath-Uid pair, when it will be implemented
pxr::SdfPath NauUsdSceneSynchronizer::primFromSceneObject(const nau::Uid& uid)
{
    auto rootAdapter = m_sceneTranslator->getRootAdapter();
    if (!rootAdapter) {
        return pxr::SdfPath("");
    }

    std::function<pxr::SdfPath(UsdTranslator::IPrimAdapter::Ptr)> findPrim;
    findPrim = [this, &findPrim, &uid](UsdTranslator::IPrimAdapter::Ptr adapter) {
        if (!adapter || !adapter->getSceneObject()) {
            return pxr::SdfPath("");
        }

        const nau::Uid objUid = adapter->getSceneObject()->getUid();
        if (uid == objUid) {
            return adapter->getPrimPath();
        }

        pxr::SdfPath primPath;
        for (auto child : adapter->getChildren()) {
            primPath = findPrim(child.second);
            if (!primPath.IsEmpty()) {
                break;
            }
        }

        return primPath;
    };

    return findPrim(rootAdapter);
}

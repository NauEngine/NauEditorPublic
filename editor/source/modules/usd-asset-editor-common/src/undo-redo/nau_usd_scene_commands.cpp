// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/undo-redo/nau_usd_scene_commands.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau_log.hpp"

#include "pxr/base/tf/errorMark.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/sdf/namespaceEdit.h"
#include "pxr/usd/sdf/changeBlock.h"
#include "pxr/usd/sdf/copyUtils.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usdGeom/xformCache.h"

#include <filesystem>


// ** NauAbstractUsdSceneCommand

NauAbstractUsdSceneCommand::NauAbstractUsdSceneCommand(pxr::UsdStageRefPtr scene)
    : m_currentScene(scene)
{
}


// ** NauCommandCreateUsdPrim

NauCommandCreateUsdPrim::NauCommandCreateUsdPrim(
    pxr::UsdStageRefPtr scene,
    const std::string& path,
    const std::string& typeName,
    const std::string& name,
    const PXR_NS::GfMatrix4d& initialTransform,
    bool isComponent
    )
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_typeName(typeName)
    , m_displayName(name)
    , m_initialTransform(initialTransform)
    , m_isComponent(isComponent)
{
}

void NauCommandCreateUsdPrim::execute()
{
    NauUsdPrimFactory::instance().createPrim(m_currentScene, pxr::SdfPath(m_path), pxr::TfToken(m_typeName), m_displayName, m_initialTransform, m_isComponent);

    NED_DEBUG("Execute object creation command, path: {}", m_path);
}

void NauCommandCreateUsdPrim::undo()
{
    NED_DEBUG("Undo object creation command, path: {}", m_path);

    m_currentScene->RemovePrim(pxr::SdfPath(m_path));
    
}

NauCommandDescriptor NauCommandCreateUsdPrim::description() const
{
    return {
        .id = id,
        .name = "Object creation",
        .objectId = m_path.c_str()
    };
}


// ** NauCommandRemoveUsdPrim

NauCommandRemoveUsdPrim::NauCommandRemoveUsdPrim(pxr::UsdStageRefPtr scene, const std::string& path)
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
{
}

void NauCommandRemoveUsdPrim::execute()
{
    const pxr::SdfPath path(m_path);
    const pxr::SdfLayerHandle layer = m_currentScene->GetRootLayer();

    // Keep a copy of everything the root layer holds for the prim, so undo can bring it back unchanged
    m_removedSpec.Reset();
    m_siblingIndex.reset();
    if (pxr::SdfPrimSpecHandle parentSpec = layer->GetPrimAtPath(path.GetParentPath())) {
        size_t index = 0;
        for (const pxr::SdfPrimSpecHandle& sibling : parentSpec->GetNameChildren()) {
            if (sibling->GetPath() == path) {
                m_siblingIndex = index;
                break;
            }
            ++index;
        }
    }

    if (m_siblingIndex) {
        m_removedSpec = pxr::SdfLayer::CreateAnonymous();
        if (path.GetParentPath() != pxr::SdfPath::AbsoluteRootPath()) {
            pxr::SdfCreatePrimInLayer(m_removedSpec, path.GetParentPath());
        }
        pxr::SdfCopySpec(layer, path, m_removedSpec, path);
    }

    m_currentScene->RemovePrim(path);

    NED_DEBUG("Execute object remove command, path: {}", m_path);
}

void NauCommandRemoveUsdPrim::undo()
{
    if (!m_removedSpec) {
        NED_WARNING("Undo object remove command: prim has no spec in the root layer, nothing to restore, path: {}", m_path);
        return;
    }

    const pxr::SdfPath path(m_path);
    const pxr::SdfLayerHandle layer = m_currentScene->GetRootLayer();

    // One change notice for the whole restored prim
    pxr::SdfChangeBlock changeBlock;

    if (path.GetParentPath() != pxr::SdfPath::AbsoluteRootPath()) {
        pxr::SdfCreatePrimInLayer(layer, path.GetParentPath());
    }

    if (!pxr::SdfCopySpec(m_removedSpec, path, layer, path)) {
        NED_WARNING("Undo object remove command failed to restore prim, path: {}", m_path);
        return;
    }

    // The copied spec is appended to its parent; put it back where it was among its siblings.
    // USD reorders correctly here but also reports a coding error ("Cannot move </B> to </B>"),
    // so its errors are dropped and the resulting order is checked instead.
    pxr::SdfBatchNamespaceEdit edit;
    edit.Add(pxr::SdfNamespaceEdit::Reorder(path, static_cast<pxr::SdfNamespaceEdit::Index>(*m_siblingIndex)));
    {
        pxr::TfErrorMark errorMark;
        layer->Apply(edit);
        errorMark.Clear();
    }

    const pxr::SdfPrimSpecHandle parentSpec = layer->GetPrimAtPath(path.GetParentPath());
    const auto siblings = parentSpec->GetNameChildren();
    if (*m_siblingIndex >= siblings.size() || siblings[*m_siblingIndex]->GetPath() != path) {
        NED_WARNING("Undo object remove command failed to restore prim order, path: {}", m_path);
    }

    NED_DEBUG("Undo object remove command, path: {}", m_path);
}

NauCommandDescriptor NauCommandRemoveUsdPrim::description() const
{
    return {
        .id = id,
        .name = "Object removing",
        .objectId = m_path.c_str()
    };
}


// ** NauCommandRenameUsdPrim

NauCommandRenameUsdPrim::NauCommandRenameUsdPrim(
    pxr::UsdStageRefPtr scene,
    const std::string& path,
    const std::string& newName
    )
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_newName(newName)
{
    m_oldName = m_currentScene->GetPrimAtPath(pxr::SdfPath(path)).GetDisplayName();
}

void NauCommandRenameUsdPrim::execute()
{
    auto prim = m_currentScene->GetPrimAtPath(pxr::SdfPath(m_path));
    prim.SetDisplayName(m_newName);

    NED_DEBUG("Execute object rename command, path: {}", m_path);
}

void NauCommandRenameUsdPrim::undo()
{
    auto prim = m_currentScene->GetPrimAtPath(pxr::SdfPath(m_path));
    prim.SetDisplayName(m_oldName);

    NED_DEBUG("Undo object rename command, path: {}", m_path);
    
}

NauCommandDescriptor NauCommandRenameUsdPrim::description() const
{
    return {
        .id = id,
        .name = "Object renaming",
        .objectId = m_path.c_str()
    };
}


// ** NauCommandReparentUsdPrim

NauCommandReparentUsdPrim::NauCommandReparentUsdPrim(pxr::UsdStageRefPtr scene,
    const std::string& currentPath, const std::string& newPath)
    : NauAbstractUsdSceneCommand(scene)
    , m_oldPath(currentPath)
    , m_newPath(newPath)
{
}

void NauCommandReparentUsdPrim::execute()
{
    if (reparentPrim(m_oldPath, m_newPath)) {
        NED_DEBUG("UndoRedo: Executed object reparent command successfully, from: {} to {}",
            m_oldPath.GetString(), m_newPath.GetString());
        return;
    }

    NED_WARNING("UndoRedo: Failed to reparent object from: {} to {}",
        m_oldPath.GetString(), m_newPath.GetString());
}

void NauCommandReparentUsdPrim::undo()
{
    if (reparentPrim(m_newPath, m_oldPath)) {
        NED_DEBUG("UndoRedo: Undo object reparent command successfully, from: {} to {}",
            m_newPath.GetString(), m_oldPath.GetString());
        return;
    }

    NED_WARNING("UndoRedo: Failed to reparent object from: {} to {}",
        m_newPath.GetString(), m_oldPath.GetString());
}

NauCommandDescriptor NauCommandReparentUsdPrim::description() const
{
    return {
        .id = id,
        .name = "Object reparent",
        .objectId = m_oldPath.GetString()
    };
}

bool NauCommandReparentUsdPrim::reparentPrim(PXR_NS::SdfPath& oldPath, PXR_NS::SdfPath& newPath)
{
    using namespace PXR_NS;

    const auto oldXform = NauUsdPrimUtils::worldPrimTransform(m_currentScene->GetPrimAtPath(oldPath));

    SdfBatchNamespaceEdit edit;
    edit.Add(SdfNamespaceEdit::Reparent(oldPath, newPath, SdfNamespaceEdit::AtEnd));

    if (!m_currentScene->GetRootLayer()->Apply(edit)) {
        return false;
    }

    newPath = newPath.AppendPath(SdfPath(oldPath.GetName()));
    oldPath = oldPath.GetParentPath();

    UsdPrim prim = m_currentScene->GetPrimAtPath(SdfPath(newPath));

    UsdGeomXformCache cache;
    NauUsdPrimUtils::setPrimTransform(prim, oldXform * cache.GetParentToWorldTransform(prim).GetInverse());

    return true;
}


// ** NauCommandChangeUsdPrimProperty

NauCommandChangeUsdPrimProperty::NauCommandChangeUsdPrimProperty(
    pxr::UsdStageRefPtr scene, 
    const PXR_NS::SdfPath& path, 
    const PXR_NS::TfToken& propName, 
    const PXR_NS::VtValue& value
    )
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_propName(propName)
    , m_value(value)
{
    if(auto proxyProp = proxyProperty(path, propName))
        proxyProp->getValue(&m_initialValue);
}

NauCommandChangeUsdPrimProperty::NauCommandChangeUsdPrimProperty(
    pxr::UsdStageRefPtr scene, 
    const PXR_NS::SdfPath& path, 
    const PXR_NS::TfToken& propName, 
    const PXR_NS::VtValue& initialValue,
    const PXR_NS::VtValue& newValue
    )
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_propName(propName)
    , m_value(newValue)
    , m_initialValue(initialValue)
{
}

UsdProxy::UsdProxyPropertyPtr NauCommandChangeUsdPrimProperty::proxyProperty(const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName)
{
    auto prim = m_currentScene->GetPrimAtPath(path);
    auto proxyPrim = UsdProxy::UsdProxyPrim(prim);
    return proxyPrim.getProperty(propName);
}

void NauCommandChangeUsdPrimProperty::execute()
{
    auto proxyProp = proxyProperty(m_path, m_propName);
    if (proxyProp)
    {
        proxyProp->setValue(m_value);
        NED_DEBUG("Execute object property changing command, path: {}", m_path.GetString());
    }
    else
    {
        NED_WARNING("Execute object property changing command failed, path: {}", m_path.GetString());
    }
}

void NauCommandChangeUsdPrimProperty::undo()
{
    auto proxyProp = proxyProperty(m_path, m_propName);
    proxyProp->setValue(m_initialValue);

    NED_DEBUG("Undo object property changing command, path: {}", m_path.GetString());
}

NauCommandDescriptor NauCommandChangeUsdPrimProperty::description() const
{
    return {
        .id = id,
        .name = "Object property changing",
        .objectId = m_path.GetString().c_str()
    };
}


// ** NauCommandChangeUsdPrimAssetReference

NauCommandChangeUsdPrimAssetReference::NauCommandChangeUsdPrimAssetReference(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& newValue)
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_value(newValue)
{
}

NauCommandChangeUsdPrimAssetReference::NauCommandChangeUsdPrimAssetReference(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue)
    : NauAbstractUsdSceneCommand(scene)
    , m_path(path)
    , m_value(newValue)
    , m_initialValue(initialValue)
{
}

void NauCommandChangeUsdPrimAssetReference::execute()
{
    m_previousReferences = PXR_NS::VtValue();
    if (PXR_NS::SdfPrimSpecHandle spec = m_currentScene->GetRootLayer()->GetPrimAtPath(m_path)) {
        m_previousReferences = spec->GetField(PXR_NS::SdfFieldKeys->References);
    }

    setPrimReference(m_value);

    NED_DEBUG("Execute asset reference changing command, path: {}", m_path.GetString());
}

void NauCommandChangeUsdPrimAssetReference::undo()
{
    if (!m_initialValue.IsEmpty()) {
        setPrimReference(m_initialValue);
        NED_DEBUG("Undo asset reference changing command, path: {}", m_path.GetString());
        return;
    }

    // No initial value was given (the usual case): put back the references the prim had before execute
    PXR_NS::SdfPrimSpecHandle spec = m_currentScene->GetRootLayer()->GetPrimAtPath(m_path);
    if (!spec) {
        NED_WARNING("Undo asset reference changing command failed, no prim spec, path: {}", m_path.GetString());
        return;
    }

    if (m_previousReferences.IsEmpty()) {
        spec->ClearField(PXR_NS::SdfFieldKeys->References);
    } else {
        spec->SetField(PXR_NS::SdfFieldKeys->References, m_previousReferences);
    }
    if (auto prim = m_currentScene->GetPrimAtPath(m_path)) {
        prim.Load();
    }

    NED_DEBUG("Undo asset reference changing command, path: {}", m_path.GetString());
}

NauCommandDescriptor NauCommandChangeUsdPrimAssetReference::description() const
{
    return {
        .id = id,
        .name = "Asset reference changing",
        .objectId = m_path.GetString().c_str()
    };
}

void NauCommandChangeUsdPrimAssetReference::setPrimReference(PXR_NS::VtValue value)
{
    auto prim = m_currentScene->GetPrimAtPath(m_path);
    auto typeName = prim.GetTypeName().GetString();
    prim.GetReferences().ClearReferences();

    PXR_NS::SdfReference ref = value.Get<PXR_NS::SdfReference>();

    prim.GetReferences().AddReference(ref);
    prim.Load();
}

// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Provides undo/redo commands for usd stage.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "commands/nau_commands.hpp"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/base/gf/matrix4d.h"

#include "usd_proxy/usd_proxy.h"

#include <optional>
#include <string>


// ** NauAbstractUsdSceneCommand

class NAU_USD_ASSET_EDITOR_COMMON_API NauAbstractUsdSceneCommand : public NauAbstractCommand
{
public:
    NauAbstractUsdSceneCommand() = delete;
    NauAbstractUsdSceneCommand(pxr::UsdStageRefPtr m_currentScene);

protected:
    pxr::UsdStageRefPtr m_currentScene;
};


// ** NauCommandCreateUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandCreateUsdPrim : public NauAbstractUsdSceneCommand
{
public:
    NauCommandCreateUsdPrim(
        pxr::UsdStageRefPtr scene,
        const std::string& path,
        const std::string& typeName,
        const std::string& displayName,
        const PXR_NS::GfMatrix4d& initialTransform,
        bool isComponent
    );

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    const PXR_NS::GfMatrix4d m_initialTransform;
    const std::string m_typeName;
    const std::string m_displayName;
    const std::string m_path;
    const bool m_isComponent;
};


// ** NauCommandRemoveUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRemoveUsdPrim : public NauAbstractUsdSceneCommand
{
public:
    NauCommandRemoveUsdPrim(pxr::UsdStageRefPtr scene, const std::string& path);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    // Copy of the removed prim spec (with attributes, metadata and children) taken from the root layer
    PXR_NS::SdfLayerRefPtr m_removedSpec;
    std::optional<size_t> m_siblingIndex;
    const std::string m_path;
};


// ** NauCommandRenameUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRenameUsdPrim : public NauAbstractUsdSceneCommand
{
public:
    NauCommandRenameUsdPrim(pxr::UsdStageRefPtr scene, const std::string& path, const std::string& newName);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    std::string m_oldName;
    std::string m_newName;
    const std::string m_path;
};


// ** NauCommandReparentUsdPrim

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandReparentUsdPrim : public NauAbstractUsdSceneCommand
{
public:
    NauCommandReparentUsdPrim(pxr::UsdStageRefPtr scene, const std::string& currentPath,
        const std::string& newPath);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    bool reparentPrim(PXR_NS::SdfPath& oldPath, PXR_NS::SdfPath& newPath);

    PXR_NS::SdfPath m_oldPath;
    PXR_NS::SdfPath m_newPath;
};


// ** NauCommandChangeUsdPrimProperty

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeUsdPrimProperty : public NauAbstractUsdSceneCommand
{
public:
    NauCommandChangeUsdPrimProperty(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName, const PXR_NS::VtValue& newValue);
    NauCommandChangeUsdPrimProperty(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    UsdProxy::UsdProxyPropertyPtr proxyProperty(const PXR_NS::SdfPath& path, const PXR_NS::TfToken& propName);

private:
    PXR_NS::VtValue m_initialValue;
    PXR_NS::VtValue m_value;

    const PXR_NS::TfToken m_propName;
    const PXR_NS::SdfPath m_path;
};


// ** NauCommandChangeUsdPrimProperty

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeUsdPrimAssetReference : public NauAbstractUsdSceneCommand
{
public:
    NauCommandChangeUsdPrimAssetReference(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& newValue);
    NauCommandChangeUsdPrimAssetReference(pxr::UsdStageRefPtr scene, const PXR_NS::SdfPath& path, const PXR_NS::VtValue& initialValue, const PXR_NS::VtValue& newValue);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    void setPrimReference(PXR_NS::VtValue value);

private:
    PXR_NS::VtValue m_initialValue;
    PXR_NS::VtValue m_value;

    // Root layer "references" field before execute; restored by undo when no initial value was given
    PXR_NS::VtValue m_previousReferences;

    const PXR_NS::SdfPath m_path;
};

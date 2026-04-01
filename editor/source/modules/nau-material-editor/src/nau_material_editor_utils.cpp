// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_material_editor_utils.hpp"

#include <filesystem>

#include "nau/io/virtual_file_system.h"
#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/service/service_provider.h"
#include "nau_log.hpp"
#include "pxr/usd/usd/stage.h"

pxr::UsdStageRefPtr NauMaterialEditorUtils::createMaterialPreviewScene()
{
    auto previewStage = pxr::UsdStage::CreateInMemory("Material.usda");
    auto previewSdfPath = pxr::SdfPath("/PreviewMesh");
    pxr::GfMatrix4d transform{};
    transform.SetIdentity();
    auto& vfs = nau::getServiceProvider().get<nau::io::IVirtualFileSystem>();
    auto meshesPath = std::filesystem::path(vfs.resolveToNativePath("/content/meshes"));
    if (std::filesystem::exists(meshesPath/"sphere.usda.nausd")) {
        transform.SetTranslate({0,1,0});
        auto creator = NauResourceUsdPrimCreator((meshesPath/"sphere.usda.nausd").string(), pxr::SdfPath("/Root/Sphere"));
        auto prim = creator.createPrim(previewStage, previewSdfPath, pxr::TfToken("NauAssetMesh"),"PreviewMesh", transform, false);
        previewStage->SetDefaultPrim(prim);
    }
    else if (std::filesystem::exists(meshesPath/"cube.usda.nausd")) {
        auto creator = NauResourceUsdPrimCreator((meshesPath/"sphere.usda.nausd").string(), pxr::SdfPath("/Root/Cube"));
        auto prim = creator.createPrim(previewStage, previewSdfPath, pxr::TfToken("NauAssetMesh"),"PreviewMesh", transform, false);
        previewStage->SetDefaultPrim(prim);
    }
    else {
        NED_ERROR("Sphere mesh not found in project. Defining empty preview mesh");
        auto prim = previewStage->DefinePrim(previewSdfPath);
        previewStage->SetDefaultPrim(prim);
    }
    return previewStage;
}
// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// USD backend of the editor document interface (USD migration, Stage 1). Together with src/document-usd/ this is
// where documents meet pxr. Only USD-side code uses this class directly: the synchronizer, tests and the asset
// editors that keep their own stages. Editors use NauDocumentInterface and nau_usd_document_factory.hpp.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau/document/nau_document_interface.hpp"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/sdf/reference.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace UsdProxy
{
    class StageObjectChangedWatcher;
}


// ** NauUsdDocument

class NAU_USD_ASSET_EDITOR_COMMON_API NauUsdDocument : public NauDocumentInterface
{
public:
    // Opens a scene file the way NauUsdSceneManager does: every Xformable gets a single transform op, every payload is loaded.
    // Returns nullptr if the file can't be opened.
    static std::shared_ptr<NauUsdDocument> open(const std::string& path);

    // Creates an empty file. Returns nullptr if it can't be created.
    static std::shared_ptr<NauUsdDocument> createNew(const std::string& path);

    static std::shared_ptr<NauUsdDocument> createInMemory();

    // Wraps a stage opened elsewhere, as it is
    static std::shared_ptr<NauUsdDocument> fromStage(PXR_NS::UsdStageRefPtr stage);

    explicit NauUsdDocument(PXR_NS::UsdStageRefPtr stage);
    ~NauUsdDocument() override;

    NauUsdDocument(const NauUsdDocument&) = delete;
    NauUsdDocument& operator=(const NauUsdDocument&) = delete;

    // USD side

    PXR_NS::UsdStageRefPtr stage() const;

    // Assigns an id the first time a path is asked for. Null if there's no prim at the path.
    NauDocumentNodeId nodeFromPath(const PXR_NS::SdfPath& path) const;

    // Empty if the id is unknown or its prim is gone
    PXR_NS::SdfPath pathFromNode(NauDocumentNodeId id) const;

    // A USD value for property key of the node, cast to the stored type first, as UsdAttribute::Set does.
    // nullopt if the node has no such property, or the value can't be stored in it.
    std::optional<NauDocumentValue> documentValue(NauDocumentNodeId id, const std::string& key, const PXR_NS::VtValue& value) const;

    static NauDocumentMat4 documentMatrix(const PXR_NS::GfMatrix4d& matrix);
    static NauDocumentAssetRef documentAssetReference(const PXR_NS::SdfReference& reference);

    // NauDocumentInterface

    NauDocumentNodeId rootNode() const override;
    bool contains(NauDocumentNodeId id) const override;
    std::optional<NauDocumentNodeInfo> nodeInfo(NauDocumentNodeId id) const override;
    std::vector<NauDocumentNodeId> children(NauDocumentNodeId id) const override;
    std::vector<NauDocumentNodeId> components(NauDocumentNodeId id) const override;

    NauDocumentNodeId createNode(const NauDocumentNodeCreateInfo& info) override;
    bool removeNode(NauDocumentNodeId id) override;
    NauDocumentNodeSnapshot snapshotNode(NauDocumentNodeId id) const override;
    bool restoreNode(const NauDocumentNodeSnapshot& snapshot) override;
    bool setDisplayName(NauDocumentNodeId id, const std::string& displayName) override;
    bool reparentNode(NauDocumentNodeId id, NauDocumentNodeId newParent) override;

    std::vector<NauDocumentPropertyDescriptor> properties(NauDocumentNodeId id) const override;
    std::optional<NauDocumentValue> property(NauDocumentNodeId id, const std::string& key) const override;
    bool setProperty(NauDocumentNodeId id, const std::string& key, const NauDocumentValue& value) override;

    NauDocumentMat4 worldTransform(NauDocumentNodeId id) const override;
    bool setWorldTransform(NauDocumentNodeId id, const NauDocumentMat4& transform) override;

    std::vector<NauDocumentAssetRef> assetReferences(NauDocumentNodeId id) const override;
    bool setAssetReferences(NauDocumentNodeId id, const std::vector<NauDocumentAssetRef>& references) override;

    NauCallbackId subscribe(std::function<void(const NauDocumentChanges&)> callback) override;
    void unsubscribe(const NauCallbackId& id) override;
    void beginChangeBatch() override;
    void endChangeBatch() override;
    void beginChangeBlock() override;
    void endChangeBlock() override;

    std::string filePath() const override;
    bool save() override;
    bool isModified() const override;

    std::string debugPath(NauDocumentNodeId id) const override;

private:
    class OwnEdit;

    PXR_NS::UsdPrim prim(NauDocumentNodeId id) const;
    bool isComponent(const PXR_NS::UsdPrim& prim) const;

    void bind(const PXR_NS::SdfPath& path, NauDocumentNodeId id) const;
    void unbindSubtree(const PXR_NS::SdfPath& path);
    void moveSubtree(const PXR_NS::SdfPath& oldPath, const PXR_NS::SdfPath& newPath);

    void onObjectsChanged(const std::vector<PXR_NS::SdfPath>& resyncedPaths);

private:
    PXR_NS::UsdStageRefPtr m_stage;
    NauDocumentNodeId m_rootId;

    // Ids are assigned lazily and live for the session (decision D6)
    mutable std::unordered_map<PXR_NS::SdfPath, NauDocumentNodeId, PXR_NS::SdfPath::Hash> m_idsByPath;
    mutable std::unordered_map<NauDocumentNodeId, PXR_NS::SdfPath> m_pathsById;

    // Nonzero while the document edits the stage itself. Resyncs by other writers drop only the ids of prims that are gone.
    int m_ownEditDepth = 0;

    std::unique_ptr<UsdProxy::StageObjectChangedWatcher> m_stageWatcher;
    bool m_modified = false;

    std::vector<std::pair<NauCallbackId, std::function<void(const NauDocumentChanges&)>>> m_subscribers;
    int m_batchDepth = 0;
    int m_blockDepth = 0;
};

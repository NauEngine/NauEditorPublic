// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/document-usd/nau_usd_document.hpp"
#include "nau/document-usd/nau_usd_document_factory.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"
#include "nau_usd_document_values.hpp"
#include "nau_log.hpp"

#include "usd_proxy/usd_prim_proxy.h"
#include "usd_proxy/usd_property_proxy.h"
#include "usd_proxy/usd_stage_watcher.h"

#include "pxr/base/tf/errorMark.h"
#include "pxr/usd/sdf/changeBlock.h"
#include "pxr/usd/sdf/copyUtils.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/listOp.h"
#include "pxr/usd/sdf/namespaceEdit.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/reference.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usd/references.h"
#include "pxr/usd/usdGeom/xformCache.h"
#include "pxr/usd/usdGeom/xformable.h"

#include <algorithm>
#include <optional>


namespace
{
    // The local transform property. Its USD attribute and the op order aren't listed as properties.
    constexpr std::string_view TransformOpPrefix = "xformOp";

    // Identity and type are node data, not properties: component and asset uids, and the engine class of a component
    bool isNodeDataAttribute(const std::string& key)
    {
        return key == "uid" || key == "componentTypeName";
    }

    // What a removed node needs to come back unchanged
    class NauUsdNodeSnapshotData final : public NauDocumentNodeSnapshotData
    {
    public:
        PXR_NS::SdfPath path;

        // A copy of everything the root layer holds for the prim; null if it had no spec there
        PXR_NS::SdfLayerRefPtr spec;
        std::optional<size_t> siblingIndex;

        // Ids of the node and of every bound node below it, by path relative to the node ("." for the node)
        std::vector<std::pair<PXR_NS::SdfPath, NauDocumentNodeId>> ids;
    };
}


// ** NauUsdDocument::OwnEdit
//
// Marks stage edits made by the document itself, so the resyncs they cause don't drop ids

class NauUsdDocument::OwnEdit
{
public:
    explicit OwnEdit(NauUsdDocument& document)
        : m_document(document)
    {
        ++m_document.m_ownEditDepth;
    }

    ~OwnEdit()
    {
        --m_document.m_ownEditDepth;
    }

    OwnEdit(const OwnEdit&) = delete;
    OwnEdit& operator=(const OwnEdit&) = delete;

private:
    NauUsdDocument& m_document;
};


// ** NauUsdDocument

std::shared_ptr<NauUsdDocument> NauUsdDocument::open(const std::string& path)
{
    PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(path);
    if (!stage) {
        NED_ERROR("Usd document: failed to open {}", path);
        return nullptr;
    }

    // The editor supports a single transform op per prim
    NauUsdSceneUtils::forceUpdatePrimsTransformOpOrder(stage);

    for (const PXR_NS::SdfPath& loadable : stage->FindLoadable()) {
        stage->Load(loadable);
    }

    // Created after the normalization, so opening doesn't count as a change
    return std::make_shared<NauUsdDocument>(stage);
}

std::shared_ptr<NauUsdDocument> NauUsdDocument::createNew(const std::string& path)
{
    PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::CreateNew(path);
    if (!stage) {
        NED_ERROR("Usd document: failed to create {}", path);
        return nullptr;
    }

    return std::make_shared<NauUsdDocument>(stage);
}

std::shared_ptr<NauUsdDocument> NauUsdDocument::createInMemory()
{
    return std::make_shared<NauUsdDocument>(PXR_NS::UsdStage::CreateInMemory());
}

std::shared_ptr<NauUsdDocument> NauUsdDocument::fromStage(PXR_NS::UsdStageRefPtr stage)
{
    return stage ? std::make_shared<NauUsdDocument>(stage) : nullptr;
}

NauUsdDocument::NauUsdDocument(PXR_NS::UsdStageRefPtr stage)
    : m_stage(stage)
    , m_rootId(nau::Uid::generate())
{
    m_stageWatcher = std::make_unique<UsdProxy::StageObjectChangedWatcher>(m_stage, [this](const PXR_NS::UsdNotice::ObjectsChanged& notice) {
        const auto resynced = notice.GetResyncedPaths();
        onObjectsChanged(std::vector<PXR_NS::SdfPath>(resynced.begin(), resynced.end()));
    });
}

NauUsdDocument::~NauUsdDocument() = default;

PXR_NS::UsdStageRefPtr NauUsdDocument::stage() const
{
    return m_stage;
}

NauDocumentNodeId NauUsdDocument::nodeFromPath(const PXR_NS::SdfPath& path) const
{
    if (path == PXR_NS::SdfPath::AbsoluteRootPath()) {
        return m_rootId;
    }

    if (!path.IsPrimPath() || !m_stage->GetPrimAtPath(path)) {
        return {};
    }

    if (const auto it = m_idsByPath.find(path); it != m_idsByPath.end()) {
        return it->second;
    }

    const NauDocumentNodeId id = nau::Uid::generate();
    bind(path, id);
    return id;
}

PXR_NS::SdfPath NauUsdDocument::pathFromNode(NauDocumentNodeId id) const
{
    const PXR_NS::UsdPrim found = prim(id);
    return found ? found.GetPath() : PXR_NS::SdfPath();
}

NauDocumentNodeId NauUsdDocument::rootNode() const
{
    return m_rootId;
}

bool NauUsdDocument::contains(NauDocumentNodeId id) const
{
    return static_cast<bool>(prim(id));
}

std::optional<NauDocumentNodeInfo> NauUsdDocument::nodeInfo(NauDocumentNodeId id) const
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found) {
        return std::nullopt;
    }

    NauDocumentNodeInfo info;
    info.id = id;
    if (found.IsPseudoRoot()) {
        return info;
    }

    info.parent = nodeFromPath(found.GetPath().GetParentPath());
    info.kind = isComponent(found) ? NauDocumentNodeKind::Component : NauDocumentNodeKind::Object;
    info.typeName = NauUsdPrimUtils::creationTypeName(found);
    info.displayTypeName = NauUsdPrimFactory::instance().displayName(info.typeName);
    if (info.displayTypeName.empty()) {
        info.displayTypeName = info.typeName;
    }
    info.name = found.GetName().GetString();
    info.displayName = found.GetDisplayName();

    return info;
}

std::vector<NauDocumentNodeId> NauUsdDocument::children(NauDocumentNodeId id) const
{
    std::vector<NauDocumentNodeId> result;

    const PXR_NS::UsdPrim parent = prim(id);
    if (!parent || isComponent(parent)) {
        return result;
    }

    for (const PXR_NS::UsdPrim& child : parent.GetAllChildren()) {
        if (!isComponent(child)) {
            result.push_back(nodeFromPath(child.GetPath()));
        }
    }

    return result;
}

std::vector<NauDocumentNodeId> NauUsdDocument::components(NauDocumentNodeId id) const
{
    std::vector<NauDocumentNodeId> result;

    const PXR_NS::UsdPrim owner = prim(id);
    if (!owner || owner.IsPseudoRoot() || isComponent(owner)) {
        return result;
    }

    for (const PXR_NS::UsdPrim& child : owner.GetAllChildren()) {
        if (isComponent(child)) {
            result.push_back(nodeFromPath(child.GetPath()));
        }
    }

    return result;
}

NauDocumentNodeId NauUsdDocument::createNode(const NauDocumentNodeCreateInfo& info)
{
    const PXR_NS::UsdPrim parent = prim(info.parent);
    if (!parent) {
        NED_WARNING("Usd document: can't create a node under a missing parent");
        return {};
    }

    const bool component = info.kind == NauDocumentNodeKind::Component;
    if (isComponent(parent) || (component && parent.IsPseudoRoot())) {
        NED_WARNING("Usd document: can't create a {} under {}", component ? "component" : "node", parent.GetPath().GetString());
        return {};
    }

    if (info.id && contains(info.id)) {
        NED_WARNING("Usd document: id of a new node is in use, {}", toString(info.id));
        return {};
    }

    const std::string parentPath = parent.IsPseudoRoot() ? std::string() : parent.GetPath().GetString();
    const PXR_NS::SdfPath path(NauUsdSceneUtils::generateUniquePrimPath(m_stage, parentPath, info.nameHint));

    PXR_NS::UsdPrim created;
    {
        OwnEdit ownEdit(*this);
        created = NauUsdPrimFactory::instance().createPrim(m_stage, path, PXR_NS::TfToken(info.typeName), info.displayName,
            NauUsdDocumentValues::toUsdMatrix(info.transform), component);
    }

    if (!created) {
        return {};
    }

    const NauDocumentNodeId id = info.id ? info.id : nau::Uid::generate();
    bind(path, id);

    return id;
}

bool NauUsdDocument::removeNode(NauDocumentNodeId id)
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return false;
    }

    const PXR_NS::SdfPath path = found.GetPath();

    bool removed = false;
    {
        OwnEdit ownEdit(*this);
        removed = m_stage->RemovePrim(path);
    }

    unbindSubtree(path);
    return removed;
}

NauDocumentNodeSnapshot NauUsdDocument::snapshotNode(NauDocumentNodeId id) const
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return nullptr;
    }

    auto snapshot = std::make_shared<NauUsdNodeSnapshotData>();
    snapshot->path = found.GetPath();

    for (const auto& [path, boundId] : m_idsByPath) {
        if (path.HasPrefix(snapshot->path)) {
            snapshot->ids.emplace_back(path.MakeRelativePath(snapshot->path), boundId);
        }
    }

    // Keep a copy of everything the root layer holds for the prim
    const PXR_NS::SdfLayerHandle layer = m_stage->GetRootLayer();
    if (PXR_NS::SdfPrimSpecHandle parentSpec = layer->GetPrimAtPath(snapshot->path.GetParentPath())) {
        size_t index = 0;
        for (const PXR_NS::SdfPrimSpecHandle& sibling : parentSpec->GetNameChildren()) {
            if (sibling->GetPath() == snapshot->path) {
                snapshot->siblingIndex = index;
                break;
            }
            ++index;
        }
    }

    if (snapshot->siblingIndex) {
        snapshot->spec = PXR_NS::SdfLayer::CreateAnonymous();
        if (snapshot->path.GetParentPath() != PXR_NS::SdfPath::AbsoluteRootPath()) {
            PXR_NS::SdfCreatePrimInLayer(snapshot->spec, snapshot->path.GetParentPath());
        }
        PXR_NS::SdfCopySpec(layer, snapshot->path, snapshot->spec, snapshot->path);
    }

    return snapshot;
}

bool NauUsdDocument::restoreNode(const NauDocumentNodeSnapshot& snapshot)
{
    const auto* data = dynamic_cast<const NauUsdNodeSnapshotData*>(snapshot.get());
    if (!data) {
        return false;
    }

    const PXR_NS::SdfPath& path = data->path;
    if (!data->spec) {
        NED_WARNING("Usd document: node had no spec in the root layer, nothing to restore, path: {}", path.GetString());
        return false;
    }

    if (!m_stage->GetPrimAtPath(path.GetParentPath()) || m_stage->GetPrimAtPath(path)) {
        NED_WARNING("Usd document: can't restore node, parent is gone or path is taken, path: {}", path.GetString());
        return false;
    }

    for (const auto& [relativePath, id] : data->ids) {
        if (contains(id)) {
            NED_WARNING("Usd document: can't restore node, id in use, path: {}", path.GetString());
            return false;
        }
    }

    const PXR_NS::SdfLayerHandle layer = m_stage->GetRootLayer();
    {
        OwnEdit ownEdit(*this);

        // One change notice for the whole restored prim
        PXR_NS::SdfChangeBlock changeBlock;

        if (path.GetParentPath() != PXR_NS::SdfPath::AbsoluteRootPath()) {
            PXR_NS::SdfCreatePrimInLayer(layer, path.GetParentPath());
        }

        if (!PXR_NS::SdfCopySpec(data->spec, path, layer, path)) {
            NED_WARNING("Usd document: failed to restore node, path: {}", path.GetString());
            return false;
        }

        // The copied spec is appended to its parent; put it back where it was among its siblings.
        // USD reorders correctly here but also reports a coding error ("Cannot move </B> to </B>"),
        // so its errors are dropped and the resulting order is checked instead.
        PXR_NS::SdfBatchNamespaceEdit edit;
        edit.Add(PXR_NS::SdfNamespaceEdit::Reorder(path, static_cast<PXR_NS::SdfNamespaceEdit::Index>(*data->siblingIndex)));
        {
            PXR_NS::TfErrorMark errorMark;
            layer->Apply(edit);
            errorMark.Clear();
        }
    }

    const PXR_NS::SdfPrimSpecHandle parentSpec = layer->GetPrimAtPath(path.GetParentPath());
    const auto siblings = parentSpec->GetNameChildren();
    if (*data->siblingIndex >= siblings.size() || siblings[*data->siblingIndex]->GetPath() != path) {
        NED_WARNING("Usd document: failed to restore node order, path: {}", path.GetString());
    }

    for (const auto& [relativePath, id] : data->ids) {
        bind(relativePath.MakeAbsolutePath(path), id);
    }

    return true;
}

bool NauUsdDocument::setDisplayName(NauDocumentNodeId id, const std::string& displayName)
{
    PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return false;
    }

    OwnEdit ownEdit(*this);
    return found.SetDisplayName(displayName);
}

bool NauUsdDocument::reparentNode(NauDocumentNodeId id, NauDocumentNodeId newParent)
{
    using namespace PXR_NS;

    const UsdPrim found = prim(id);
    const UsdPrim parent = prim(newParent);
    if (!found || found.IsPseudoRoot() || isComponent(found) || !parent || isComponent(parent)) {
        return false;
    }

    const SdfPath oldPath = found.GetPath();
    const SdfPath parentPath = parent.GetPath();
    if (parentPath.HasPrefix(oldPath)) {
        return false;
    }

    const GfMatrix4d worldTransform = NauUsdPrimUtils::worldPrimTransform(found);
    const SdfPath newPath = parentPath.AppendChild(oldPath.GetNameToken());

    {
        OwnEdit ownEdit(*this);

        SdfBatchNamespaceEdit edit;
        edit.Add(SdfNamespaceEdit::Reparent(oldPath, parentPath, SdfNamespaceEdit::AtEnd));
        if (!m_stage->GetRootLayer()->Apply(edit)) {
            return false;
        }

        moveSubtree(oldPath, newPath);

        // Keep the world transform, as the engine's SceneObject::setParent does by default
        const UsdPrim moved = m_stage->GetPrimAtPath(newPath);
        if (UsdGeomXformable(moved)) {
            UsdGeomXformCache cache;
            NauUsdPrimUtils::setPrimTransform(moved, worldTransform * cache.GetParentToWorldTransform(moved).GetInverse());
        }
    }

    return true;
}

std::vector<NauDocumentPropertyDescriptor> NauUsdDocument::properties(NauDocumentNodeId id) const
{
    std::vector<NauDocumentPropertyDescriptor> result;

    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return result;
    }

    if (PXR_NS::UsdGeomXformable(found)) {
        result.push_back({
            .key = std::string(NauDocumentPropertyKeys::transform),
            .displayName = "Transform",
            .type = NauDocumentValueType::Matrix4,
            .role = NauDocumentPropertyRole::Transform,
        });
    }

    for (const auto& [name, property] : UsdProxy::UsdProxyPrim(found).getProperties()) {
        const std::string& key = name.GetString();
        if (key.starts_with(TransformOpPrefix) || isNodeDataAttribute(key)) {
            continue;
        }

        NauDocumentPropertyDescriptor descriptor;
        descriptor.key = key;
        descriptor.displayName = key;
        descriptor.group = property->getNamespace().GetString();
        descriptor.type = NauUsdDocumentValues::valueType(property->getTypeName());

        // Same checks as today's inspector
        PXR_NS::VtValue visible;
        if (property->getMetadata(PXR_NS::TfToken("visible"), visible) && visible.IsHolding<bool>() && !visible.UncheckedGet<bool>()) {
            descriptor.hidden = true;
        }
        if (found.GetAttribute(name).IsHidden()) {
            descriptor.hidden = true;
        }

        result.push_back(std::move(descriptor));
    }

    return result;
}

std::optional<NauDocumentValue> NauUsdDocument::property(NauDocumentNodeId id, const std::string& key) const
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return std::nullopt;
    }

    if (key == NauDocumentPropertyKeys::transform) {
        if (!PXR_NS::UsdGeomXformable(found)) {
            return std::nullopt;
        }
        return NauUsdDocumentValues::toDocumentMatrix(NauUsdPrimUtils::localPrimTransform(found));
    }

    if (isNodeDataAttribute(key)) {
        return std::nullopt;
    }

    const auto property = UsdProxy::UsdProxyPrim(found).getProperty(PXR_NS::TfToken(key));
    if (!property || !property->isValid()) {
        return std::nullopt;
    }

    PXR_NS::VtValue value;
    if (!property->getValue(&value)) {
        return std::nullopt;
    }

    return NauUsdDocumentValues::toDocumentValue(value, NauUsdDocumentValues::valueType(property->getTypeName()));
}

bool NauUsdDocument::setProperty(NauDocumentNodeId id, const std::string& key, const NauDocumentValue& value)
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return false;
    }

    if (key == NauDocumentPropertyKeys::transform) {
        const auto* matrix = std::get_if<NauDocumentMat4>(&value);
        if (!matrix || !PXR_NS::UsdGeomXformable(found)) {
            return false;
        }
        OwnEdit ownEdit(*this);
        NauUsdPrimUtils::setPrimTransform(found, NauUsdDocumentValues::toUsdMatrix(*matrix));
        return true;
    }

    if (isNodeDataAttribute(key)) {
        NED_WARNING("Usd document: {} is node data, not a property, on {}", key, found.GetPath().GetString());
        return false;
    }

    const auto property = UsdProxy::UsdProxyPrim(found).getProperty(PXR_NS::TfToken(key));
    if (!property || !property->isValid()) {
        NED_WARNING("Usd document: no property {} on {}", key, found.GetPath().GetString());
        return false;
    }

    const auto usdValue = NauUsdDocumentValues::toUsdValue(value, NauUsdDocumentValues::valueType(property->getTypeName()));
    if (!usdValue) {
        NED_WARNING("Usd document: value doesn't match the type of {} on {}", key, found.GetPath().GetString());
        return false;
    }

    OwnEdit ownEdit(*this);
    return property->setValue(*usdValue);
}

NauDocumentMat4 NauUsdDocument::worldTransform(NauDocumentNodeId id) const
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || !PXR_NS::UsdGeomXformable(found)) {
        return {};
    }

    return NauUsdDocumentValues::toDocumentMatrix(NauUsdPrimUtils::worldPrimTransform(found));
}

bool NauUsdDocument::setWorldTransform(NauDocumentNodeId id, const NauDocumentMat4& transform)
{
    const PXR_NS::UsdPrim found = prim(id);
    if (!found || !PXR_NS::UsdGeomXformable(found)) {
        return false;
    }

    OwnEdit ownEdit(*this);
    NauUsdPrimUtils::setPrimWorldTransform(found, NauUsdDocumentValues::toUsdMatrix(transform));
    return true;
}

std::vector<NauDocumentAssetRef> NauUsdDocument::assetReferences(NauDocumentNodeId id) const
{
    std::vector<NauDocumentAssetRef> result;

    const PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return result;
    }

    const PXR_NS::SdfPrimSpecHandle spec = m_stage->GetRootLayer()->GetPrimAtPath(found.GetPath());
    if (!spec) {
        return result;
    }

    const PXR_NS::VtValue field = spec->GetField(PXR_NS::SdfFieldKeys->References);
    if (!field.IsHolding<PXR_NS::SdfReferenceListOp>()) {
        return result;
    }

    const auto& references = field.UncheckedGet<PXR_NS::SdfReferenceListOp>();
    const auto append = [&result](const std::vector<PXR_NS::SdfReference>& items) {
        for (const PXR_NS::SdfReference& reference : items) {
            result.push_back({ reference.GetAssetPath(), reference.GetPrimPath().GetString() });
        }
    };

    if (references.IsExplicit()) {
        append(references.GetExplicitItems());
    } else {
        append(references.GetPrependedItems());
        append(references.GetAppendedItems());
    }

    return result;
}

bool NauUsdDocument::setAssetReferences(NauDocumentNodeId id, const std::vector<NauDocumentAssetRef>& references)
{
    PXR_NS::UsdPrim found = prim(id);
    if (!found || found.IsPseudoRoot()) {
        return false;
    }

    OwnEdit ownEdit(*this);

    PXR_NS::UsdReferences usdReferences = found.GetReferences();
    usdReferences.ClearReferences();
    for (const NauDocumentAssetRef& reference : references) {
        const PXR_NS::SdfPath primPath = reference.subAsset.empty() ? PXR_NS::SdfPath() : PXR_NS::SdfPath(reference.subAsset);
        usdReferences.AddReference(PXR_NS::SdfReference(reference.asset, primPath));
    }
    found.Load();

    return true;
}

NauCallbackId NauUsdDocument::subscribe(std::function<void(const NauDocumentChanges&)> callback)
{
    NauCallbackId id;
    m_subscribers.emplace_back(id, std::move(callback));
    return id;
}

void NauUsdDocument::unsubscribe(const NauCallbackId& id)
{
    std::erase_if(m_subscribers, [&id](auto& subscriber) {
        return subscriber.first == id;
    });
}

void NauUsdDocument::beginChangeBatch()
{
    ++m_batchDepth;
}

void NauUsdDocument::endChangeBatch()
{
    --m_batchDepth;
}

void NauUsdDocument::beginChangeBlock()
{
    ++m_blockDepth;
}

void NauUsdDocument::endChangeBlock()
{
    --m_blockDepth;
}

std::string NauUsdDocument::filePath() const
{
    return m_stage->GetRootLayer()->GetRealPath();
}

bool NauUsdDocument::save()
{
    // Same as NauUsdSceneManager::saveCurrentScene: every dirty layer of the stage
    m_stage->Save();
    m_modified = false;
    return true;
}

bool NauUsdDocument::isModified() const
{
    return m_modified;
}

std::string NauUsdDocument::debugPath(NauDocumentNodeId id) const
{
    return pathFromNode(id).GetString();
}

PXR_NS::UsdPrim NauUsdDocument::prim(NauDocumentNodeId id) const
{
    if (!id) {
        return {};
    }

    if (id == m_rootId) {
        return m_stage->GetPseudoRoot();
    }

    const auto it = m_pathsById.find(id);
    return it != m_pathsById.end() ? m_stage->GetPrimAtPath(it->second) : PXR_NS::UsdPrim();
}

bool NauUsdDocument::isComponent(const PXR_NS::UsdPrim& prim) const
{
    return NauUsdPrimUtils::isPrimComponent(prim);
}

void NauUsdDocument::bind(const PXR_NS::SdfPath& path, NauDocumentNodeId id) const
{
    // Neither the path nor the id keeps a stale binding
    if (const auto it = m_idsByPath.find(path); it != m_idsByPath.end()) {
        m_pathsById.erase(it->second);
    }
    if (const auto it = m_pathsById.find(id); it != m_pathsById.end()) {
        m_idsByPath.erase(it->second);
    }

    m_idsByPath[path] = id;
    m_pathsById[id] = path;
}

void NauUsdDocument::unbindSubtree(const PXR_NS::SdfPath& path)
{
    for (auto it = m_idsByPath.begin(); it != m_idsByPath.end();) {
        if (it->first.HasPrefix(path)) {
            m_pathsById.erase(it->second);
            it = m_idsByPath.erase(it);
        } else {
            ++it;
        }
    }
}

void NauUsdDocument::moveSubtree(const PXR_NS::SdfPath& oldPath, const PXR_NS::SdfPath& newPath)
{
    std::vector<std::pair<PXR_NS::SdfPath, NauDocumentNodeId>> moved;
    for (auto it = m_idsByPath.begin(); it != m_idsByPath.end();) {
        if (it->first.HasPrefix(oldPath)) {
            moved.emplace_back(it->first.ReplacePrefix(oldPath, newPath), it->second);
            it = m_idsByPath.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& [path, id] : moved) {
        bind(path, id);
    }
}

void NauUsdDocument::onObjectsChanged(const std::vector<PXR_NS::SdfPath>& resyncedPaths)
{
    m_modified = true;

    if (m_ownEditDepth > 0) {
        return;
    }

    // Another writer restructured the stage: what it touched gets new ids (decision D6)
    for (const PXR_NS::SdfPath& path : resyncedPaths) {
        if (path.IsPrimPath()) {
            unbindSubtree(path);
        }
    }
}


// ** Nau

NauDocumentPtr Nau::OpenUsdDocument(const std::string& path)
{
    return NauUsdDocument::open(path);
}

NauDocumentPtr Nau::CreateUsdDocument(const std::string& path)
{
    return NauUsdDocument::createNew(path);
}

NauDocumentPtr Nau::CreateInMemoryUsdDocument()
{
    return NauUsdDocument::createInMemory();
}

// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Editor document interface. The inspector, outliner, selection, undo commands, scene editor and UI editor work with
// objects, components and properties through it instead of pxr APIs. The structure follows the engine's scene model:
// objects form a tree, and each object owns a list of components. In Stage 1 of the USD migration the only backend is USD
// (NauUsdAssetEditorCommon, src/document-usd/); no USD concepts (layers, composition, prim paths) appear here.
// Design notes and the mapping of today's calls: document-interface.md (local, T1.1).

#pragma once

#include "nau/document/nau_document_value.hpp"
#include "nau/nau_delegate.hpp"
#include "nau/utils/uid.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>


// ** NauDocumentNodeId
//
// Node identity (decision D6). The document assigns it when it first sees a node. It stays the same through rename,
// reparent and undo/redo for the whole session, isn't written to files in Stage 1, and is never a property, so no client
// can show or edit it. A null uid means "no node".

using NauDocumentNodeId = nau::Uid;


// ** NauDocumentNodeKind

enum class NauDocumentNodeKind
{
    // A scene object or UI node: has child objects and components, and its own row in the outliner (engine: SceneObject)
    Object,

    // Belongs to one object, has no children, shows as a section of its object in the inspector (engine: Component).
    // USD backend: a child prim of kind "component" or "subcomponent". Engine components are NauComponent prims,
    // but other types can be created as components too (AnimationController, RigidBody*).
    Component
};


// ** NauDocumentNodeInfo

struct NauDocumentNodeInfo
{
    NauDocumentNodeId id;
    NauDocumentNodeId parent;  // The parent object, or for a component the object that owns it. Null for the root node.
    NauDocumentNodeKind kind = NauDocumentNodeKind::Object;

    // The type the node was created with, and is copied as (NauDocumentNodeTypeInfo::typeName).
    // USD backend: componentTypeName for NauComponent prims, the prim type name otherwise.
    std::string typeName;

    // Human-readable type for the outliner Type column and inspector headers: the display name typeName is registered with
    // (NauDocumentNodeTypeInfo::displayName), or typeName if it isn't registered. Engine components show what they are
    // ("Directional Light"), not "NauComponent" or a class name.
    std::string displayTypeName;

    // Assigned by the backend, unique among siblings (USD backend: the prim name). Clients never parse it:
    // it's shown when the display name is empty, and used as the name hint when the node is copied.
    std::string name;

    // As authored, empty if not set. See Nau::DocumentDisplayName().
    std::string displayName;
};


// ** NauDocumentNodeCreateInfo

struct NauDocumentNodeCreateInfo
{
    // The root node for top-level objects. For a component, the object that will own it.
    NauDocumentNodeId parent;
    std::string typeName;      // A registered type, see NauDocumentTypeRegistryInterface
    NauDocumentNodeKind kind = NauDocumentNodeKind::Object;

    // The document derives a unique name from it ("Xform", "Xform1", ...)
    std::string nameHint;

    // Ignored for components: they get the display name their type is registered with (today's prim factory rule)
    std::string displayName;

    // Local transform; ignored for nodes without one
    NauDocumentMat4 transform;

    // Null: the document generates an id. Otherwise the node gets this id, which must not be in use.
    // Redo of a create passes the id from the first execute, so commands further up the redo stack still find the node.
    NauDocumentNodeId id;
};


// ** NauDocumentNodeSnapshotData
//
// Backend-defined content of a node snapshot.

class NauDocumentNodeSnapshotData
{
public:
    virtual ~NauDocumentNodeSnapshotData() = default;
};


// ** NauDocumentNodeSnapshot
//
// Everything needed to bring a removed node back unchanged: its subtree with all properties and asset references,
// the id of every node in the subtree, its parent and its position among its siblings. Immutable; copies share the data.
// USD backend: a copy of the root layer spec in an anonymous layer and the sibling index (as NauCommandRemoveUsdPrim
// keeps today), plus the ids of the subtree by relative path.

using NauDocumentNodeSnapshot = std::shared_ptr<const NauDocumentNodeSnapshotData>;


// ** NauDocumentPropertyRole

enum class NauDocumentPropertyRole
{
    Value,

    // The node's local transform: key NauDocumentPropertyKeys::transform, type Matrix4, shown in its own inspector section.
    // USD backend: xformOp:transform. The xformOp* attributes and xformOpOrder aren't listed as properties.
    Transform
};


// ** NauDocumentPropertyKeys

struct NauDocumentPropertyKeys
{
    static constexpr std::string_view transform = "transform";
};


// ** NauDocumentPropertyDescriptor

struct NauDocumentPropertyDescriptor
{
    // Unique within the node. USD backend: the attribute name with its namespace ("physics:collisionChannel").
    std::string key;

    // Label shown to the user. USD backend: the key, as today's inspector shows it.
    std::string displayName;

    // USD backend: the attribute namespace ("Texture", "physics"), empty if none. Today's inspector passes it to asset
    // widgets as the asset type filter, and shows a namespaced non-asset property only if a widget is registered for its key.
    std::string group;

    NauDocumentValueType type = NauDocumentValueType::Unsupported;
    NauDocumentPropertyRole role = NauDocumentPropertyRole::Value;

    // USD backend: `hidden` metadata, or the custom `visible = false` metadata. Hidden properties are still listed;
    // the inspector skips them.
    bool hidden = false;

    // False for every property in Stage 1
    bool readOnly = false;
};


// ** NauDocumentNodeTypeInfo

struct NauDocumentNodeTypeInfo
{
    // Name nodes are created with: a schema type ("Xform", "NauGuiButton") or an engine component class
    // ("nau::scene::OmnilightComponent")
    std::string typeName;

    // Human-readable, shown in creation lists and as NauDocumentNodeInfo::displayTypeName: no namespaces or class name
    // suffixes ("Directional Light"). Also the display name new components of this type get.
    std::string displayName;

    // Nodes of this type are engine components of class typeName.
    // USD backend: a NauComponent prim with a hidden componentTypeName attribute.
    bool engineComponent = false;

    // Added to every new node of this type, e.g. {"../meshes/cube.usda.nausd", "/Root/Cube"} for Mesh
    std::optional<NauDocumentAssetRef> defaultAssetReference;
};


// ** NauDocumentTypeRegistryInterface
//
// Types nodes can be created with. Process-wide and shared by all documents, like NauUsdPrimFactory today:
// editors register their types at startup. How clients get the registry is decided in T1.2 (probably an editor service).

class NauDocumentTypeRegistryInterface
{
public:
    virtual ~NauDocumentTypeRegistryInterface() = default;

    // Replaces an earlier registration with the same type name
    virtual void registerType(const NauDocumentNodeTypeInfo& type) = 0;

    // Sorted by type name
    virtual std::vector<NauDocumentNodeTypeInfo> types() const = 0;
};


// ** NauDocumentNodeEvent

struct NauDocumentNodeEvent
{
    NauDocumentNodeId node;
    NauDocumentNodeId parent;  // For a removed node, the parent it had
};


// ** NauDocumentNodeMoveEvent

struct NauDocumentNodeMoveEvent
{
    NauDocumentNodeId node;
    NauDocumentNodeId oldParent;
    NauDocumentNodeId newParent;
};


// ** NauDocumentPropertyEvent

struct NauDocumentPropertyEvent
{
    NauDocumentNodeId node;
    std::string key;
};


// ** NauDocumentChanges
//
// One batch of changes, delivered after they're applied. Replaces NauUITranslatorProxyNotice.
// Within a batch a node appears at most once per list, and the lists don't contradict each other: a node added and removed
// in the same batch isn't reported, a removed node has no other entries, an added node has no moved, changed or property
// entries. (USD backend: this is the de-duplication left as a TODO in NauUsdSceneUITranslator::recomposeEvents.)
// Changes made outside the document, such as an asset editor writing to the same stage, show up as removed + added,
// and the added nodes get new ids.

struct NauDocumentChanges
{
    // Every new node, parents before children
    std::vector<NauDocumentNodeEvent> addedNodes;

    // Every node that's gone, children before parents
    std::vector<NauDocumentNodeEvent> removedNodes;

    // Reparented through the document. Ids don't change; descendants move along and aren't listed.
    std::vector<NauDocumentNodeMoveEvent> movedNodes;

    // Node-level data changed: display name, type, kind, asset references, order of children
    std::vector<NauDocumentNodeId> changedNodes;

    // A property was set, added or removed
    std::vector<NauDocumentPropertyEvent> changedProperties;

    bool empty() const
    {
        return addedNodes.empty() && removedNodes.empty() && movedNodes.empty() && changedNodes.empty() && changedProperties.empty();
    }
};


// ** NauDocumentInterface
//
// An edited document: a tree of nodes with properties. Main thread only.
// Edits made through it are not undoable by themselves; undo commands call them and keep what's needed to revert.
// Creating, opening and closing documents is backend-specific (e.g. the USD backend opens a stage), so it isn't part of this interface.

class NauDocumentInterface
{
public:
    virtual ~NauDocumentInterface() = default;

    // Structure

    // USD backend: the pseudo-root; top-level prims are its children. It has no properties and can't be edited.
    virtual NauDocumentNodeId rootNode() const = 0;

    virtual bool contains(NauDocumentNodeId id) const = 0;
    virtual std::optional<NauDocumentNodeInfo> nodeInfo(NauDocumentNodeId id) const = 0;

    // Child objects in document order; empty for components (engine: SceneObject::getChildObjects(false)).
    // USD backend: child prims that aren't components, inactive ones included (GetAllChildren order).
    virtual std::vector<NauDocumentNodeId> children(NauDocumentNodeId id) const = 0;

    // Components owned by an object, in document order; empty for the root node and for components
    // (engine: SceneObject::getDirectComponents()). USD backend: child prims of kind component or subcomponent.
    virtual std::vector<NauDocumentNodeId> components(NauDocumentNodeId id) const = 0;

    // Editing structure

    // Appends an object to the parent's children, or a component to the owner's components. Returns its id, or a null id if
    // the parent doesn't exist or is a component, the type isn't registered, or the given id is in use.
    virtual NauDocumentNodeId createNode(const NauDocumentNodeCreateInfo& info) = 0;

    // Removes the node with its child objects and components. Take a snapshot first to be able to bring it back.
    virtual bool removeNode(NauDocumentNodeId id) = 0;

    virtual NauDocumentNodeSnapshot snapshotNode(NauDocumentNodeId id) const = 0;

    // Brings back a node removed after the snapshot was taken: same ids, same parent, same position among the siblings.
    // Fails if the parent is gone or any id of the subtree is in use.
    virtual bool restoreNode(const NauDocumentNodeSnapshot& snapshot) = 0;

    // Stores the display name as given. An empty one makes clients show the node name; the USD backend still authors it,
    // as the rename command's undo does today.
    virtual bool setDisplayName(NauDocumentNodeId id, const std::string& displayName) = 0;

    // Moves an object to the end of the new parent's children, keeping its world transform (engine: SceneObject::setParent
    // with default options). Ids of the object, its components and descendants don't change. Fails for the root node and
    // components, and if newParent is a component, the object itself or one of its descendants.
    virtual bool reparentNode(NauDocumentNodeId id, NauDocumentNodeId newParent) = 0;

    // Properties

    // In backend order (USD backend: sorted by key, as UsdProxy lists them). Includes properties declared by the node's
    // schema but not authored yet. Never includes identity or type, which are node data, not properties: the USD backend
    // lists no attribute named `uid` (component uids, and asset uids composed through references) and no `componentTypeName`.
    virtual std::vector<NauDocumentPropertyDescriptor> properties(NauDocumentNodeId id) const = 0;

    // nullopt if the node or the property doesn't exist, the property has no value, or its type is Unsupported
    virtual std::optional<NauDocumentValue> property(NauDocumentNodeId id, const std::string& key) const = 0;

    // The value must hold the alternative the property's type maps to; the backend narrows it to the stored type.
    // Authors the property if the schema declares it but it isn't authored yet.
    virtual bool setProperty(NauDocumentNodeId id, const std::string& key, const NauDocumentValue& value) = 0;

    // Transforms. The local transform is the property NauDocumentPropertyKeys::transform.

    // Identity for nodes without a transform and for the root node
    virtual NauDocumentMat4 worldTransform(NauDocumentNodeId id) const = 0;

    // Sets the local transform that gives this world transform under the current parent. Fails for nodes without a transform.
    virtual bool setWorldTransform(NauDocumentNodeId id, const NauDocumentMat4& transform) = 0;

    // Asset references of the node itself, e.g. a mesh instance referencing its mesh asset. Asset-typed properties are separate.

    // USD backend: the references authored on the prim in the root layer
    virtual std::vector<NauDocumentAssetRef> assetReferences(NauDocumentNodeId id) const = 0;

    // Replaces all asset references of the node; an empty list removes them
    virtual bool setAssetReferences(NauDocumentNodeId id, const std::vector<NauDocumentAssetRef>& references) = 0;

    // Change events

    virtual NauCallbackId subscribe(std::function<void(const NauDocumentChanges&)> callback) = 0;
    virtual void unsubscribe(const NauCallbackId& id) = 0;

    // Use NauDocumentChangeBatch and NauDocumentChangeBlock instead of calling these directly
    virtual void beginChangeBatch() = 0;
    virtual void endChangeBatch() = 0;
    virtual void beginChangeBlock() = 0;
    virtual void endChangeBlock() = 0;

    // File

    virtual std::string filePath() const = 0;
    virtual bool save() = 0;

    // True after any change since the document was opened or last saved. Undoing back to the saved state
    // doesn't clear it (same as today).
    virtual bool isModified() const = 0;

    // Debugging

    // Human-readable location of the node for tests, logs and debug display (USD backend: the prim path). Never parse it.
    virtual std::string debugPath(NauDocumentNodeId id) const = 0;
};

using NauDocumentPtr = std::shared_ptr<NauDocumentInterface>;


// ** NauDocumentChangeBatch
//
// Changes made while it lives are delivered as one merged NauDocumentChanges when the outermost batch ends.
// Replaces NauUITranslatorNotificationAccumulator. Can be nested.

class NauDocumentChangeBatch
{
public:
    explicit NauDocumentChangeBatch(NauDocumentInterface& document)
        : m_document(document)
    {
        m_document.beginChangeBatch();
    }

    ~NauDocumentChangeBatch()
    {
        m_document.endChangeBatch();
    }

    NauDocumentChangeBatch(const NauDocumentChangeBatch&) = delete;
    NauDocumentChangeBatch& operator=(const NauDocumentChangeBatch&) = delete;

private:
    NauDocumentInterface& m_document;
};


// ** NauDocumentChangeBlock
//
// Changes made while it lives are not delivered to subscribers at all; the document's own state (ids, modified flag)
// is still updated. Replaces NauUITranslatorNotificationBlock, which the editors put around property commands so that
// the inspector doesn't rebuild under the widget being edited. Can be nested.

class NauDocumentChangeBlock
{
public:
    explicit NauDocumentChangeBlock(NauDocumentInterface& document)
        : m_document(document)
    {
        m_document.beginChangeBlock();
    }

    ~NauDocumentChangeBlock()
    {
        m_document.endChangeBlock();
    }

    NauDocumentChangeBlock(const NauDocumentChangeBlock&) = delete;
    NauDocumentChangeBlock& operator=(const NauDocumentChangeBlock&) = delete;

private:
    NauDocumentInterface& m_document;
};


namespace Nau
{
    // What the outliner and inspector show: the display name, or the node name if no display name is set
    inline const std::string& DocumentDisplayName(const NauDocumentNodeInfo& info)
    {
        return info.displayName.empty() ? info.name : info.displayName;
    }
}

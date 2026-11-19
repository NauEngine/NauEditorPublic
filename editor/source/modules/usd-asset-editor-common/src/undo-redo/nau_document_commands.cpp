// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/undo-redo/nau_document_commands.hpp"
#include "nau_log.hpp"


// ** NauAbstractDocumentCommand

NauAbstractDocumentCommand::NauAbstractDocumentCommand(NauDocumentPtr document)
    : m_document(std::move(document))
{
}


// ** NauCommandCreateNode

NauCommandCreateNode::NauCommandCreateNode(NauDocumentPtr document, const NauDocumentNodeCreateInfo& info)
    : NauAbstractDocumentCommand(std::move(document))
    , m_info(info)
{
}

void NauCommandCreateNode::execute()
{
    const NauDocumentNodeId node = m_document->createNode(m_info);
    if (!node) {
        NED_WARNING("Execute object creation command failed, type: {}", m_info.typeName);
        return;
    }

    m_info.id = node;
    m_info.nameHint = m_document->nodeInfo(node)->name;
    m_objectId = m_document->debugPath(node);

    NED_DEBUG("Execute object creation command, path: {}", m_objectId);
}

void NauCommandCreateNode::undo()
{
    NED_DEBUG("Undo object creation command, path: {}", m_objectId);

    m_document->removeNode(m_info.id);
}

NauCommandDescriptor NauCommandCreateNode::description() const
{
    return {
        .id = id,
        .name = "Object creation",
        .objectId = m_objectId
    };
}

NauDocumentNodeId NauCommandCreateNode::node() const
{
    return m_info.id;
}


// ** NauCommandRemoveNode

NauCommandRemoveNode::NauCommandRemoveNode(NauDocumentPtr document, NauDocumentNodeId node)
    : NauAbstractDocumentCommand(std::move(document))
    , m_node(node)
    , m_objectId(m_document->debugPath(node))
{
}

void NauCommandRemoveNode::execute()
{
    // The snapshot keeps everything undo needs to bring the node back unchanged
    m_snapshot = m_document->snapshotNode(m_node);
    m_document->removeNode(m_node);

    NED_DEBUG("Execute object remove command, path: {}", m_objectId);
}

void NauCommandRemoveNode::undo()
{
    if (!m_snapshot || !m_document->restoreNode(m_snapshot)) {
        NED_WARNING("Undo object remove command failed, path: {}", m_objectId);
        return;
    }

    NED_DEBUG("Undo object remove command, path: {}", m_objectId);
}

NauCommandDescriptor NauCommandRemoveNode::description() const
{
    return {
        .id = id,
        .name = "Object removing",
        .objectId = m_objectId
    };
}


// ** NauCommandRenameNode

NauCommandRenameNode::NauCommandRenameNode(NauDocumentPtr document, NauDocumentNodeId node, const std::string& newName)
    : NauAbstractDocumentCommand(std::move(document))
    , m_node(node)
    , m_objectId(m_document->debugPath(node))
    , m_newName(newName)
{
    // As authored: undo puts back an empty display name too
    if (const auto info = m_document->nodeInfo(node)) {
        m_oldName = info->displayName;
    }
}

void NauCommandRenameNode::execute()
{
    m_document->setDisplayName(m_node, m_newName);

    NED_DEBUG("Execute object rename command, path: {}", m_objectId);
}

void NauCommandRenameNode::undo()
{
    m_document->setDisplayName(m_node, m_oldName);

    NED_DEBUG("Undo object rename command, path: {}", m_objectId);
}

NauCommandDescriptor NauCommandRenameNode::description() const
{
    return {
        .id = id,
        .name = "Object renaming",
        .objectId = m_objectId
    };
}


// ** NauCommandReparentNode

NauCommandReparentNode::NauCommandReparentNode(NauDocumentPtr document, NauDocumentNodeId node, NauDocumentNodeId newParent)
    : NauAbstractDocumentCommand(std::move(document))
    , m_node(node)
    , m_objectId(m_document->debugPath(node))
    , m_newParent(newParent)
{
}

void NauCommandReparentNode::execute()
{
    if (const auto info = m_document->nodeInfo(m_node)) {
        m_oldParent = info->parent;
    }

    if (!m_document->reparentNode(m_node, m_newParent)) {
        NED_WARNING("UndoRedo: Failed to reparent object {} to {}", m_objectId, m_document->debugPath(m_newParent));
        return;
    }

    NED_DEBUG("UndoRedo: Executed object reparent command successfully, {} to {}", m_objectId, m_document->debugPath(m_newParent));
}

void NauCommandReparentNode::undo()
{
    if (!m_document->reparentNode(m_node, m_oldParent)) {
        NED_WARNING("UndoRedo: Failed to reparent object {} back to {}", m_objectId, m_document->debugPath(m_oldParent));
        return;
    }

    NED_DEBUG("UndoRedo: Undo object reparent command successfully, {} back to {}", m_objectId, m_document->debugPath(m_oldParent));
}

NauCommandDescriptor NauCommandReparentNode::description() const
{
    return {
        .id = id,
        .name = "Object reparent",
        .objectId = m_objectId
    };
}


// ** NauCommandChangeNodeProperty

NauCommandChangeNodeProperty::NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key,
    const NauDocumentValue& newValue)
    : NauCommandChangeNodeProperty(document, node, key, document->property(node, key), newValue)
{
}

NauCommandChangeNodeProperty::NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key,
    const NauDocumentValue& initialValue, const NauDocumentValue& newValue)
    : NauCommandChangeNodeProperty(std::move(document), node, key, std::optional(initialValue), std::optional(newValue))
{
}

NauCommandChangeNodeProperty::NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key,
    std::optional<NauDocumentValue> initialValue, std::optional<NauDocumentValue> newValue)
    : NauAbstractDocumentCommand(std::move(document))
    , m_node(node)
    , m_key(key)
    , m_objectId(m_document->debugPath(node))
    , m_initialValue(std::move(initialValue))
    , m_value(std::move(newValue))
{
}

void NauCommandChangeNodeProperty::execute()
{
    if (!m_value || !m_document->setProperty(m_node, m_key, *m_value)) {
        NED_WARNING("Execute object property changing command failed, path: {}, property: {}", m_objectId, m_key);
        return;
    }

    NED_DEBUG("Execute object property changing command, path: {}", m_objectId);
}

void NauCommandChangeNodeProperty::undo()
{
    // A property that had no value keeps the new one, as it has nothing to go back to
    if (m_initialValue) {
        m_document->setProperty(m_node, m_key, *m_initialValue);
    }

    NED_DEBUG("Undo object property changing command, path: {}", m_objectId);
}

NauCommandDescriptor NauCommandChangeNodeProperty::description() const
{
    return {
        .id = id,
        .name = "Object property changing",
        .objectId = m_objectId
    };
}


// ** NauCommandChangeNodeAssetReference

NauCommandChangeNodeAssetReference::NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node,
    const NauDocumentAssetRef& newValue)
    : NauCommandChangeNodeAssetReference(std::move(document), node, std::nullopt, std::optional(newValue))
{
}

NauCommandChangeNodeAssetReference::NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node,
    const NauDocumentAssetRef& initialValue, const NauDocumentAssetRef& newValue)
    : NauCommandChangeNodeAssetReference(std::move(document), node, std::optional(initialValue), std::optional(newValue))
{
}

NauCommandChangeNodeAssetReference::NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node,
    std::optional<NauDocumentAssetRef> initialValue, std::optional<NauDocumentAssetRef> newValue)
    : NauAbstractDocumentCommand(std::move(document))
    , m_node(node)
    , m_objectId(m_document->debugPath(node))
    , m_initialValue(std::move(initialValue))
    , m_value(std::move(newValue))
{
}

void NauCommandChangeNodeAssetReference::execute()
{
    m_previousReferences = m_document->assetReferences(m_node);

    if (!m_value || !m_document->setAssetReferences(m_node, { *m_value })) {
        NED_WARNING("Execute asset reference changing command failed, path: {}", m_objectId);
        return;
    }

    NED_DEBUG("Execute asset reference changing command, path: {}", m_objectId);
}

void NauCommandChangeNodeAssetReference::undo()
{
    const std::vector<NauDocumentAssetRef> references = m_initialValue
        ? std::vector<NauDocumentAssetRef>{ *m_initialValue }
        : m_previousReferences;

    if (!m_document->setAssetReferences(m_node, references)) {
        NED_WARNING("Undo asset reference changing command failed, path: {}", m_objectId);
        return;
    }

    NED_DEBUG("Undo asset reference changing command, path: {}", m_objectId);
}

NauCommandDescriptor NauCommandChangeNodeAssetReference::description() const
{
    return {
        .id = id,
        .name = "Asset reference changing",
        .objectId = m_objectId
    };
}

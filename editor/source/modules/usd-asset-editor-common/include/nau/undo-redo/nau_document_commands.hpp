// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Undo/redo commands for editor documents. They edit only through NauDocumentInterface.

#pragma once

#include "nau/nau_usd_asset_editor_common_config.hpp"
#include "nau/document/nau_document_interface.hpp"
#include "commands/nau_commands.hpp"

#include <optional>
#include <string>
#include <vector>


// ** NauAbstractDocumentCommand

class NAU_USD_ASSET_EDITOR_COMMON_API NauAbstractDocumentCommand : public NauAbstractCommand
{
public:
    NauAbstractDocumentCommand() = delete;
    explicit NauAbstractDocumentCommand(NauDocumentPtr document);

protected:
    NauDocumentPtr m_document;
};


// ** NauCommandCreateNode
//
// Redo creates the node again with the id and name it got on the first execute, so commands further up the stack still find it.

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandCreateNode : public NauAbstractDocumentCommand
{
public:
    NauCommandCreateNode(NauDocumentPtr document, const NauDocumentNodeCreateInfo& info);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

    // Null until executed, or if the node couldn't be created
    NauDocumentNodeId node() const;

private:
    NauDocumentNodeCreateInfo m_info;
    std::string m_objectId;
};


// ** NauCommandRemoveNode

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRemoveNode : public NauAbstractDocumentCommand
{
public:
    NauCommandRemoveNode(NauDocumentPtr document, NauDocumentNodeId node);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    const NauDocumentNodeId m_node;
    const std::string m_objectId;
    NauDocumentNodeSnapshot m_snapshot;
};


// ** NauCommandRenameNode
//
// Changes the display name; the node name stays

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandRenameNode : public NauAbstractDocumentCommand
{
public:
    NauCommandRenameNode(NauDocumentPtr document, NauDocumentNodeId node, const std::string& newName);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    const NauDocumentNodeId m_node;
    const std::string m_objectId;
    std::string m_oldName;
    const std::string m_newName;
};


// ** NauCommandReparentNode
//
// Undo moves the node back to the end of its old parent's children

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandReparentNode : public NauAbstractDocumentCommand
{
public:
    NauCommandReparentNode(NauDocumentPtr document, NauDocumentNodeId node, NauDocumentNodeId newParent);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

private:
    const NauDocumentNodeId m_node;
    const std::string m_objectId;
    NauDocumentNodeId m_oldParent;
    const NauDocumentNodeId m_newParent;
};


// ** NauCommandChangeNodeProperty

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeNodeProperty : public NauAbstractDocumentCommand
{
public:
    // Undo sets the value the property has now
    NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key, const NauDocumentValue& newValue);

    // Undo sets initialValue. For edits already applied, e.g. a gizmo drag added with execute = false.
    NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key,
        const NauDocumentValue& initialValue, const NauDocumentValue& newValue);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

protected:
    // Without a new value, execute does nothing; without an initial value, undo does nothing
    NauCommandChangeNodeProperty(NauDocumentPtr document, NauDocumentNodeId node, const std::string& key,
        std::optional<NauDocumentValue> initialValue, std::optional<NauDocumentValue> newValue);

private:
    const NauDocumentNodeId m_node;
    const std::string m_key;
    const std::string m_objectId;
    const std::optional<NauDocumentValue> m_initialValue;
    const std::optional<NauDocumentValue> m_value;
};


// ** NauCommandChangeNodeAssetReference
//
// Replaces the node's asset references with one reference

class NAU_USD_ASSET_EDITOR_COMMON_API NauCommandChangeNodeAssetReference : public NauAbstractDocumentCommand
{
public:
    // Undo puts back the references the node had before execute
    NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node, const NauDocumentAssetRef& newValue);

    // Undo sets initialValue as the only reference
    NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node,
        const NauDocumentAssetRef& initialValue, const NauDocumentAssetRef& newValue);

    void execute() override;
    void undo() override;
    NauCommandDescriptor description() const override;

protected:
    // Without a new value, execute does nothing
    NauCommandChangeNodeAssetReference(NauDocumentPtr document, NauDocumentNodeId node,
        std::optional<NauDocumentAssetRef> initialValue, std::optional<NauDocumentAssetRef> newValue);

private:
    const NauDocumentNodeId m_node;
    const std::string m_objectId;
    const std::optional<NauDocumentAssetRef> m_initialValue;
    const std::optional<NauDocumentAssetRef> m_value;
    std::vector<NauDocumentAssetRef> m_previousReferences;
};

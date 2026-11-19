// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Unit-tests for the USD backend of the editor document interface

#pragma once

#include "nau_usd_scene_snapshot.hpp"
#include "nau/document-usd/nau_usd_document.hpp"
#include "nau/undo-redo/nau_usd_scene_undo_redo.hpp"

#include "gtest/gtest.h"


// ** NauUsdDocumentTests

class NauUsdDocumentTests : public testing::Test
{
protected:
    static void SetUpTestSuite();
    virtual void SetUp() override;

    NauSceneSnapshot snapshot() const;

    // Authors an Xform the same way the prim factory does, without going through the document
    void defineXform(const std::string& path, const std::string& displayName, const PXR_NS::GfMatrix4d& transform);

    NauDocumentNodeId node(const std::string& path) const;

    NauDocumentNodeId createObject(NauDocumentNodeId parent, const std::string& name, const PXR_NS::GfMatrix4d& transform);
    NauDocumentNodeId createComponent(NauDocumentNodeId owner, const std::string& name);

protected:
    PXR_NS::UsdStageRefPtr m_stage;
    std::shared_ptr<NauUsdDocument> m_document;
};


// ** NauDocumentCommandsTests
//
// The undo commands through node ids, as editors will call them once they use the document

class NauDocumentCommandsTests : public NauUsdDocumentTests
{
protected:
    virtual void SetUp() override;

    bool undo();
    bool redo();

protected:
    NauUndoable m_undoable;
    NauUsdSceneUndoRedoSystem m_undoRedo{ &m_undoable };
};

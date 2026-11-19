// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Unit-tests for usd scene undo/redo commands

#pragma once

#include "nau_usd_scene_snapshot.hpp"

#include "nau/undo-redo/nau_usd_scene_undo_redo.hpp"

#include "gtest/gtest.h"


//** NauUsdUndoCommandsTests

class NauUsdUndoCommandsTests : public testing::Test
{
protected:
    static void SetUpTestSuite();
    virtual void SetUp() override;

    NauSceneSnapshot snapshot() const;

    // Authors an Xform the same way the prim factory does, without going through undo/redo
    void defineXform(const std::string& path, const std::string& displayName, const PXR_NS::GfMatrix4d& transform);

    // Creates a prim through undo/redo with m_createTransform
    void createPrim(const std::string& path, const std::string& typeName, const std::string& displayName, bool isComponent);

    bool undo();
    bool redo();

protected:
    NauUndoable m_undoable;
    NauUsdSceneUndoRedoSystem m_undoRedo{ &m_undoable };
    PXR_NS::UsdStageRefPtr m_stage;
    PXR_NS::GfMatrix4d m_createTransform;
};


//** NauUsdAssetReferenceCommandTests
// /Obj references /Library/MeshA; /Library/MeshB is there to switch to

class NauUsdAssetReferenceCommandTests : public NauUsdUndoCommandsTests
{
protected:
    virtual void SetUp() override;
};

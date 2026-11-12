// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// These tests pin down what the commands do to the scene on execute, undo and redo, so the
// rewrite onto the editor document interface can be checked against them.
// Changing an expectation here is a behavior change.

#include "nau_usd_undo_commands_tests.hpp"
#include "nau_gtest_printers.hpp"

#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/selection/nau_usd_selection_container.hpp"
#include "nau/undo-redo/nau_usd_scene_commands.hpp"
#include "nau/undo-redo/nau_usd_selection_commands.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"

#include "pxr/usd/usd/references.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/sdf/reference.h"


namespace
{
    const std::string TestComponentType = "TestLightComponent";
    const std::string TestComponentDisplayName = "Test Light";

    const std::string IdentityTransform = NauUsdSnapshotUtils::translationString(0, 0, 0);

    PXR_NS::GfMatrix4d translation(double x, double y, double z)
    {
        PXR_NS::GfMatrix4d matrix(1.0);
        matrix.SetTranslateOnly(PXR_NS::GfVec3d(x, y, z));
        return matrix;
    }

    // An Xform as authored by NauDefaultUsdPrimCreator through the prim factory
    NauNodeSnapshot xformNode(const std::string& displayName, const std::string& transform)
    {
        return {
            .typeName = "Xform",
            .displayName = displayName,
            .attributes = {
                { "xformOp:transform", { transform } },
                { "xformOpOrder", { "[xformOp:transform]" } },
            },
        };
    }

    // A component as authored by NauUsdPrimComponentCreator through the prim factory
    NauNodeSnapshot componentNode(const std::string& transform)
    {
        return {
            .typeName = "NauComponent",
            .displayName = TestComponentDisplayName,
            .kind = "component",
            .attributes = {
                { "componentTypeName", { TestComponentType, true } },
                { "xformOp:transform", { transform } },
                { "xformOpOrder", { "[xformOp:transform]" } },
            },
        };
    }

    PXR_NS::SdfReference internalReference(const std::string& primPath)
    {
        PXR_NS::SdfReference reference;
        reference.SetPrimPath(PXR_NS::SdfPath(primPath));
        return reference;
    }

    std::vector<std::string> rootChildrenNames(const PXR_NS::UsdStageRefPtr& stage)
    {
        std::vector<std::string> names;
        for (const auto& prim : stage->GetPseudoRoot().GetChildren()) {
            names.push_back(prim.GetName().GetString());
        }
        return names;
    }
}


// ** NauUsdUndoCommandsTests

void NauUsdUndoCommandsTests::SetUpTestSuite()
{
    // Same registrations the scene editor makes (NauUsdSceneEditor::initialize / postInitialize)
    auto& factory = NauUsdPrimFactory::instance();
    factory.addCreator("Xform", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator(TestComponentType, std::make_shared<NauUsdPrimComponentCreator>(TestComponentType), TestComponentDisplayName);
}

void NauUsdUndoCommandsTests::SetUp()
{
    m_stage = PXR_NS::UsdStage::CreateInMemory();
    m_undoRedo.bindCurrentScene(m_stage);
    m_createTransform = translation(1, 2, 3);
}

NauSceneSnapshot NauUsdUndoCommandsTests::snapshot() const
{
    return NauUsdSnapshotUtils::snapshotStage(m_stage);
}

void NauUsdUndoCommandsTests::defineXform(const std::string& path, const std::string& displayName, const PXR_NS::GfMatrix4d& transform)
{
    auto xform = PXR_NS::UsdGeomXform::Define(m_stage, PXR_NS::SdfPath(path));
    xform.GetPrim().SetDisplayName(displayName);
    xform.AddTransformOp(PXR_NS::UsdGeomXformOp::PrecisionDouble).Set(transform);
}

void NauUsdUndoCommandsTests::createPrim(const std::string& path, const std::string& typeName, const std::string& displayName, bool isComponent)
{
    m_undoRedo.addCommand<NauCommandCreateUsdPrim>(path, typeName, displayName, m_createTransform, isComponent);
}

bool NauUsdUndoCommandsTests::undo()
{
    return m_undoable.undo();
}

bool NauUsdUndoCommandsTests::redo()
{
    return m_undoable.redo();
}


// ** NauUsdAssetReferenceCommandTests

void NauUsdAssetReferenceCommandTests::SetUp()
{
    NauUsdUndoCommandsTests::SetUp();

    // Internal references keep the tests free of file IO; the command treats them like asset references
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA"));
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshB"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).GetReferences().AddReference(internalReference("/Library/MeshA"));
}


// ** Create

TEST_F(NauUsdUndoCommandsTests, createPrimExecuteUndoRedo)
{
    // Arrange
    const NauSceneSnapshot before = snapshot();
    const NauSceneSnapshot expected = { { "/Obj", xformNode("Obj", NauUsdSnapshotUtils::translationString(1, 2, 3)) } };

    // Act
    createPrim("/Obj", "Xform", "Obj", false);
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}

TEST_F(NauUsdUndoCommandsTests, createPrimRedoUsesTransformFromCreation)
{
    // NauUsdSceneEditor::createPrim passes a local matrix that is gone by the time redo runs,
    // so the command must keep its own copy

    // Arrange
    const NauSceneSnapshot expected = { { "/Obj", xformNode("Obj", NauUsdSnapshotUtils::translationString(1, 2, 3)) } };
    createPrim("/Obj", "Xform", "Obj", false);
    const bool undone = undo();

    // Act
    m_createTransform = translation(7, 8, 9);
    const bool redone = redo();

    // Assert
    EXPECT_TRUE(undone);
    EXPECT_TRUE(redone);
    EXPECT_EQ(snapshot(), expected);
}

TEST_F(NauUsdUndoCommandsTests, createComponentExecuteUndoRedo)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_createTransform = translation(0, 0, 0);
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj/Light"] = componentNode(IdentityTransform);

    // Act
    // The editor passes the component type as both type and display name; the factory replaces the
    // display name with the one registered for the type
    createPrim("/Obj/Light", TestComponentType, TestComponentType, true);
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}

TEST_F(NauUsdUndoCommandsTests, createPrimOfUnregisteredTypeCreatesNothing)
{
    // Paste/duplicate used to pass "NauComponent" here and crash on a null creator

    // Act
    createPrim("/Obj", "NauComponent", "Obj", true);
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, NauSceneSnapshot{});
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, NauSceneSnapshot{});
}

TEST_F(NauUsdUndoCommandsTests, componentCopyUsesEngineClassAsCreationType)
{
    // What paste/duplicate do for each copied prim: create it again by its creation type

    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_createTransform = translation(0, 0, 0);
    createPrim("/Obj/Light", TestComponentType, TestComponentType, true);
    const auto component = m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj/Light"));
    const auto object = m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj"));

    // Act
    const std::string componentCreationType = NauUsdPrimUtils::creationTypeName(component);
    const std::string objectCreationType = NauUsdPrimUtils::creationTypeName(object);
    createPrim("/Obj/LightCopy", componentCreationType, component.GetName().GetString(), NauUsdPrimUtils::isPrimComponent(component));

    // Assert
    EXPECT_EQ(componentCreationType, TestComponentType);
    EXPECT_EQ(objectCreationType, "Xform");
    EXPECT_EQ(snapshot().at("/Obj/LightCopy"), componentNode(IdentityTransform));
}


// ** Remove

TEST_F(NauUsdUndoCommandsTests, removePrimUndoRestoresAttributesAndChildren)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(1, 2, 3));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(2.0f);
    defineXform("/Obj/Child", "Child", translation(0, 0, 2));
    const NauSceneSnapshot before = snapshot();

    // Act
    m_undoRedo.addCommand<NauCommandRemoveUsdPrim>(std::string("/Obj"));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, NauSceneSnapshot{});
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, NauSceneSnapshot{});
}

TEST_F(NauUsdUndoCommandsTests, removePrimUndoKeepsSiblingOrder)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    defineXform("/C", "C", translation(0, 0, 0));

    // Act
    m_undoRedo.addCommand<NauCommandRemoveUsdPrim>(std::string("/B"));
    const std::vector<std::string> afterExecute = rootChildrenNames(m_stage);
    const bool undone = undo();
    const std::vector<std::string> afterUndo = rootChildrenNames(m_stage);

    // Assert
    EXPECT_EQ(afterExecute, (std::vector<std::string>{ "A", "C" }));
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, (std::vector<std::string>{ "A", "B", "C" }));
}

TEST_F(NauUsdUndoCommandsTests, removeComponentUndoRestoresComponent)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_createTransform = translation(0, 0, 0);
    createPrim("/Obj/Light", TestComponentType, TestComponentType, true);
    const NauSceneSnapshot withComponent = snapshot();
    NauSceneSnapshot removed = withComponent;
    removed.erase("/Obj/Light");

    // Act
    // Same call as the inspector's "remove component" handler
    m_undoRedo.addCommand<NauCommandRemoveUsdPrim>(std::string("/Obj/Light"));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, removed);
    EXPECT_TRUE(undone);
    // componentTypeName must come back, or the engine can't map the component
    EXPECT_EQ(afterUndo, withComponent);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, removed);
}

TEST_F(NauUsdUndoCommandsTests, removePrimWithChildAsGroupLikeEditor)
{
    // NauUsdSceneEditor deletes children first, one command per prim, in a group sized by
    // NauUsdNormalizedContainer::count()

    // Arrange
    defineXform("/A", "A", translation(1, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 2));
    const NauSceneSnapshot before = snapshot();

    // Act
    m_undoRedo.groupBegin(2);
    m_undoRedo.addCommand<NauCommandRemoveUsdPrim>(std::string("/A/Child"));
    m_undoRedo.addCommand<NauCommandRemoveUsdPrim>(std::string("/A"));
    const bool groupIsOpenAfterExecute = m_undoable.groupIsOpen();
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool undoneAgain = undo();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_FALSE(groupIsOpenAfterExecute);
    EXPECT_EQ(afterExecute, NauSceneSnapshot{});
    // One undo restores both
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_FALSE(undoneAgain);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, NauSceneSnapshot{});
}


// ** Rename

TEST_F(NauUsdUndoCommandsTests, renamePrimExecuteUndoRedo)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(1, 2, 3));
    const NauSceneSnapshot before = snapshot();
    // Rename changes the display name only; the prim path stays
    NauSceneSnapshot expected = before;
    expected["/Obj"].displayName = "Renamed";

    // Act
    m_undoRedo.addCommand<NauCommandRenameUsdPrim>(std::string("/Obj"), std::string("Renamed"));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}


// ** Reparent

TEST_F(NauUsdUndoCommandsTests, reparentPrimKeepsWorldTransform)
{
    // Arrange
    defineXform("/A", "A", translation(1, 0, 0));
    defineXform("/B", "B", translation(0, 5, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 2));
    const NauSceneSnapshot before = snapshot();
    // World position (1, 0, 2) is kept, so the local transform under /B becomes (1, -5, 2)
    NauSceneSnapshot expected = before;
    expected.erase("/A/Child");
    expected["/B/Child"] = xformNode("Child", NauUsdSnapshotUtils::translationString(1, -5, 2));

    // Act
    // Same arguments as the outliner's drag and drop: current prim path and new parent path
    m_undoRedo.addCommand<NauCommandReparentUsdPrim>(std::string("/A/Child"), std::string("/B"));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}


// ** Change property

TEST_F(NauUsdUndoCommandsTests, changePropertyExecuteUndoRedo)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(1.0f);
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj"].attributes["intensity"].value = "2.5";

    // Act
    // Same call as the inspector's property change handler: the initial value is read by the command
    m_undoRedo.addCommand<NauCommandChangeUsdPrimProperty>(PXR_NS::SdfPath("/Obj"), PXR_NS::TfToken("intensity"), PXR_NS::VtValue(2.5f));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}

TEST_F(NauUsdUndoCommandsTests, changePropertyWithInitialValueUndoSetsGivenValue)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(1.0f);

    // Act
    // The explicit initial value wins over what the stage held
    m_undoRedo.addCommand<NauCommandChangeUsdPrimProperty>(PXR_NS::SdfPath("/Obj"), PXR_NS::TfToken("intensity"), PXR_NS::VtValue(0.5f), PXR_NS::VtValue(2.5f));
    const std::string afterExecute = snapshot()["/Obj"].attributes["intensity"].value;
    const bool undone = undo();
    const std::string afterUndo = snapshot()["/Obj"].attributes["intensity"].value;
    const bool redone = redo();
    const std::string afterRedo = snapshot()["/Obj"].attributes["intensity"].value;

    // Assert
    EXPECT_EQ(afterExecute, "2.5");
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, "0.5");
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, "2.5");
}


// ** Change asset reference

TEST_F(NauUsdAssetReferenceCommandTests, withInitialValueExecuteUndoRedo)
{
    // Arrange
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj"].references = { "</Library/MeshB>" };

    // Act
    m_undoRedo.addCommand<NauCommandChangeUsdPrimAssetReference>(PXR_NS::SdfPath("/Obj"),
        PXR_NS::VtValue(internalReference("/Library/MeshA")), PXR_NS::VtValue(internalReference("/Library/MeshB")));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    ASSERT_EQ(before.at("/Obj").references, std::vector<std::string>{ "</Library/MeshA>" });
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}

TEST_F(NauUsdAssetReferenceCommandTests, withoutInitialValueUndoRestoresPreviousReference)
{
    // Arrange
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj"].references = { "</Library/MeshB>" };

    // Act
    // Same call as every editor call site (scene, UI, material, VFX): no initial value is passed
    m_undoRedo.addCommand<NauCommandChangeUsdPrimAssetReference>(PXR_NS::SdfPath("/Obj"), PXR_NS::VtValue(internalReference("/Library/MeshB")));
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, expected);
}

TEST_F(NauUsdAssetReferenceCommandTests, withoutInitialValueUndoRemovesReferenceAddedToPrimWithout)
{
    // Arrange
    defineXform("/Empty", "Empty", translation(0, 0, 0));
    const NauSceneSnapshot before = snapshot();

    // Act
    m_undoRedo.addCommand<NauCommandChangeUsdPrimAssetReference>(PXR_NS::SdfPath("/Empty"), PXR_NS::VtValue(internalReference("/Library/MeshB")));
    const std::vector<std::string> referencesAfterExecute = snapshot().at("/Empty").references;
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();

    // Assert
    EXPECT_EQ(referencesAfterExecute, std::vector<std::string>{ "</Library/MeshB>" });
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
}


// ** Selection

TEST_F(NauUsdUndoCommandsTests, selectionChangedExecuteUndoRedo)
{
    // Note: no editor code issues this command today; the outliner and viewport change selection directly

    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    const NauSceneSnapshot before = snapshot();

    auto selection = std::make_shared<NauUsdSelectionContainer>();
    selection->updateFromPaths(m_stage, { "/A" });

    int broadcasts = 0;
    selection->selectionChangedDelegate.addCallback([&broadcasts](const NauUsdPrimsSelection&) { ++broadcasts; });

    auto selectedPaths = [&selection] {
        std::vector<std::string> paths;
        for (const auto& prim : selection->selection()) {
            paths.push_back(prim.GetPath().GetString());
        }
        return paths;
    };

    const NauUsdPrimsSelection oldSelection = selection->selection();
    const NauUsdPrimsSelection newSelection = { m_stage->GetPrimAtPath(PXR_NS::SdfPath("/B")) };

    // Act
    m_undoRedo.addCommand<NauCommandSelectionChanged>(selection, oldSelection, newSelection);
    const std::vector<std::string> afterExecute = selectedPaths();
    const bool undone = undo();
    const std::vector<std::string> afterUndo = selectedPaths();
    const bool redone = redo();
    const std::vector<std::string> afterRedo = selectedPaths();

    // Assert
    EXPECT_EQ(afterExecute, std::vector<std::string>{ "/B" });
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, std::vector<std::string>{ "/A" });
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, std::vector<std::string>{ "/B" });
    EXPECT_EQ(broadcasts, 3);
    EXPECT_EQ(snapshot(), before);
}


// ** Grouping

TEST_F(NauUsdUndoCommandsTests, groupedCommandsUndoAndRedoTogether)
{
    // Act
    m_undoRedo.groupBegin(2);
    createPrim("/A", "Xform", "A", false);
    const bool groupIsOpenAfterFirst = m_undoable.groupIsOpen();
    createPrim("/B", "Xform", "B", false);
    const bool groupIsOpenAfterSecond = m_undoable.groupIsOpen();
    const NauSceneSnapshot afterExecute = snapshot();

    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool undoneAgain = undo();

    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();
    const bool redoneAgain = redo();

    // Assert
    EXPECT_TRUE(groupIsOpenAfterFirst);
    EXPECT_FALSE(groupIsOpenAfterSecond);
    EXPECT_EQ(afterExecute.size(), 2u);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, NauSceneSnapshot{});
    EXPECT_FALSE(undoneAgain);
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo, afterExecute);
    EXPECT_FALSE(redoneAgain);
}

TEST_F(NauUsdUndoCommandsTests, groupOfOneCommandIsNotGrouped)
{
    // Act
    m_undoRedo.groupBegin(1);
    const bool groupIsOpen = m_undoable.groupIsOpen();
    createPrim("/A", "Xform", "A", false);
    createPrim("/B", "Xform", "B", false);

    // Two separate undo steps
    const bool firstUndone = undo();
    const size_t sizeAfterFirstUndo = snapshot().size();
    const bool secondUndone = undo();
    const NauSceneSnapshot afterSecondUndo = snapshot();

    // Assert
    EXPECT_FALSE(groupIsOpen);
    EXPECT_TRUE(firstUndone);
    EXPECT_EQ(sizeAfterFirstUndo, 1u);
    EXPECT_TRUE(secondUndone);
    EXPECT_EQ(afterSecondUndo, NauSceneSnapshot{});
}

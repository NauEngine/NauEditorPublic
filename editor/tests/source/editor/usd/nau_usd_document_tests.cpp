// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// What the USD backend of the editor document does to the stage. Where a test does what an undo command does today,
// it expects the same stage as the command's test in nau_usd_undo_commands_tests.cpp, so the commands can move onto
// the document without changing what they author.

#include "nau_usd_document_tests.hpp"
#include "nau_gtest_printers.hpp"

#include "nau/document-usd/nau_usd_document_factory.hpp"
#include "nau/prim-factory/nau_usd_prim_creator.hpp"
#include "nau/prim-factory/nau_usd_prim_factory.hpp"
#include "nau/utils/nau_usd_editor_utils.hpp"

#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/primSpec.h"
#include "pxr/usd/sdf/reference.h"
#include "pxr/usd/sdf/schema.h"
#include "pxr/usd/usd/references.h"
#include "pxr/usd/usdGeom/xform.h"

#include <algorithm>
#include <filesystem>


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

    NauDocumentMat4 documentMatrix(const PXR_NS::GfMatrix4d& matrix)
    {
        NauDocumentMat4 result;
        std::copy(matrix.data(), matrix.data() + 16, result.values.begin());
        return result;
    }

    NauDocumentMat4 documentTranslation(double x, double y, double z)
    {
        return documentMatrix(translation(x, y, z));
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
    NauNodeSnapshot componentNode(const std::string& componentType, const std::string& displayName)
    {
        return {
            .typeName = "NauComponent",
            .displayName = displayName,
            .kind = "component",
            .attributes = {
                { "componentTypeName", { componentType, true } },
                { "xformOp:transform", { IdentityTransform } },
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

    const NauDocumentPropertyDescriptor* findProperty(const std::vector<NauDocumentPropertyDescriptor>& properties, const std::string& key)
    {
        const auto it = std::find_if(properties.begin(), properties.end(), [&key](const NauDocumentPropertyDescriptor& property) {
            return property.key == key;
        });
        return it != properties.end() ? &*it : nullptr;
    }

    using NodeEvents = std::vector<std::pair<NauDocumentNodeId, NauDocumentNodeId>>;
    using PropertyEvents = std::vector<std::pair<NauDocumentNodeId, std::string>>;

    // (node, parent) pairs
    NodeEvents nodeEvents(const std::vector<NauDocumentNodeEvent>& events)
    {
        NodeEvents result;
        for (const NauDocumentNodeEvent& event : events) {
            result.emplace_back(event.node, event.parent);
        }
        return result;
    }

    PropertyEvents propertyEvents(const NauDocumentChanges& changes)
    {
        PropertyEvents result;
        for (const NauDocumentPropertyEvent& event : changes.changedProperties) {
            result.emplace_back(event.node, event.key);
        }
        return result;
    }

    // A file in the temp folder, removed before and after the test
    class TempFile
    {
    public:
        explicit TempFile(const std::string& name)
            : m_path((std::filesystem::temp_directory_path() / name).string())
        {
            std::filesystem::remove(m_path);
        }

        ~TempFile()
        {
            std::filesystem::remove(m_path);
        }

        const std::string& path() const
        {
            return m_path;
        }

    private:
        std::string m_path;
    };
}


// ** NauUsdDocumentTests

void NauUsdDocumentTests::SetUpTestSuite()
{
    // Same registrations the scene editor makes (NauUsdSceneEditor::initialize / postInitialize)
    auto& factory = NauUsdPrimFactory::instance();
    factory.addCreator("Xform", std::make_shared<NauDefaultUsdPrimCreator>());
    factory.addCreator(TestComponentType, std::make_shared<NauUsdPrimComponentCreator>(TestComponentType), TestComponentDisplayName);
}

void NauUsdDocumentTests::SetUp()
{
    m_stage = PXR_NS::UsdStage::CreateInMemory();
    m_document = NauUsdDocument::fromStage(m_stage);
}

NauSceneSnapshot NauUsdDocumentTests::snapshot() const
{
    return NauUsdSnapshotUtils::snapshotStage(m_stage);
}

void NauUsdDocumentTests::defineXform(const std::string& path, const std::string& displayName, const PXR_NS::GfMatrix4d& transform)
{
    auto xform = PXR_NS::UsdGeomXform::Define(m_stage, PXR_NS::SdfPath(path));
    xform.GetPrim().SetDisplayName(displayName);
    xform.AddTransformOp(PXR_NS::UsdGeomXformOp::PrecisionDouble).Set(transform);
}

NauDocumentNodeId NauUsdDocumentTests::node(const std::string& path) const
{
    return m_document->nodeFromPath(PXR_NS::SdfPath(path));
}

NauDocumentNodeId NauUsdDocumentTests::createObject(NauDocumentNodeId parent, const std::string& name, const PXR_NS::GfMatrix4d& transform)
{
    return m_document->createNode({
        .parent = parent,
        .typeName = "Xform",
        .nameHint = name,
        .displayName = name,
        .transform = documentMatrix(transform),
    });
}

NauDocumentNodeId NauUsdDocumentTests::createComponent(NauDocumentNodeId owner, const std::string& name)
{
    // The editor passes the component type as the display name; the registered one is used instead
    return m_document->createNode({
        .parent = owner,
        .typeName = TestComponentType,
        .kind = NauDocumentNodeKind::Component,
        .nameHint = name,
        .displayName = TestComponentType,
    });
}


// ** NauUsdDocumentEventsTests

void NauUsdDocumentEventsTests::SetUp()
{
    NauUsdDocumentTests::SetUp();
    m_document->subscribe([this](const NauDocumentChanges& changes) {
        m_delivered.push_back(changes);
    });
}


// ** NauDocumentCommandsTests

void NauDocumentCommandsTests::SetUp()
{
    NauUsdDocumentTests::SetUp();
    m_undoRedo.bindCurrentDocument(m_document);
}

bool NauDocumentCommandsTests::undo()
{
    return m_undoable.undo();
}

bool NauDocumentCommandsTests::redo()
{
    return m_undoable.redo();
}


// ** Structure

TEST_F(NauUsdDocumentTests, rootNodeIsThePseudoRoot)
{
    // Act
    const NauDocumentNodeId root = m_document->rootNode();
    const auto info = m_document->nodeInfo(root);

    // Assert
    EXPECT_TRUE(static_cast<bool>(root));
    EXPECT_TRUE(m_document->contains(root));
    ASSERT_TRUE(info.has_value());
    EXPECT_FALSE(static_cast<bool>(info->parent));
    EXPECT_EQ(node("/"), root);
    EXPECT_EQ(m_document->debugPath(root), "/");
    EXPECT_TRUE(m_document->properties(root).empty());
    EXPECT_FALSE(m_document->removeNode(root));
}

TEST_F(NauUsdDocumentTests, childrenAndComponentsFollowEngineModel)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    defineXform("/Obj/Child", "Child", translation(0, 0, 0));
    NauUsdPrimFactory::instance().createPrim(m_stage, PXR_NS::SdfPath("/Obj/Light"), PXR_NS::TfToken(TestComponentType),
        TestComponentType, PXR_NS::GfMatrix4d(1.0), true);
    defineXform("/Obj/Child2", "Child2", translation(0, 0, 0));

    const NauDocumentNodeId root = m_document->rootNode();
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    const NauDocumentNodeId child2 = node("/Obj/Child2");
    const NauDocumentNodeId light = node("/Obj/Light");

    // Act
    const auto objInfo = m_document->nodeInfo(obj);
    const auto lightInfo = m_document->nodeInfo(light);

    // Assert
    EXPECT_EQ(m_document->children(root), std::vector<NauDocumentNodeId>{ obj });
    EXPECT_TRUE(m_document->components(root).empty());
    EXPECT_EQ(m_document->children(obj), (std::vector<NauDocumentNodeId>{ child, child2 }));
    EXPECT_EQ(m_document->components(obj), std::vector<NauDocumentNodeId>{ light });
    EXPECT_TRUE(m_document->children(light).empty());
    EXPECT_TRUE(m_document->components(light).empty());

    ASSERT_TRUE(objInfo.has_value());
    EXPECT_EQ(objInfo->parent, root);
    EXPECT_EQ(objInfo->kind, NauDocumentNodeKind::Object);
    EXPECT_EQ(objInfo->typeName, "Xform");
    EXPECT_EQ(objInfo->displayTypeName, "Xform");
    EXPECT_EQ(objInfo->name, "Obj");
    EXPECT_EQ(objInfo->displayName, "Obj");

    ASSERT_TRUE(lightInfo.has_value());
    EXPECT_EQ(lightInfo->parent, obj);
    EXPECT_EQ(lightInfo->kind, NauDocumentNodeKind::Component);
    EXPECT_EQ(lightInfo->typeName, TestComponentType);
    EXPECT_EQ(lightInfo->displayTypeName, TestComponentDisplayName);
    EXPECT_EQ(lightInfo->name, "Light");
}

TEST_F(NauUsdDocumentTests, nodeIdsStayTheSame)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));

    // Act
    const NauDocumentNodeId first = node("/Obj");
    const NauDocumentNodeId second = node("/Obj");
    const NauDocumentNodeId missing = node("/Missing");

    // Assert
    EXPECT_TRUE(static_cast<bool>(first));
    EXPECT_EQ(first, second);
    EXPECT_FALSE(static_cast<bool>(missing));
    EXPECT_EQ(m_document->debugPath(first), "/Obj");
    EXPECT_EQ(m_document->pathFromNode(first), PXR_NS::SdfPath("/Obj"));
}


// ** Create

TEST_F(NauUsdDocumentTests, createNodeAuthorsLikePrimFactory)
{
    // Arrange
    const NauSceneSnapshot expected = { { "/Obj", xformNode("Obj", NauUsdSnapshotUtils::translationString(1, 2, 3)) } };

    // Act
    const NauDocumentNodeId obj = createObject(m_document->rootNode(), "Obj", translation(1, 2, 3));

    // Assert
    EXPECT_EQ(snapshot(), expected);
    EXPECT_TRUE(static_cast<bool>(obj));
    EXPECT_EQ(node("/Obj"), obj);
}

TEST_F(NauUsdDocumentTests, createNodeMakesNameUnique)
{
    // Act
    const NauDocumentNodeId first = createObject(m_document->rootNode(), "Obj", translation(0, 0, 0));
    const NauDocumentNodeId second = createObject(m_document->rootNode(), "Obj", translation(0, 0, 0));

    // Assert
    EXPECT_EQ(m_document->debugPath(first), "/Obj");
    EXPECT_EQ(m_document->debugPath(second), "/Obj1");
    EXPECT_EQ(m_document->nodeInfo(second)->name, "Obj1");
}

TEST_F(NauUsdDocumentTests, createComponentGetsRegisteredDisplayName)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    NauSceneSnapshot expected = snapshot();
    expected["/Obj/Light"] = componentNode(TestComponentType, TestComponentDisplayName);

    // Act
    const NauDocumentNodeId light = createComponent(node("/Obj"), "Light");

    // Assert
    EXPECT_EQ(snapshot(), expected);
    EXPECT_EQ(m_document->components(node("/Obj")), std::vector<NauDocumentNodeId>{ light });
}

TEST_F(NauUsdDocumentTests, createNodeOfUnregisteredTypeCreatesNothing)
{
    // Act
    const NauDocumentNodeId created = m_document->createNode({
        .parent = m_document->rootNode(),
        .typeName = "NauComponent",
        .nameHint = "Obj",
    });

    // Assert
    EXPECT_FALSE(static_cast<bool>(created));
    EXPECT_EQ(snapshot(), NauSceneSnapshot{});
}

TEST_F(NauUsdDocumentTests, createNodeRejectsInvalidParent)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId light = createComponent(node("/Obj"), "Light");
    const NauSceneSnapshot before = snapshot();

    // Act
    const NauDocumentNodeId componentOfRoot = createComponent(m_document->rootNode(), "Light");
    const NauDocumentNodeId objectUnderComponent = createObject(light, "Child", translation(0, 0, 0));
    const NauDocumentNodeId componentOfComponent = createComponent(light, "Light");
    const NauDocumentNodeId missingParent = createObject(nau::Uid::generate(), "Obj", translation(0, 0, 0));

    // Assert
    EXPECT_FALSE(static_cast<bool>(componentOfRoot));
    EXPECT_FALSE(static_cast<bool>(objectUnderComponent));
    EXPECT_FALSE(static_cast<bool>(componentOfComponent));
    EXPECT_FALSE(static_cast<bool>(missingParent));
    EXPECT_EQ(snapshot(), before);
}

TEST_F(NauUsdDocumentTests, createNodeWithGivenId)
{
    // Redo of a create passes the id the node had

    // Arrange
    const NauDocumentNodeId id = nau::Uid::generate();
    NauDocumentNodeCreateInfo info = {
        .parent = m_document->rootNode(),
        .typeName = "Xform",
        .nameHint = "Obj",
        .id = id,
    };

    // Act
    const NauDocumentNodeId created = m_document->createNode(info);
    const NauDocumentNodeId createdAgain = m_document->createNode(info);

    // Assert
    EXPECT_EQ(created, id);
    EXPECT_EQ(node("/Obj"), id);
    EXPECT_FALSE(static_cast<bool>(createdAgain));
    EXPECT_EQ(rootChildrenNames(m_stage), std::vector<std::string>{ "Obj" });
}


// ** Remove and restore

TEST_F(NauUsdDocumentTests, removeAndRestoreKeepContentAndIds)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(1, 2, 3));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(2.0f);
    defineXform("/Obj/Child", "Child", translation(0, 0, 2));
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    const NauSceneSnapshot before = snapshot();

    // Act
    const NauDocumentNodeSnapshot nodeSnapshot = m_document->snapshotNode(obj);
    const bool removed = m_document->removeNode(obj);
    const NauSceneSnapshot afterRemove = snapshot();
    const bool containsAfterRemove = m_document->contains(obj) || m_document->contains(child);
    const bool restored = m_document->restoreNode(nodeSnapshot);
    const NauSceneSnapshot afterRestore = snapshot();

    // Assert
    EXPECT_TRUE(removed);
    EXPECT_EQ(afterRemove, NauSceneSnapshot{});
    EXPECT_FALSE(containsAfterRemove);
    EXPECT_TRUE(restored);
    EXPECT_EQ(afterRestore, before);
    EXPECT_EQ(node("/Obj"), obj);
    EXPECT_EQ(m_document->children(obj), std::vector<NauDocumentNodeId>{ child });
    EXPECT_EQ(m_document->debugPath(child), "/Obj/Child");
}

TEST_F(NauUsdDocumentTests, restoreKeepsSiblingOrder)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    defineXform("/C", "C", translation(0, 0, 0));
    const NauDocumentNodeId b = node("/B");

    // Act
    const NauDocumentNodeSnapshot nodeSnapshot = m_document->snapshotNode(b);
    m_document->removeNode(b);
    const std::vector<std::string> afterRemove = rootChildrenNames(m_stage);
    const bool restored = m_document->restoreNode(nodeSnapshot);
    const std::vector<std::string> afterRestore = rootChildrenNames(m_stage);

    // Assert
    EXPECT_EQ(afterRemove, (std::vector<std::string>{ "A", "C" }));
    EXPECT_TRUE(restored);
    EXPECT_EQ(afterRestore, (std::vector<std::string>{ "A", "B", "C" }));
    EXPECT_EQ(m_document->children(m_document->rootNode()), (std::vector<NauDocumentNodeId>{ node("/A"), b, node("/C") }));
}

TEST_F(NauUsdDocumentTests, removeAndRestoreComponent)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId light = createComponent(node("/Obj"), "Light");
    const NauSceneSnapshot withComponent = snapshot();
    NauSceneSnapshot removed = withComponent;
    removed.erase("/Obj/Light");

    // Act
    const NauDocumentNodeSnapshot nodeSnapshot = m_document->snapshotNode(light);
    m_document->removeNode(light);
    const NauSceneSnapshot afterRemove = snapshot();
    const bool restored = m_document->restoreNode(nodeSnapshot);
    const NauSceneSnapshot afterRestore = snapshot();

    // Assert
    EXPECT_EQ(afterRemove, removed);
    EXPECT_TRUE(restored);
    // componentTypeName must come back, or the engine can't map the component
    EXPECT_EQ(afterRestore, withComponent);
    EXPECT_EQ(m_document->components(node("/Obj")), std::vector<NauDocumentNodeId>{ light });
}

TEST_F(NauUsdDocumentTests, restoreFailsIfNodeIsThereOrParentIsGone)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 0));
    const NauDocumentNodeSnapshot childSnapshot = m_document->snapshotNode(node("/A/Child"));

    // Act
    const bool restoredExisting = m_document->restoreNode(childSnapshot);
    m_document->removeNode(node("/A"));
    const bool restoredWithoutParent = m_document->restoreNode(childSnapshot);

    // Assert
    EXPECT_FALSE(restoredExisting);
    EXPECT_FALSE(restoredWithoutParent);
    EXPECT_EQ(snapshot(), NauSceneSnapshot{});
}


// ** Display name

TEST_F(NauUsdDocumentTests, setDisplayNameChangesDisplayNameOnly)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(1, 2, 3));
    const NauDocumentNodeId obj = node("/Obj");
    NauSceneSnapshot expected = snapshot();
    expected["/Obj"].displayName = "Renamed";

    // Act
    const bool renamed = m_document->setDisplayName(obj, "Renamed");
    const NauSceneSnapshot afterRename = snapshot();
    m_document->setDisplayName(obj, "");
    const auto infoAfterClear = m_document->nodeInfo(obj);

    // Assert
    EXPECT_TRUE(renamed);
    EXPECT_EQ(afterRename, expected);
    ASSERT_TRUE(infoAfterClear.has_value());
    EXPECT_EQ(infoAfterClear->displayName, "");
    EXPECT_EQ(Nau::DocumentDisplayName(*infoAfterClear), "Obj");
    EXPECT_EQ(node("/Obj"), obj);
}


// ** Reparent

TEST_F(NauUsdDocumentTests, reparentKeepsWorldTransformAndIds)
{
    // Arrange
    defineXform("/A", "A", translation(1, 0, 0));
    defineXform("/B", "B", translation(0, 5, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 2));
    const NauDocumentNodeId child = node("/A/Child");
    const NauDocumentNodeId light = createComponent(child, "Light");
    const NauSceneSnapshot before = snapshot();
    // World position (1, 0, 2) is kept, so the local transform under /B becomes (1, -5, 2)
    NauSceneSnapshot expected = before;
    expected.erase("/A/Child");
    expected.erase("/A/Child/Light");
    expected["/B/Child"] = xformNode("Child", NauUsdSnapshotUtils::translationString(1, -5, 2));
    expected["/B/Child/Light"] = componentNode(TestComponentType, TestComponentDisplayName);

    // Act
    const bool reparented = m_document->reparentNode(child, node("/B"));
    const NauSceneSnapshot afterReparent = snapshot();
    const auto infoAfterReparent = m_document->nodeInfo(child);
    const std::vector<NauDocumentNodeId> componentsAfterReparent = m_document->components(child);
    const bool reparentedBack = m_document->reparentNode(child, node("/A"));
    const NauSceneSnapshot afterReparentBack = snapshot();

    // Assert
    EXPECT_TRUE(reparented);
    EXPECT_EQ(afterReparent, expected);
    ASSERT_TRUE(infoAfterReparent.has_value());
    EXPECT_EQ(infoAfterReparent->parent, node("/B"));
    EXPECT_EQ(componentsAfterReparent, std::vector<NauDocumentNodeId>{ light });
    EXPECT_TRUE(reparentedBack);
    EXPECT_EQ(afterReparentBack, before);
    EXPECT_EQ(m_document->debugPath(light), "/A/Child/Light");
}

TEST_F(NauUsdDocumentTests, reparentMovesToTheEnd)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));

    // Act
    const bool reparented = m_document->reparentNode(node("/A/Child"), m_document->rootNode());

    // Assert
    EXPECT_TRUE(reparented);
    EXPECT_EQ(rootChildrenNames(m_stage), (std::vector<std::string>{ "A", "B", "Child" }));
}

TEST_F(NauUsdDocumentTests, reparentRejectsInvalidMoves)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 0));
    const NauDocumentNodeId a = node("/A");
    const NauDocumentNodeId child = node("/A/Child");
    const NauDocumentNodeId light = createComponent(a, "Light");
    const NauSceneSnapshot before = snapshot();

    // Act
    const bool root = m_document->reparentNode(m_document->rootNode(), a);
    const bool component = m_document->reparentNode(light, child);
    const bool underComponent = m_document->reparentNode(child, light);
    const bool underItself = m_document->reparentNode(a, a);
    const bool underDescendant = m_document->reparentNode(a, child);

    // Assert
    EXPECT_FALSE(root);
    EXPECT_FALSE(component);
    EXPECT_FALSE(underComponent);
    EXPECT_FALSE(underItself);
    EXPECT_FALSE(underDescendant);
    EXPECT_EQ(snapshot(), before);
}


// ** Properties

TEST_F(NauUsdDocumentTests, propertiesStartWithTransformAndSkipTransformOps)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(1.0f);

    // Act
    const std::vector<NauDocumentPropertyDescriptor> properties = m_document->properties(node("/Obj"));

    // Assert
    ASSERT_FALSE(properties.empty());
    EXPECT_EQ(properties.front().key, NauDocumentPropertyKeys::transform);
    EXPECT_EQ(properties.front().type, NauDocumentValueType::Matrix4);
    EXPECT_EQ(properties.front().role, NauDocumentPropertyRole::Transform);
    EXPECT_TRUE(std::none_of(properties.begin(), properties.end(), [](const NauDocumentPropertyDescriptor& property) {
        return property.key.starts_with("xformOp");
    }));

    const NauDocumentPropertyDescriptor* intensity = findProperty(properties, "intensity");
    ASSERT_NE(intensity, nullptr);
    EXPECT_EQ(intensity->type, NauDocumentValueType::Float);
    EXPECT_EQ(intensity->role, NauDocumentPropertyRole::Value);
    EXPECT_EQ(intensity->group, "");
    EXPECT_FALSE(intensity->hidden);
}

TEST_F(NauUsdDocumentTests, identityAndTypeAreNotProperties)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("uid"), PXR_NS::SdfValueTypeNames->String).Set(std::string("uid"));
    const NauDocumentNodeId light = createComponent(node("/Obj"), "Light");
    const NauDocumentNodeId obj = node("/Obj");

    // Act
    const std::vector<NauDocumentPropertyDescriptor> objProperties = m_document->properties(obj);
    const std::vector<NauDocumentPropertyDescriptor> lightProperties = m_document->properties(light);
    const auto uid = m_document->property(obj, "uid");
    const auto componentTypeName = m_document->property(light, "componentTypeName");
    const bool setUid = m_document->setProperty(obj, "uid", std::string("other"));
    const bool setComponentTypeName = m_document->setProperty(light, "componentTypeName", std::string("other"));

    // Assert
    EXPECT_EQ(findProperty(objProperties, "uid"), nullptr);
    EXPECT_EQ(findProperty(lightProperties, "componentTypeName"), nullptr);
    EXPECT_NE(findProperty(lightProperties, std::string(NauDocumentPropertyKeys::transform)), nullptr);
    EXPECT_FALSE(uid.has_value());
    EXPECT_FALSE(componentTypeName.has_value());
    EXPECT_FALSE(setUid);
    EXPECT_FALSE(setComponentTypeName);
    EXPECT_EQ(m_document->nodeInfo(light)->typeName, TestComponentType);
}

TEST_F(NauUsdDocumentTests, setPropertyNarrowsToStoredType)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(1.0f);
    const NauDocumentNodeId obj = node("/Obj");
    NauSceneSnapshot expected = snapshot();
    expected["/Obj"].attributes["intensity"].value = "2.5";

    // Act
    const auto initial = m_document->property(obj, "intensity");
    const bool set = m_document->setProperty(obj, "intensity", 2.5);
    const NauSceneSnapshot afterSet = snapshot();
    const bool setWrongType = m_document->setProperty(obj, "intensity", int64_t(3));
    const bool setMissing = m_document->setProperty(obj, "missing", 1.0);

    // Assert
    EXPECT_EQ(initial, NauDocumentValue(1.0));
    EXPECT_TRUE(set);
    EXPECT_EQ(afterSet, expected);
    EXPECT_FALSE(setWrongType);
    EXPECT_FALSE(setMissing);
    EXPECT_EQ(snapshot(), expected);
    EXPECT_EQ(m_document->property(obj, "intensity"), NauDocumentValue(2.5));
}

TEST_F(NauUsdDocumentTests, valuesConvertBothWays)
{
    // Arrange
    using namespace PXR_NS;
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    UsdPrim prim = m_stage->GetPrimAtPath(SdfPath("/Obj"));
    prim.CreateAttribute(TfToken("flag"), SdfValueTypeNames->Bool);
    prim.CreateAttribute(TfToken("count"), SdfValueTypeNames->Int);
    prim.CreateAttribute(TfToken("label"), SdfValueTypeNames->String);
    prim.CreateAttribute(TfToken("position"), SdfValueTypeNames->Point3d);
    prim.CreateAttribute(TfToken("cell"), SdfValueTypeNames->Int2);
    prim.CreateAttribute(TfToken("tint"), SdfValueTypeNames->Color4d);
    prim.CreateAttribute(TfToken("pose"), SdfValueTypeNames->Matrix4d);
    prim.CreateAttribute(TfToken("texture"), SdfValueTypeNames->Asset);
    const NauDocumentNodeId obj = node("/Obj");

    const std::vector<std::pair<std::string, NauDocumentValue>> values = {
        { "flag", true },
        { "count", int64_t(7) },
        { "label", std::string("text") },
        { "position", NauDocumentVec3{ 1.0, 2.0, 3.0 } },
        { "cell", NauDocumentIVec2{ 4, 5 } },
        { "tint", NauDocumentColor4{ 0.25, 0.5, 0.75, 1.0 } },
        { "pose", documentTranslation(1, 2, 3) },
        { "texture", NauDocumentAssetRef{ "textures/a.png", "" } },
    };

    // Act
    std::vector<std::string> failed;
    for (const auto& [key, value] : values) {
        if (!m_document->setProperty(obj, key, value) || m_document->property(obj, key) != value) {
            failed.push_back(key);
        }
    }

    int count = 0;
    GfVec3d position;
    GfVec4d tint;
    SdfAssetPath texture;
    prim.GetAttribute(TfToken("count")).Get(&count);
    prim.GetAttribute(TfToken("position")).Get(&position);
    prim.GetAttribute(TfToken("tint")).Get(&tint);
    prim.GetAttribute(TfToken("texture")).Get(&texture);

    // Assert
    EXPECT_TRUE(failed.empty()) << "First failed: " << (failed.empty() ? "" : failed.front());
    EXPECT_EQ(count, 7);
    EXPECT_EQ(position, GfVec3d(1.0, 2.0, 3.0));
    EXPECT_EQ(tint, GfVec4d(0.25, 0.5, 0.75, 1.0));
    EXPECT_EQ(texture.GetAssetPath(), "textures/a.png");
}

TEST_F(NauUsdDocumentTests, transformPropertyIsTheLocalTransform)
{
    // Arrange
    defineXform("/A", "A", translation(1, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 2));
    const NauDocumentNodeId child = node("/A/Child");

    // Act
    const auto local = m_document->property(child, std::string(NauDocumentPropertyKeys::transform));
    const NauDocumentMat4 world = m_document->worldTransform(child);
    const bool setWorld = m_document->setWorldTransform(child, documentTranslation(5, 5, 5));
    const NauSceneSnapshot afterSetWorld = snapshot();
    const bool setLocal = m_document->setProperty(child, std::string(NauDocumentPropertyKeys::transform), documentTranslation(0, 1, 0));
    const NauSceneSnapshot afterSetLocal = snapshot();

    // Assert
    EXPECT_EQ(local, NauDocumentValue(documentTranslation(0, 0, 2)));
    EXPECT_EQ(world, documentTranslation(1, 0, 2));
    EXPECT_TRUE(setWorld);
    EXPECT_EQ(afterSetWorld.at("/A/Child"), xformNode("Child", NauUsdSnapshotUtils::translationString(4, 5, 5)));
    EXPECT_TRUE(setLocal);
    EXPECT_EQ(afterSetLocal.at("/A/Child"), xformNode("Child", NauUsdSnapshotUtils::translationString(0, 1, 0)));
    EXPECT_EQ(m_document->worldTransform(m_document->rootNode()), NauDocumentMat4{});
}


// ** Asset references

TEST_F(NauUsdDocumentTests, setAssetReferencesReplacesReferences)
{
    // Internal references keep the test free of file IO; the document treats them like asset references

    // Arrange
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA"));
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshB"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).GetReferences().AddReference(internalReference("/Library/MeshA"));
    const NauDocumentNodeId obj = node("/Obj");
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj"].references = { "</Library/MeshB>" };
    NauSceneSnapshot expectedCleared = before;
    expectedCleared["/Obj"].references = {};

    // Act
    const std::vector<NauDocumentAssetRef> initial = m_document->assetReferences(obj);
    const bool set = m_document->setAssetReferences(obj, { { "", "/Library/MeshB" } });
    const NauSceneSnapshot afterSet = snapshot();
    const bool cleared = m_document->setAssetReferences(obj, {});
    const NauSceneSnapshot afterClear = snapshot();
    // Cleared like the asset reference command's undo does today, not left as an empty list
    const bool fieldAfterClear = m_stage->GetRootLayer()->GetPrimAtPath(PXR_NS::SdfPath("/Obj"))->HasField(PXR_NS::SdfFieldKeys->References);
    m_document->setAssetReferences(obj, initial);
    const NauSceneSnapshot afterSetBack = snapshot();

    // Assert
    EXPECT_EQ(initial, (std::vector<NauDocumentAssetRef>{ { "", "/Library/MeshA" } }));
    EXPECT_TRUE(set);
    EXPECT_EQ(afterSet, expected);
    EXPECT_TRUE(cleared);
    EXPECT_EQ(afterClear, expectedCleared);
    EXPECT_FALSE(fieldAfterClear);
    EXPECT_EQ(afterSetBack, before);
    // A reference change recomposes the prim, the id stays
    EXPECT_EQ(node("/Obj"), obj);
    EXPECT_TRUE(m_document->assetReferences(node("/Library/MeshB")).empty());
}


// ** Ids and edits made outside the document

TEST_F(NauUsdDocumentTests, outsideRecompositionKeepsIdsOfNodesStillThere)
{
    // E.g. a referenced asset changes, or another editor changes references on the stage

    // Arrange
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    defineXform("/Obj/Child", "Child", translation(0, 0, 0));
    defineXform("/Obj/Other", "Other", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    const NauDocumentNodeId other = node("/Obj/Other");

    // Act
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).GetReferences().AddReference(internalReference("/Library/MeshA"));
    const bool containsAfterRecompose = m_document->contains(obj) && m_document->contains(child) && m_document->contains(other);
    m_stage->RemovePrim(PXR_NS::SdfPath("/Obj/Other"));

    // Assert
    EXPECT_TRUE(containsAfterRecompose);
    EXPECT_TRUE(m_document->contains(obj));
    EXPECT_TRUE(m_document->contains(child));
    EXPECT_FALSE(m_document->contains(other));
    EXPECT_EQ(node("/Obj/Child"), child);
}

TEST_F(NauUsdDocumentTests, outsideRestructureGivesNewIds)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");

    // Act
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).SetDisplayName("Renamed");
    const bool containsAfterValueEdit = m_document->contains(obj);
    m_stage->RemovePrim(PXR_NS::SdfPath("/Obj"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const bool containsAfterRestructure = m_document->contains(obj);

    // Assert
    EXPECT_TRUE(containsAfterValueEdit);
    EXPECT_FALSE(containsAfterRestructure);
    EXPECT_TRUE(static_cast<bool>(node("/Obj")));
    EXPECT_NE(node("/Obj"), obj);
}


// ** File

TEST_F(NauUsdDocumentTests, modifiedUntilSaved)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const auto document = NauUsdDocument::fromStage(m_stage);

    // Act
    const bool modifiedInitially = document->isModified();
    document->setDisplayName(document->nodeFromPath(PXR_NS::SdfPath("/Obj")), "Renamed");
    const bool modifiedAfterChange = document->isModified();
    // An in-memory stage writes nothing (USD warns about it), but the flag is cleared as after saving a file
    document->save();
    const bool modifiedAfterSave = document->isModified();

    // Assert
    EXPECT_FALSE(modifiedInitially);
    EXPECT_TRUE(modifiedAfterChange);
    EXPECT_FALSE(modifiedAfterSave);
}

TEST_F(NauUsdDocumentTests, createSaveAndOpen)
{
    // Arrange
    const TempFile file("nau_usd_document_tests_create.usda");

    // Act
    std::string savedPath;
    {
        const auto created = NauUsdDocument::createNew(file.path());
        ASSERT_NE(created, nullptr);
        created->createNode({ .parent = created->rootNode(), .typeName = "Xform", .nameHint = "Obj", .displayName = "Obj" });
        created->save();
        savedPath = created->filePath();
    }
    const auto opened = NauUsdDocument::open(file.path());
    ASSERT_NE(opened, nullptr);
    const std::vector<NauDocumentNodeId> children = opened->children(opened->rootNode());

    // Assert
    EXPECT_EQ(std::filesystem::path(savedPath), std::filesystem::path(file.path()));
    EXPECT_FALSE(opened->isModified());
    ASSERT_EQ(children.size(), 1u);
    EXPECT_EQ(opened->nodeInfo(children.front())->displayName, "Obj");
}

TEST_F(NauUsdDocumentTests, openGivesEveryXformableOneTransformOp)
{
    // Arrange
    const TempFile file("nau_usd_document_tests_open.usda");
    {
        auto stage = PXR_NS::UsdStage::CreateNew(file.path());
        auto xform = PXR_NS::UsdGeomXform::Define(stage, PXR_NS::SdfPath("/Obj"));
        xform.AddTranslateOp().Set(PXR_NS::GfVec3d(1, 2, 3));
        stage->Save();
    }

    // Act
    const auto opened = NauUsdDocument::open(file.path());
    ASSERT_NE(opened, nullptr);
    const NauSceneSnapshot openedSnapshot = NauUsdSnapshotUtils::snapshotStage(opened->stage());

    // Assert
    EXPECT_EQ(openedSnapshot.at("/Obj").attributes.at("xformOpOrder").value, "[xformOp:transform]");
    EXPECT_EQ(opened->worldTransform(opened->nodeFromPath(PXR_NS::SdfPath("/Obj"))), documentTranslation(1, 2, 3));
    EXPECT_FALSE(opened->isModified());
}


// ** Type registry

TEST_F(NauUsdDocumentTests, registeredEngineComponentCreatesComponentPrim)
{
    // Arrange
    Nau::DocumentTypeRegistry().registerType({
        .typeName = "TestRegistryComponent",
        .displayName = "Test Registry Component",
        .engineComponent = true,
    });
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    NauSceneSnapshot expected = snapshot();
    expected["/Obj/Component"] = componentNode("TestRegistryComponent", "Test Registry Component");

    // Act
    const NauDocumentNodeId component = m_document->createNode({
        .parent = node("/Obj"),
        .typeName = "TestRegistryComponent",
        .kind = NauDocumentNodeKind::Component,
        .nameHint = "Component",
    });
    const std::vector<NauDocumentNodeTypeInfo> types = Nau::DocumentTypeRegistry().types();
    const auto registered = std::find_if(types.begin(), types.end(), [](const NauDocumentNodeTypeInfo& type) {
        return type.typeName == "TestRegistryComponent";
    });
    const auto xform = std::find_if(types.begin(), types.end(), [](const NauDocumentNodeTypeInfo& type) {
        return type.typeName == "Xform";
    });

    // Assert
    EXPECT_EQ(snapshot(), expected);
    EXPECT_EQ(m_document->nodeInfo(component)->displayTypeName, "Test Registry Component");
    ASSERT_NE(registered, types.end());
    EXPECT_TRUE(registered->engineComponent);
    EXPECT_EQ(registered->displayName, "Test Registry Component");
    // Registered with the prim factory directly
    ASSERT_NE(xform, types.end());
    EXPECT_EQ(xform->displayName, "Xform");
    EXPECT_FALSE(xform->engineComponent);
}

TEST_F(NauUsdDocumentTests, registeredTypeWithDefaultReferenceReferencesIt)
{
    // Arrange
    Nau::DocumentTypeRegistry().registerType({
        .typeName = "TestRegistryMesh",
        .displayName = "Test Registry Mesh",
        .defaultAssetReference = NauDocumentAssetRef{ "", "/Library/MeshA" },
    });
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA"));

    // Act
    const NauDocumentNodeId mesh = m_document->createNode({
        .parent = m_document->rootNode(),
        .typeName = "TestRegistryMesh",
        .nameHint = "Mesh",
        .displayName = "Mesh",
    });

    // Assert
    EXPECT_TRUE(static_cast<bool>(mesh));
    EXPECT_EQ(m_document->assetReferences(mesh), (std::vector<NauDocumentAssetRef>{ { "", "/Library/MeshA" } }));
}


// ** Commands

TEST_F(NauDocumentCommandsTests, redoneCreateKeepsIdForLaterCommands)
{
    // A rename further up the stack must find the node that redo created again

    // Arrange
    const NauSceneSnapshot expected = { { "/Obj", xformNode("Renamed", NauUsdSnapshotUtils::translationString(1, 2, 3)) } };
    m_undoRedo.addCommand<NauCommandCreateNode>(NauDocumentNodeCreateInfo{
        .parent = m_document->rootNode(),
        .typeName = "Xform",
        .nameHint = "Obj",
        .displayName = "Obj",
        .transform = documentTranslation(1, 2, 3),
    });
    const NauDocumentNodeId obj = node("/Obj");
    m_undoRedo.addCommand<NauCommandRenameNode>(obj, std::string("Renamed"));

    // Act
    const bool undone = undo() && undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo() && redo();

    // Assert
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, NauSceneSnapshot{});
    EXPECT_TRUE(redone);
    EXPECT_EQ(snapshot(), expected);
    EXPECT_EQ(node("/Obj"), obj);
}

TEST_F(NauDocumentCommandsTests, undoneRemoveKeepsIdsForEarlierCommands)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    defineXform("/Obj/Child", "Child", translation(0, 0, 0));
    const NauSceneSnapshot before = snapshot();
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    m_undoRedo.addCommand<NauCommandRenameNode>(child, std::string("Renamed"));
    m_undoRedo.addCommand<NauCommandRemoveNode>(obj);

    // Act
    const bool undone = undo() && undo();

    // Assert
    EXPECT_TRUE(undone);
    EXPECT_EQ(snapshot(), before);
    EXPECT_EQ(node("/Obj/Child"), child);
}

TEST_F(NauDocumentCommandsTests, reparentUndoRedo)
{
    // Arrange
    defineXform("/A", "A", translation(1, 0, 0));
    defineXform("/B", "B", translation(0, 5, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 2));
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected.erase("/A/Child");
    expected["/B/Child"] = xformNode("Child", NauUsdSnapshotUtils::translationString(1, -5, 2));
    const NauDocumentNodeId child = node("/A/Child");

    // Act
    m_undoRedo.addCommand<NauCommandReparentNode>(child, node("/B"));
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

TEST_F(NauDocumentCommandsTests, transformChangeAddedAfterDragUndoRedo)
{
    // The gizmo moves the node itself and adds the command with execute = false

    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");
    const std::string key(NauDocumentPropertyKeys::transform);
    m_document->setProperty(obj, key, documentTranslation(4, 5, 6));

    // Act
    m_undoRedo.addCommand<NauCommandChangeNodeProperty, false>(obj, key, NauDocumentValue(documentTranslation(0, 0, 0)),
        NauDocumentValue(documentTranslation(4, 5, 6)));
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();
    const bool redone = redo();
    const NauSceneSnapshot afterRedo = snapshot();

    // Assert
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo.at("/Obj"), xformNode("Obj", IdentityTransform));
    EXPECT_TRUE(redone);
    EXPECT_EQ(afterRedo.at("/Obj"), xformNode("Obj", NauUsdSnapshotUtils::translationString(4, 5, 6)));
}

TEST_F(NauDocumentCommandsTests, assetReferenceUndoPutsBackPreviousReferences)
{
    // Arrange
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA"));
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshB"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).GetReferences().AddReference(internalReference("/Library/MeshA"));
    const NauSceneSnapshot before = snapshot();
    NauSceneSnapshot expected = before;
    expected["/Obj"].references = { "</Library/MeshB>" };

    // Act
    m_undoRedo.addCommand<NauCommandChangeNodeAssetReference>(node("/Obj"), NauDocumentAssetRef{ "", "/Library/MeshB" });
    const NauSceneSnapshot afterExecute = snapshot();
    const bool undone = undo();
    const NauSceneSnapshot afterUndo = snapshot();

    // Assert
    EXPECT_EQ(afterExecute, expected);
    EXPECT_TRUE(undone);
    EXPECT_EQ(afterUndo, before);
}


// ** Change events

TEST_F(NauUsdDocumentEventsTests, createNodeReportsNodeWithItsSubtree)
{
    // Arrange
    Nau::DocumentTypeRegistry().registerType({
        .typeName = "TestEventsMesh",
        .displayName = "Test Events Mesh",
        .defaultAssetReference = NauDocumentAssetRef{ "", "/Library/MeshA" },
    });
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA/Part"));
    m_delivered.clear();

    // Act
    const NauDocumentNodeId mesh = m_document->createNode({
        .parent = m_document->rootNode(),
        .typeName = "TestEventsMesh",
        .nameHint = "Mesh",
    });

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].addedNodes), (NodeEvents{ { mesh, m_document->rootNode() }, { node("/Mesh/Part"), mesh } }));
    EXPECT_TRUE(m_delivered[0].changedNodes.empty());
    EXPECT_TRUE(m_delivered[0].changedProperties.empty());
}

TEST_F(NauUsdDocumentEventsTests, removeNodeReportsKnownNodesChildrenFirst)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    defineXform("/Obj/Child", "Child", translation(0, 0, 0));
    {
        // Never reported, so no client knows it
        NauDocumentChangeBlock block(*m_document);
        defineXform("/Obj/Unknown", "Unknown", translation(0, 0, 0));
    }
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    m_delivered.clear();

    // Act
    m_document->removeNode(obj);

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].removedNodes), (NodeEvents{ { child, obj }, { obj, m_document->rootNode() } }));
    EXPECT_TRUE(m_delivered[0].addedNodes.empty());
}

TEST_F(NauUsdDocumentEventsTests, removeNodeReportsParentThatHadNoId)
{
    // Arrange
    {
        NauDocumentChangeBlock block(*m_document);
        defineXform("/Parent", "Parent", translation(0, 0, 0));
        defineXform("/Parent/Obj", "Obj", translation(0, 0, 0));
    }
    const NauDocumentNodeId obj = node("/Parent/Obj");
    m_delivered.clear();

    // Act
    m_document->removeNode(obj);

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].removedNodes), (NodeEvents{ { obj, node("/Parent") } }));
}

TEST_F(NauUsdDocumentEventsTests, restoreNodeReportsSubtreeParentsFirst)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    defineXform("/Obj/Child", "Child", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId child = node("/Obj/Child");
    const NauDocumentNodeSnapshot nodeSnapshot = m_document->snapshotNode(obj);
    m_document->removeNode(obj);
    m_delivered.clear();

    // Act
    m_document->restoreNode(nodeSnapshot);

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].addedNodes), (NodeEvents{ { obj, m_document->rootNode() }, { child, obj } }));
}

TEST_F(NauUsdDocumentEventsTests, editsReportWhatChanged)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    defineXform("/A/Child", "Child", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/A")).CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(1.0f);
    const NauDocumentNodeId a = node("/A");
    const NauDocumentNodeId b = node("/B");
    const NauDocumentNodeId child = node("/A/Child");
    const std::string transform(NauDocumentPropertyKeys::transform);
    m_delivered.clear();

    // Act
    m_document->setDisplayName(a, "Renamed");
    m_document->setProperty(a, "intensity", 2.0);
    m_document->setWorldTransform(b, documentTranslation(1, 0, 0));
    m_document->reparentNode(child, b);

    // Assert
    ASSERT_EQ(m_delivered.size(), 4u);
    EXPECT_EQ(m_delivered[0].changedNodes, std::vector<NauDocumentNodeId>{ a });
    EXPECT_EQ(propertyEvents(m_delivered[1]), (PropertyEvents{ { a, "intensity" } }));
    EXPECT_EQ(propertyEvents(m_delivered[2]), (PropertyEvents{ { b, transform } }));
    ASSERT_EQ(m_delivered[3].movedNodes.size(), 1u);
    EXPECT_EQ(m_delivered[3].movedNodes[0].node, child);
    EXPECT_EQ(m_delivered[3].movedNodes[0].oldParent, a);
    EXPECT_EQ(m_delivered[3].movedNodes[0].newParent, b);
    EXPECT_EQ(propertyEvents(m_delivered[3]), (PropertyEvents{ { child, transform } }));
}

TEST_F(NauUsdDocumentEventsTests, setAssetReferencesReportsRecomposition)
{
    // Arrange
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshA/PartA"));
    m_stage->DefinePrim(PXR_NS::SdfPath("/Library/MeshB/PartB"));
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj")).GetReferences().AddReference(internalReference("/Library/MeshA"));
    const NauDocumentNodeId obj = node("/Obj");
    const NauDocumentNodeId partA = node("/Obj/PartA");
    m_delivered.clear();

    // Act
    m_document->setAssetReferences(obj, { { "", "/Library/MeshB" } });

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].removedNodes), (NodeEvents{ { partA, obj } }));
    EXPECT_EQ(nodeEvents(m_delivered[0].addedNodes), (NodeEvents{ { node("/Obj/PartB"), obj } }));
    EXPECT_EQ(m_delivered[0].changedNodes, std::vector<NauDocumentNodeId>{ obj });
}

TEST_F(NauUsdDocumentEventsTests, batchMergesChangesOfNewNode)
{
    // Act
    NauDocumentNodeId obj;
    {
        NauDocumentChangeBatch batch(*m_document);
        obj = createObject(m_document->rootNode(), "Obj", translation(0, 0, 0));
        m_document->setDisplayName(obj, "Renamed");
        m_document->setProperty(obj, std::string(NauDocumentPropertyKeys::transform), documentTranslation(1, 2, 3));
    }

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(nodeEvents(m_delivered[0].addedNodes), (NodeEvents{ { obj, m_document->rootNode() } }));
    EXPECT_TRUE(m_delivered[0].changedNodes.empty());
    EXPECT_TRUE(m_delivered[0].changedProperties.empty());
}

TEST_F(NauUsdDocumentEventsTests, batchDropsNodeAddedAndRemoved)
{
    // Act
    {
        NauDocumentChangeBatch batch(*m_document);
        const NauDocumentNodeId obj = createObject(m_document->rootNode(), "Obj", translation(0, 0, 0));
        m_document->setDisplayName(obj, "Renamed");
        m_document->removeNode(obj);
    }

    // Assert
    EXPECT_TRUE(m_delivered.empty());
}

TEST_F(NauUsdDocumentEventsTests, batchReportsRemovedAndRestoredNodeAsChanged)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");
    m_delivered.clear();

    // Act
    {
        NauDocumentChangeBatch batch(*m_document);
        const NauDocumentNodeSnapshot nodeSnapshot = m_document->snapshotNode(obj);
        m_document->removeNode(obj);
        m_document->restoreNode(nodeSnapshot);
    }

    // Assert
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_TRUE(m_delivered[0].addedNodes.empty());
    EXPECT_TRUE(m_delivered[0].removedNodes.empty());
    EXPECT_TRUE(m_delivered[0].movedNodes.empty());
    EXPECT_EQ(m_delivered[0].changedNodes, std::vector<NauDocumentNodeId>{ obj });
}

TEST_F(NauUsdDocumentEventsTests, nestedBatchesDeliverOnceAtTheOutermostEnd)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    const NauDocumentNodeId a = node("/A");
    const NauDocumentNodeId b = node("/B");
    m_delivered.clear();

    // Act
    size_t deliveredAfterInnerBatch = 0;
    {
        NauDocumentChangeBatch outer(*m_document);
        {
            NauDocumentChangeBatch inner(*m_document);
            m_document->setDisplayName(a, "RenamedA");
        }
        deliveredAfterInnerBatch = m_delivered.size();
        m_document->setDisplayName(b, "RenamedB");
    }

    // Assert
    EXPECT_EQ(deliveredAfterInnerBatch, 0u);
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(m_delivered[0].changedNodes, (std::vector<NauDocumentNodeId>{ a, b }));
}

TEST_F(NauUsdDocumentEventsTests, blockDropsChangesButKeepsIdsCurrent)
{
    // Arrange
    defineXform("/A", "A", translation(0, 0, 0));
    defineXform("/B", "B", translation(0, 0, 0));
    const NauDocumentNodeId a = node("/A");
    const NauDocumentNodeId b = node("/B");
    m_delivered.clear();

    // Act
    {
        NauDocumentChangeBlock block(*m_document);
        m_document->setDisplayName(b, "Renamed");
        m_document->removeNode(a);
    }
    const size_t deliveredAfterBlock = m_delivered.size();
    m_document->setDisplayName(b, "RenamedAgain");

    // Assert
    EXPECT_EQ(deliveredAfterBlock, 0u);
    EXPECT_FALSE(m_document->contains(a));
    ASSERT_EQ(m_delivered.size(), 1u);
    EXPECT_EQ(m_delivered[0].changedNodes, std::vector<NauDocumentNodeId>{ b });
}

TEST_F(NauUsdDocumentEventsTests, outsideEditsAreReported)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    const NauDocumentNodeId obj = node("/Obj");
    PXR_NS::UsdPrim prim = m_stage->GetPrimAtPath(PXR_NS::SdfPath("/Obj"));
    const std::string transform(NauDocumentPropertyKeys::transform);
    m_delivered.clear();

    // Act
    defineXform("/New", "New", translation(0, 0, 0));
    const NauDocumentChanges afterDefine = m_delivered.front();
    m_delivered.clear();

    prim.SetDisplayName("Renamed");
    const NauDocumentChanges afterRename = m_delivered.back();
    m_delivered.clear();

    prim.CreateAttribute(PXR_NS::TfToken("intensity"), PXR_NS::SdfValueTypeNames->Float).Set(2.0f);
    const NauDocumentChanges afterAttribute = m_delivered.back();
    m_delivered.clear();

    NauUsdPrimUtils::setPrimTransform(prim, translation(1, 2, 3));
    const NauDocumentChanges afterTransform = m_delivered.back();
    m_delivered.clear();

    m_stage->RemovePrim(PXR_NS::SdfPath("/Obj"));
    const NauDocumentChanges afterRemove = m_delivered.back();

    // Assert
    EXPECT_EQ(nodeEvents(afterDefine.addedNodes), (NodeEvents{ { node("/New"), m_document->rootNode() } }));
    EXPECT_EQ(afterRename.changedNodes, std::vector<NauDocumentNodeId>{ obj });
    EXPECT_EQ(propertyEvents(afterAttribute), (PropertyEvents{ { obj, "intensity" } }));
    EXPECT_EQ(propertyEvents(afterTransform), (PropertyEvents{ { obj, transform } }));
    EXPECT_EQ(nodeEvents(afterRemove.removedNodes), (NodeEvents{ { obj, m_document->rootNode() } }));
}

TEST_F(NauUsdDocumentEventsTests, unsubscribedCallbackGetsNothing)
{
    // Arrange
    defineXform("/Obj", "Obj", translation(0, 0, 0));
    int calls = 0;
    const NauCallbackId id = m_document->subscribe([&calls](const NauDocumentChanges&) { ++calls; });
    m_document->unsubscribe(id);

    // Act
    m_document->setDisplayName(node("/Obj"), "Renamed");

    // Assert
    EXPECT_EQ(calls, 0);
}

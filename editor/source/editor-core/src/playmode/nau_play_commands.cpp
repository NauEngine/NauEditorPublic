// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/playmode/nau_play_commands.hpp"
#include "nau/editor-engine/nau_editor_engine_services.hpp"
#include "nau/app/nau_editor_services.hpp"

#include "nau/nau_editor_plugin_manager.hpp"
#include "nau/scene/nau_scene_editor_interface.hpp"


// ** NauPlayCommands

void NauPlayCommands::startPlay()
{
    // Unload UI in GUI editor
    auto uiEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "UI Editor";
    });

    if (uiEditor) {
        uiEditor->startPlay();
    } 
    
    // Save scene
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauSceneEditorInterface>();
    sceneEditor.sceneManager()->saveCurrentScene();

    // Reimport project
    Nau::Editor().assetManager()->importAsset("");

    // Change engine mode
    Nau::EditorEngine().startPlay();

    // Change scene editor mode
    const bool isPlaymode = true;
    sceneEditor.changeMode(isPlaymode);
}

void NauPlayCommands::pausePlay(const bool isPaused)
{
    Nau::EditorEngine().pausePlay(isPaused);
}

void NauPlayCommands::stopPlay()
{
    Nau::EditorEngine().stopPlay();
    auto transform = Nau::EditorEngine().cameraManager()->activeCamera()->getTransform();

    const bool isPlaymode = false;
    auto& sceneEditor = Nau::EditorServiceProvider().get<NauSceneEditorInterface>();
    sceneEditor.changeMode(isPlaymode);

    // Load UI in GUI editor
    auto uiEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "UI Editor";
    });

    if (uiEditor) {
        uiEditor->stopPlay();
    }
    Nau::EditorEngine().cameraManager()->activeCamera()->setTransform(transform);
}

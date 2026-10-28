// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_content_creator.hpp"
#include "nau_log.hpp"
#include "browser/nau_file_operations.hpp"
#include "nau/nau_constants.hpp"
#include "nau/nau_editor_plugin_manager.hpp"
#include "nau/assets/nau_asset_editor.hpp"

#include "nau/app/nau_editor_services.hpp"


// ** NauAddContentMenu

NauAddContentMenu::NauAddContentMenu(const QString& currentDir, NauWidget* parent)
    : NauMenu(tr("Add content"), parent)
{
    // TODO: Uncomment when support for new materials is implemented
    addAction(tr("Input Action"), [currentDir] { NauAddContentMenu::createInputAction(currentDir, "NewInputAction"); });
    addAction(tr("Audio Container"), [currentDir] { NauAddContentMenu::createAudioContainer(currentDir, "AudioContainer"); });
    addAction(tr("Animation"), [currentDir] { NauAddContentMenu::createAnimation(currentDir, "Animation"); });
    addAction(tr("VFX"), [currentDir] { NauAddContentMenu::createVFX(currentDir, "NewVFX"); });
    addAction(tr("UI"), [currentDir] { NauAddContentMenu::createUI(currentDir, "NewUI"); });
    addAction(tr("PBR material"), [currentDir] { NauAddContentMenu::createMaterial(currentDir); });
    addAction(tr("Physics Material"), [currentDir] { NauAddContentMenu::createAsset(currentDir, QStringLiteral("physics-material"), QStringLiteral("PhysicsEditor")); });
}

void NauAddContentMenu::createAnimation(const QString& path, const QString& name)
{
    const QString containerPath = NauFileOperations::generateFileNameIfExists(path + "/" + name + NAU_ANIMATION_FILE_EXTENSION);

    auto animationEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "Animation Editor";
    });

    if (animationEditor) {
        animationEditor->createAsset(containerPath.toStdString());
    }
}

void NauAddContentMenu::createInputAction(const QString& path, const QString& name)
{
    const QString containerPath = NauFileOperations::generateFileNameIfExists(path + "/" + name + ".usda");

    auto inputEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "Input Editor";
    });

    if (inputEditor) {
        inputEditor->createAsset(containerPath.toStdString());
    }
}

void NauAddContentMenu::createAudioContainer(const QString& path, const QString& name)
{
    const QString containerPath = NauFileOperations::generateFileNameIfExists(path + "/" + name + ".usda");
    
    auto audioEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "Audio Editor";
    });

    if (audioEditor) {
        audioEditor->createAsset(containerPath.toStdString());
    }
}

void NauAddContentMenu::createUI(const QString& path, const QString& name)
{
    const QString containerPath = NauFileOperations::generateFileNameIfExists(path + "/" + name + ".usda");

    auto uiEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "UI Editor";
    });

    if (uiEditor) {
        uiEditor->createAsset(containerPath.toStdString());
    }
}

void NauAddContentMenu::createVFX(const QString& path, const QString& name)
{
    const QString vfxPath = NauFileOperations::generateFileNameIfExists(path + "/" + name + ".usda");

    auto vfxEditor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>([](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == "VFX Editor";
    });

    if (vfxEditor) {
        vfxEditor->createAsset(vfxPath.toStdString());
    }
}

void NauAddContentMenu::createMaterial(const QString& path)
{
    const std::filesystem::path templatesPath = Nau::Editor().currentProject()->assetTemplatesFolder().toStdWString();
    const std::filesystem::path materialTemplateDir = templatesPath / "material";

    std::vector<std::filesystem::path > templates;

    std::function<void(const std::filesystem::path&)> fillTemplates = [&](const std::filesystem::path& materialTemplateDir)
        {
            for (const auto& entry : std::filesystem::directory_iterator(materialTemplateDir))
            {
                if (entry.is_directory())
                {
                    fillTemplates(entry.path());
                }
                if (entry.path().extension() != ".nausd")
                    continue;

                templates.push_back(entry.path());
                

            }
        };

    fillTemplates(materialTemplateDir);

    QDialog dialog;
    dialog.setWindowTitle("Template Selector");
    QLabel* templateLabel = new QLabel("Select template:");
    QComboBox* templateComboBox = new QComboBox();
    QStringList templateNames;
    const QString templateLabelText = "Template";
    
    for(const auto& temp : templates)
    {
        auto templateName = QString::fromStdWString(temp.stem().wstring());
        const auto pos = templateName.indexOf(templateLabelText);
        if (pos != -1)
        {
            templateName.remove(pos, templateLabelText.length());
            templateNames.append(templateName);
        }
    }
    templateComboBox->addItems(templateNames);

    QLabel* materialLabel = new QLabel("Material name:");
    QLineEdit* materialEdit = new QLineEdit("NewMaterial");

    QHBoxLayout* materialLayout = new QHBoxLayout;
    materialLayout->addWidget(materialLabel);
    materialLayout->addWidget(materialEdit);

    QHBoxLayout* templateLayout = new QHBoxLayout;
    templateLayout->addWidget(templateLabel);
    templateLayout->addWidget(templateComboBox);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Create");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("Cancel");

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(materialLayout);
    mainLayout->addLayout(templateLayout);
    mainLayout->addWidget(buttonBox);

    dialog.setLayout(mainLayout);

    if (dialog.exec() == QDialog::Rejected)
        return;

    createMaterialByTemplate(materialEdit->text(), path, templateComboBox->currentText(), templates[templateComboBox->currentIndex()]);
}

void NauAddContentMenu::createMaterialByTemplate(const QString& newMaterialName, const QString& path, const QString& name, const std::filesystem::path& materialTemplatePath)
{
    const QString newMaterialFileName = NauFileOperations::generateFileNameIfExists(path + "/" + newMaterialName + ".nausd");
    const std::filesystem::path newMaterialPath = newMaterialFileName.toStdWString();

    std::filesystem::copy_file(materialTemplatePath, newMaterialPath);

    QFile file(newMaterialFileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) 
    {
        NED_ERROR("Could not open file {} for reading.", newMaterialFileName.toStdString());
        return;
    }

    QString fileContent;
    QTextStream in(&file);
    fileContent = in.readAll();
    file.close();

    QString replacement = "material_" + QString::fromStdWString(newMaterialPath.stem().wstring());
    replacement.replace("(", "_").replace(")", "");
    auto uid = "string uid = \"" + nau::toString(nau::Uid::generate()) + "\"";
    fileContent.replace(name, replacement).replace("string uid = \"\"", uid.c_str());

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        NED_ERROR("Could not open file {} for writing.", newMaterialFileName.toStdString());
        return;
    }

    QTextStream out(&file);
    out << fileContent;
    file.close();
}

void NauAddContentMenu::createAsset(const QString& path, const QString& assetName, const QString& editorName)
{
    const QString assetPath = NauFileOperations::generateFileNameIfExists(path + "/" + assetName + ".usda");

    const auto predicate = [name = editorName.toStdString()](NauAssetEditorInterface& editor) -> bool {
        return editor.editorName() == name;
    };

    if (auto editor = Nau::EditorServiceProvider().findIf<NauAssetEditorInterface>(predicate)) {
        editor->createAsset(assetPath.toStdString());
    } else {
        NED_ERROR("Unable to create asset {}. Unknown editor {}", assetName, editorName);
    }

}

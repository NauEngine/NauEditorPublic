// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_shortcut_hub.hpp"
#include "nau_assert.hpp"
#include "nau_log.hpp"

#include <type_traits>

NauShortcutHub::NauShortcutHub(NauMainWindow& parent)
    : QObject(&parent)
{
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserCopy, NauKeySequence::Copy);

    setOperationKeySequence(NauShortcutOperation::ProjectBrowserCut, NauKeySequence::Cut);
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserDelete, NauKeySequence(Qt::Key_Delete));

    setOperationKeySequence(NauShortcutOperation::ProjectBrowserPaste, NauKeySequence::Paste);
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserRename, NauKeySequence(Qt::Key_F2));
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserDuplicate, NauKeySequence(Qt::CTRL | Qt::Key_D) );
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserViewInShell, NauKeySequence(Qt::CTRL | Qt::Key_O) );
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserCreateDir, NauKeySequence(Qt::Key_F7));
    setOperationKeySequence(NauShortcutOperation::ProjectBrowserFindAsset, NauKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_F));
   
    setOperationKeySequence(NauShortcutOperation::NewScene, NauKeySequence::New);
    setOperationKeySequence(NauShortcutOperation::OpenScene, NauKeySequence::Open);
    setOperationKeySequence(NauShortcutOperation::SaveScene, NauKeySequence::Save);
    setOperationKeySequence(NauShortcutOperation::Undo, NauKeySequence::Undo);
    setOperationKeySequence(NauShortcutOperation::Redo, NauKeySequence::Redo);

    setOperationKeySequence(NauShortcutOperation::WorldOutlineCopy, NauKeySequence::Copy);
    setOperationKeySequence(NauShortcutOperation::WorldOutlineCut, NauKeySequence::Cut);
    setOperationKeySequence(NauShortcutOperation::WorldOutlinePaste, NauKeySequence::Paste);
    setOperationKeySequence(NauShortcutOperation::WorldOutlineRename, NauKeySequence(Qt::Key_F2));
    setOperationKeySequence(NauShortcutOperation::WorldOutlineDelete, NauKeySequence(Qt::Key_Delete));
    setOperationKeySequence(NauShortcutOperation::WorldOutlineDuplicate, NauKeySequence(Qt::CTRL | Qt::Key_D));
    setOperationKeySequence(NauShortcutOperation::WorldOutlineFocusCamera, NauKeySequence(Qt::Key_F));
    setOperationKeySequence(NauShortcutOperation::WorldOutlineSelectAll, NauKeySequence::SelectAll);

    setOperationKeySequence(NauShortcutOperation::ViewportFocus, NauKeySequence(Qt::Key_F));
    setOperationKeySequence(NauShortcutOperation::ViewportCopy, NauKeySequence::Copy);
    setOperationKeySequence(NauShortcutOperation::ViewportCut, NauKeySequence::Cut);
    setOperationKeySequence(NauShortcutOperation::ViewportPaste, NauKeySequence::Paste);
    setOperationKeySequence(NauShortcutOperation::ViewportDuplicate, NauKeySequence(Qt::CTRL | Qt::Key_D));
    setOperationKeySequence(NauShortcutOperation::ViewportDelete, NauKeySequence(Qt::Key_Delete));
    
    setOperationKeySequence(NauShortcutOperation::ViewportSelectTool, NauKeySequence(Qt::Key_Q));
    setOperationKeySequence(NauShortcutOperation::ViewportTranslateTool, NauKeySequence(Qt::Key_W));
    setOperationKeySequence(NauShortcutOperation::ViewportRotateTool, NauKeySequence(Qt::Key_E));
    setOperationKeySequence(NauShortcutOperation::ViewportScaleTool, NauKeySequence(Qt::Key_R));

    setOperationKeySequence(NauShortcutOperation::LoggerCopySelectedMessages, NauKeySequence::Copy);
    setOperationKeySequence(NauShortcutOperation::LoggerCopyTextSelection, NauKeySequence::Copy);
}

NauShortcutOperation NauShortcutHub::registerCustomOperation()
{
    return NauShortcutOperation(++m_lastUserDefinedOperations);
}

NauKeySequence NauShortcutHub::getAssociatedKeySequence(NauShortcutOperation operation) const
{
    auto keySequenceIt = m_keySequenceByOperation.find(operation);
    if (keySequenceIt != m_keySequenceByOperation.end()) {
        return keySequenceIt->second;
    }

    return NauKeySequence();
}

void NauShortcutHub::setOperationKeySequence(NauShortcutOperation operation, const NauKeySequence& newKeySequence)
{
    auto keySequenceIt = m_keySequenceByOperation.find(operation);
    if (keySequenceIt == m_keySequenceByOperation.end()) {
        m_keySequenceByOperation.insert({operation, newKeySequence});

        // We have just added to operation a key sequence.
        // It means that to this operation there are no attached shortcut and receivers.
        // So there is nothing to update.
        return;
    }

    const NauKeySequence& currentKeySequence = keySequenceIt->second;

    auto shortcutIt = m_shortcutByKeySequence.find(currentKeySequence);
    if (shortcutIt != m_shortcutByKeySequence.end()) {
        shortcutIt->second.shortcut->setKey(newKeySequence);
    }

    keySequenceIt->second = newKeySequence;
}

bool NauShortcutHub::addWidgetShortcut(NauShortcutOperation operation,
    QWidget& widget, ShortcutEventHandler triggerCallback)
{
    auto shortcutIt = findOrCreateShortcut(operation);
    if (shortcutIt == m_shortcutByKeySequence.end()) {
        NED_WARNING("NauShortcutHub: failed to register operation {} to shortcut", operation);
        return false;
    }

    auto[it, inserted] = shortcutIt->second.widgetReceivers.insert({
        &widget, {true, std::move(triggerCallback) }
    });

    if (!inserted) {
        NED_WARNING("NauShortcutHub: failed to register {} as endpoint to operation {}", widget.objectName(), operation);
        return false;
    }

    NED_DEBUG("NauShortcutHub: widget {} registered as a listener for operation {} / {}",
        widget.objectName(), operation, shortcutIt->second.shortcut->key().toString());

    return true;
}

void NauShortcutHub::setWidgetShortcutEnabled(NauShortcutOperation operation, QWidget& widget, bool enabled)
{
    auto keySequenceIt = m_keySequenceByOperation.find(operation);
    if (keySequenceIt == m_keySequenceByOperation.end()) {
        NED_WARNING("NauShortcutHub: cannot update an unknown operation {}", operation);
        return;
    }

    auto shortcutIt = m_shortcutByKeySequence.find(keySequenceIt->second);
    if (shortcutIt == m_shortcutByKeySequence.end()) {
        NED_WARNING("NauShortcutHub: unregistered shortcut for operation {}", operation);
        return;
    }
    auto widgetIt = shortcutIt->second.widgetReceivers.find(&widget);
    if (widgetIt == shortcutIt->second.widgetReceivers.end()) {
        NED_WARNING("NauShortcutHub: cannot update shortcut {} for an unknown widget {}", operation, widget.objectName());
        return;
    }

    widgetIt->second.enabled = enabled;
}

bool NauShortcutHub::addApplicationShortcut(NauShortcutOperation operation, ShortcutEventHandler triggerCallback)
{
    auto shortcutIt =  findOrCreateShortcut(operation);
     if (shortcutIt == m_shortcutByKeySequence.end()) {
        NED_WARNING("NauShortcutHub: failed to register operation {} to shortcut", operation);
        return false;
     }

    if (shortcutIt->second.applicationReceiver) {
        NED_WARNING("NauShortcutHub: shortcut operation {} already has an application handler", operation);
        return false;
    }

    shortcutIt->second.applicationReceiver = std::move(triggerCallback);
    NED_DEBUG("NauShortcutHub: application handler registered for operation {}", operation);
    return true;
}

void NauShortcutHub::handleShortcutActivation(NauShortcutOperation operation)
{
    auto keySequenceIt = m_keySequenceByOperation.find(operation);
    if (keySequenceIt == m_keySequenceByOperation.end()) {
        NED_WARNING("NauShortcutHub: unknown shortcut handled for operation {}", operation);
        return;
    }
    
    auto shortcutIt = m_shortcutByKeySequence.find(keySequenceIt->second);
    if (shortcutIt == m_shortcutByKeySequence.end()) {
        NED_WARNING("NauShortcutHub: unregistered shortcut for operation {}", operation);
        return;
    }

    const ShortcutData& data = shortcutIt->second;

    for (const auto&[widget, dataHandler] : shortcutIt->second.widgetReceivers) {
        if (widget->hasFocus()) {
            if (!dataHandler.enabled) {
                NED_DEBUG("NauShortcutHub: shortcut will be skipped for handler {} in {} is disabled", operation, widget->objectName());
                return;
            }

            NED_DEBUG("NauShortcutHub: shortcut handler for operation {} in {} is invoked", operation, widget->objectName());

            std::invoke(dataHandler.handler, operation);
            return;
        }
    } 

    if (data.applicationReceiver) {
        NED_DEBUG("NauShortcutHub: shortcut handler for operation {} in application context is invoking", operation);
        std::invoke(*data.applicationReceiver, operation);
        return;
    }
}

void NauShortcutHub::handleShortcutAmbiguousActivation(NauShortcutOperation operation)
{
    NED_WARNING("Ambiguous shortcut activation for operation {}. That's not supposed to happen.", operation);
}

NauShortcutHub::ShortcutRepository::iterator NauShortcutHub::findOrCreateShortcut(NauShortcutOperation operation)
{
    auto keySequenceIt = m_keySequenceByOperation.find(operation);
    if (keySequenceIt == m_keySequenceByOperation.end()) {
        NED_WARNING("NauShortcutHub: cannot register a shortcut for unknown operation {}", operation);
        return m_shortcutByKeySequence.end();
    }

    const NauKeySequence& keySequence = keySequenceIt->second;
    if (keySequence.isEmpty()) {
        NED_WARNING("NauShortcutHub: cannot register shortcut {} with an empty key sequence", operation);
        return m_shortcutByKeySequence.end();
    }

    auto shortcutIt = m_shortcutByKeySequence.find(keySequence);
    if (shortcutIt == m_shortcutByKeySequence.end()) {
        auto shortcut = new NauShortcut(keySequence, parent());
        shortcut->setContext(Qt::ApplicationShortcut);

        connect(shortcut, &NauShortcut::activated,
            std::bind(&NauShortcutHub::handleShortcutActivation, this, operation));

        connect(shortcut, &NauShortcut::activatedAmbiguously,
            std::bind(&NauShortcutHub::handleShortcutAmbiguousActivation, this, operation));

        shortcutIt = m_shortcutByKeySequence.insert({keySequence, ShortcutData{shortcut, {}, {} } }).first;
    }

    return shortcutIt;
}

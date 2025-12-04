// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Custom menu implementation.

#pragma once

#include "nau/nau_editor_config.hpp"

#include "nau_action.hpp"

#include <QMenu>
#include <QWidget>

class NauWidget;


// ** NauMenu

class NAU_EDITOR_API NauMenu : public QMenu
{
    Q_OBJECT
    template <typename...Args>
    using compatible_action_slot_args = std::enable_if_t<std::conjunction_v<
            std::disjunction<
                std::is_same<Args, Qt::ConnectionType>,
                std::negation<std::is_convertible<Args, QKeySequence>>
            >...,
            std::negation<std::is_convertible<Args, QIcon>>...,
            std::negation<std::is_convertible<Args, const char*>>...,
            std::negation<std::is_convertible<Args, QString>>...
    >>;

public:
    NauMenu(QWidget* parent = nullptr);
    NauMenu(const QString& title, NauWidget* parent = nullptr);

    void addAction(QAction* action);
    NauAction* addAction(const QString& text);
    NauAction* addAction(const NauIcon& icon, const QString& text);
    NauAction* addAction(const QString& text, const QObject* receiver,
        const char* member, Qt::ConnectionType type = Qt::AutoConnection);
    NauAction* addAction(const NauIcon& icon, const QString& text, const QObject* receiver,
        const char* member, Qt::ConnectionType type = Qt::AutoConnection);

    template <typename...Args, typename = compatible_action_slot_args<Args...>>
    NauAction* addAction(const QString& text, Args&&...args)
    {
        NauAction* result = addAction(text);
        QObject::connect(result, &QAction::triggered, std::forward<Args>(args)...);
        return result;
    }

    template <typename...Args, typename = compatible_action_slot_args<Args...>>
    NauAction* addAction(const NauIcon& icon, const QString& text, Args&&...args)
    {
        NauAction* result = addAction(icon, text);
        QObject::connect(result, &NauAction::triggered, std::forward<Args>(args)...);
        return result;
    }

    NauAction* addAction(const QString& text, const NauKeySequence& shortcut);
    NauAction* addAction(const NauIcon& icon, const QString& text, const NauKeySequence& shortcut);
    NauAction* addAction(const QString& text, const NauKeySequence& shortcut,
        const QObject* receiver, const char* member,
        Qt::ConnectionType type = Qt::AutoConnection);
    NauAction* addAction(const NauIcon& icon, const QString& text, const NauKeySequence& shortcut,
        const QObject* receiver, const char* member,
        Qt::ConnectionType type = Qt::AutoConnection);

    template <typename...Args, typename = compatible_action_slot_args<Args...>>
    NauAction* addAction(const QString& text, const NauKeySequence& shortcut, Args&&...args)
    {
        NauAction* result = addAction(text, shortcut);
        connect(result, &NauAction::triggered, std::forward<Args>(args)...);
        return result;
    }

    template <typename...Args, typename = compatible_action_slot_args<Args...>>
    NauAction* addAction(const NauIcon& icon, const QString& text,
        const NauKeySequence& shortcut, Args&&...args)
    {
        NauAction* result = addAction(icon, text, shortcut);
        connect(result, &NauAction::triggered, std::forward<Args>(args)...);

        return result;
    }

private:

    inline static constexpr int HorizontalMargin = 16;
    inline static constexpr int VerticalMargin = 8;
};

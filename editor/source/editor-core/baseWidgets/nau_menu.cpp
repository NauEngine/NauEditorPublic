// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_menu.hpp"
#include "nau_widget.hpp"


// ** NauMenu

NauMenu::NauMenu(QWidget* parent)
    : QMenu(parent)
{
    this->setStyleSheet("background-color: #343434");
    this->setContentsMargins(HorizontalMargin, VerticalMargin, HorizontalMargin, 0);
}

NauMenu::NauMenu(const QString& title, NauWidget* widget)
    : QMenu(title, widget)
{
    this->setStyleSheet("background-color: #343434");
    this->setContentsMargins(HorizontalMargin, VerticalMargin, HorizontalMargin, 0);
}

void NauMenu::addAction(QAction *action)
{
    return QMenu::addAction(action);
}

NauAction* NauMenu::addAction(const QString& text)
{
    auto result = new NauAction(text, this);
    QMenu::addAction(result);

    return result;
}

NauAction* NauMenu::addAction(const NauIcon& icon, const QString& text)
{
    auto result = new NauAction(icon, text, this);
    QMenu::addAction(result);
    return result;
}

NauAction* NauMenu::addAction(const QString& text, const QObject* receiver, 
    const char* member, Qt::ConnectionType type)
{
    auto result = addAction(text);
    QObject::connect(result, SIGNAL(triggered(bool)), receiver, member, type);

    return result;
}

NauAction* NauMenu::addAction(const NauIcon& icon, const QString& text,
    const QObject* receiver, const char* member, Qt::ConnectionType type)
{
    auto result = addAction(icon, text);
    QObject::connect(result, SIGNAL(triggered(bool)), receiver, member, type);

    return result;
}

NauAction* NauMenu::addAction(const QString& text, const NauKeySequence& shortcut)
{
    return addAction(NauIcon(), text, shortcut);
}

NauAction* NauMenu::addAction(const NauIcon& icon, const QString& text, const NauKeySequence& shortcut)
{
    auto result = new NauAction(icon, text, shortcut, this);
    QMenu::addAction(result);

    return result;
}

NauAction* NauMenu::addAction(const QString& text, const NauKeySequence& shortcut,
    const QObject* receiver, const char* member, Qt::ConnectionType type)
{
    auto result = addAction(text, shortcut);
    QObject::connect(result, SIGNAL(triggered(bool)), receiver, member, type);

    return result;
}

NauAction* NauMenu::addAction(const NauIcon& icon, const QString& text,
    const NauKeySequence& shortcut, const QObject* receiver, const char* member, Qt::ConnectionType type)
{
    auto result = addAction(icon, text, shortcut);
    QObject::connect(result, SIGNAL(triggered(bool)), receiver, member, type);

    return result;
}

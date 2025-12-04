// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_log_model.hpp"
#include "nau_plus_enum.hpp"
#include "themes/nau_theme.hpp"
#include "magic_enum/magic_enum.hpp"


// ** NauLogModel

NauLogModel::NauLogModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void NauLogModel::addEntry(int64_t time, NauEngineLogLevel level, 
    const std::string& logger, const std::string& message)
{
    if (m_maxEntries && m_entries.size() == m_maxEntries) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_entries.pop_front();
        endRemoveRows();
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());

    m_entries.push_back({ QDateTime::fromSecsSinceEpoch(time), level,
        QString::fromUtf8(logger.c_str()), QString::fromUtf8(message.c_str()), {}});

    endInsertRows();

#ifdef NDEBUG
    if (level == NauEngineLogLevel::Critical) {
#else
    if ((level == NauEngineLogLevel::Error) || (level == NauEngineLogLevel::Critical)) {
#endif
        emit eventCriticalEventOccurred();
    }
}

void NauLogModel::setLevelIcons(std::vector<NauIcon> levelIcons)
{
    m_icons = std::move(levelIcons);

    emit dataChanged(
        index(0, +Column::Level),
        index(static_cast<int>(m_entries.size()) - 1, +Column::Level),
        { Qt::DecorationRole });
}

void NauLogModel::setMaxEntries(std::optional<std::size_t> maxEntries)
{
    m_maxEntries = maxEntries;

    // In case the new maximum is below the current amount of items.
    if (m_maxEntries && m_entries.size() > m_maxEntries) {
        const std::size_t offset = m_entries.size() - m_maxEntries.value();

        beginRemoveRows(QModelIndex(), 0, static_cast<int>(offset) - 1);
        m_entries.erase(m_entries.begin(), m_entries.begin() + offset);
        endRemoveRows();
    }
}

std::optional<std::size_t> NauLogModel::getMaxEntries() const
{
    return m_maxEntries;
}

void NauLogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

int NauLogModel::rowCount(const QModelIndex& parent) const
{
    return static_cast<int>(m_entries.size());
}

int NauLogModel::columnCount(const QModelIndex& parent) const
{
    return magic_enum::enum_count<Column>();
}

QVariant NauLogModel::data(const QModelIndex& index, int role) const
{
    static const std::vector<QString> levelNameRepository = {
        tr("Debug"), tr("Info"), tr("Warning"),
        tr("Error"), tr("Critical"), tr("Trace")
    };

    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }
    const Entry& item = m_entries[index.row()];
    const auto levelIdx = +item.level;

    switch (role) {
        case Qt::DisplayRole: {
            switch (static_cast<Column>(index.column())) {
                case Column::Level: {
                    if (levelIdx >= 0 && levelIdx < levelNameRepository.size()) {
                        return levelNameRepository[levelIdx];
                    }

                    return QVariant();
                }
                case Column::Time: return item.time;
                case Column::Message: return item.message;
                case Column::Tags: return item.tags;
                default: break;
            }
            break;
        }

        case Qt::DecorationRole: {
            if (index.column() == 0) {
                if (levelIdx >= 0 && levelIdx < m_icons.size()) {
                    return m_icons[levelIdx];
                }
            }
            break;
        }
        case Qt::BackgroundRole: {
            return index.row() % 2 
                ? Nau::Theme::current().paletteLogger().brush(NauPalette::Role::Background)
                : Nau::Theme::current().paletteLogger().brush(NauPalette::Role::AlternateBackground);
        }
        case Qt::ForegroundRole: {
            if (index.column() == +Column::Level) {
                return Nau::Theme::current().paletteLogger().color(NauPalette::Role::ForegroundBrightText);
            }

            return Nau::Theme::current().paletteLogger().color(NauPalette::Role::Foreground);
        }
        case Qt::FontRole: {
            if (index.column() == +Column::Level) {
                return Nau::Theme::current().fontLoggerLevel();
            }

            return Nau::Theme::current().fontLogger();
        }
        case LevelIconRole: {
            if (levelIdx >= 0 && levelIdx < m_icons.size()) {
                return m_icons[levelIdx];
            }
            break;
        }
        case LevelDisplayTextRole: {
            if (levelIdx >= 0 && levelIdx < levelNameRepository.size()) {
                return levelNameRepository[levelIdx];
            }

            return QVariant();
        }
        case TimeRole: {
             return item.time;
        }
        case MessageRole: {
             return item.message;
        }
        case TagsRole: {
             return item.tags;
        }
        case SourceRole: {
             return item.logger;
        }
        case LevelDetailsRole: {
             return levelDetails(item.level);
        }
        case LevelRole: {
             return levelIdx;
        }
        case Qt::ToolTipRole: {
            return item.message;
        }
        default:
            break;
    }

    return QVariant();
}

QVariant NauLogModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    static const std::vector<QString> column_names = { tr("Level"), tr("Date"), tr("Message"), tr("Tags") };

    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        return column_names[section];
    }

    return QVariant();
}

QString NauLogModel::levelDetails(NauEngineLogLevel logLevel)
{
    static const std::vector<QString> columnNames = {
        tr("Message used to track debug messages "),
        tr("A simple message with meaningful information about the program's operation"),
        tr("Warnings about the risk of undefined behavior or any potentially unwanted events that do not result in errors"),
        tr("Unhandled errors leading to forced application termination in Release builds"),
        tr("Critical error leading to abnormal shutdown"),
        tr("Technical messages that do not require a reaction, but carry information about the work"),
    };

    if (+logLevel >= 0 && +logLevel < columnNames.size()) {
        return columnNames[+logLevel];
    }

    return QString();
}

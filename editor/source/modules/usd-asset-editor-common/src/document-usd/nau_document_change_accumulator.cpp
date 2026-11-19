// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_document_change_accumulator.hpp"

#include <algorithm>


// ** NauDocumentChangeAccumulator

void NauDocumentChangeAccumulator::nodeAdded(NauDocumentNodeId node, NauDocumentNodeId parent)
{
    const auto it = m_records.find(node);
    if (it == m_records.end()) {
        record(node, State::Added).parent = parent;
        return;
    }

    Record& existing = it->second;
    if (existing.state == State::Removed) {
        // Removed and brought back: it existed before the batch and still does
        const NauDocumentNodeId parentBefore = existing.parent;
        existing.state = State::Existing;
        existing.changed = true;
        if (parentBefore != parent) {
            existing.oldParent = parentBefore;
            existing.newParent = parent;
        }
        return;
    }

    existing.changed = existing.state == State::Existing;
}

void NauDocumentChangeAccumulator::nodeRemoved(NauDocumentNodeId node, NauDocumentNodeId parent)
{
    const auto it = m_records.find(node);
    if (it == m_records.end()) {
        record(node, State::Removed).parent = parent;
        return;
    }

    Record& existing = it->second;
    switch (existing.state) {
    case State::Added:
        // Never visible outside the batch
        m_records.erase(it);
        break;
    case State::Existing:
        existing.state = State::Removed;
        existing.parent = existing.oldParent.value_or(parent);
        existing.oldParent.reset();
        existing.changed = false;
        break;
    case State::Removed:
        break;
    }
}

void NauDocumentChangeAccumulator::nodeMoved(NauDocumentNodeId node, NauDocumentNodeId oldParent, NauDocumentNodeId newParent)
{
    const auto it = m_records.find(node);
    if (it == m_records.end()) {
        Record& moved = record(node, State::Existing);
        moved.oldParent = oldParent;
        moved.newParent = newParent;
        return;
    }

    Record& existing = it->second;
    switch (existing.state) {
    case State::Added:
        existing.parent = newParent;
        break;
    case State::Existing:
        if (!existing.oldParent) {
            existing.oldParent = oldParent;
        }
        existing.newParent = newParent;
        break;
    case State::Removed:
        break;
    }
}

void NauDocumentChangeAccumulator::nodeChanged(NauDocumentNodeId node)
{
    const auto it = m_records.find(node);
    if (it == m_records.end()) {
        record(node, State::Existing).changed = true;
        return;
    }

    if (it->second.state == State::Existing) {
        it->second.changed = true;
    }
}

void NauDocumentChangeAccumulator::propertyChanged(NauDocumentNodeId node, const std::string& key)
{
    const std::pair<NauDocumentNodeId, std::string> entry(node, key);
    if (std::find(m_properties.begin(), m_properties.end(), entry) == m_properties.end()) {
        m_properties.push_back(entry);
    }
}

bool NauDocumentChangeAccumulator::empty() const
{
    return m_records.empty() && m_properties.empty();
}

NauDocumentChanges NauDocumentChangeAccumulator::take()
{
    std::vector<std::pair<NauDocumentNodeId, const Record*>> ordered;
    ordered.reserve(m_records.size());
    for (const auto& [node, record] : m_records) {
        ordered.emplace_back(node, &record);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto& left, const auto& right) {
        return left.second->order < right.second->order;
    });

    NauDocumentChanges changes;
    for (const auto& [node, record] : ordered) {
        switch (record->state) {
        case State::Added:
            changes.addedNodes.push_back({ node, record->parent });
            break;
        case State::Removed:
            changes.removedNodes.push_back({ node, record->parent });
            break;
        case State::Existing:
            if (record->oldParent) {
                changes.movedNodes.push_back({ node, *record->oldParent, record->newParent });
            }
            if (record->changed) {
                changes.changedNodes.push_back(node);
            }
            break;
        }
    }

    // Added and removed nodes have no property entries
    for (const auto& [node, key] : m_properties) {
        const auto it = m_records.find(node);
        if (it == m_records.end() || it->second.state == State::Existing) {
            changes.changedProperties.push_back({ node, key });
        }
    }

    m_records.clear();
    m_properties.clear();
    m_nextOrder = 0;

    return changes;
}

NauDocumentChangeAccumulator::Record& NauDocumentChangeAccumulator::record(NauDocumentNodeId node, State state)
{
    Record& created = m_records[node];
    created.state = state;
    created.order = m_nextOrder++;
    return created;
}

// Copyright 2026 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
//
// Merges the changes of a batch into one NauDocumentChanges, following the rules documented there.
// Nothing in it is USD-specific.

#pragma once

#include "nau/document/nau_document_interface.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>


// ** NauDocumentChangeAccumulator

class NauDocumentChangeAccumulator
{
public:
    // Record changes in the order they were made: parents added before children, children removed before parents
    void nodeAdded(NauDocumentNodeId node, NauDocumentNodeId parent);
    void nodeRemoved(NauDocumentNodeId node, NauDocumentNodeId parent);
    void nodeMoved(NauDocumentNodeId node, NauDocumentNodeId oldParent, NauDocumentNodeId newParent);
    void nodeChanged(NauDocumentNodeId node);
    void propertyChanged(NauDocumentNodeId node, const std::string& key);

    bool empty() const;

    // The merged changes. Leaves the accumulator empty.
    NauDocumentChanges take();

private:
    enum class State
    {
        // New in this batch
        Added,

        // Existed before the batch and is gone
        Removed,

        // Existed before the batch and still exists
        Existing
    };

    struct Record
    {
        State state = State::Existing;
        size_t order = 0;

        // Added: the parent it has now. Removed: the parent it had.
        NauDocumentNodeId parent;

        // Existing: set if it moved
        std::optional<NauDocumentNodeId> oldParent;
        NauDocumentNodeId newParent;

        bool changed = false;
    };

    Record& record(NauDocumentNodeId node, State state);

private:
    std::unordered_map<NauDocumentNodeId, Record> m_records;
    std::vector<std::pair<NauDocumentNodeId, std::string>> m_properties;
    size_t m_nextOrder = 0;
};

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "react/detail/defs.h"

#include <algorithm>
#include <atomic>
#include <type_traits>
#include <utility>
#include <vector>
#include <map>
#include <mutex>
#include <cassert>

#include <tbb/concurrent_queue.h>
#include <tbb/task.h>

#include "react/detail/graph_interface.h"
#include "react/detail/graph_impl.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

NodeId ReactGraph::RegisterNode(IReactNode* nodePtr)
{
    return nodeData_.Insert(NodeData{ nodePtr });
}

void ReactGraph::UnregisterNode(NodeId nodeId)
{
    nodeData_.Erase(nodeId);
}

void ReactGraph::AttachNode(NodeId nodeId, NodeId parentId)
{
    auto& node = nodeData_[nodeId];
    auto& parent = nodeData_[parentId];

    parent.successors.push_back(nodeId);

    if (node.level <= parent.level)
        node.level = parent.level + 1;
}

void ReactGraph::DetachNode(NodeId nodeId, NodeId parentId)
{
    auto& parent = nodeData_[parentId];
    auto& successors = parent.successors;

    successors.erase(std::find(successors.begin(), successors.end(), nodeId));
}

void ReactGraph::AddSyncPointDependency(SyncPoint::Dependency dep)
{
    localDependencies_.push_back(std::move(dep));
}

void ReactGraph::Propagate()
{
    // Fill update queue with successors of changed inputs.
    for (NodeId nodeId : changedInputs_)
    {
        auto& node = nodeData_[nodeId];
        auto* nodePtr = node.nodePtr;

        UpdateResult res = nodePtr->Update();

        if (res == UpdateResult::changed)
        {
            changedNodes_.push_back(nodePtr);
            ScheduleSuccessors(node);
        }
    }

    // Propagate changes.
    while (scheduledNodes_.FetchNext())
    {
        for (NodeId nodeId : scheduledNodes_.Next())
        {
            auto& node = nodeData_[nodeId];
            auto* nodePtr = node.nodePtr;

            // A predecessor of this node has shifted to a lower level?
            if (node.level < node.newLevel)
            {
                // Re-schedule this node.
                node.level = node.newLevel;

                RecalculateSuccessorLevels(node);
                scheduledNodes_.Push(nodeId, node.level);
                continue;
            }

            UpdateResult res = nodePtr->Update();

            // Topology changed?
            if (res == UpdateResult::shifted)
            {
                // Re-schedule this node.
                RecalculateSuccessorLevels(node);
                scheduledNodes_.Push(nodeId, node.level);
                continue;
            }
            
            if (res == UpdateResult::changed)
            {
                changedNodes_.push_back(nodePtr);
                ScheduleSuccessors(node);
            }

            node.queued = false;
        }
    }

    // Cleanup buffers in changed nodes.
    for (IReactNode* nodePtr : changedNodes_)
        nodePtr->Clear();
    changedNodes_.clear();

    // Clean link state.
    localDependencies_.clear();
}

void ReactGraph::ScheduleSuccessors(NodeData& node)
{
    for (NodeId succId : node.successors)
    {
        auto& succ = nodeData_[succId];

        if (!succ.queued)
        {
            succ.queued = true;
            scheduledNodes_.Push(succId, succ.level);
        }
    }
}

void ReactGraph::RecalculateSuccessorLevels(NodeData& node)
{
    for (NodeId succId : node.successors)
    {
        auto& succ = nodeData_[succId];

        if (succ.newLevel <= node.level)
            succ.newLevel = node.level + 1;
    }
}

bool ReactGraph::TopoQueue::FetchNext()
{
    // Throw away previous values
    nextData_.clear();

    // Find min level of nodes in queue data
    minLevel_ = (std::numeric_limits<int>::max)();
    for (const auto& e : queueData_)
        if (minLevel_ > e.second)
            minLevel_ = e.second;

    // Swap entries with min level to the end
    auto p = std::partition(queueData_.begin(), queueData_.end(), [t = minLevel_] (const Entry& e) { return t != e.second; });

    // Move min level values to next data
    nextData_.reserve(std::distance(p, queueData_.end()));

    for (auto it = p; it != queueData_.end(); ++it)
        nextData_.push_back(it->first);

    // Truncate moved entries
    queueData_.resize(std::distance(queueData_.begin(), p));

    return !nextData_.empty();
}

void TransactionQueue::ProcessQueue()
{
    for (;;)
    {
        size_t popCount = ProcessNextBatch();
        if (count_.fetch_sub(popCount) == popCount)
            return;
    }
}

size_t TransactionQueue::ProcessNextBatch()
{
    StoredTransaction curTransaction;
    size_t popCount = 0;

    bool canMerge = false;

    bool skipPop = false;

    // Outer loop. One transaction per iteration.
    for (;;)
    {
        if (!skipPop)
        {
            if (!transactions_.try_pop(curTransaction))
                return popCount;

            canMerge = IsBitmaskSet(curTransaction.flags, TransactionFlags::allow_merging);

            ++popCount;
        }
        else
        {
            skipPop = false;
        }

        graph_.DoTransaction([&]
        {
            curTransaction.func();
            graph_.AddSyncPointDependency(std::move(curTransaction.dep));

            if (canMerge)
            {
                // Pull in additional mergeable transactions.
                for (;;)
                {
                    if (!transactions_.try_pop(curTransaction))
                        return;

                    canMerge = IsBitmaskSet(curTransaction.flags, TransactionFlags::allow_merging);

                    ++popCount;

                    if (!canMerge)
                    {
                        skipPop = true;
                        return;
                    }

                    curTransaction.func();
                    graph_.AddSyncPointDependency(std::move(curTransaction.dep));
                }
            }
        });
    }
}

/****************************************/ REACT_IMPL_END /***************************************/
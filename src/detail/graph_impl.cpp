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

node_id react_graph::register_node(IReactNode* nodePtr)
{
    return node_id{m_node_data.insert( node_data{ nodePtr })};
}

void react_graph::unregister_node(node_id nodeId)
{
    m_node_data.erase(nodeId);
}

void react_graph::attach_node(node_id nodeId, node_id parentId)
{
    auto& node = m_node_data[nodeId];
    auto& parent = m_node_data[parentId];

    parent.successors.push_back(nodeId);

    if (node.level <= parent.level)
        node.level = parent.level + 1;
}

void react_graph::detach_node(node_id node, node_id parentId)
{
    auto& parent = m_node_data[parentId];
    auto& successors = parent.successors;

    successors.erase(std::find(successors.begin(), successors.end(), node ));
}

void react_graph::add_sync_point_dependency(SyncPoint::Dependency dep)
{
    m_local_dependencies.push_back(std::move(dep));
}

void react_graph::propagate()
{
    std::vector<IReactNode*> changed_nodes;

    // Fill update queue with successors of changed inputs.
    for (node_id nodeId : m_changed_inputs )
    {
        auto& node = m_node_data[nodeId];
        auto* nodePtr = node.node_ptr;

        UpdateResult res = nodePtr->Update();

        if (res == UpdateResult::changed)
        {
            changed_nodes.push_back(nodePtr);
            schedule_successors( node );
        }
    }

    // Propagate changes.
    while ( m_scheduled_nodes.fetch_next())
    {
        for (node_id nodeId : m_scheduled_nodes.next_values())
        {
            auto& node = m_node_data[nodeId];
            auto* nodePtr = node.node_ptr;

            // A predecessor of this node has shifted to a lower level?
            if (node.level < node.new_level )
            {
                // Re-schedule this node.
                node.level = node.new_level;

                recalculate_successor_levels( node );
                m_scheduled_nodes.push( nodeId, node.level );
                continue;
            }

            UpdateResult res = nodePtr->Update();

            // Topology changed?
            if (res == UpdateResult::shifted)
            {
                // Re-schedule this node.
                recalculate_successor_levels( node );
                m_scheduled_nodes.push( nodeId, node.level );
                continue;
            }
            
            if (res == UpdateResult::changed)
            {
                changed_nodes.push_back(nodePtr);
                schedule_successors( node );
            }

            node.queued = false;
        }
    }

    // Cleanup buffers in changed nodes.
    for (IReactNode* nodePtr : changed_nodes )
        nodePtr->finalize();
    changed_nodes.clear();

    // Clean link state.
    m_local_dependencies.clear();
}

void react_graph::schedule_successors( node_data& node)
{
    for (node_id succId : node.successors)
    {
        auto& succ = m_node_data[succId];

        if (!succ.queued)
        {
            succ.queued = true;
            m_scheduled_nodes.push( succId, succ.level );
        }
    }
}

void react_graph::recalculate_successor_levels( node_data& node)
{
    for (node_id succId : node.successors)
    {
        auto& succ = m_node_data[succId];

        if (succ.new_level <= node.level)
            succ.new_level = node.level + 1;
    }
}

bool react_graph::topological_queue::fetch_next()
{
    // Throw away previous values
    m_next_data.clear();

    // Find min level of nodes in queue data
    auto minimal_level = (std::numeric_limits<int>::max)();
    for (const auto& e : m_queue_data )
        if (minimal_level > e.second)
            minimal_level = e.second;

    // Swap entries with min level to the end
    auto p = std::partition(m_queue_data.begin(),
        m_queue_data.end(), [t = minimal_level] (const Entry& e) { return t != e.second; });

    // Move min level values to next data
    m_next_data.reserve(std::distance(p, m_queue_data.end()));

    for (auto it = p; it != m_queue_data.end(); ++it)
        m_next_data.push_back(it->first);

    // Truncate moved entries
    m_queue_data.resize(std::distance(m_queue_data.begin(), p));

    return !m_next_data.empty();
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

        graph_.do_transaction( [&] {
            curTransaction.func();
            graph_.add_sync_point_dependency( std::move( curTransaction.dep ) );

            if( canMerge )
            {
                // Pull in additional mergeable transactions.
                for( ;; )
                {
                    if( !transactions_.try_pop( curTransaction ) )
                        return;

                    canMerge
                        = IsBitmaskSet( curTransaction.flags, TransactionFlags::allow_merging );

                    ++popCount;

                    if( !canMerge )
                    {
                        skipPop = true;
                        return;
                    }

                    curTransaction.func();
                    graph_.add_sync_point_dependency( std::move( curTransaction.dep ) );
                }
            }
        } );
    }
}

/****************************************/ REACT_IMPL_END /***************************************/
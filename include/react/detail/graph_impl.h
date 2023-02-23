
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_IMPL_H_INCLUDED
#define REACT_DETAIL_GRAPH_IMPL_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <algorithm>
#include <atomic>
#include <type_traits>
#include <utility>
#include <vector>
#include <map>
#include <mutex>

#include <tbb/concurrent_queue.h>
#include <tbb/task.h>

#include "react/common/slotmap.h"
#include "react/common/syncpoint.h"
#include "react/detail/graph_interface.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

class react_graph;

class TransactionQueue
{
public:
    TransactionQueue(react_graph& graph) :
        graph_( graph )
    { }

    template <typename F>
    void Push(F&& func, SyncPoint::Dependency dep, TransactionFlags flags)
    {
        transactions_.push(StoredTransaction{ std::forward<F>(func), std::move(dep), flags });

        if (count_.fetch_add(1, std::memory_order_release) == 0)
            tbb::task::enqueue(*new(tbb::task::allocate_root()) WorkerTask(*this));
    }

private:
    struct StoredTransaction
    {
        std::function<void()>   func;
        SyncPoint::Dependency   dep;
        TransactionFlags        flags;
    };

    class WorkerTask : public tbb::task
    {
    public:
        WorkerTask(TransactionQueue& parent) :
            parent_( parent )
        { }

        tbb::task* execute()
        {
            parent_.ProcessQueue();
            return nullptr;
        }

    private:
        TransactionQueue& parent_;
    };

    void ProcessQueue();

    size_t ProcessNextBatch();

    tbb::concurrent_queue<StoredTransaction> transactions_;

    std::atomic<size_t> count_{ 0 };

    react_graph& graph_;
};

class react_graph
{
public:
    node_id register_node(reactive_node_interface* nodePtr);
    void unregister_node(node_id nodeId);

    void attach_node(node_id nodeId, node_id parentId);
    void detach_node(node_id nodeId, node_id parentId);

    void push_input(node_id nodeId);

    void add_sync_point_dependency(SyncPoint::Dependency dep);

    template <typename F>
    void do_transaction(F&& transactionCallback);

    template <typename F>
    void enqueue_transaction(F&& func, SyncPoint::Dependency dep, TransactionFlags flags);

private:
    struct node_data
    {
        node_data(const node_data&) = delete;
        node_data& operator=(const node_data&) = delete;
        node_data(node_data&&) noexcept = default;
        node_data& operator=(node_data&&) noexcept = default;

        explicit node_data(reactive_node_interface* node_ptr ) : node_ptr( node_ptr )
        { }

        int level = 0;
        int new_level = 0 ;
        bool queued = false;

        reactive_node_interface* node_ptr = nullptr;

        std::vector<node_id> successors;
    };

    class topological_queue
    {
    public:
        void push(node_id nodeId, int level)
            { m_queue_data.emplace_back(nodeId, level); }

        bool fetch_next();

        [[nodiscard]] const std::vector<node_id>& next_values() const
            { return m_next_data; }

    private:
        using Entry = std::pair<node_id /*nodeId*/, int /*level*/>;

        std::vector<Entry> m_queue_data;
        std::vector<node_id> m_next_data;
    };

    void propagate();

    void schedule_successors( node_data& node);
    void recalculate_successor_levels( node_data& node);

private:
    TransactionQueue m_transaction_queue{ *this };

    slot_map<node_data> m_node_data;

    topological_queue m_scheduled_nodes;

    std::vector<node_id> m_changed_inputs;

    std::vector<SyncPoint::Dependency> m_local_dependencies;

    int m_transaction_level = 0;
};

inline void react_graph::push_input(node_id nodeId)
{
    m_changed_inputs.push_back(nodeId);

    if( m_transaction_level == 0 )
    {
        propagate();
    }
}

template <typename F>
void react_graph::do_transaction(F&& transactionCallback)
{
    // Transaction callback may add multiple inputs.
    ++m_transaction_level;
    std::forward<F>(transactionCallback)();
    --m_transaction_level;

    propagate();
}

template <typename F>
void react_graph::enqueue_transaction(F&& func, SyncPoint::Dependency dep, TransactionFlags flags)
{
    m_transaction_queue.Push(std::forward<F>(func), std::move(dep), flags);
}

inline node_id react_graph::register_node(reactive_node_interface* nodePtr)
{
    return node_id{m_node_data.insert( node_data{ nodePtr })};
}

inline void react_graph::unregister_node(node_id nodeId)
{
    m_node_data.erase(nodeId);
}

inline void react_graph::attach_node(node_id nodeId, node_id parentId)
{
    auto& node = m_node_data[nodeId];
    auto& parent = m_node_data[parentId];

    parent.successors.push_back(nodeId);

    if (node.level <= parent.level)
        node.level = parent.level + 1;
}

inline void react_graph::detach_node(node_id node, node_id parentId)
{
    auto& parent = m_node_data[parentId];
    auto& successors = parent.successors;

    successors.erase(std::find(successors.begin(), successors.end(), node ));
}

inline void react_graph::add_sync_point_dependency(SyncPoint::Dependency dep)
{
    m_local_dependencies.push_back(std::move(dep));
}

inline void react_graph::propagate()
{
    std::vector<reactive_node_interface*> changed_nodes;

    // Fill update queue with successors of changed inputs.
    for (node_id nodeId : m_changed_inputs )
    {
        auto& node = m_node_data[nodeId];
        auto* nodePtr = node.node_ptr;

        update_result res = nodePtr->update();

        if (res == update_result::changed)
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

            update_result res = nodePtr->update();

            // Topology changed?
            if (res == update_result::shifted)
            {
                // Re-schedule this node.
                recalculate_successor_levels( node );
                m_scheduled_nodes.push( nodeId, node.level );
                continue;
            }
            
            if (res == update_result::changed)
            {
                changed_nodes.push_back(nodePtr);
                schedule_successors( node );
            }

            node.queued = false;
        }
    }

    // Cleanup buffers in changed nodes.
    for (reactive_node_interface* nodePtr : changed_nodes )
        nodePtr->finalize();
    changed_nodes.clear();

    // Clean link state.
    m_local_dependencies.clear();
}

inline void react_graph::schedule_successors( node_data& node)
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

inline void react_graph::recalculate_successor_levels( node_data& node)
{
    for (node_id succId : node.successors)
    {
        auto& succ = m_node_data[succId];

        if (succ.new_level <= node.level)
            succ.new_level = node.level + 1;
    }
}

inline bool react_graph::topological_queue::fetch_next()
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

inline void TransactionQueue::ProcessQueue()
{
    for (;;)
    {
        size_t popCount = ProcessNextBatch();
        if (count_.fetch_sub(popCount) == popCount)
            return;
    }
}

inline size_t TransactionQueue::ProcessNextBatch()
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GroupInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
class context_internals
{
public:
    context_internals() : m_graph_ptr( std::make_shared<react_graph>() )
    {  }

    context_internals(const context_internals&) = default;
    context_internals& operator=(const context_internals&) = default;

    context_internals( context_internals&&) = default;
    context_internals& operator=( context_internals&&) = default;

    auto get_graph() -> react_graph&
        { return *m_graph_ptr; }

    auto get_graph() const -> const react_graph&
        { return *m_graph_ptr; }

private:
    std::shared_ptr<react_graph> m_graph_ptr;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_IMPL_H_INCLUDED
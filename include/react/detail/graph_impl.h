
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
    NodeId register_node(IReactNode* nodePtr);
    void unregister_node(NodeId nodeId);

    void attach_node(NodeId nodeId, NodeId parentId);
    void detach_node(NodeId nodeId, NodeId parentId);

    template <typename F>
    void push_input(NodeId nodeId, F&& inputCallback);

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

        explicit node_data(IReactNode* node_ptr ) : node_ptr( node_ptr )
        { }

        int level = 0;
        int new_level = 0 ;
        bool queued = false;

        IReactNode* node_ptr = nullptr;

        std::vector<NodeId> successors;
    };

    class topological_queue
    {
    public:
        void push(NodeId nodeId, int level)
            { m_queue_data.emplace_back(nodeId, level); }

        bool fetch_next();

        [[nodiscard]] const std::vector<NodeId>& next_values() const
            { return m_next_data; }

    private:
        using Entry = std::pair<NodeId /*nodeId*/, int /*level*/>;

        std::vector<Entry> m_queue_data;
        std::vector<NodeId> m_next_data;
    };

    void propagate();

    void schedule_successors( node_data& node);
    void recalculate_successor_levels( node_data& node);

private:
    TransactionQueue m_transaction_queue{ *this };

    SlotMap<node_data> m_node_data;

    topological_queue m_scheduled_nodes;

    std::vector<NodeId> m_changed_inputs;

    std::vector<SyncPoint::Dependency> m_local_dependencies;

    int m_transaction_level = 0;
};

template <typename F>
void react_graph::push_input(NodeId nodeId, F&& inputCallback)
{
    // This writes to the input buffer of the respective node.
    std::forward<F>(inputCallback)();

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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GroupInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
class GroupInternals
{
public:
    GroupInternals() :
        graphPtr_( std::make_shared<react_graph>() )
    {  }

    GroupInternals(const GroupInternals&) = default;
    GroupInternals& operator=(const GroupInternals&) = default;

    GroupInternals(GroupInternals&&) = default;
    GroupInternals& operator=(GroupInternals&&) = default;

    auto GetGraphPtr() -> std::shared_ptr<react_graph>&
        { return graphPtr_; }

    auto GetGraphPtr() const -> const std::shared_ptr<react_graph>&
        { return graphPtr_; }

private:
    std::shared_ptr<react_graph> graphPtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_IMPL_H_INCLUDED
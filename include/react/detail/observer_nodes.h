
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_OBSERVER_NODES_H_INCLUDED
#define REACT_DETAIL_OBSERVER_NODES_H_INCLUDED

#pragma once

#include "react/detail/defs.h"
#include "react/api.h"
#include "react/common/utility.h"

#include <memory>
#include <utility>
#include <tuple>

#include "node_base.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateNode;

template <typename E>
class EventStreamNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class ObserverNode : public node_base
{
public:
    explicit ObserverNode(const group& group) :
        ObserverNode::node_base( group )
    { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename ... TDeps>
class StateObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    StateObserverNode(const group& group, FIn&& func, const State<TDeps>& ... deps) :
        StateObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( deps ).get_node_id() ));

        std::apply([this] (const auto& ... deps)
            { this->func_(get_internals(deps).Value() ...); }, depHolder_);
    }

    ~StateObserverNode()
    {
        std::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( deps ).get_node_id() )); }, depHolder_);
    }

    virtual update_result update() noexcept override
    {
        std::apply([this] (const auto& ... deps)
            { this->func_(get_internals(deps).Value() ...); }, depHolder_);
        return update_result::unchanged;
    }

private:
    F func_;

    std::tuple<State<TDeps> ...> depHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E>
class EventObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    EventObserverNode(const group& group, FIn&& func, const Event<E>& subject) :
        EventObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject )
    {
        this->attach_to_me( get_internals( subject ).get_node_id() );
    }

    ~EventObserverNode()
    {
        this->detach_from_me( get_internals( subject_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        func_(get_internals(subject_).Events());
        return update_result::unchanged;
    }

private:
    F func_;

    Event<E> subject_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E, typename ... TSyncs>
class SyncedEventObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    SyncedEventObserverNode(const group& group, FIn&& func, const Event<E>& subject, const State<TSyncs>& ... syncs) :
        SyncedEventObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject ),
        syncHolder_( syncs ... )
    {
        this->attach_to_me( get_internals( subject ).get_node_id() );
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( syncs ).get_node_id() ));
    }

    ~SyncedEventObserverNode()
    {
        std::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( syncs ).get_node_id() )); }, syncHolder_);
        this->detach_from_me( get_internals( subject_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (get_internals(this->subject_).Events().empty())
            return update_result::unchanged;

        std::apply([this] (const auto& ... syncs)
            { func_(get_internals(this->subject_).Events(), get_internals(syncs).Value() ...); }, syncHolder_);

        return update_result::unchanged;
    }

private:
    F func_;

    Event<E> subject_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
class ObserverInternals
{
public:
    ObserverInternals(const ObserverInternals&) = default;
    ObserverInternals& operator=(const ObserverInternals&) = default;

    ObserverInternals(ObserverInternals&&) = default;
    ObserverInternals& operator=(ObserverInternals&&) = default;

    explicit ObserverInternals(std::shared_ptr<ObserverNode>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto get_node_ptr() -> std::shared_ptr<ObserverNode>&
        { return nodePtr_; }

    auto get_node_ptr() const -> const std::shared_ptr<ObserverNode>&
        { return nodePtr_; }

    node_id get_node_id() const
        { return nodePtr_->get_node_id(); }

protected:
    ObserverInternals() = default;

private:
    std::shared_ptr<ObserverNode> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_OBSERVER_NODES_H_INCLUDED
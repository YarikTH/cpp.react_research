
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
class state_node;

template <typename E>
class EventStreamNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class observer_node : public node_base
{
public:
    explicit observer_node(const group& group)
        : observer_node::node_base( group )
    { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename ... TDeps>
class state_observer_node : public observer_node
{
public:
    template <typename FIn>
    state_observer_node(const group& group, FIn&& func, const State<TDeps>& ... deps)
        : state_observer_node::observer_node( group ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( deps ).get_node_id() ));

        std::apply([this] (const auto& ... deps)
            { this->func_( get_internals( deps ).value() ...); }, depHolder_);
    }

    ~state_observer_node() override
    {
        std::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( deps ).get_node_id() )); }, depHolder_);
    }

    update_result update() noexcept override
    {
        std::apply([this] (const auto& ... deps)
            { this->func_( get_internals( deps ).value() ...); }, depHolder_);
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
class event_observer_node : public observer_node
{
public:
    template <typename FIn>
    event_observer_node(const group& group, FIn&& func, const Event<E>& subject)
        : event_observer_node::observer_node( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject )
    {
        this->attach_to_me( get_internals( subject ).get_node_id() );
    }

    ~event_observer_node() override
    {
        this->detach_from_me( get_internals( subject_ ).get_node_id() );
    }

    update_result update() noexcept override
    {
        func_(get_internals( subject_ ).events());
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
class synced_event_observer_node : public observer_node
{
public:
    template <typename FIn>
    synced_event_observer_node(const group& group, FIn&& func, const Event<E>& subject, const State<TSyncs>& ... syncs)
        : synced_event_observer_node::observer_node( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject ),
        syncHolder_( syncs ... )
    {
        this->attach_to_me( get_internals( subject ).get_node_id() );
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( syncs ).get_node_id() ));
    }

    ~synced_event_observer_node() override
    {
        std::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( syncs ).get_node_id() )); }, syncHolder_);
        this->detach_from_me( get_internals( subject_ ).get_node_id() );
    }

    update_result update() noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if ( get_internals( this->subject_ ).events().empty())
            return update_result::unchanged;

        std::apply([this] (const auto& ... syncs)
            { func_( get_internals( this->subject_ ).events(), get_internals( syncs ).value() ...); }, syncHolder_);

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
class observer_internals
{
public:
    observer_internals(const observer_internals&) = default;
    observer_internals& operator=(const observer_internals&) = default;

    observer_internals( observer_internals&&) = default;
    observer_internals& operator=( observer_internals&&) = default;

    explicit observer_internals(std::shared_ptr<observer_node>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto get_node_ptr() -> std::shared_ptr<observer_node>&
        { return nodePtr_; }

    auto get_node_ptr() const -> const std::shared_ptr<observer_node>&
        { return nodePtr_; }

    node_id get_node_id() const
        { return nodePtr_->get_node_id(); }

protected:
    observer_internals() = default;

private:
    std::shared_ptr<observer_node> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_OBSERVER_NODES_H_INCLUDED

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_EVENT_NODES_H_INCLUDED
#define REACT_DETAIL_EVENT_NODES_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <atomic>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

//#include "tbb/spin_mutex.h"

#include "node_base.h"
#include "react/common/utility.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterators for event processing
///////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class state_node;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class event_node : public node_base
{
public:
    explicit event_node(const context& group)
        : event_node::node_base( group )
    { }

    EventValueList<E>& events()
        { return events_; }

    const EventValueList<E>& events() const
        { return events_; }

    void finalize() noexcept override
        { events_.clear(); }

private:
    EventValueList<E> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class event_source_node : public event_node<E>
{
public:
    explicit event_source_node(const context& group)
        : event_source_node::event_node( group )
    {
    }

    update_result update() noexcept override
    {
        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

    template <typename U>
    void EmitValue(U&& value)
        { this->events().push_back(std::forward<U>(value)); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventMergeNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename ... TInputs>
class EventMergeNode : public event_node<E>
{
public:
    EventMergeNode(const context& group, const Event<TInputs>& ... deps) :
        EventMergeNode::event_node( group ),
        inputs_( deps ... )
    {
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( deps ).get_node_id() ));
    }

    ~EventMergeNode()
    {
        std::apply([this] (const auto& ... inputs)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( inputs ).get_node_id() )); }, inputs_);
    }

    virtual update_result update() noexcept override
    {
        std::apply([this] (auto& ... inputs)
            { REACT_EXPAND_PACK(this->MergeFromInput(inputs)); }, inputs_);

        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    template <typename U>
    void MergeFromInput(Event<U>& dep)
    {
        auto& depInternals = get_internals(dep);
        this->events().insert(
            this->events().end(), depInternals.events().begin(), depInternals.events().end());
    }

    std::tuple<Event<TInputs> ...> inputs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSlotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSlotNode : public event_node<E>
{
public:
    EventSlotNode(const context& group) :
        EventSlotNode::event_node( group )
    {
        inputNodeId_ = this->get_graph().register_node(&slotInput_);

        this->attach_to_me( inputNodeId_ );
    }

    ~EventSlotNode()
    {
        RemoveAllSlotInputs();
        this->detach_from_me( inputNodeId_ );

        this->get_graph().unregister_node(inputNodeId_);
    }

    virtual update_result update() noexcept override
    {
        for (auto& e : inputs_)
        {
            const auto& events = get_internals( e ).events();
            this->events().insert( this->events().end(), events.begin(), events.end());
        }

        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

    void AddSlotInput(const Event<E>& input)
    {
        auto it = std::find(inputs_.begin(), inputs_.end(), input);
        if (it == inputs_.end())
        {
            inputs_.push_back(input);
            this->attach_to_me( get_internals( input ).get_node_id() );
        }
    }

    void RemoveSlotInput(const Event<E>& input)
    {
        auto it = std::find(inputs_.begin(), inputs_.end(), input);
        if (it != inputs_.end())
        {
            inputs_.erase(it);
            this->detach_from_me( get_internals( input ).get_node_id() );
        }
    }

    void RemoveAllSlotInputs()
    {
        for (const auto& e : inputs_)
            this->detach_from_me( get_internals( e ).get_node_id() );

        inputs_.clear();
    }

    node_id GetInputNodeId() const
        { return inputNodeId_; }

private:        
    struct VirtualInputNode : public reactive_node_interface
    {
        virtual update_result update() noexcept override
            { return update_result::changed; }
    };

    std::vector<Event<E>> inputs_;

    node_id              inputNodeId_;
    VirtualInputNode    slotInput_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename TIn, typename F>
class EventProcessingNode : public event_node<TOut>
{
public:
    template <typename FIn>
    EventProcessingNode(const context& group, FIn&& func, const Event<TIn>& dep) :
        EventProcessingNode::event_node( group ),
        func_( std::forward<FIn>(func) ),
        dep_( dep )
    {
            this->attach_to_me( get_internals( dep ).get_node_id() );
    }

    ~EventProcessingNode()
    {
            this->detach_from_me( get_internals( dep_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        func_( get_internals( dep_ ).events(), std::back_inserter( this->events()));

        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    F func_;

    Event<TIn> dep_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename TIn, typename F, typename ... TSyncs>
class SyncedEventProcessingNode : public event_node<TOut>
{
public:
    template <typename FIn>
    SyncedEventProcessingNode(const context& group, FIn&& func, const Event<TIn>& dep, const State<TSyncs>& ... syncs) :
        SyncedEventProcessingNode::event_node( group ),
        func_( std::forward<FIn>(func) ),
        dep_( dep ),
        syncHolder_( syncs ... )
    {
        this->attach_to_me( get_internals( dep ).get_node_id() );
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( syncs ).get_node_id() ));
    }

    ~SyncedEventProcessingNode()
    {
        std::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( syncs ).get_node_id() )); }, syncHolder_);
        this->detach_from_me( get_internals( dep_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if ( get_internals( dep_ ).events().empty())
            return update_result::unchanged;

        std::apply([this] (const auto& ... syncs)
            {
                func_( get_internals( dep_ ).events(), std::back_inserter( this->events()),
                    get_internals( syncs ).value() ...);
            },
            syncHolder_);

        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    F func_;

    Event<TIn> dep_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventJoinNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ... Ts>
class EventJoinNode : public event_node<std::tuple<Ts ...>>
{
public:
    EventJoinNode(const context& group, const Event<Ts>& ... deps) :
        EventJoinNode::event_node( group ),
        slots_( deps ... )
    {
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( deps ).get_node_id() ));
    }

    ~EventJoinNode()
    {
        std::apply([this] (const auto& ... slots)
            { REACT_EXPAND_PACK(
                    this->detach_from_me( get_internals( slots.source ).get_node_id() )); }, slots_);
    }

    virtual update_result update() noexcept override
    {
        // Move events into buffers.
        std::apply([this] (Slot<Ts>& ... slots)
            { REACT_EXPAND_PACK(this->FetchBuffer(slots)); }, slots_);

        while (true)
        {
            bool isReady = true;

            // All slots ready?
            std::apply([&isReady] (Slot<Ts>& ... slots)
                {
                    // Todo: combine return values instead
                    REACT_EXPAND_PACK(CheckSlot(slots, isReady));
                },
                slots_);

            if (!isReady)
                break;

            // Pop values from buffers and emit tuple.
            std::apply([this] (Slot<Ts>& ... slots)
                {
                    this->events().emplace_back(slots.buffer.front() ...);
                    REACT_EXPAND_PACK(slots.buffer.pop_front());
                },
                slots_);
        }

        if (!this->events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    template <typename U>
    struct Slot
    {
        Slot(const Event<U>& src) :
            source( src )
        { }

        Event<U>        source;
        std::deque<U>   buffer;
    };

    template <typename U>
    static void FetchBuffer(Slot<U>& slot)
    {
        slot.buffer.insert(slot.buffer.end(),
            get_internals( slot.source ).events().begin(),
            get_internals( slot.source ).events().end());
    }

    template <typename T>
    static void CheckSlot(Slot<T>& slot, bool& isReady)
    {
        bool t = isReady && !slot.buffer.empty();
        isReady = t;
    }

    std::tuple<Slot<Ts>...> slots_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class event_internals
{
public:
    event_internals() = default;

    event_internals(const event_internals&) = default;
    event_internals& operator=(const event_internals&) = default;

    event_internals( event_internals&&) = default;
    event_internals& operator=( event_internals&&) = default;

    explicit event_internals(std::shared_ptr<event_node<E>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto get_node_ptr() -> std::shared_ptr<event_node<E>>&
        { return nodePtr_; }

    auto get_node_ptr() const -> const std::shared_ptr<event_node<E>>&
        { return nodePtr_; }

    node_id get_node_id() const
        { return nodePtr_->get_node_id(); }

    EventValueList<E>& events()
        { return nodePtr_->events(); }

    const EventValueList<E>& events() const
        { return nodePtr_->events(); }

private:
    std::shared_ptr<event_node<E>> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_EVENT_NODES_H_INCLUDED
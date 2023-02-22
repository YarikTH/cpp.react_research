
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
class StateNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventNode : public node_base
{
public:
    explicit EventNode(const group& group) :
        EventNode::node_base( group )
    { }

    EventValueList<E>& Events()
        { return events_; }

    const EventValueList<E>& Events() const
        { return events_; }

    virtual void finalize() noexcept override
        { events_.clear(); }

private:
    EventValueList<E> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSourceNode : public EventNode<E>
{
public:
    EventSourceNode(const group& group) :
        EventSourceNode::EventNode( group )
    {
    }

    virtual update_result update() noexcept override
    {
        if (! this->Events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

    template <typename U>
    void EmitValue(U&& value)
        { this->Events().push_back(std::forward<U>(value)); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventMergeNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename ... TInputs>
class EventMergeNode : public EventNode<E>
{
public:
    EventMergeNode(const group& group, const Event<TInputs>& ... deps) :
        EventMergeNode::EventNode( group ),
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

        if (! this->Events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    template <typename U>
    void MergeFromInput(Event<U>& dep)
    {
        auto& depInternals = get_internals(dep);
        this->Events().insert(this->Events().end(), depInternals.Events().begin(), depInternals.Events().end());
    }

    std::tuple<Event<TInputs> ...> inputs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSlotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSlotNode : public EventNode<E>
{
public:
    EventSlotNode(const group& group) :
        EventSlotNode::EventNode( group )
    {
        inputNodeId_ = this->get_graph_ptr()->register_node(&slotInput_);

        this->attach_to_me( inputNodeId_ );
    }

    ~EventSlotNode()
    {
        RemoveAllSlotInputs();
        this->detach_from_me( inputNodeId_ );

        this->get_graph_ptr()->unregister_node(inputNodeId_);
    }

    virtual update_result update() noexcept override
    {
        for (auto& e : inputs_)
        {
            const auto& events = get_internals(e).Events();
            this->Events().insert(this->Events().end(), events.begin(), events.end());
        }

        if (! this->Events().empty())
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
class EventProcessingNode : public EventNode<TOut>
{
public:
    template <typename FIn>
    EventProcessingNode(const group& group, FIn&& func, const Event<TIn>& dep) :
        EventProcessingNode::EventNode( group ),
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
        func_(get_internals(dep_).Events(), std::back_inserter(this->Events()));

        if (! this->Events().empty())
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
class SyncedEventProcessingNode : public EventNode<TOut>
{
public:
    template <typename FIn>
    SyncedEventProcessingNode(const group& group, FIn&& func, const Event<TIn>& dep, const State<TSyncs>& ... syncs) :
        SyncedEventProcessingNode::EventNode( group ),
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
        if (get_internals(dep_).Events().empty())
            return update_result::unchanged;

        std::apply([this] (const auto& ... syncs)
            {
                func_(get_internals(dep_).Events(), std::back_inserter(this->Events()), get_internals(syncs).Value() ...);
            },
            syncHolder_);

        if (! this->Events().empty())
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
class EventJoinNode : public EventNode<std::tuple<Ts ...>>
{
public:
    EventJoinNode(const group& group, const Event<Ts>& ... deps) :
        EventJoinNode::EventNode( group ),
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
                    this->Events().emplace_back(slots.buffer.front() ...);
                    REACT_EXPAND_PACK(slots.buffer.pop_front());
                },
                slots_);
        }

        if (! this->Events().empty())
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
        slot.buffer.insert(slot.buffer.end(), get_internals(slot.source).Events().begin(), get_internals(slot.source).Events().end());
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
class EventInternals
{
public:
    EventInternals() = default;

    EventInternals(const EventInternals&) = default;
    EventInternals& operator=(const EventInternals&) = default;

    EventInternals(EventInternals&&) = default;
    EventInternals& operator=(EventInternals&&) = default;

    explicit EventInternals(std::shared_ptr<EventNode<E>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto get_node_ptr() -> std::shared_ptr<EventNode<E>>&
        { return nodePtr_; }

    auto get_node_ptr() const -> const std::shared_ptr<EventNode<E>>&
        { return nodePtr_; }

    node_id get_node_id() const
        { return nodePtr_->get_node_id(); }

    EventValueList<E>& Events()
        { return nodePtr_->Events(); }

    const EventValueList<E>& Events() const
        { return nodePtr_->Events(); }

private:
    std::shared_ptr<EventNode<E>> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_EVENT_NODES_H_INCLUDED

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_EVENT_H_INCLUDED
#define REACT_EVENT_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "react/api.h"
#include "react/group.h"

#include "react/detail/state_nodes.h"
#include "react/detail/event_nodes.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Event
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class Event : protected REACT_IMPL::event_internals<E>
{
public:
    // Construct with explicit group
    template <typename F, typename T>
    static Event Create(const group& group, F&& func, const Event<T>& dep)
        { return Event(CreateProcessingNode(group, std::forward<F>(func), dep)); }

    // Construct with implicit group
    template <typename F, typename T>
    static Event Create(F&& func, const Event<T>& dep)
        { return Event(CreateProcessingNode(dep.get_group(), std::forward<F>(func), dep)); }

    // Construct with explicit group
    template <typename F, typename T, typename ... Us>
    static Event Create(const group& group, F&& func, const Event<T>& dep, const State<Us>& ... states)
        { return Event(CreateSyncedProcessingNode(group, std::forward<F>(func), dep, states ...)); }

    // Construct with implicit group
    template <typename F, typename T, typename ... Us>
    static Event Create(F&& func, const Event<T>& dep, const State<Us>& ... states)
        { return Event(CreateSyncedProcessingNode(dep.get_group(), std::forward<F>(func), dep, states ...)); }

    Event() = default;

    Event(const Event&) = default;
    Event& operator=(const Event&) = default;

    Event(Event&&) = default;
    Event& operator=(Event&&) = default;

    auto get_group() const -> const group&
        { return this->get_node_ptr()->get_group(); }

    auto get_group() -> group&
        { return this->get_node_ptr()->get_group(); }

    friend bool operator==(const Event<E>& a, const Event<E>& b)
        { return a.get_node_ptr() == b.get_node_ptr(); }

    friend bool operator!=(const Event<E>& a, const Event<E>& b)
        { return !(a == b); }

    friend auto get_internals(Event<E>& e) -> REACT_IMPL::event_internals<E>&
        { return e; }

    friend auto get_internals(const Event<E>& e) -> const REACT_IMPL::event_internals<E>&
        { return e; }

protected:
    Event(std::shared_ptr<REACT_IMPL::event_node<E>>&& nodePtr) :
        Event::event_internals( std::move(nodePtr) )
    { }

    template <typename F, typename T>
    static auto CreateProcessingNode(const group& group, F&& func, const Event<T>& dep) -> decltype(auto)
    {
        using REACT_IMPL::EventProcessingNode;

        return std::make_shared<EventProcessingNode<E, T, typename std::decay<F>::type>>(
            group, std::forward<F>(func), dep);
    }

    template <typename F, typename T, typename ... Us>
    static auto CreateSyncedProcessingNode(const group& group, F&& func, const Event<T>& dep, const State<Us>& ... syncs) -> decltype(auto)
    {
        using REACT_IMPL::SyncedEventProcessingNode;

        return std::make_shared<SyncedEventProcessingNode<E, T, typename std::decay<F>::type, Us ...>>(
            group, std::forward<F>(func), dep, syncs ...);
    }

    template <typename RET, typename NODE, typename ... ARGS>
    friend RET impl::create_wrapped_node(ARGS&& ... args);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSource : public Event<E>
{
public:
    // Construct event source
    static EventSource Create(const group& group)
        { return EventSource(CreateSourceNode(group)); }

    EventSource() = default;

    EventSource(const EventSource&) = default;
    EventSource& operator=(const EventSource&) = default;

    EventSource(EventSource&& other) = default;
    EventSource& operator=(EventSource&& other) = default;
    
    void Emit(const E& value)
        { EmitValue(value); }

    void Emit(E&& value)
        { EmitValue(std::move(value)); }

    template <typename T = E, typename = std::enable_if_t<std::is_same_v<T, Token>>>
    void Emit()
        { EmitValue(Token::value); }

    EventSource& operator<<(const E& value)
        { EmitValue(value); return *this; }

    EventSource& operator<<(E&& value)
        { EmitValue(std::move(value)); return *this; }

protected:
    EventSource(std::shared_ptr<REACT_IMPL::event_node<E>>&& nodePtr) :
        EventSource::Event( std::move(nodePtr) )
    { }

private:
    static auto CreateSourceNode(const group& group) -> decltype(auto)
    {
        using REACT_IMPL::event_source_node;
        return std::make_shared<event_source_node<E>>(group);
    }

    template <typename T>
    void EmitValue(T&& value)
    {
        using REACT_IMPL::node_id;
        using REACT_IMPL::event_source_node;

        auto* castedPtr = static_cast<event_source_node<E>*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->get_node_id();
        auto& graphPtr = get_internals( this->get_group() ).get_graph_ptr();

        castedPtr->EmitValue(std::forward<T>(value));
        graphPtr->push_input(nodeId);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSlotBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSlot : public Event<E>
{
public:
    // Construct emtpy slot
    static EventSlot Create(const group& group)
        { return EventSlot(CreateSlotNode(group)); }

    EventSlot() = default;

    EventSlot(const EventSlot&) = default;
    EventSlot& operator=(const EventSlot&) = default;

    EventSlot(EventSlot&&) = default;
    EventSlot& operator=(EventSlot&&) = default;

    void Add(const Event<E>& input)
        { AddSlotInput(input); }

    void Remove(const Event<E>& input)
        { RemoveSlotInput(input); }

    void RemoveAll()
        { RemoveAllSlotInputs(); }

protected:
    EventSlot(std::shared_ptr<REACT_IMPL::event_node<E>>&& nodePtr) :
        EventSlot::Event( std::move(nodePtr) )
    { }

private:
    static auto CreateSlotNode(const group& group) -> decltype(auto)
    {
        using REACT_IMPL::EventSlotNode;
        return std::make_shared<EventSlotNode<E>>(group);
    }

    void AddSlotInput(const Event<E>& input)
    {
        using REACT_IMPL::node_id;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = get_internals( this->get_group() ).get_graph_ptr();

        castedPtr->AddSlotInput(input);
        graphPtr->push_input(nodeId);
    }

    void RemoveSlotInput(const Event<E>& input)
    {
        using REACT_IMPL::node_id;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = get_internals( this->get_group() ).get_graph_ptr();

        castedPtr->RemoveSlotInput(input);
        graphPtr->push_input(nodeId);
    }

    void RemoveAllSlotInputs()
    {
        using REACT_IMPL::node_id;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = get_internals( this->get_group() ).get_graph_ptr();

        castedPtr->RemoveAllSlotInputs();
        graphPtr->push_input(nodeId);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename ... Us>
static auto Merge(const group& group, const Event<E>& dep1, const Event<Us>& ... deps) -> Event<E>
{
    using REACT_IMPL::EventMergeNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<Event<E>, EventMergeNode<E, E, Us...>>( group, dep1, deps... );
}

template <typename T = void, typename U1, typename ... Us>
static auto Merge(const Event<U1>& dep1, const Event<Us>& ... deps) -> decltype(auto)
    { return Merge(dep1.get_group(), dep1, deps ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E>
static auto Filter(const group& group, F&& pred, const Event<E>& dep) -> Event<E>
{
    auto filterFunc = [capturedPred = std::forward<F>(pred)] (const EventValueList<E>& events, EventValueSink<E> out)
        { std::copy_if(events.begin(), events.end(), out, capturedPred); };

    return Event<E>::Create(group, std::move(filterFunc), dep);
}

template <typename F, typename E>
static auto Filter(F&& pred, const Event<E>& dep) -> Event<E>
    { return Filter(dep.get_group(), std::forward<F>(pred), dep); }

template <typename F, typename E, typename ... Ts>
static auto Filter(const group& group, F&& pred, const Event<E>& dep, const State<Ts>& ... states) -> Event<E>
{
    auto filterFunc = [capturedPred = std::forward<F>(pred)] (const EventValueList<E>& evts, EventValueSink<E> out, const Ts& ... values)
        {
            for (const auto& v : evts)
                if (capturedPred(v, values ...))
                    *out++ = v;
        };

    return Event<E>::Create(group, std::move(filterFunc), dep, states ...);
}

template <typename F, typename E, typename ... Ts>
static auto Filter(F&& pred, const Event<E>& dep, const State<Ts>& ... states) -> Event<E>
    { return Filter(dep.get_group(), std::forward<F>(pred), dep, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename F, typename T>
static auto Transform(const group& group, F&& op, const Event<T>& dep) -> Event<E>
{
    auto transformFunc = [capturedOp = std::forward<F>(op)] (const EventValueList<T>& evts, EventValueSink<E> out)
        { std::transform(evts.begin(), evts.end(), out, capturedOp); };

    return Event<E>::Create(group, std::move(transformFunc), dep);
}

template <typename E, typename F, typename T>
static auto Transform(F&& op, const Event<T>& dep) -> Event<E>
    { return Transform<E>(dep.get_group(), std::forward<F>(op), dep); }

template <typename E, typename F, typename T, typename ... Us>
static auto Transform(const group& group, F&& op, const Event<T>& dep, const State<Us>& ... states) -> Event<E>
{
    auto transformFunc = [capturedOp = std::forward<F>(op)] (const EventValueList<T>& evts, EventValueSink<E> out, const Us& ... values)
        {
            for (const auto& v : evts)
                *out++ = capturedOp(v, values ...);
        };

    return Event<E>::Create(group, std::move(transformFunc), dep, states ...);
}

template <typename E, typename F, typename T, typename ... Us>
static auto Transform(F&& op, const Event<T>& dep, const State<Us>& ... states) -> Event<E>
    { return Transform<E>(dep.get_group(), std::forward<F>(op), dep, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename U1, typename ... Us>
static auto Join(const group& group, const Event<U1>& dep1, const Event<Us>& ... deps) -> Event<std::tuple<U1, Us ...>>
{
    using REACT_IMPL::EventJoinNode;
    using REACT_IMPL::create_wrapped_node;

    static_assert(sizeof...(Us) > 0, "Join requires at least 2 inputs.");

    return create_wrapped_node<Event<std::tuple<U1, Us...>>, EventJoinNode<U1, Us...>>(
        group, dep1, deps... );
}

template <typename U1, typename ... Us>
static auto Join(const Event<U1>& dep1, const Event<Us>& ... deps) -> Event<std::tuple<U1, Us ...>>
    { return Join(dep1.get_group(), dep1, deps ...); }

/******************************************/ REACT_END /******************************************/

#endif // REACT_EVENT_H_INCLUDED

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_ALGORITHM_H_INCLUDED
#define REACT_ALGORITHM_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <memory>
#include <type_traits>
#include <utility>

#include "react/api.h"
#include "react/state.h"
#include "react/event.h"

#include "react/detail/algorithm_nodes.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Holds the most recent event in a state
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename E>
auto Hold(const group& group, T&& initialValue, const Event<E>& evnt) -> State<E>
{
    using REACT_IMPL::HoldNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<E>, HoldNode<E>>(
        group, std::forward<T>( initialValue ), evnt );
}

template <typename T, typename E>
auto Hold(T&& initialValue, const Event<E>& evnt) -> State<E>
    { return Hold(evnt.get_group(), std::forward<T>(initialValue), evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Emits value changes of target state.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
auto Monitor(const group& group, const State<S>& state) -> Event<S>
{
    using REACT_IMPL::MonitorNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<Event<S>, MonitorNode<S>>( group, state );
}

template <typename S>
auto Monitor(const State<S>& state) -> Event<S>
    { return Monitor(state.get_group(), state); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iteratively combines state value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename T, typename F, typename E>
auto Iterate(const group& group, T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::IterateNode;
    using REACT_IMPL::create_wrapped_node;

    using FuncType = typename std::decay<F>::type;

    return create_wrapped_node<State<S>, IterateNode<S, FuncType, E>>(
        group, std::forward<T>( initialValue ), std::forward<F>( func ), evnt );
}

template <typename S, typename T, typename F, typename E>
auto IterateByRef(const group& group, T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::IterateByRefNode;
    using REACT_IMPL::create_wrapped_node;

    using FuncType = typename std::decay<F>::type;

    return create_wrapped_node<State<S>, IterateByRefNode<S, FuncType, E>>(
        group, std::forward<T>( initialValue ), std::forward<F>( func ), evnt );
}

template <typename S, typename T, typename F, typename E>
auto Iterate(T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
    { return Iterate<S>(evnt.get_group(), std::forward<T>(initialValue), std::forward<F>(func), evnt); }

template <typename S, typename T, typename F, typename E>
auto IterateByRef(T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
    { return IterateByRef<S>(evnt.get_group(), std::forward<T>(initialValue), std::forward<F>(func), evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename T, typename F, typename E, typename ... Us>
auto Iterate(const group& group, T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
{
    using REACT_IMPL::SyncedIterateNode;
    using REACT_IMPL::create_wrapped_node;

    using FuncType = typename std::decay<F>::type;

    return create_wrapped_node<State<S>, SyncedIterateNode<S, FuncType, E, Us...>>(
        group, std::forward<T>( initialValue ), std::forward<F>( func ), evnt, states... );
}

template <typename S, typename T, typename F, typename E, typename ... Us>
auto IterateByRef(const group& group, T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
{
    using REACT_IMPL::SyncedIterateByRefNode;
    using REACT_IMPL::create_wrapped_node;

    using FuncType = typename std::decay<F>::type;

    return create_wrapped_node<State<S>, SyncedIterateByRefNode<S, FuncType, E, Us...>>(
        group, std::forward<T>( initialValue ), std::forward<F>( func ), evnt, states... );
}

template <typename S, typename T, typename F, typename E, typename ... Us>
auto Iterate(T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
    { return Iterate<S>(evnt.get_group(), std::forward<T>(initialValue), std::forward<F>(func), evnt, states ...); }

template <typename S, typename T, typename F, typename E, typename ... Us>
auto IterateByRef(T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
    { return IterateByRef<S>(evnt.get_group(), std::forward<T>(initialValue), std::forward<F>(func), evnt, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot - Sets state value to value of other state when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
auto Snapshot(const group& group, const State<S>& state, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::SnapshotNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<S>, SnapshotNode<S, E>>( group, state, evnt );
}

template <typename S, typename E>
auto Snapshot(const State<S>& state, const Event<E>& evnt) -> State<S>
    { return Snapshot(state.get_group(), state, evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target state when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
auto Pulse(const group& group, const State<S>& state, const Event<E>& evnt) -> Event<S>
{
    using REACT_IMPL::PulseNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<Event<S>, PulseNode<S, E>>( group, state, evnt );
}

template <typename S, typename E>
auto Pulse(const State<S>& state, const Event<E>& evnt) -> Event<S>
    { return Pulse(state.get_group(), state, evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, template <typename> class TState,
    typename = std::enable_if_t<std::is_base_of_v<State<S>, TState<S>>>>
auto Flatten(const group& group, const State<TState<S>>& state) -> State<S>
{
    using REACT_IMPL::FlattenStateNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<S>, FlattenStateNode<S, TState>>( group, state );
}

template <typename S, template <typename> class TState,
    typename = std::enable_if_t<std::is_base_of_v<State<S>, TState<S>>>>
auto Flatten(const State<TState<S>>& state) -> State<S>
    { return Flatten(state.get_group(), state); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenList
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenList(const group& group, const State<TList<TState<V>, TParams ...>>& list) -> State<TList<V>>
{
    using REACT_IMPL::FlattenStateListNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<TList<V>>, FlattenStateListNode<TList, TState, V, TParams ...>>(
        group, list);
}

template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenList(const State<TList<TState<V>, TParams ...>>& list) -> State<TList<V>>
    { return FlattenList( list.get_group(), list); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenMap
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenMap(const group& group, const State<TMap<K, TState<V>, TParams ...>>& map) -> State<TMap<K, V>>
{
    using REACT_IMPL::FlattenStateMapNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<TMap<K, V>>, FlattenStateMapNode<TMap, TState, K, V, TParams ...>>(
        group, map);
}

template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenMap(const State<TMap<K, TState<V>, TParams ...>>& map) -> State<TMap<K, V>>
    { return FlattenMap( map.get_group(), map); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flattened
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename C>
class Flattened : public C
{
public:
    using C::C;

    Flattened(const C& base) :
        C( base )
    { }

    Flattened(const C& base, REACT_IMPL::FlattenedInitTag) :
        C( base ),
        initMode_( true )
    { }

    Flattened(const C& base, REACT_IMPL::FlattenedInitTag, std::vector<REACT_IMPL::node_id>&& emptyMemberIds) :
        C( base ),
        initMode_( true ),
        memberIds_( std::move(emptyMemberIds) ) // This will be empty, but has pre-allocated storage. It's a tweak.
    { }

    template <typename T>
    Ref<T> Flatten(State<T>& signal)
    {
        if (initMode_)
            memberIds_.push_back( get_internals( signal ).get_node_id());
        
        return get_internals( signal ).value();
    }

private:
    bool initMode_ = false;
    std::vector<REACT_IMPL::node_id> memberIds_;

    template <typename T, typename TFlat>
    friend class impl::FlattenObjectNode;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenObject
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const group& group, const State<T>& obj) -> State<TFlat>
{
    using REACT_IMPL::FlattenObjectNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<TFlat>, FlattenObjectNode<T, TFlat>>(group, obj);
}

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const State<T>& obj) -> State<TFlat>
    { return FlattenObject( obj.get_group(), obj); }

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const group& group, const State<Ref<T>>& obj) -> State<TFlat>
{
    using REACT_IMPL::FlattenObjectNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<TFlat>, FlattenObjectNode<Ref<T>, TFlat>>(group, obj);
}

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const State<Ref<T>>& obj) -> State<TFlat>
    { return FlattenObject( obj.get_group(), obj); }

/******************************************/ REACT_END /******************************************/

#endif // REACT_ALGORITHM_H_INCLUDED
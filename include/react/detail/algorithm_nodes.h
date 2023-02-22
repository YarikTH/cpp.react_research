
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED
#define REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "state_nodes.h"
#include "event_nodes.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E>
class IterateNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    IterateNode(const group& group, T&& init, FIn&& func, const Event<E>& evnt) :
        IterateNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt )
    {
        this->attach_to_me( get_internals( evnt ).get_node_id() );
    }

    ~IterateNode()
    {
        this->detach_from_me( get_internals( evnt_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        S newValue = func_(get_internals(evnt_).Events(), this->Value());

        if (! (newValue == this->Value()))
        {
            this->Value() = std::move(newValue);
            return update_result::changed;
        }
        else
        {
            return update_result::unchanged;
        }
    }

private:
    F           func_;
    Event<E>    evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E>
class IterateByRefNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    IterateByRefNode(const group& group, T&& init, FIn&& func, const Event<E>& evnt) :
        IterateByRefNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt )
    {
        this->attach_to_me( get_internals( evnt_ ).get_node_id() );
    }

    ~IterateByRefNode()
    {
        this->detach_from_me( get_internals( evnt_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        func_(get_internals(evnt_).Events(), this->Value());

        // Always assume a change
        return update_result::changed;
    }

protected:
    F           func_;
    Event<E>    evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E, typename ... TSyncs>
class SyncedIterateNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    SyncedIterateNode(const group& group, T&& init, FIn&& func, const Event<E>& evnt, const State<TSyncs>& ... syncs) :
        SyncedIterateNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt ),
        syncHolder_( syncs ... )
    {
        this->attach_to_me( get_internals( evnt ).get_node_id() );
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( syncs ).get_node_id() ));
    }

    ~SyncedIterateNode()
    {
        std::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK( this->detach_from_me( get_internals( syncs ).get_node_id() )); }, syncHolder_);
        this->detach_from_me( get_internals( evnt_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (get_internals(evnt_).Events().empty())
            return update_result::unchanged;

        S newValue = std::apply([this] (const auto& ... syncs)
            {
                return func_(get_internals(evnt_).Events(), this->Value(), get_internals(syncs).Value() ...);
            }, syncHolder_);

        if (! (newValue == this->Value()))
        {
            this->Value() = std::move(newValue);
            return update_result::changed;
        }
        else
        {
            return update_result::unchanged;
        }   
    }

private:
    F           func_;
    Event<E>    evnt_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E, typename ... TSyncs>
class SyncedIterateByRefNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    SyncedIterateByRefNode(const group& group, T&& init, FIn&& func, const Event<E>& evnt, const State<TSyncs>& ... syncs) :
        SyncedIterateByRefNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt ),
        syncHolder_( syncs ... )
    {
        this->attach_to_me( get_internals( evnt ).get_node_id() );
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( syncs ).get_node_id() ));
    }

    ~SyncedIterateByRefNode()
    {
        std::apply([this] (const auto& ... syncs) { REACT_EXPAND_PACK( this->detach_from_me( get_internals( syncs ).get_node_id() )); }, syncHolder_);
        this->detach_from_me( get_internals( evnt_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (get_internals(evnt_).Events().empty())
            return update_result::unchanged;

        std::apply(
            [this] (const auto& ... args)
            {
                func_(get_internals(evnt_).Events(), this->Value(), get_internals(args).Value() ...);
            },
            syncHolder_);

        return update_result::changed;
    }

private:
    F           func_;
    Event<E>    evnt_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// HoldNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class HoldNode : public StateNode<S>
{
public:
    template <typename T>
    HoldNode(const group& group, T&& init, const Event<S>& evnt) :
        HoldNode::StateNode( group, std::forward<T>(init) ),
        evnt_( evnt )
    {
        this->attach_to_me( get_internals( evnt ).get_node_id() );
    }

    ~HoldNode()
    {
        this->detach_from_me( get_internals( evnt_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        bool changed = false;

        if (! get_internals(evnt_).Events().empty())
        {
            const S& newValue = get_internals(evnt_).Events().back();

            if (! (newValue == this->Value()))
            {
                changed = true;
                this->Value() = newValue;
            }
        }

        if (changed)
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    Event<S>  evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SnapshotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
class SnapshotNode : public StateNode<S>
{
public:
    SnapshotNode(const group& group, const State<S>& target, const Event<E>& trigger) :
        SnapshotNode::StateNode( group, get_internals(target).Value() ),
        target_( target ),
        trigger_( trigger )
    {
        this->attach_to_me( get_internals( target ).get_node_id() );
        this->attach_to_me( get_internals( trigger ).get_node_id() );
    }

    ~SnapshotNode()
    {
        this->detach_from_me( get_internals( trigger_ ).get_node_id() );
        this->detach_from_me( get_internals( target_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        bool changed = false;
        
        if (! get_internals(trigger_).Events().empty())
        {
            const S& newValue = get_internals(target_).Value();

            if (! (newValue == this->Value()))
            {
                changed = true;
                this->Value() = newValue;
            }
        }

        if (changed)
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    State<S>    target_;
    Event<E>    trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MonitorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class MonitorNode : public EventNode<S>
{
public:
    MonitorNode(const group& group, const State<S>& input) :
        MonitorNode::EventNode( group ),
        input_( input )
    {
        this->attach_to_me( get_internals( input_ ).get_node_id() );
    }

    ~MonitorNode()
    {
        this->detach_from_me( get_internals( input_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        this->Events().push_back(get_internals(input_).Value());
        return update_result::changed;
    }

private:
    State<S>    input_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulseNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
class PulseNode : public EventNode<S>
{
public:
    PulseNode(const group& group, const State<S>& input, const Event<E>& trigger) :
        PulseNode::EventNode( group ),
        input_( input ),
        trigger_( trigger )
    {
        this->attach_to_me( get_internals( input ).get_node_id() );
        this->attach_to_me( get_internals( trigger ).get_node_id() );
    }

    ~PulseNode()
    {
        this->detach_from_me( get_internals( trigger_ ).get_node_id() );
        this->detach_from_me( get_internals( input_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        for (size_t i = 0; i < get_internals(trigger_).Events().size(); ++i)
            this->Events().push_back(get_internals(input_).Value());

        if (! this->Events().empty())
            return update_result::changed;
        else
            return update_result::unchanged;
    }

private:
    State<S>    input_;
    Event<E>    trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, template <typename> class TState>
class FlattenStateNode : public StateNode<S>
{
public:
    FlattenStateNode(const group& group, const State<TState<S>>& outer) :
        FlattenStateNode::StateNode( group, get_internals(get_internals(outer).Value()).Value() ),
        outer_( outer ),
        inner_( get_internals(outer).Value() )
    {
        this->attach_to_me( get_internals( outer_ ).get_node_id() );
        this->attach_to_me( get_internals( inner_ ).get_node_id() );
    }

    ~FlattenStateNode()
    {
        this->detach_from_me( get_internals( inner_ ).get_node_id() );
        this->detach_from_me( get_internals( outer_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        const State<S>& newInner = get_internals(outer_).Value();

        // Check if there's a new inner node.
        if (! (newInner == inner_))
        {
            this->detach_from_me( get_internals( inner_ ).get_node_id() );
            this->attach_to_me( get_internals( newInner ).get_node_id() );
            inner_ = newInner;
            return update_result::shifted;
        }

        const S& newValue = get_internals(inner_).Value();

        if (HasChanged(newValue, this->Value()))
        {
            this->Value() = newValue;
            return update_result::changed;
        }

        return update_result::unchanged;
    }

private:
    State<TState<S>>    outer_;
    State<S>            inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateListNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams>
class FlattenStateListNode : public StateNode<TList<V>>
{
public:
    using InputListType = TList<TState<V>, TParams ...>;
    using FlatListType = TList<V>;

    FlattenStateListNode(const group& group, const State<InputListType>& outer) :
        FlattenStateListNode::StateNode( group, MakeFlatList(get_internals(outer).Value()) ),
        outer_( outer ),
        inner_( get_internals(outer).Value() )
    {
        this->attach_to_me( get_internals( outer_ ).get_node_id() );

        for (const State<V>& state : inner_)
            this->attach_to_me( get_internals( state ).get_node_id() );
    }

    ~FlattenStateListNode()
    {
        for (const State<V>& state : inner_)
            this->detach_from_me( get_internals( state ).get_node_id() );

        this->detach_from_me( get_internals( outer_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        const InputListType& newInner = get_internals(outer_).Value();

        // Check if there's a new inner node.
        if (! (std::equal(begin(newInner), end(newInner), begin(inner_), end(inner_))))
        {
            for (const State<V>& state : inner_)
                this->detach_from_me( get_internals( state ).get_node_id() );
            for (const State<V>& state : newInner)
                this->attach_to_me( get_internals( state ).get_node_id() );

            inner_ = newInner;
            return update_result::shifted;
        }

        FlatListType newValue = MakeFlatList(inner_);
        const FlatListType& curValue = this->Value();

        if (! (std::equal(begin(newValue), end(newValue), begin(curValue), end(curValue), [] (const auto& a, const auto& b)
            { return !HasChanged(a, b); })))
        {
            this->Value() = std::move(newValue);
            return update_result::changed;
        }

        return update_result::unchanged;
    }

private:
    static FlatListType MakeFlatList(const InputListType& list)
    {
        FlatListType res;
        for (const State<V>& state : list)
            ListInsert(res, get_internals(state).Value());
        return res;
    }

    State<InputListType>    outer_;
    InputListType           inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateListNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams>
class FlattenStateMapNode : public StateNode<TMap<K, V>>
{
public:
    using InputMapType = TMap<K, TState<V>, TParams ...>;
    using FlatMapType = TMap<K, V>;

    FlattenStateMapNode(const group& group, const State<InputMapType>& outer) :
        FlattenStateMapNode::StateNode( group, MakeFlatMap(get_internals(outer).Value()) ),
        outer_( outer ),
        inner_( get_internals(outer).Value() )
    {
        this->attach_to_me( get_internals( outer_ ).get_node_id() );

        for (const auto& entry : inner_)
            this->attach_to_me( get_internals( entry.second ).get_node_id() );
    }

    ~FlattenStateMapNode()
    {
        for (const auto& entry : inner_)
            this->detach_from_me( get_internals( entry.second ).get_node_id() );

        this->detach_from_me( get_internals( outer_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        const InputMapType& newInner = get_internals(outer_).Value();

        // Check if there's a new inner node.
        if (! (std::equal(begin(newInner), end(newInner), begin(inner_), end(inner_))))
        {
            for (const auto& entry : inner_)
                this->detach_from_me( get_internals( entry.second ).get_node_id() );
            for (const auto& entry : newInner)
                this->attach_to_me( get_internals( entry.second ).get_node_id() );

            inner_ = newInner;
            return update_result::shifted;
        }

        FlatMapType newValue = MakeFlatMap(inner_);
        const FlatMapType& curValue = this->Value();

        if (! (std::equal(begin(newValue), end(newValue), begin(curValue), end(curValue), [] (const auto& a, const auto& b)
            { return !HasChanged(a.first, b.first) && !HasChanged(a.second, b.second); })))
        {
            this->Value() = std::move(newValue);
            return update_result::changed;
        }

        return update_result::unchanged;
    }

private:
    static FlatMapType MakeFlatMap(const InputMapType& map)
    {
        FlatMapType res;
        for (const auto& entry : map)
            MapInsert(res, typename FlatMapType::value_type{ entry.first, get_internals(entry.second).Value() });
        return res;
    }

    State<InputMapType>    outer_;
    InputMapType           inner_;
};

struct FlattenedInitTag { };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenObjectNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename TFlat>
class FlattenObjectNode : public StateNode<TFlat>
{
public:
    FlattenObjectNode(const group& group, const State<T>& obj) :
        StateNode<TFlat>( in_place, group, get_internals(obj).Value(), FlattenedInitTag{ } ),
        obj_( obj )
    {
        this->attach_to_me( get_internals( obj ).get_node_id() );

        for (node_id nodeId : this->Value().memberIds_)
            this->attach_to_me( nodeId );

        this->Value().initMode_ = false;
    }

    ~FlattenObjectNode()
    {
        for (node_id nodeId :  this->Value().memberIds_)
            this->detach_from_me( nodeId );

        this->detach_from_me( get_internals( obj_ ).get_node_id() );
    }

    virtual update_result update() noexcept override
    {
        const T& newValue = get_internals(obj_).Value();

        if (HasChanged(newValue, static_cast<const T&>(this->Value())))
        {
            for (node_id nodeId : this->Value().memberIds_)
                this->detach_from_me( nodeId );

            // Steal array from old value for new value so we don't have to re-allocate.
            // The old value will freed after the assignment.
            this->Value().memberIds_.clear();
            this->Value() = TFlat { newValue, FlattenedInitTag{ }, std::move(this->Value().memberIds_) };

            for (node_id nodeId : this->Value().memberIds_)
                this->attach_to_me( nodeId );

            return update_result::shifted;
        }

        return update_result::changed;
    }
private:
    State<T> obj_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED
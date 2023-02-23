
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_STATE_H_INCLUDED
#define REACT_STATE_H_INCLUDED

#pragma once

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "react/api.h"
#include "react/context.h"
#include "react/detail/defs.h"
#include "react/detail/state_nodes.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// State
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class State : protected REACT_IMPL::state_internals<S>
{
public:
    // Construct with explicit group
    template <typename F, typename T1, typename ... Ts>
    static State Create(const context& group, F&& func, const State<T1>& dep1, const State<Ts>& ... deps)
        { return State(CreateFuncNode(group, std::forward<F>(func), dep1, deps ...)); }

    // Construct with implicit group
    template <typename F, typename T1, typename ... Ts>
    static State Create(F&& func, const State<T1>& dep1, const State<Ts>& ... deps)
        { return State(CreateFuncNode(dep1.get_group(), std::forward<F>(func), dep1, deps ...)); }

    // Construct with constant value
    template <typename T>
    static State Create(const context& group, T&& init)
        { return State(CreateFuncNode(group, [value = std::move(init)] { return value; })); }

    State() = default;

    State(const State&) = default;
    State& operator=(const State&) = default;

    State(State&&) = default;
    State& operator=(State&&) = default;

    auto get_group() const -> const context&
        { return this->get_node_ptr()->get_group(); }

    auto get_group() -> context&
        { return this->get_node_ptr()->get_group(); }

    friend bool operator==(const State<S>& a, const State<S>& b)
        { return a.get_node_ptr() == b.get_node_ptr(); }

    friend bool operator!=(const State<S>& a, const State<S>& b)
        { return !(a == b); }

    friend auto get_internals(State<S>& s) -> REACT_IMPL::state_internals<S>&
        { return s; }

    friend auto get_internals(const State<S>& s) -> const REACT_IMPL::state_internals<S>&
        { return s; }

protected:
    State(std::shared_ptr<REACT_IMPL::state_node<S>>&& nodePtr) :
        State::state_internals( std::move(nodePtr) )
    { }

private:
    template <typename F, typename T1, typename ... Ts>
    static auto CreateFuncNode(const context& group, F&& func, const State<T1>& dep1, const State<Ts>& ... deps) -> decltype(auto)
    {
        using REACT_IMPL::state_func_node;

        return std::make_shared<state_func_node<S, typename std::decay<F>::type, T1, Ts ...>>(
            group, std::forward<F>(func), dep1, deps...);
    }

    template <typename RET, typename NODE, typename ... ARGS>
    friend RET impl::create_wrapped_node(ARGS&& ... args);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateVar
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateVar : public State<S>
{
public:
    // Construct with group + default
    static StateVar Create(const context& group)
        { return StateVar(CreateVarNode(group)); }

    // Construct with group + value
    template <typename T>
    static StateVar Create(const context& group, T&& value)
        { return StateVar(CreateVarNode(group, std::forward<T>(value))); }

    StateVar() = default;

    StateVar(const StateVar&) = default;
    StateVar& operator=(const StateVar&) = default;

    StateVar(StateVar&&) = default;
    StateVar& operator=(StateVar&&) = default;

    void Set(const S& newValue)
        { SetValue(newValue); }

    void Set(S&& newValue)
        { SetValue(std::move(newValue)); }

    template <typename F>
    void Modify(const F& func)
        { ModifyValue(func); }

    friend bool operator==(const StateVar<S>& a, StateVar<S>& b)
        { return a.get_node_ptr() == b.get_node_ptr(); }

    friend bool operator!=(const StateVar<S>& a, StateVar<S>& b)
        { return !(a == b); }

    S* operator->()
        { return &this->value(); }

protected:
    StateVar(std::shared_ptr<REACT_IMPL::state_node<S>>&& nodePtr) :
        StateVar::State( std::move(nodePtr) )
    { }

private:
    static auto CreateVarNode(const context& group) -> decltype(auto)
    {
        using REACT_IMPL::state_var_node;
        return std::make_shared<state_var_node<S>>(group);
    }

    template <typename T>
    static auto CreateVarNode(const context& group, T&& value) -> decltype(auto)
    {
        using REACT_IMPL::state_var_node;
        return std::make_shared<state_var_node<S>>(group, std::forward<T>(value));
    }

    template <typename T>
    void SetValue(T&& newValue)
    {
        using REACT_IMPL::node_id;
        using VarNodeType = REACT_IMPL::state_var_node<S>;

        VarNodeType* castedPtr = static_cast<VarNodeType*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->get_node_id();
        auto& graph = get_internals( this->get_group() ).get_graph();

        castedPtr->set_value( std::forward<T>( newValue ) );
        graph.push_input(nodeId);
    }

    template <typename F>
    void ModifyValue(const F& func)
    {
        using REACT_IMPL::node_id;
        using VarNodeType = REACT_IMPL::state_var_node<S>;

        VarNodeType* castedPtr = static_cast<VarNodeType*>(this->get_node_ptr().get());
        
        node_id nodeId = castedPtr->get_node_id();
        auto& graph = get_internals( this->get_group() ).get_graph();

        castedPtr->modify_value( func );
        graph.push_input(nodeId);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateSlotBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateSlot : public State<S>
{
public:
    // Construct with explicit group
    static StateSlot Create(const context& group, const State<S>& input)
        { return StateSlot(CreateSlotNode(group, input)); }

    // Construct with implicit group
    static StateSlot Create(const State<S>& input)
        { return StateSlot(CreateSlotNode(input.get_group(), input)); }

    StateSlot() = default;

    StateSlot(const StateSlot&) = default;
    StateSlot& operator=(const StateSlot&) = default;

    StateSlot(StateSlot&&) = default;
    StateSlot& operator=(StateSlot&&) = default;

    void Set(const State<S>& newInput)
        { SetSlotInput(newInput); }

protected:
    StateSlot(std::shared_ptr<REACT_IMPL::state_node<S>>&& nodePtr) :
        StateSlot::State( std::move(nodePtr) )
    { }

private:
    static auto CreateSlotNode(const context& group, const State<S>& input) -> decltype(auto)
    {
        using REACT_IMPL::StateSlotNode;

        return std::make_shared<StateSlotNode<S>>(group, input);
    }

    void SetSlotInput(const State<S>& newInput)
    {
        using REACT_IMPL::node_id;
        using REACT_IMPL::StateSlotNode;

        auto* castedPtr = static_cast<StateSlotNode<S>*>(this->get_node_ptr().get());

        node_id nodeId = castedPtr->GetInputNodeId();
        auto& graph = get_internals( this->get_group() ).get_graph();

        castedPtr->SetInput(newInput);
        graph.push_input(nodeId);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CreateRef
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
auto CreateRef(const State<S>& state) -> State<Ref<S>>
{
    using REACT_IMPL::StateRefNode;
    using REACT_IMPL::create_wrapped_node;

    return create_wrapped_node<State<Ref<S>>, StateRefNode<S>>( state.get_group(), state );
}

/******************************************/ REACT_END /******************************************/

#endif // REACT_STATE_H_INCLUDED
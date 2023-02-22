
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#ifndef REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED

#include "react/detail/defs.h"

#include <cassert>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "node_base.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class state_node : public node_base
{
public:
    explicit state_node(const group& group)
        : state_node::node_base( group ),
        value_( )
    { }

    template <typename T>
    state_node(const group& group, T&& value)
        : state_node::node_base( group ),
        value_( std::forward<T>(value) )
    { }

    template <typename ... Ts>
    state_node(InPlaceTag, const group& group, Ts&& ... args)
        : state_node::node_base( group ),
        value_( std::forward<Ts>(args) ... )
    { }

    S& value()
        { return value_; }

    const S& value() const
        { return value_; }

private:
    S value_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class state_var_node : public state_node<S>
{
public:
    explicit state_var_node(const group& group)
        : state_var_node::state_node( group ),
        newValue_( )
    {
    }

    template <typename T>
    state_var_node(const group& group, T&& value)
        : state_var_node::state_node( group, std::forward<T>(value) ),
        newValue_( value )
    {
    }

    update_result update() noexcept override
    {
        if (isInputAdded_)
        {
            isInputAdded_ = false;

            if ( has_changed( this->value(), newValue_ ))
            {
                this->value() = std::move(newValue_);
                return update_result::changed;
            }
            else
            {
                return update_result::unchanged;
            }
        }
        else if (isInputModified_)
        {            
            isInputModified_ = false;
            return update_result::changed;
        }

        else
        {
            return update_result::unchanged;
        }
    }

    template <typename T>
    void set_value(T&& newValue)
    {
        newValue_ = std::forward<T>(newValue);

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    template <typename F>
    void modify_value(F&& func)
    {
        // There hasn't been any Set(...) input yet, modify.
        if (! isInputAdded_)
        {
            func( this->value());

            isInputModified_ = true;
        }
        // There's a newValue, modify newValue instead.
        // The modified newValue will handled like before, i.e. it'll be compared to value_
        // in ApplyInput
        else
        {
            func(newValue_);
        }
    }

private:
    S       newValue_;
    bool    isInputAdded_ = false;
    bool    isInputModified_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateFuncNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename ... TDeps>
class state_func_node : public state_node<S>
{
public:
    template <typename FIn>
    state_func_node(const group& group, FIn&& func, const State<TDeps>& ... deps)
        : state_func_node::state_node( group, func( get_internals( deps ).value() ...) ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        REACT_EXPAND_PACK( this->attach_to_me( get_internals( deps ).get_node_id() ));
    }

    ~state_func_node()
    {
        std::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK(
                    this->detach_from_me( get_internals( deps ).get_node_ptr()->get_node_id() )); }, depHolder_);
    }

    update_result update() noexcept override
    {
        S newValue = std::apply([this] (const auto& ... deps)
            { return this->func_( get_internals( deps ).value() ...); }, depHolder_);

        if ( has_changed( this->value(), newValue ))
        {
            this->value() = std::move(newValue);
            return update_result::changed;
        }
        else
        {
            return update_result::unchanged;
        }
    }

private:
    F func_;
    std::tuple<State<TDeps> ...> depHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateSlotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateSlotNode : public state_node<S>
{
public:
    StateSlotNode(const group& group, const State<S>& dep) :
        StateSlotNode::state_node( group, get_internals( dep ).value() ),
        input_( dep )
    {
        inputNodeId_ = this->get_graph_ptr()->register_node(&slotInput_);

        this->attach_to_me( inputNodeId_ );
        this->attach_to_me( get_internals( dep ).get_node_id() );
    }

    ~StateSlotNode()
    {
        this->detach_from_me( get_internals( input_ ).get_node_id() );
        this->detach_from_me( inputNodeId_ );

        this->get_graph_ptr()->unregister_node(inputNodeId_);
    }

    update_result update() noexcept override
    {
        if (! ( this->value() == get_internals( input_ ).value()))
        {
            this->value() = get_internals( input_ ).value();
            return update_result::changed;
        }
        else
        {
            return update_result::unchanged;
        }
    }

    void SetInput(const State<S>& newInput)
    {
        if (newInput == input_)
            return;

        this->detach_from_me( get_internals( input_ ).get_node_id() );
        this->attach_to_me( get_internals( newInput ).get_node_id() );

        input_ = newInput;
    }

    node_id GetInputNodeId() const
        { return inputNodeId_; }

private:        
    struct VirtualInputNode : public reactive_node_interface
    {
        update_result update() noexcept override
            { return update_result::changed; }
    };

    State<S>            input_;
    node_id              inputNodeId_;
    VirtualInputNode    slotInput_;
    
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// state_internals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class state_internals
{
public:
    state_internals() = default;

    state_internals(const state_internals&) = default;
    state_internals& operator=(const state_internals&) = default;

    state_internals(state_internals&&)  noexcept = default;
    state_internals& operator=(state_internals&&)  noexcept = default;

    explicit state_internals(std::shared_ptr<state_node<S>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto get_node_ptr() -> std::shared_ptr<state_node<S>>&
        { return nodePtr_; }

    auto get_node_ptr() const -> const std::shared_ptr<state_node<S>>&
        { return nodePtr_; }

    node_id get_node_id() const
        { return nodePtr_->get_node_id(); }

    S& value()
        { return nodePtr_->value(); }

    const S& value() const
        { return nodePtr_->value(); }

private:
    std::shared_ptr<state_node<S>> nodePtr_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateRefNode : public state_node<Ref<S>>
{
public:
    StateRefNode(const group& group, const State<S>& input) :
        StateRefNode::state_node( group, std::cref( get_internals( input ).value()) ),
        input_( input )
    {
        this->attach_to_me( get_internals( input ).get_node_id() );
    }

    ~StateRefNode()
    {
        this->detach_from_me( get_internals( input_ ).get_node_id() );
    }

    update_result update() noexcept override
    {
        this->value() = std::cref( get_internals( input_ ).value());
        return update_result::changed;
    }

private:
    State<S> input_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED
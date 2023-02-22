
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
class StateNode : public NodeBase
{
public:
    explicit StateNode(const Group& group) :
        StateNode::NodeBase( group ),
        value_( )
    { }

    template <typename T>
    StateNode(const Group& group, T&& value) :
        StateNode::NodeBase( group ),
        value_( std::forward<T>(value) )
    { }

    template <typename ... Ts>
    StateNode(InPlaceTag, const Group& group, Ts&& ... args) :
        StateNode::NodeBase( group ),
        value_( std::forward<Ts>(args) ... )
    { }

    S& Value()
        { return value_; }

    const S& Value() const
        { return value_; }

private:
    S value_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateVarNode : public StateNode<S>
{
public:
    explicit StateVarNode(const Group& group) :
        StateVarNode::StateNode( group ),
        newValue_( )
    {
    }

    template <typename T>
    StateVarNode(const Group& group, T&& value) :
        StateVarNode::StateNode( group, std::forward<T>(value) ),
        newValue_( value )
    {
    }

    virtual UpdateResult Update() noexcept override
    {
        if (isInputAdded_)
        {
            isInputAdded_ = false;

            if (HasChanged(this->Value(), newValue_))
            {
                this->Value() = std::move(newValue_);
                return UpdateResult::changed;
            }
            else
            {
                return UpdateResult::unchanged;
            }
        }
        else if (isInputModified_)
        {            
            isInputModified_ = false;
            return UpdateResult::changed;
        }

        else
        {
            return UpdateResult::unchanged;
        }
    }

    template <typename T>
    void SetValue(T&& newValue)
    {
        newValue_ = std::forward<T>(newValue);

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    template <typename F>
    void ModifyValue(F&& func)
    {
        // There hasn't been any Set(...) input yet, modify.
        if (! isInputAdded_)
        {
            func(this->Value());

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
class StateFuncNode : public StateNode<S>
{
public:
    template <typename FIn>
    StateFuncNode(const Group& group, FIn&& func, const State<TDeps>& ... deps) :
        StateFuncNode::StateNode( group, func(GetInternals(deps).Value() ...) ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(deps).GetNodeId()));
    }

    ~StateFuncNode()
    {
        react::impl::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(deps).GetNodePtr()->GetNodeId())); }, depHolder_);
    }

    virtual UpdateResult Update() noexcept override
    {
        S newValue = react::impl::apply([this] (const auto& ... deps)
            { return this->func_(GetInternals(deps).Value() ...); }, depHolder_);

        if (HasChanged(this->Value(), newValue))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
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
class StateSlotNode : public StateNode<S>
{
public:
    StateSlotNode(const Group& group, const State<S>& dep) :
        StateSlotNode::StateNode( group, GetInternals(dep).Value() ),
        input_( dep )
    {
        inputNodeId_ = this->GetGraphPtr()->register_node(&slotInput_);
        
        this->AttachToMe(inputNodeId_);
        this->AttachToMe(GetInternals(dep).GetNodeId());
    }

    ~StateSlotNode()
    {
        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->DetachFromMe(inputNodeId_);

        this->GetGraphPtr()->unregister_node(inputNodeId_);
    }

    virtual UpdateResult Update() noexcept override
    {
        if (! (this->Value() == GetInternals(input_).Value()))
        {
            this->Value() = GetInternals(input_).Value();
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
        }
    }

    void SetInput(const State<S>& newInput)
    {
        if (newInput == input_)
            return;

        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->AttachToMe(GetInternals(newInput).GetNodeId());

        input_ = newInput;
    }

    node_id GetInputNodeId() const
        { return inputNodeId_; }

private:        
    struct VirtualInputNode : public IReactNode
    {
        virtual UpdateResult Update() noexcept override
            { return UpdateResult::changed; }
    };

    State<S>            input_;
    node_id              inputNodeId_;
    VirtualInputNode    slotInput_;
    
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateInternals
{
public:
    StateInternals() = default;

    StateInternals(const StateInternals&) = default;
    StateInternals& operator=(const StateInternals&) = default;

    StateInternals(StateInternals&&) = default;
    StateInternals& operator=(StateInternals&&) = default;

    explicit StateInternals(std::shared_ptr<StateNode<S>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto GetNodePtr() -> std::shared_ptr<StateNode<S>>&
        { return nodePtr_; }

    auto GetNodePtr() const -> const std::shared_ptr<StateNode<S>>&
        { return nodePtr_; }

    node_id GetNodeId() const
        { return nodePtr_->GetNodeId(); }

    S& Value()
        { return nodePtr_->Value(); }

    const S& Value() const
        { return nodePtr_->Value(); }

private:
    std::shared_ptr<StateNode<S>> nodePtr_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateRefNode : public StateNode<Ref<S>>
{
public:
    StateRefNode(const Group& group, const State<S>& input) :
        StateRefNode::StateNode( group, std::cref(GetInternals(input).Value()) ),
        input_( input )
    {
        this->AttachToMe(GetInternals(input).GetNodeId());
    }

    ~StateRefNode()
    {
        this->DetachFromMe(GetInternals(input_).GetNodeId());
    }

    virtual UpdateResult Update() noexcept override
    {
        this->Value() = std::cref(GetInternals(input_).Value());
        return UpdateResult::changed;
    }

private:
    State<S> input_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED
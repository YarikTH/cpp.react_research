
//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_GROUP_H_INCLUDED
#define REACT_GROUP_H_INCLUDED

#pragma once

#include "react/detail/defs.h"

#include <memory>
#include <utility>

#include "react/api.h"
#include "react/common/syncpoint.h"

#include "react/detail/graph_interface.h"
#include "react/detail/graph_impl.h"

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Group
///////////////////////////////////////////////////////////////////////////////////////////////////
class context : protected REACT_IMPL::context_internals
{
public:
    context() = default;

    context(const context&) = default;
    context& operator=(const context&) = default;

    context( context&&) = default;
    context& operator=( context&&) = default;

    template <typename F>
    void do_transaction(F&& func)
        {
            get_graph().do_transaction(std::forward<F>(func)); }

    template <typename F>
    void enqueue_transaction(F&& func, TransactionFlags flags = TransactionFlags::none )
        {
            get_graph().enqueue_transaction(std::forward<F>(func), SyncPoint::Dependency{ }, flags); }

    template <typename F>
    void enqueue_transaction(F&& func, const SyncPoint& syncPoint, TransactionFlags flags = TransactionFlags::none )
        {
            get_graph().enqueue_transaction(std::forward<F>(func), SyncPoint::Dependency{ syncPoint }, flags); }

    friend bool operator==(const context& a, const context& b)
        { return &a.get_graph() == &b.get_graph(); }

    friend bool operator!=(const context& a, const context& b)
        { return !(a == b); }

    friend auto get_internals( context& g) -> REACT_IMPL::context_internals&
        { return g; }

    friend auto get_internals(const context& g) -> const REACT_IMPL::context_internals&
        { return g; }
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_GROUP_H_INCLUDED
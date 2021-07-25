
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <vector>

#include "gtest/gtest.h"
#include "react/react.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

using namespace react;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverTest fixture
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TParams>
class ObserverTest : public testing::Test
{
public:
    REACTIVE_DOMAIN( MyDomain )
};

TYPED_TEST_CASE_P( ObserverTest );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// detach test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( ObserverTest, detach )
{
    using D = typename detach::MyDomain;

    auto a1 = make_var<D>( 1 );
    auto a2 = make_var<D>( 1 );

    auto result = make_signal<D>( ( a1, a2 ), []( int a, int b ) { return a + b; } );

    int observeCount1 = 0;
    int observeCount2 = 0;
    int observeCount3 = 0;

    int phase;

    auto obs1 = observe( result, [&]( int v ) {
        observeCount1++;

        if( phase == 0 )
            ASSERT_EQ( v, 3 );
        else if( phase == 1 )
            ASSERT_EQ( v, 4 );
        else
            ASSERT_TRUE( false );
    } );

    auto obs2 = observe( result, [&]( int v ) {
        observeCount2++;

        if( phase == 0 )
            ASSERT_EQ( v, 3 );
        else if( phase == 1 )
            ASSERT_EQ( v, 4 );
        else
            ASSERT_TRUE( false );
    } );

    auto obs3 = observe( result, [&]( int v ) {
        observeCount3++;

        if( phase == 0 )
            ASSERT_EQ( v, 3 );
        else if( phase == 1 )
            ASSERT_EQ( v, 4 );
        else
            ASSERT_TRUE( false );
    } );

    phase = 0;
    a1 <<= 2;
    ASSERT_EQ( observeCount1, 1 );
    ASSERT_EQ( observeCount2, 1 );
    ASSERT_EQ( observeCount3, 1 );

    phase = 1;
    obs1.detach();
    a1 <<= 3;
    ASSERT_EQ( observeCount1, 1 );
    ASSERT_EQ( observeCount2, 2 );
    ASSERT_EQ( observeCount3, 2 );

    phase = 2;
    obs2.detach();
    obs3.detach();
    a1 <<= 4;
    ASSERT_EQ( observeCount1, 1 );
    ASSERT_EQ( observeCount2, 2 );
    ASSERT_EQ( observeCount3, 2 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ScopedObserver test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( ObserverTest, ScopedObserverTest )
{
    using D = typename ScopedObserverTest::MyDomain;

    std::vector<int> results;

    auto in = make_var<D>( 1 );

    {
        scoped_observer<D> obs = observe( in, [&]( int v ) { results.push_back( v ); } );

        in <<= 2;
    }

    in <<= 3;

    ASSERT_EQ( results.size(), 1 );
    ASSERT_EQ( results[0], 2 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Synced observe test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( ObserverTest, SyncedObserveTest )
{
    using D = typename SyncedObserveTest::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto sum = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a + b; } );
    auto prod = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a * b; } );
    auto diff = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a - b; } );

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    observe( src1, with( sum, prod, diff ), []( token, int sum, int prod, int diff ) {
        ASSERT_EQ( sum, 33 );
        ASSERT_EQ( prod, 242 );
        ASSERT_EQ( diff, 11 );
    } );

    observe( src2, with( sum, prod, diff ), []( int e, int sum, int prod, int diff ) {
        ASSERT_EQ( e, 42 );
        ASSERT_EQ( sum, 33 );
        ASSERT_EQ( prod, 242 );
        ASSERT_EQ( diff, 11 );
    } );

    in1 <<= 22;
    in2 <<= 11;

    src1.emit();
    src2.emit( 42 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachThisObserver1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( ObserverTest, DetachThisObserver1 )
{
    using D = typename DetachThisObserver1::MyDomain;

    auto src = make_event_source<D>();

    int count = 0;

    observe( src, [&]( token ) -> observer_action {
        ++count;
        return observer_action::stop_and_detach;
    } );

    src.emit();
    src.emit();

    printf( "Count %d\n", count );
    ASSERT_EQ( count, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachThisObserver2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( ObserverTest, DetachThisObserver2 )
{
    using D = typename DetachThisObserver2::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto sum = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a + b; } );
    auto prod = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a * b; } );
    auto diff = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a - b; } );

    auto src = make_event_source<D>();

    int count = 0;

    observe(
        src, with( sum, prod, diff ), [&]( token, int sum, int prod, int diff ) -> observer_action {
            ++count;
            return observer_action::stop_and_detach;
        } );

    in1 <<= 22;
    in2 <<= 11;

    src.emit();
    src.emit();

    ASSERT_EQ( count, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_TYPED_TEST_CASE_P( ObserverTest,
    detach,
    ScopedObserverTest,
    SyncedObserveTest,
    DetachThisObserver1,
    DetachThisObserver2 );

} // namespace


//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <queue>
#include <vector>

#include "gtest/gtest.h"
#include "react/react.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

using namespace react;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalTest fixture
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TParams>
class SignalTest : public testing::Test
{
public:
    REACTIVE_DOMAIN( MyDomain )
};

TYPED_TEST_CASE_P( SignalTest );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVars test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, MakeVars )
{
    using D = typename MakeVars::MyDomain;

    auto v1 = make_var<D>( 1 );
    auto v2 = make_var<D>( 2 );
    auto v3 = make_var<D>( 3 );
    auto v4 = make_var<D>( 4 );

    ASSERT_EQ( v1(), 1 );
    ASSERT_EQ( v2(), 2 );
    ASSERT_EQ( v3(), 3 );
    ASSERT_EQ( v4(), 4 );

    v1 <<= 10;
    v2 <<= 20;
    v3 <<= 30;
    v4 <<= 40;

    ASSERT_EQ( v1(), 10 );
    ASSERT_EQ( v2(), 20 );
    ASSERT_EQ( v3(), 30 );
    ASSERT_EQ( v4(), 40 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signals1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Signals1 )
{
    using D = typename Signals1::MyDomain;

    auto summ = []( int a, int b ) { return a + b; };

    auto v1 = make_var<D>( 1 );
    auto v2 = make_var<D>( 2 );
    auto v3 = make_var<D>( 3 );
    auto v4 = make_var<D>( 4 );

    auto s1 = make_signal( with( v1, v2 ), []( int a, int b ) { return a + b; } );

    auto s2 = make_signal( with( v3, v4 ), []( int a, int b ) { return a + b; } );

    auto s3 = ( s1, s2 )->*summ;

    ASSERT_EQ( s1(), 3 );
    ASSERT_EQ( s2(), 7 );
    ASSERT_EQ( s3(), 10 );

    v1 <<= 10;
    v2 <<= 20;
    v3 <<= 30;
    v4 <<= 40;

    ASSERT_EQ( s1(), 30 );
    ASSERT_EQ( s2(), 70 );
    ASSERT_EQ( s3(), 100 );

    bool b = false;

    b = is_signal<decltype( v1 )>::value;
    ASSERT_TRUE( b );

    b = is_signal<decltype( s1 )>::value;
    ASSERT_TRUE( b );

    b = is_signal<decltype( s2 )>::value;
    ASSERT_TRUE( b );

    b = is_signal<decltype( 10 )>::value;
    ASSERT_FALSE( b );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signals2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Signals2 )
{
    using D = typename Signals2::MyDomain;

    auto a1 = make_var<D>( 1 );
    auto a2 = make_var<D>( 1 );

    auto plus0 = []( int value ) { return value + 0; };
    auto summ = []( int a, int b ) { return a + b; };

    auto b1 = a1->*plus0;
    auto b2 = ( a1, a2 )->*summ;
    auto b3 = a2->*plus0;

    auto c1 = ( b1, b2 )->*summ;
    auto c2 = ( b2, b3 )->*summ;

    auto result = ( c1, c2 )->*summ;

    int observeCount = 0;

    observe( result, [&observeCount]( int v ) {
        observeCount++;
        if( observeCount == 1 )
            ASSERT_EQ( v, 9 );
        else
            ASSERT_EQ( v, 12 );
    } );

    ASSERT_EQ( a1(), 1 );
    ASSERT_EQ( a2(), 1 );

    ASSERT_EQ( b1(), 1 );
    ASSERT_EQ( b2(), 2 );
    ASSERT_EQ( b3(), 1 );

    ASSERT_EQ( c1(), 3 );
    ASSERT_EQ( c2(), 3 );

    ASSERT_EQ( result(), 6 );

    a1 <<= 2;

    ASSERT_EQ( observeCount, 1 );

    ASSERT_EQ( a1(), 2 );
    ASSERT_EQ( a2(), 1 );

    ASSERT_EQ( b1(), 2 );
    ASSERT_EQ( b2(), 3 );
    ASSERT_EQ( b3(), 1 );

    ASSERT_EQ( c1(), 5 );
    ASSERT_EQ( c2(), 4 );

    ASSERT_EQ( result(), 9 );

    a2 <<= 2;

    ASSERT_EQ( observeCount, 2 );

    ASSERT_EQ( a1(), 2 );
    ASSERT_EQ( a2(), 2 );

    ASSERT_EQ( b1(), 2 );
    ASSERT_EQ( b2(), 4 );
    ASSERT_EQ( b3(), 2 );

    ASSERT_EQ( c1(), 6 );
    ASSERT_EQ( c2(), 6 );

    ASSERT_EQ( result(), 12 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signals3 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Signals3 )
{
    using D = typename Signals3::MyDomain;

    auto a1 = make_var<D>( 1 );
    auto a2 = make_var<D>( 1 );

    auto plus0 = []( int value ) { return value + 0; };
    auto summ = []( int a, int b ) { return a + b; };

    auto b1 = a1->*plus0;
    auto b2 = ( a1, a2 )->*summ;
    auto b3 = a2->*plus0;

    auto c1 = ( b1, b2 )->*summ;
    auto c2 = ( b2, b3 )->*summ;

    auto result = ( c1, c2 )->*summ;

    int observeCount = 0;

    observe( result, [&observeCount]( int v ) {
        observeCount++;
        ASSERT_EQ( v, 12 );
    } );

    ASSERT_EQ( a1(), 1 );
    ASSERT_EQ( a2(), 1 );

    ASSERT_EQ( b1(), 1 );
    ASSERT_EQ( b2(), 2 );
    ASSERT_EQ( b3(), 1 );

    ASSERT_EQ( c1(), 3 );
    ASSERT_EQ( c2(), 3 );

    ASSERT_EQ( result(), 6 );

    do_transaction<D>( [&] {
        a1 <<= 2;
        a2 <<= 2;
    } );

    ASSERT_EQ( observeCount, 1 );

    ASSERT_EQ( a1(), 2 );
    ASSERT_EQ( a2(), 2 );

    ASSERT_EQ( b1(), 2 );
    ASSERT_EQ( b2(), 4 );
    ASSERT_EQ( b3(), 2 );

    ASSERT_EQ( c1(), 6 );
    ASSERT_EQ( c2(), 6 );

    ASSERT_EQ( result(), 12 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signals4 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Signals4 )
{
    using D = typename Signals4::MyDomain;

    auto a1 = make_var<D>( 1 );
    auto a2 = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto b1 = ( a1, a2 )->*summ;
    auto b2 = ( b1, a2 )->*summ;

    ASSERT_EQ( a1(), 1 );
    ASSERT_EQ( a2(), 1 );

    ASSERT_EQ( b1(), 2 );
    ASSERT_EQ( b2(), 3 );

    a1 <<= 10;

    ASSERT_EQ( a1(), 10 );
    ASSERT_EQ( a2(), 1 );

    ASSERT_EQ( b1(), 11 );
    ASSERT_EQ( b2(), 12 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FunctionBind1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, FunctionBind1 )
{
    using D = typename FunctionBind1::MyDomain;

    auto v1 = make_var<D>( 2 );
    auto v2 = make_var<D>( 30 );
    auto v3 = make_var<D>( 10 );

    auto signal = ( v1, v2, v3 )->*[=]( int a, int b, int c ) -> int { return a * b * c; };

    ASSERT_EQ( signal(), 600 );
    v3 <<= 100;
    ASSERT_EQ( signal(), 6000 );
}

int myfunc( int a, int b )
{
    return a + b;
}
float myfunc2( int a )
{
    return a / 2.0f;
}
float myfunc3( float a, float b )
{
    return a * b;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FunctionBind2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, FunctionBind2 )
{
    using D = typename FunctionBind2::MyDomain;

    auto a = make_var<D>( 1 );
    auto b = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto c = ( ( ( a, b )->*summ ), ( ( a, make_var<D>( 100 ) )->*summ ) )->*&myfunc;
    auto d = c->*&myfunc2;
    auto e = ( d, d )->*&myfunc3;
    auto f = make_signal<D>( e, []( float value ) { return -value + 100; } );

    ASSERT_EQ( c(), 103 );
    ASSERT_EQ( d(), 51.5f );
    ASSERT_EQ( e(), 2652.25f );
    ASSERT_EQ( f(), -2552.25 );

    a <<= 10;

    ASSERT_EQ( c(), 121 );
    ASSERT_EQ( d(), 60.5f );
    ASSERT_EQ( e(), 3660.25f );
    ASSERT_EQ( f(), -3560.25f );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Flatten1 )
{
    using D = typename Flatten1::MyDomain;

    auto inner1 = make_var<D>( 123 );
    auto inner2 = make_var<D>( 789 );

    auto outer = make_var<D>( inner1 );

    auto flattened = flatten( outer );

    std::queue<int> results;

    observe( flattened, [&]( int v ) { results.push( v ); } );

    ASSERT_TRUE( outer().equals( inner1 ) );
    ASSERT_EQ( flattened(), 123 );

    inner1 <<= 456;

    ASSERT_EQ( flattened(), 456 );

    ASSERT_EQ( results.front(), 456 );
    results.pop();
    ASSERT_TRUE( results.empty() );

    outer <<= inner2;

    ASSERT_TRUE( outer().equals( inner2 ) );
    ASSERT_EQ( flattened(), 789 );

    ASSERT_EQ( results.front(), 789 );
    results.pop();
    ASSERT_TRUE( results.empty() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Flatten2 )
{
    using D = typename Flatten2::MyDomain;

    auto a0 = make_var<D>( 100 );

    auto inner1 = make_var<D>( 200 );

    auto plus0 = []( int value ) { return value + 0; };

    auto a1 = make_var<D>( 300 );
    auto a2 = make_signal<D>( a1, plus0 );
    auto a3 = make_signal<D>( a2, plus0 );
    auto a4 = make_signal<D>( a3, plus0 );
    auto a5 = make_signal<D>( a4, plus0 );
    auto a6 = make_signal<D>( a5, plus0 );
    auto inner2 = make_signal<D>( a6, plus0 );

    ASSERT_EQ( inner1(), 200 );
    ASSERT_EQ( inner2(), 300 );

    auto outer = make_var<D>( inner1 );

    auto flattened = flatten( outer );

    ASSERT_EQ( flattened(), 200 );

    int observeCount = 0;

    observe( flattened, [&observeCount]( int v ) { observeCount++; } );

    auto o1 = make_signal<D>( ( a0, flattened ), []( int a, int b ) { return a + b; } );
    auto o2 = make_signal<D>( o1, plus0 );
    auto o3 = make_signal<D>( o2, plus0 );
    auto result = make_signal<D>( o3, plus0 );

    ASSERT_EQ( result(), 100 + 200 );

    inner1 <<= 1234;

    ASSERT_EQ( result(), 100 + 1234 );
    ASSERT_EQ( observeCount, 1 );

    outer <<= inner2;

    ASSERT_EQ( result(), 100 + 300 );
    ASSERT_EQ( observeCount, 2 );

    do_transaction<D>( [&] {
        a0 <<= 5000;
        a1 <<= 6000;
    } );

    ASSERT_EQ( result(), 5000 + 6000 );
    ASSERT_EQ( observeCount, 3 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten3 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Flatten3 )
{
    using D = typename Flatten3::MyDomain;

    auto plus0 = []( int value ) { return value + 0; };
    auto summ = []( int a, int b ) { return a + b; };

    auto inner1 = make_var<D>( 10 );

    auto a1 = make_var<D>( 20 );
    auto a2 = a1->*plus0;
    auto a3 = a2->*plus0;
    auto inner2 = a3->*plus0;

    auto outer = make_var<D>( inner1 );

    auto a0 = make_var<D>( 30 );

    auto flattened = flatten( outer );

    int observeCount = 0;

    observe( flattened, [&observeCount]( int v ) { observeCount++; } );

    auto result = ( flattened, a0 )->*summ;

    ASSERT_EQ( result(), 10 + 30 );
    ASSERT_EQ( observeCount, 0 );

    do_transaction<D>( [&] {
        inner1 <<= 1000;
        a0 <<= 200000;
        a1 <<= 50000;
        outer <<= inner2;
    } );

    ASSERT_EQ( result(), 50000 + 200000 );
    ASSERT_EQ( observeCount, 1 );

    do_transaction<D>( [&] {
        a0 <<= 667;
        a1 <<= 776;
    } );

    ASSERT_EQ( result(), 776 + 667 );
    ASSERT_EQ( observeCount, 2 );

    do_transaction<D>( [&] {
        inner1 <<= 999;
        a0 <<= 888;
    } );

    ASSERT_EQ( result(), 776 + 888 );
    ASSERT_EQ( observeCount, 2 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten4 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Flatten4 )
{
    using D = typename Flatten4::MyDomain;

    std::vector<int> results;

    auto plus0 = []( int value ) { return value + 0; };
    auto summ = []( int a, int b ) { return a + b; };

    auto a1 = make_var<D>( 100 );
    auto inner1 = a1->*plus0;

    auto a2 = make_var<D>( 200 );
    auto inner2 = a2;

    auto a3 = make_var<D>( 200 );

    auto outer = make_var<D>( inner1 );

    auto flattened = flatten( outer );

    auto result = ( flattened, a3 )->*summ;

    observe( result, [&]( int v ) { results.push_back( v ); } );

    do_transaction<D>( [&] {
        a3 <<= 400;
        outer <<= inner2;
    } );

    ASSERT_EQ( results.size(), 1 );

    ASSERT_TRUE( std::find( results.begin(), results.end(), 600 ) != results.end() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Member1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Member1 )
{
    using D = typename Member1::MyDomain;

    auto outer = make_var<D>( 10 );
    auto inner = make_var<D>( outer );

    auto flattened = flatten( inner );

    observe( flattened, []( int v ) { ASSERT_EQ( v, 30 ); } );

    outer <<= 30;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Modify1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Modify1 )
{
    using D = typename Modify1::MyDomain;

    using std::vector;

    auto v = make_var<D>( vector<int>{} );

    int obsCount = 0;

    observe( v, [&]( const vector<int>& v ) {
        ASSERT_EQ( v[0], 30 );
        ASSERT_EQ( v[1], 50 );
        ASSERT_EQ( v[2], 70 );

        obsCount++;
    } );

    v.modify( []( vector<int>& v ) {
        v.push_back( 30 );
        v.push_back( 50 );
        v.push_back( 70 );
    } );

    ASSERT_EQ( obsCount, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Modify2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Modify2 )
{
    using D = typename Modify2::MyDomain;

    using std::vector;

    auto v = make_var<D>( vector<int>{} );

    int obsCount = 0;

    observe( v, [&]( const vector<int>& v ) {
        ASSERT_EQ( v[0], 30 );
        ASSERT_EQ( v[1], 50 );
        ASSERT_EQ( v[2], 70 );

        obsCount++;
    } );

    do_transaction<D>( [&] {
        v.modify( []( vector<int>& v ) { v.push_back( 30 ); } );

        v.modify( []( vector<int>& v ) { v.push_back( 50 ); } );

        v.modify( []( vector<int>& v ) { v.push_back( 70 ); } );
    } );


    ASSERT_EQ( obsCount, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Modify3 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( SignalTest, Modify3 )
{
    using D = typename Modify3::MyDomain;

    using std::vector;

    auto vect = make_var<D>( vector<int>{} );

    int obsCount = 0;

    observe( vect, [&]( const vector<int>& v ) {
        ASSERT_EQ( v[0], 30 );
        ASSERT_EQ( v[1], 50 );
        ASSERT_EQ( v[2], 70 );

        obsCount++;
    } );

    // Also terrible
    do_transaction<D>( [&] {
        vect.set( vector<int>{ 30, 50 } );

        vect.modify( []( vector<int>& v ) { v.push_back( 70 ); } );
    } );

    ASSERT_EQ( obsCount, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_TYPED_TEST_CASE_P( SignalTest,
    MakeVars,
    Signals1,
    Signals2,
    Signals3,
    Signals4,
    FunctionBind1,
    FunctionBind2,
    Flatten1,
    Flatten2,
    Flatten3,
    Flatten4,
    Member1,
    Modify1,
    Modify2,
    Modify3

);

} // namespace

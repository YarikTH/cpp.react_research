
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <queue>
#include <string>
#include <tuple>

#include "gtest/gtest.h"
#include "react/react.hpp"

///////////////////////////////////////////////////////////////////////////////////////////////////
namespace
{

using namespace react;
using namespace std;

template <typename T>
struct Incrementer
{
    T operator()( token, T v ) const
    {
        return v + 1;
    }
};

template <typename T>
struct Decrementer
{
    T operator()( token, T v ) const
    {
        return v - 1;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventStreamTest fixture
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TParams>
class OperationsTest : public testing::Test
{
public:
    REACTIVE_DOMAIN( MyDomain )
};

TYPED_TEST_CASE_P( OperationsTest );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Iterate1 )
{
    using D = typename Iterate1::MyDomain;

    auto numSrc = make_event_source<D, int>();
    auto numFold = iterate( numSrc, 0, []( int d, int v ) { return v + d; } );

    for( auto i = 1; i <= 100; i++ )
    {
        numSrc << i;
    }

    ASSERT_EQ( numFold(), 5050 );

    auto charSrc = make_event_source<D, char>();
    auto strFold = iterate( charSrc, string( "" ), []( char c, string s ) { return s + c; } );

    charSrc << 'T' << 'e' << 's' << 't';

    ASSERT_EQ( strFold(), "Test" );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Iterate2 )
{
    using D = typename Iterate2::MyDomain;

    auto numSrc = make_event_source<D, int>();
    auto numFold = iterate( numSrc, 0, []( int d, int v ) { return v + d; } );

    int c = 0;

    observe( numFold, [&]( int v ) {
        c++;
        ASSERT_EQ( v, 5050 );
    } );

    do_transaction<D>( [&] {
        for( auto i = 1; i <= 100; i++ )
            numSrc << i;
    } );

    ASSERT_EQ( numFold(), 5050 );
    ASSERT_EQ( c, 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate3 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Iterate3 )
{
    using D = typename Iterate3::MyDomain;

    auto trigger = make_event_source<D>();

    {
        auto inc = iterate( trigger, 0, Incrementer<int>{} );
        for( auto i = 1; i <= 100; i++ )
            trigger.emit();

        ASSERT_EQ( inc(), 100 );
    }

    {
        auto dec = iterate( trigger, 100, Decrementer<int>{} );
        for( auto i = 1; i <= 100; i++ )
            trigger.emit();

        ASSERT_EQ( dec(), 0 );
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Monitor1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Monitor1 )
{
    using D = typename Monitor1::MyDomain;

    auto target = make_var<D>( 10 );

    vector<int> results;

    auto filterFunc = []( int v ) { return v > 10; };

    auto obs = observe(
        filter( monitor( target ), filterFunc ), [&]( int v ) { results.push_back( v ); } );

    target <<= 10;
    target <<= 20;
    target <<= 20;
    target <<= 10;

    ASSERT_EQ( results.size(), 1 );
    ASSERT_EQ( results[0], 20 );

    obs.detach();

    target <<= 100;

    ASSERT_EQ( results.size(), 1 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Hold1 )
{
    using D = typename Hold1::MyDomain;

    auto src = make_event_source<D, int>();

    auto h = hold( src, 0 );

    ASSERT_EQ( h(), 0 );

    src << 10;

    ASSERT_EQ( h(), 10 );

    src << 20 << 30;

    ASSERT_EQ( h(), 30 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Pulse1 )
{
    using D = typename Pulse1::MyDomain;

    auto trigger = make_event_source<D>();
    auto target = make_var<D>( 10 );

    vector<int> results;

    auto p = pulse( trigger, target );

    observe( p, [&]( int v ) { results.push_back( v ); } );

    target <<= 10;
    trigger.emit();

    ASSERT_EQ( results[0], 10 );

    target <<= 20;
    trigger.emit();

    ASSERT_EQ( results[1], 20 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, Snapshot1 )
{
    using D = typename Snapshot1::MyDomain;

    auto trigger = make_event_source<D>();
    auto target = make_var<D>( 10 );

    auto snap = snapshot( trigger, target );

    target <<= 10;
    trigger.emit();
    target <<= 20;

    ASSERT_EQ( snap(), 10 );

    target <<= 20;
    trigger.emit();
    target <<= 30;

    ASSERT_EQ( snap(), 20 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRef1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, IterateByRef1 )
{
    using D = typename IterateByRef1::MyDomain;

    auto src = make_event_source<D, int>();
    auto f = iterate(
        src, std::vector<int>(), []( int d, std::vector<int>& v ) { v.push_back( d ); } );

    // Push
    for( auto i = 1; i <= 100; i++ )
        src << i;

    ASSERT_EQ( f().size(), 100 );

    // Check
    for( auto i = 1; i <= 100; i++ )
        ASSERT_EQ( f()[i - 1], i );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRef2 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, IterateByRef2 )
{
    using D = typename IterateByRef2::MyDomain;

    auto src = make_event_source<D>();
    auto x = iterate(
        src, std::vector<int>(), []( token, std::vector<int>& v ) { v.push_back( 123 ); } );

    // Push
    for( auto i = 0; i < 100; i++ )
        src.emit();

    ASSERT_EQ( x().size(), 100 );

    // Check
    for( auto i = 0; i < 100; i++ )
        ASSERT_EQ( x()[i], 123 );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedTransform1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedTransform1 )
{
    using D = typename SyncedTransform1::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto sum = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a + b; } );
    auto prod = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a * b; } );
    auto diff = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a - b; } );

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    auto out1 = transform( src1, with( sum, prod, diff ), []( token, int sum, int prod, int diff ) {
        return make_tuple( sum, prod, diff );
    } );

    auto out2 = transform( src2, with( sum, prod, diff ), []( int e, int sum, int prod, int diff ) {
        return make_tuple( e, sum, prod, diff );
    } );

    int obsCount1 = 0;
    int obsCount2 = 0;

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 33 );
            ASSERT_EQ( get<1>( t ), 242 );
            ASSERT_EQ( get<2>( t ), 11 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 42 );
            ASSERT_EQ( get<1>( t ), 33 );
            ASSERT_EQ( get<2>( t ), 242 );
            ASSERT_EQ( get<3>( t ), 11 );
        } );

        in1 <<= 22;
        in2 <<= 11;

        src1.emit();
        src2.emit( 42 );

        ASSERT_EQ( obsCount1, 1 );
        ASSERT_EQ( obsCount2, 1 );

        obs1.detach();
        obs2.detach();
    }

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 330 );
            ASSERT_EQ( get<1>( t ), 24200 );
            ASSERT_EQ( get<2>( t ), 110 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 420 );
            ASSERT_EQ( get<1>( t ), 330 );
            ASSERT_EQ( get<2>( t ), 24200 );
            ASSERT_EQ( get<3>( t ), 110 );
        } );

        in1 <<= 220;
        in2 <<= 110;

        src1.emit();
        src2.emit( 420 );

        ASSERT_EQ( obsCount1, 2 );
        ASSERT_EQ( obsCount2, 2 );

        obs1.detach();
        obs2.detach();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterate1 test
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedIterate1 )
{
    using D = typename SyncedIterate1::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto op1 = ( in1, in2 )->*summ;
    auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    auto out1 = iterate( src1,
        make_tuple( 0, 0 ),
        with( op1, op2 ),
        []( token, const tuple<int, int>& t, int op1, int op2 ) {
            return make_tuple( get<0>( t ) + op1, get<1>( t ) + op2 );
        } );

    auto out2 = iterate( src2,
        make_tuple( 0, 0, 0 ),
        with( op1, op2 ),
        []( int e, const tuple<int, int, int>& t, int op1, int op2 ) {
            return make_tuple( get<0>( t ) + e, get<1>( t ) + op1, get<2>( t ) + op2 );
        } );

    int obsCount1 = 0;
    int obsCount2 = 0;

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 33 );
            ASSERT_EQ( get<1>( t ), 330 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 42 );
            ASSERT_EQ( get<1>( t ), 33 );
            ASSERT_EQ( get<2>( t ), 330 );
        } );

        in1 <<= 22;
        in2 <<= 11;

        src1.emit();
        src2.emit( 42 );

        ASSERT_EQ( obsCount1, 1 );
        ASSERT_EQ( obsCount2, 1 );

        obs1.detach();
        obs2.detach();
    }

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 33 + 330 );
            ASSERT_EQ( get<1>( t ), 330 + 3300 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 42 + 420 );
            ASSERT_EQ( get<1>( t ), 33 + 330 );
            ASSERT_EQ( get<2>( t ), 330 + 3300 );
        } );

        in1 <<= 220;
        in2 <<= 110;

        src1.emit();
        src2.emit( 420 );

        ASSERT_EQ( obsCount1, 2 );
        ASSERT_EQ( obsCount2, 2 );

        obs1.detach();
        obs2.detach();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterate2 test (by ref)
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedIterate2 )
{
    using D = typename SyncedIterate2::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto op1 = ( in1, in2 )->*summ;
    auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    auto out1 = iterate( src1,
        vector<int>{},
        with( op1, op2 ),
        []( token, vector<int>& v, int op1, int op2 ) -> void {
            v.push_back( op1 );
            v.push_back( op2 );
        } );

    auto out2 = iterate( src2,
        vector<int>{},
        with( op1, op2 ),
        []( int e, vector<int>& v, int op1, int op2 ) -> void {
            v.push_back( e );
            v.push_back( op1 );
            v.push_back( op2 );
        } );

    int obsCount1 = 0;
    int obsCount2 = 0;

    {
        auto obs1 = observe( out1, [&]( const vector<int>& v ) {
            ++obsCount1;

            ASSERT_EQ( v.size(), 2 );

            ASSERT_EQ( v[0], 33 );
            ASSERT_EQ( v[1], 330 );
        } );

        auto obs2 = observe( out2, [&]( const vector<int>& v ) {
            ++obsCount2;

            ASSERT_EQ( v.size(), 3 );

            ASSERT_EQ( v[0], 42 );
            ASSERT_EQ( v[1], 33 );
            ASSERT_EQ( v[2], 330 );
        } );

        in1 <<= 22;
        in2 <<= 11;

        src1.emit();
        src2.emit( 42 );

        ASSERT_EQ( obsCount1, 1 );
        ASSERT_EQ( obsCount2, 1 );

        obs1.detach();
        obs2.detach();
    }

    {
        auto obs1 = observe( out1, [&]( const vector<int>& v ) {
            ++obsCount1;

            ASSERT_EQ( v.size(), 4 );

            ASSERT_EQ( v[0], 33 );
            ASSERT_EQ( v[1], 330 );
            ASSERT_EQ( v[2], 330 );
            ASSERT_EQ( v[3], 3300 );
        } );

        auto obs2 = observe( out2, [&]( const vector<int>& v ) {
            ++obsCount2;

            ASSERT_EQ( v.size(), 6 );

            ASSERT_EQ( v[0], 42 );
            ASSERT_EQ( v[1], 33 );
            ASSERT_EQ( v[2], 330 );

            ASSERT_EQ( v[3], 420 );
            ASSERT_EQ( v[4], 330 );
            ASSERT_EQ( v[5], 3300 );
        } );

        in1 <<= 220;
        in2 <<= 110;

        src1.emit();
        src2.emit( 420 );

        ASSERT_EQ( obsCount1, 2 );
        ASSERT_EQ( obsCount2, 2 );

        obs1.detach();
        obs2.detach();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterate3 test (event range)
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedIterate3 )
{
    using D = typename SyncedIterate3::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto op1 = ( in1, in2 )->*summ;
    auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    auto out1 = iterate( src1,
        make_tuple( 0, 0 ),
        with( op1, op2 ),
        []( event_range<token> range, const tuple<int, int>& t, int op1, int op2 ) {
            return make_tuple(
                get<0>( t ) + ( op1 * range.size() ), get<1>( t ) + ( op2 * range.size() ) );
        } );

    auto out2 = iterate( src2,
        make_tuple( 0, 0, 0 ),
        with( op1, op2 ),
        []( event_range<int> range, const tuple<int, int, int>& t, int op1, int op2 ) {
            int sum = 0;
            for( const auto& e : range )
                sum += e;

            return make_tuple( get<0>( t ) + sum,
                get<1>( t ) + ( op1 * range.size() ),
                get<2>( t ) + ( op2 * range.size() ) );
        } );

    int obsCount1 = 0;
    int obsCount2 = 0;

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 33 );
            ASSERT_EQ( get<1>( t ), 330 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 42 );
            ASSERT_EQ( get<1>( t ), 33 );
            ASSERT_EQ( get<2>( t ), 330 );
        } );

        in1 <<= 22;
        in2 <<= 11;

        src1.emit();
        src2.emit( 42 );

        ASSERT_EQ( obsCount1, 1 );
        ASSERT_EQ( obsCount2, 1 );

        obs1.detach();
        obs2.detach();
    }

    {
        auto obs1 = observe( out1, [&]( const tuple<int, int>& t ) {
            ++obsCount1;

            ASSERT_EQ( get<0>( t ), 33 + 330 );
            ASSERT_EQ( get<1>( t ), 330 + 3300 );
        } );

        auto obs2 = observe( out2, [&]( const tuple<int, int, int>& t ) {
            ++obsCount2;

            ASSERT_EQ( get<0>( t ), 42 + 420 );
            ASSERT_EQ( get<1>( t ), 33 + 330 );
            ASSERT_EQ( get<2>( t ), 330 + 3300 );
        } );

        in1 <<= 220;
        in2 <<= 110;

        src1.emit();
        src2.emit( 420 );

        ASSERT_EQ( obsCount1, 2 );
        ASSERT_EQ( obsCount2, 2 );

        obs1.detach();
        obs2.detach();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterate4 test (event range, by ref)
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedIterate4 )
{
    using D = typename SyncedIterate4::MyDomain;

    auto in1 = make_var<D>( 1 );
    auto in2 = make_var<D>( 1 );

    auto summ = []( int a, int b ) { return a + b; };

    auto op1 = ( in1, in2 )->*summ;
    auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };

    auto src1 = make_event_source<D>();
    auto src2 = make_event_source<D, int>();

    auto out1 = iterate( src1,
        vector<int>{},
        with( op1, op2 ),
        []( event_range<token> range, vector<int>& v, int op1, int op2 ) -> void {
            for( const auto& e : range )
            {
                (void)e;
                v.push_back( op1 );
                v.push_back( op2 );
            }
        } );

    auto out2 = iterate( src2,
        vector<int>{},
        with( op1, op2 ),
        []( event_range<int> range, vector<int>& v, int op1, int op2 ) -> void {
            for( const auto& e : range )
            {
                v.push_back( e );
                v.push_back( op1 );
                v.push_back( op2 );
            }
        } );

    int obsCount1 = 0;
    int obsCount2 = 0;

    {
        auto obs1 = observe( out1, [&]( const vector<int>& v ) {
            ++obsCount1;

            ASSERT_EQ( v.size(), 2 );

            ASSERT_EQ( v[0], 33 );
            ASSERT_EQ( v[1], 330 );
        } );

        auto obs2 = observe( out2, [&]( const vector<int>& v ) {
            ++obsCount2;

            ASSERT_EQ( v.size(), 3 );

            ASSERT_EQ( v[0], 42 );
            ASSERT_EQ( v[1], 33 );
            ASSERT_EQ( v[2], 330 );
        } );

        in1 <<= 22;
        in2 <<= 11;

        src1.emit();
        src2.emit( 42 );

        ASSERT_EQ( obsCount1, 1 );
        ASSERT_EQ( obsCount2, 1 );

        obs1.detach();
        obs2.detach();
    }

    {
        auto obs1 = observe( out1, [&]( const vector<int>& v ) {
            ++obsCount1;

            ASSERT_EQ( v.size(), 4 );

            ASSERT_EQ( v[0], 33 );
            ASSERT_EQ( v[1], 330 );
            ASSERT_EQ( v[2], 330 );
            ASSERT_EQ( v[3], 3300 );
        } );

        auto obs2 = observe( out2, [&]( const vector<int>& v ) {
            ++obsCount2;

            ASSERT_EQ( v.size(), 6 );

            ASSERT_EQ( v[0], 42 );
            ASSERT_EQ( v[1], 33 );
            ASSERT_EQ( v[2], 330 );

            ASSERT_EQ( v[3], 420 );
            ASSERT_EQ( v[4], 330 );
            ASSERT_EQ( v[5], 3300 );
        } );

        in1 <<= 220;
        in2 <<= 110;

        src1.emit();
        src2.emit( 420 );

        ASSERT_EQ( obsCount1, 2 );
        ASSERT_EQ( obsCount2, 2 );

        obs1.detach();
        obs2.detach();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventFilter1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedEventFilter1 )
{
    using D = typename SyncedEventFilter1::MyDomain;

    using std::string;

    std::queue<string> results;

    auto in = make_event_source<D, string>();

    auto sig1 = make_var<D>( 1338 );
    auto sig2 = make_var<D>( 1336 );

    auto filtered = filter( in, with( sig1, sig2 ), []( const string& s, int sig1, int sig2 ) {
        return s == "Hello World" && sig1 > sig2;
    } );


    observe( filtered, [&]( const string& s ) { results.push( s ); } );

    in << string( "Hello Worlt" ) << string( "Hello World" ) << string( "Hello Vorld" );
    sig1 <<= 1335;
    in << string( "Hello Vorld" );

    ASSERT_FALSE( results.empty() );
    ASSERT_EQ( results.front(), "Hello World" );
    results.pop();

    ASSERT_TRUE( results.empty() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventTransform1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedEventTransform1 )
{
    using D = typename SyncedEventTransform1::MyDomain;

    using std::string;

    std::vector<string> results;

    auto in1 = make_event_source<D, string>();
    auto in2 = make_event_source<D, string>();

    auto merged = merge( in1, in2 );

    auto first = make_var<D>( string( "Ace" ) );
    auto last = make_var<D>( string( "McSteele" ) );

    auto transformed = transform( merged,
        with( first, last ),
        []( string s, const string& first, const string& last ) -> string {
            std::transform( s.begin(), s.end(), s.begin(), ::toupper );
            s += string( ", " ) + first + string( " " ) + last;
            return s;
        } );

    observe( transformed, [&]( const string& s ) { results.push_back( s ); } );

    in1 << string( "Hello Worlt" ) << string( "Hello World" );

    do_transaction<D>( [&] {
        in2 << string( "Hello Vorld" );
        first.Set( string( "Alice" ) );
        last.Set( string( "Anderson" ) );
    } );

    ASSERT_EQ( results.size(), 3 );
    ASSERT_TRUE(
        std::find( results.begin(), results.end(), "HELLO WORLT, Ace McSteele" ) != results.end() );
    ASSERT_TRUE(
        std::find( results.begin(), results.end(), "HELLO WORLD, Ace McSteele" ) != results.end() );
    ASSERT_TRUE( std::find( results.begin(), results.end(), "HELLO VORLD, Alice Anderson" )
                 != results.end() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcess1
///////////////////////////////////////////////////////////////////////////////////////////////////
TYPED_TEST_P( OperationsTest, SyncedEventProcess1 )
{
    using D = typename SyncedEventProcess1::MyDomain;

    std::vector<float> results;

    auto in1 = make_event_source<D, int>();
    auto in2 = make_event_source<D, int>();

    auto mult = make_var<D>( 10 );

    auto merged = merge( in1, in2 );
    int callCount = 0;

    auto processed = process<float>(
        merged, with( mult ), [&]( event_range<int> range, event_emitter<float> out, int mult ) {
            for( const auto& e : range )
            {
                *out = 0.1f * e * mult;
                *out = 1.5f * e * mult;
            }

            callCount++;
        } );

    observe( processed, [&]( float s ) { results.push_back( s ); } );

    do_transaction<D>( [&] { in1 << 10 << 20; } );

    in2 << 30;

    ASSERT_EQ( results.size(), 6 );
    ASSERT_EQ( callCount, 2 );

    ASSERT_EQ( results[0], 10.0f );
    ASSERT_EQ( results[1], 150.0f );
    ASSERT_EQ( results[2], 20.0f );
    ASSERT_EQ( results[3], 300.0f );
    ASSERT_EQ( results[4], 30.0f );
    ASSERT_EQ( results[5], 450.0f );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
REGISTER_TYPED_TEST_CASE_P( OperationsTest,
    Iterate1,
    Iterate2,
    Iterate3,
    Monitor1,
    Hold1,
    Pulse1,
    Snapshot1,
    IterateByRef1,
    IterateByRef2,
    SyncedTransform1,
    SyncedIterate1,
    SyncedIterate2,
    SyncedIterate3,
    SyncedIterate4,
    SyncedEventFilter1,
    SyncedEventTransform1,
    SyncedEventProcess1 );

} // namespace

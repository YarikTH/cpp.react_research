#include "tests_stdafx.hpp"
#include "react/react.hpp"
#include <queue>

namespace
{
    using namespace react;
    
    REACTIVE_DOMAIN( D )
    
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
}

TEST_SUITE( "OperationsTest" )
{
    TEST_CASE( "Iterate1" )
    {
        auto numSrc = make_event_source<D, int>();
        auto numFold = iterate( numSrc, 0, []( int d, int v ) { return v + d; } );
    
        for( auto i = 1; i <= 100; i++ )
        {
            numSrc << i;
        }
    
        CHECK_EQ( numFold(), 5050 );
    
        auto charSrc = make_event_source<D, char>();
        auto strFold = iterate( charSrc, std::string( "" ), []( char c, const std::string& s ) { return s + c; } );
    
        charSrc << 'T' << 'e' << 's' << 't';
    
        CHECK_EQ( strFold(), "Test" );
    }
    
    TEST_CASE( "Iterate2" )
    {
        auto numSrc = make_event_source<D, int>();
        auto numFold = iterate( numSrc, 0, []( int d, int v ) { return v + d; } );
    
        int c = 0;
    
        observe( numFold, [&]( int v ) {
            c++;
            CHECK_EQ( v, 5050 );
        } );
    
        do_transaction<D>( [&] {
            for( auto i = 1; i <= 100; i++ )
                numSrc << i;
        } );
    
        CHECK_EQ( numFold(), 5050 );
        CHECK_EQ( c, 1 );
    }
    
    TEST_CASE( "Iterate3" )
    {
        auto trigger = make_event_source<D>();
    
        {
            auto inc = iterate( trigger, 0, Incrementer<int>{} );
            for( auto i = 1; i <= 100; i++ )
                trigger.emit();
    
            CHECK_EQ( inc(), 100 );
        }
    
        {
            auto dec = iterate( trigger, 100, Decrementer<int>{} );
            for( auto i = 1; i <= 100; i++ )
                trigger.emit();
    
            CHECK_EQ( dec(), 0 );
        }
    }
    
    TEST_CASE( "Monitor1" )
    {
        auto target = make_var<D>( 10 );
    
        std::vector<int> results;
    
        auto filterFunc = []( int v ) { return v > 10; };
    
        auto obs = observe(
            filter( monitor( target ), filterFunc ), [&]( int v ) { results.push_back( v ); } );
    
        target <<= 10;
        target <<= 20;
        target <<= 20;
        target <<= 10;
    
        CHECK_EQ( results.size(), 1 );
        CHECK_EQ( results[0], 20 );
    
        obs.detach();
    
        target <<= 100;
    
        CHECK_EQ( results.size(), 1 );
    }
    
    TEST_CASE( "Hold1" )
    {
        auto src = make_event_source<D, int>();
    
        auto h = hold( src, 0 );
    
        CHECK_EQ( h(), 0 );
    
        src << 10;
    
        CHECK_EQ( h(), 10 );
    
        src << 20 << 30;
    
        CHECK_EQ( h(), 30 );
    }
    
    TEST_CASE( "Pulse1" )
    {
        auto trigger = make_event_source<D>();
        auto target = make_var<D>( 10 );
    
        std::vector<int> results;
    
        auto p = pulse( trigger, target );
    
        observe( p, [&]( int v ) { results.push_back( v ); } );
    
        target <<= 10;
        trigger.emit();
    
        CHECK_EQ( results[0], 10 );
    
        target <<= 20;
        trigger.emit();
    
        CHECK_EQ( results[1], 20 );
    }
    
    TEST_CASE( "Snapshot1" )
    {
        auto trigger = make_event_source<D>();
        auto target = make_var<D>( 10 );
    
        auto snap = snapshot( trigger, target );
    
        target <<= 10;
        trigger.emit();
        target <<= 20;
    
        CHECK_EQ( snap(), 10 );
    
        target <<= 20;
        trigger.emit();
        target <<= 30;
    
        CHECK_EQ( snap(), 20 );
    }
    
    TEST_CASE( "IterateByRef1" )
    {
        auto src = make_event_source<D, int>();
        auto f = iterate(
            src, std::vector<int>(), []( int d, std::vector<int>& v ) { v.push_back( d ); } );
    
        // Push
        for( auto i = 1; i <= 100; i++ )
            src << i;
    
        CHECK_EQ( f().size(), 100 );
    
        // Check
        for( auto i = 1; i <= 100; i++ )
            CHECK_EQ( f()[i - 1], i );
    }
    
    TEST_CASE( "IterateByRef2" )
    {
        auto src = make_event_source<D>();
        auto x = iterate(
            src, std::vector<int>(), []( token, std::vector<int>& v ) { v.push_back( 123 ); } );
    
        // Push
        for( auto i = 0; i < 100; i++ )
            src.emit();
    
        CHECK_EQ( x().size(), 100 );
    
        // Check
        for( auto i = 0; i < 100; i++ )
            CHECK_EQ( x()[i], 123 );
    }
    
    TEST_CASE( "SyncedTransform1" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto sum = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a + b; } );
        auto prod = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a * b; } );
        auto diff = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a - b; } );
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        auto out1 = transform( src1, with( sum, prod, diff ), []( token, int sum, int prod, int diff ) {
            return std::make_tuple( sum, prod, diff );
        } );
    
        auto out2 = transform( src2, with( sum, prod, diff ), []( int e, int sum, int prod, int diff ) {
            return std::make_tuple( e, sum, prod, diff );
        } );
    
        int obsCount1 = 0;
        int obsCount2 = 0;
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 33 );
                CHECK_EQ( std::get<1>( t ), 242 );
                CHECK_EQ( std::get<2>( t ), 11 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 42 );
                CHECK_EQ( std::get<1>( t ), 33 );
                CHECK_EQ( std::get<2>( t ), 242 );
                CHECK_EQ( std::get<3>( t ), 11 );
            } );
    
            in1 <<= 22;
            in2 <<= 11;
    
            src1.emit();
            src2.emit( 42 );
    
            CHECK_EQ( obsCount1, 1 );
            CHECK_EQ( obsCount2, 1 );
    
            obs1.detach();
            obs2.detach();
        }
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 330 );
                CHECK_EQ( std::get<1>( t ), 24200 );
                CHECK_EQ( std::get<2>( t ), 110 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 420 );
                CHECK_EQ( std::get<1>( t ), 330 );
                CHECK_EQ( std::get<2>( t ), 24200 );
                CHECK_EQ( std::get<3>( t ), 110 );
            } );
    
            in1 <<= 220;
            in2 <<= 110;
    
            src1.emit();
            src2.emit( 420 );
    
            CHECK_EQ( obsCount1, 2 );
            CHECK_EQ( obsCount2, 2 );
    
            obs1.detach();
            obs2.detach();
        }
    }
    
    TEST_CASE( "SyncedIterate1" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto summ = []( int a, int b ) { return a + b; };
    
        auto op1 = ( in1, in2 )->*summ;
        auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        auto out1 = iterate( src1,
            std::make_tuple( 0, 0 ),
            with( op1, op2 ),
            []( token, const std::tuple<int, int>& t, int op1, int op2 ) {
                return std::make_tuple( std::get<0>( t ) + op1, std::get<1>( t ) + op2 );
            } );
    
        auto out2 = iterate( src2,
            std::make_tuple( 0, 0, 0 ),
            with( op1, op2 ),
            []( int e, const std::tuple<int, int, int>& t, int op1, int op2 ) {
                return std::make_tuple( std::get<0>( t ) + e, std::get<1>( t ) + op1, std::get<2>( t ) + op2 );
            } );
    
        int obsCount1 = 0;
        int obsCount2 = 0;
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 33 );
                CHECK_EQ( std::get<1>( t ), 330 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 42 );
                CHECK_EQ( std::get<1>( t ), 33 );
                CHECK_EQ( std::get<2>( t ), 330 );
            } );
    
            in1 <<= 22;
            in2 <<= 11;
    
            src1.emit();
            src2.emit( 42 );
    
            CHECK_EQ( obsCount1, 1 );
            CHECK_EQ( obsCount2, 1 );
    
            obs1.detach();
            obs2.detach();
        }
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 33 + 330 );
                CHECK_EQ( std::get<1>( t ), 330 + 3300 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 42 + 420 );
                CHECK_EQ( std::get<1>( t ), 33 + 330 );
                CHECK_EQ( std::get<2>( t ), 330 + 3300 );
            } );
    
            in1 <<= 220;
            in2 <<= 110;
    
            src1.emit();
            src2.emit( 420 );
    
            CHECK_EQ( obsCount1, 2 );
            CHECK_EQ( obsCount2, 2 );
    
            obs1.detach();
            obs2.detach();
        }
    }
    
    TEST_CASE( "SyncedIterate2" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto summ = []( int a, int b ) { return a + b; };
    
        auto op1 = ( in1, in2 )->*summ;
        auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        auto out1 = iterate( src1,
            std::vector<int>{},
            with( op1, op2 ),
            []( token, std::vector<int>& v, int op1, int op2 ) -> void {
                v.push_back( op1 );
                v.push_back( op2 );
            } );
    
        auto out2 = iterate( src2,
            std::vector<int>{},
            with( op1, op2 ),
            []( int e, std::vector<int>& v, int op1, int op2 ) -> void {
                v.push_back( e );
                v.push_back( op1 );
                v.push_back( op2 );
            } );
    
        int obsCount1 = 0;
        int obsCount2 = 0;
    
        {
            auto obs1 = observe( out1, [&]( const std::vector<int>& v ) {
                ++obsCount1;
    
                CHECK_EQ( v.size(), 2 );
    
                CHECK_EQ( v[0], 33 );
                CHECK_EQ( v[1], 330 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::vector<int>& v ) {
                ++obsCount2;
    
                CHECK_EQ( v.size(), 3 );
    
                CHECK_EQ( v[0], 42 );
                CHECK_EQ( v[1], 33 );
                CHECK_EQ( v[2], 330 );
            } );
    
            in1 <<= 22;
            in2 <<= 11;
    
            src1.emit();
            src2.emit( 42 );
    
            CHECK_EQ( obsCount1, 1 );
            CHECK_EQ( obsCount2, 1 );
    
            obs1.detach();
            obs2.detach();
        }
    
        {
            auto obs1 = observe( out1, [&]( const std::vector<int>& v ) {
                ++obsCount1;
    
                CHECK_EQ( v.size(), 4 );
    
                CHECK_EQ( v[0], 33 );
                CHECK_EQ( v[1], 330 );
                CHECK_EQ( v[2], 330 );
                CHECK_EQ( v[3], 3300 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::vector<int>& v ) {
                ++obsCount2;
    
                CHECK_EQ( v.size(), 6 );
    
                CHECK_EQ( v[0], 42 );
                CHECK_EQ( v[1], 33 );
                CHECK_EQ( v[2], 330 );
    
                CHECK_EQ( v[3], 420 );
                CHECK_EQ( v[4], 330 );
                CHECK_EQ( v[5], 3300 );
            } );
    
            in1 <<= 220;
            in2 <<= 110;
    
            src1.emit();
            src2.emit( 420 );
    
            CHECK_EQ( obsCount1, 2 );
            CHECK_EQ( obsCount2, 2 );
    
            obs1.detach();
            obs2.detach();
        }
    }
    
    TEST_CASE( "SyncedIterate3" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto summ = []( int a, int b ) { return a + b; };
    
        auto op1 = ( in1, in2 )->*summ;
        auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        auto out1 = iterate( src1,
            std::make_tuple( 0, 0 ),
            with( op1, op2 ),
            []( event_range<token> range, const std::tuple<int, int>& t, int op1, int op2 ) {
                return std::make_tuple(
                    std::get<0>( t ) + ( op1 * range.size() ), std::get<1>( t ) + ( op2 * range.size() ) );
            } );
    
        auto out2 = iterate( src2,
            std::make_tuple( 0, 0, 0 ),
            with( op1, op2 ),
            []( event_range<int> range, const std::tuple<int, int, int>& t, int op1, int op2 ) {
                int sum = 0;
                for( const auto& e : range )
                    sum += e;
    
                return std::make_tuple( std::get<0>( t ) + sum,
                    std::get<1>( t ) + ( op1 * range.size() ),
                    std::get<2>( t ) + ( op2 * range.size() ) );
            } );
    
        int obsCount1 = 0;
        int obsCount2 = 0;
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 33 );
                CHECK_EQ( std::get<1>( t ), 330 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 42 );
                CHECK_EQ( std::get<1>( t ), 33 );
                CHECK_EQ( std::get<2>( t ), 330 );
            } );
    
            in1 <<= 22;
            in2 <<= 11;
    
            src1.emit();
            src2.emit( 42 );
    
            CHECK_EQ( obsCount1, 1 );
            CHECK_EQ( obsCount2, 1 );
    
            obs1.detach();
            obs2.detach();
        }
    
        {
            auto obs1 = observe( out1, [&]( const std::tuple<int, int>& t ) {
                ++obsCount1;
    
                CHECK_EQ( std::get<0>( t ), 33 + 330 );
                CHECK_EQ( std::get<1>( t ), 330 + 3300 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::tuple<int, int, int>& t ) {
                ++obsCount2;
    
                CHECK_EQ( std::get<0>( t ), 42 + 420 );
                CHECK_EQ( std::get<1>( t ), 33 + 330 );
                CHECK_EQ( std::get<2>( t ), 330 + 3300 );
            } );
    
            in1 <<= 220;
            in2 <<= 110;
    
            src1.emit();
            src2.emit( 420 );
    
            CHECK_EQ( obsCount1, 2 );
            CHECK_EQ( obsCount2, 2 );
    
            obs1.detach();
            obs2.detach();
        }
    }
    
    TEST_CASE( "SyncedIterate4" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto summ = []( int a, int b ) { return a + b; };
    
        auto op1 = ( in1, in2 )->*summ;
        auto op2 = ( ( in1, in2 )->*summ )->*[]( int a ) { return a * 10; };
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        auto out1 = iterate( src1,
            std::vector<int>{},
            with( op1, op2 ),
            []( event_range<token> range, std::vector<int>& v, int op1, int op2 ) -> void {
                for( const auto& e : range )
                {
                    (void)e;
                    v.push_back( op1 );
                    v.push_back( op2 );
                }
            } );
    
        auto out2 = iterate( src2,
            std::vector<int>{},
            with( op1, op2 ),
            []( event_range<int> range, std::vector<int>& v, int op1, int op2 ) -> void {
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
            auto obs1 = observe( out1, [&]( const std::vector<int>& v ) {
                ++obsCount1;
    
                CHECK_EQ( v.size(), 2 );
    
                CHECK_EQ( v[0], 33 );
                CHECK_EQ( v[1], 330 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::vector<int>& v ) {
                ++obsCount2;
    
                CHECK_EQ( v.size(), 3 );
    
                CHECK_EQ( v[0], 42 );
                CHECK_EQ( v[1], 33 );
                CHECK_EQ( v[2], 330 );
            } );
    
            in1 <<= 22;
            in2 <<= 11;
    
            src1.emit();
            src2.emit( 42 );
    
            CHECK_EQ( obsCount1, 1 );
            CHECK_EQ( obsCount2, 1 );
    
            obs1.detach();
            obs2.detach();
        }
    
        {
            auto obs1 = observe( out1, [&]( const std::vector<int>& v ) {
                ++obsCount1;
    
                CHECK_EQ( v.size(), 4 );
    
                CHECK_EQ( v[0], 33 );
                CHECK_EQ( v[1], 330 );
                CHECK_EQ( v[2], 330 );
                CHECK_EQ( v[3], 3300 );
            } );
    
            auto obs2 = observe( out2, [&]( const std::vector<int>& v ) {
                ++obsCount2;
    
                CHECK_EQ( v.size(), 6 );
    
                CHECK_EQ( v[0], 42 );
                CHECK_EQ( v[1], 33 );
                CHECK_EQ( v[2], 330 );
    
                CHECK_EQ( v[3], 420 );
                CHECK_EQ( v[4], 330 );
                CHECK_EQ( v[5], 3300 );
            } );
    
            in1 <<= 220;
            in2 <<= 110;
    
            src1.emit();
            src2.emit( 420 );
    
            CHECK_EQ( obsCount1, 2 );
            CHECK_EQ( obsCount2, 2 );
    
            obs1.detach();
            obs2.detach();
        }
    }
    
    TEST_CASE( "SyncedEventFilter1" )
    {
        std::queue<std::string> results;
    
        auto in = make_event_source<D, std::string>();
    
        auto sig1 = make_var<D>( 1338 );
        auto sig2 = make_var<D>( 1336 );
    
        auto filtered = filter( in, with( sig1, sig2 ), []( const std::string& s, int sig1, int sig2 ) {
            return s == "Hello World" && sig1 > sig2;
        } );
    
    
        observe( filtered, [&]( const std::string& s ) { results.push( s ); } );
    
        in << std::string( "Hello Worlt" ) << std::string( "Hello World" ) << std::string( "Hello Vorld" );
        sig1 <<= 1335;
        in << std::string( "Hello Vorld" );
    
        CHECK_FALSE( results.empty() );
        CHECK_EQ( results.front(), "Hello World" );
        results.pop();
    
        CHECK( results.empty() );
    }
    
    TEST_CASE( "SyncedEventTransform1" )
    {
        std::vector<std::string> results;
    
        auto in1 = make_event_source<D, std::string>();
        auto in2 = make_event_source<D, std::string>();
    
        auto merged = merge( in1, in2 );
    
        auto first = make_var<D>( std::string( "Ace" ) );
        auto last = make_var<D>( std::string( "McSteele" ) );
    
        auto transformed = transform( merged,
            with( first, last ),
            []( std::string s, const std::string& first, const std::string& last ) -> std::string {
                std::transform( s.begin(), s.end(), s.begin(), ::toupper );
                s += std::string( ", " ) + first + std::string( " " ) + last;
                return s;
            } );
    
        observe( transformed, [&]( const std::string& s ) { results.push_back( s ); } );
    
        in1 << std::string( "Hello Worlt" ) << std::string( "Hello World" );
    
        do_transaction<D>( [&] {
            in2 << std::string( "Hello Vorld" );
            first.set( std::string( "Alice" ) );
            last.set( std::string( "Anderson" ) );
        } );
    
        CHECK_EQ( results.size(), 3 );
        CHECK(
            std::find( results.begin(), results.end(), "HELLO WORLT, Ace McSteele" ) != results.end() );
        CHECK(
            std::find( results.begin(), results.end(), "HELLO WORLD, Ace McSteele" ) != results.end() );
        CHECK( std::find( results.begin(), results.end(), "HELLO VORLD, Alice Anderson" )
                     != results.end() );
    }
    
    TEST_CASE( "SyncedEventProcess1" )
    {
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
    
        CHECK_EQ( results.size(), 6 );
        CHECK_EQ( callCount, 2 );
    
        CHECK_EQ( results[0], 10.0f );
        CHECK_EQ( results[1], 150.0f );
        CHECK_EQ( results[2], 20.0f );
        CHECK_EQ( results[3], 300.0f );
        CHECK_EQ( results[4], 30.0f );
        CHECK_EQ( results[5], 450.0f );
    }
}

#include "tests_stdafx.hpp"
#include "react/react.hpp"

namespace 
{
    using namespace react;
    
    REACTIVE_DOMAIN( D )
}

TEST_SUITE( "OperationsTest" )
{
    TEST_CASE( "detach" )
    {
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
                CHECK_EQ( v, 3 );
            else if( phase == 1 )
                CHECK_EQ( v, 4 );
            else
                CHECK( false );
        } );
    
        auto obs2 = observe( result, [&]( int v ) {
            observeCount2++;
    
            if( phase == 0 )
                CHECK_EQ( v, 3 );
            else if( phase == 1 )
                CHECK_EQ( v, 4 );
            else
                CHECK( false );
        } );
    
        auto obs3 = observe( result, [&]( int v ) {
            observeCount3++;
    
            if( phase == 0 )
                CHECK_EQ( v, 3 );
            else if( phase == 1 )
                CHECK_EQ( v, 4 );
            else
                CHECK( false );
        } );
    
        phase = 0;
        a1 <<= 2;
        CHECK_EQ( observeCount1, 1 );
        CHECK_EQ( observeCount2, 1 );
        CHECK_EQ( observeCount3, 1 );
    
        phase = 1;
        obs1.detach();
        a1 <<= 3;
        CHECK_EQ( observeCount1, 1 );
        CHECK_EQ( observeCount2, 2 );
        CHECK_EQ( observeCount3, 2 );
    
        phase = 2;
        obs2.detach();
        obs3.detach();
        a1 <<= 4;
        CHECK_EQ( observeCount1, 1 );
        CHECK_EQ( observeCount2, 2 );
        CHECK_EQ( observeCount3, 2 );
    }
    
    TEST_CASE( "ScopedObserverTest" )
    {
        std::vector<int> results;
    
        auto in = make_var<D>( 1 );
    
        {
            scoped_observer<D> obs = observe( in, [&]( int v ) { results.push_back( v ); } );
    
            in <<= 2;
        }
    
        in <<= 3;
    
        CHECK_EQ( results.size(), 1 );
        CHECK_EQ( results[0], 2 );
    }
    
    TEST_CASE( "SyncedObserveTest" )
    {
        auto in1 = make_var<D>( 1 );
        auto in2 = make_var<D>( 1 );
    
        auto sum = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a + b; } );
        auto prod = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a * b; } );
        auto diff = make_signal<D>( ( in1, in2 ), []( int a, int b ) { return a - b; } );
    
        auto src1 = make_event_source<D>();
        auto src2 = make_event_source<D, int>();
    
        observe( src1, with( sum, prod, diff ), []( token, int sum, int prod, int diff ) {
            CHECK_EQ( sum, 33 );
            CHECK_EQ( prod, 242 );
            CHECK_EQ( diff, 11 );
        } );
    
        observe( src2, with( sum, prod, diff ), []( int e, int sum, int prod, int diff ) {
            CHECK_EQ( e, 42 );
            CHECK_EQ( sum, 33 );
            CHECK_EQ( prod, 242 );
            CHECK_EQ( diff, 11 );
        } );
    
        in1 <<= 22;
        in2 <<= 11;
    
        src1.emit();
        src2.emit( 42 );
    }
    
    TEST_CASE( "DetachThisObserver1" )
    {
        auto src = make_event_source<D>();
    
        int count = 0;
    
        observe( src, [&]( token ) -> observer_action {
            ++count;
            return observer_action::stop_and_detach;
        } );
    
        src.emit();
        src.emit();
        
        CHECK_EQ( count, 1 );
    }
    
    TEST_CASE( "DetachThisObserver2" )
    {
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
                if( count == 1 )
                {
                    return observer_action::next;
                }
                return observer_action::stop_and_detach;
            } );
    
        in1 <<= 22;
        in2 <<= 11;
    
        src.emit();
        src.emit();
        src.emit();
        src.emit();
    
        CHECK_EQ( count, 2 );
    }
}

#include <queue>

#include "react/react.hpp"
#include "tests_stdafx.hpp"

namespace
{
using namespace react;

REACTIVE_DOMAIN( D )

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

class Company
{
public:
    int index;
    var_signal<D, std::string> name;

    Company( context& ctx, const int index, const char* name )
        : index( index )
        , name( make_var<D>( ctx, std::string( name ) ) )
    {}

    friend bool operator==( const Company& lhs, const Company& rhs )
    {
        // clang-format off
            return std::tie( lhs.index, lhs.name.value() )
                == std::tie( rhs.index, rhs.name.value() );
        // clang-format on
    }
};

std::ostream& operator<<( std::ostream& os, const Company& company )
{
    os << "Company{ index: " << company.index << ", name: \"" << company.name.value() << "\" }";
    return os;
}

class Employee
{
public:
    var_signal<D, Company&> company;

    Employee( context& ctx, Company& company )
        : company( make_var<D>( ctx, std::ref( company ) ) )
    {}
};
} // namespace

TEST_SUITE( "SignalTest" )
{
    TEST_CASE( "MakeVars" )
    {
        context ctx;

        auto v1 = make_var<D>( ctx, 1 );
        auto v2 = make_var<D>( ctx, 2 );
        auto v3 = make_var<D>( ctx, 3 );
        auto v4 = make_var<D>( ctx, 4 );

        CHECK_EQ( v1.value(), 1 );
        CHECK_EQ( v2.value(), 2 );
        CHECK_EQ( v3.value(), 3 );
        CHECK_EQ( v4.value(), 4 );

        v1 <<= 10;
        v2 <<= 20;
        v3 <<= 30;
        v4 <<= 40;

        CHECK_EQ( v1.value(), 10 );
        CHECK_EQ( v2.value(), 20 );
        CHECK_EQ( v3.value(), 30 );
        CHECK_EQ( v4.value(), 40 );
    }

    TEST_CASE( "Signals1" )
    {
        auto summ = []( int a, int b ) { return a + b; };

        context ctx;

        auto v1 = make_var<D>( ctx, 1 );
        auto v2 = make_var<D>( ctx, 2 );
        auto v3 = make_var<D>( ctx, 3 );
        auto v4 = make_var<D>( ctx, 4 );

        auto s1 = make_signal( with( v1, v2 ), []( int a, int b ) { return a + b; } );

        auto s2 = make_signal( with( v3, v4 ), []( int a, int b ) { return a + b; } );

        auto s3 = ( s1, s2 )->*summ;

        CHECK_EQ( s1.value(), 3 );
        CHECK_EQ( s2.value(), 7 );
        CHECK_EQ( s3.value(), 10 );

        v1 <<= 10;
        v2 <<= 20;
        v3 <<= 30;
        v4 <<= 40;

        CHECK_EQ( s1.value(), 30 );
        CHECK_EQ( s2.value(), 70 );
        CHECK_EQ( s3.value(), 100 );

        bool b = false;

        b = is_signal<decltype( v1 )>::value;
        CHECK( b );

        b = is_signal<decltype( s1 )>::value;
        CHECK( b );

        b = is_signal<decltype( s2 )>::value;
        CHECK( b );

        b = is_signal<decltype( 10 )>::value;
        CHECK_FALSE( b );
    }

    TEST_CASE( "Signals2" )
    {
        context ctx;

        auto a1 = make_var<D>( ctx, 1 );
        auto a2 = make_var<D>( ctx, 1 );

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
                CHECK_EQ( v, 9 );
            else
                CHECK_EQ( v, 12 );
        } );

        CHECK_EQ( a1(), 1 );
        CHECK_EQ( a2(), 1 );

        CHECK_EQ( b1(), 1 );
        CHECK_EQ( b2(), 2 );
        CHECK_EQ( b3(), 1 );

        CHECK_EQ( c1(), 3 );
        CHECK_EQ( c2(), 3 );

        CHECK_EQ( result(), 6 );

        a1 <<= 2;

        CHECK_EQ( observeCount, 1 );

        CHECK_EQ( a1(), 2 );
        CHECK_EQ( a2(), 1 );

        CHECK_EQ( b1(), 2 );
        CHECK_EQ( b2(), 3 );
        CHECK_EQ( b3(), 1 );

        CHECK_EQ( c1(), 5 );
        CHECK_EQ( c2(), 4 );

        CHECK_EQ( result(), 9 );

        a2 <<= 2;

        CHECK_EQ( observeCount, 2 );

        CHECK_EQ( a1(), 2 );
        CHECK_EQ( a2(), 2 );

        CHECK_EQ( b1(), 2 );
        CHECK_EQ( b2(), 4 );
        CHECK_EQ( b3(), 2 );

        CHECK_EQ( c1(), 6 );
        CHECK_EQ( c2(), 6 );

        CHECK_EQ( result(), 12 );
    }

    TEST_CASE( "Signals3" )
    {
        context ctx;

        auto a1 = make_var<D>( ctx, 1 );
        auto a2 = make_var<D>( ctx, 1 );

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
            CHECK_EQ( v, 12 );
        } );

        CHECK_EQ( a1(), 1 );
        CHECK_EQ( a2(), 1 );

        CHECK_EQ( b1(), 1 );
        CHECK_EQ( b2(), 2 );
        CHECK_EQ( b3(), 1 );

        CHECK_EQ( c1(), 3 );
        CHECK_EQ( c2(), 3 );

        CHECK_EQ( result(), 6 );

        do_transaction<D>( [&] {
            a1 <<= 2;
            a2 <<= 2;
        } );

        CHECK_EQ( observeCount, 1 );

        CHECK_EQ( a1(), 2 );
        CHECK_EQ( a2(), 2 );

        CHECK_EQ( b1(), 2 );
        CHECK_EQ( b2(), 4 );
        CHECK_EQ( b3(), 2 );

        CHECK_EQ( c1(), 6 );
        CHECK_EQ( c2(), 6 );

        CHECK_EQ( result(), 12 );
    }

    TEST_CASE( "Signals4" )
    {
        context ctx;

        auto a1 = make_var<D>( ctx, 1 );
        auto a2 = make_var<D>( ctx, 1 );

        auto summ = []( int a, int b ) { return a + b; };

        auto b1 = ( a1, a2 )->*summ;
        auto b2 = ( b1, a2 )->*summ;

        CHECK_EQ( a1(), 1 );
        CHECK_EQ( a2(), 1 );

        CHECK_EQ( b1(), 2 );
        CHECK_EQ( b2(), 3 );

        a1 <<= 10;

        CHECK_EQ( a1(), 10 );
        CHECK_EQ( a2(), 1 );

        CHECK_EQ( b1(), 11 );
        CHECK_EQ( b2(), 12 );
    }

    TEST_CASE( "FunctionBind1" )
    {
        context ctx;

        auto v1 = make_var<D>( ctx, 2 );
        auto v2 = make_var<D>( ctx, 30 );
        auto v3 = make_var<D>( ctx, 10 );

        auto signal = ( v1, v2, v3 )->*[=]( int a, int b, int c ) -> int { return a * b * c; };

        CHECK_EQ( signal(), 600 );
        v3 <<= 100;
        CHECK_EQ( signal(), 6000 );
    }

    TEST_CASE( "FunctionBind2" )
    {
        context ctx;

        auto a = make_var<D>( ctx, 1 );
        auto b = make_var<D>( ctx, 1 );

        auto summ = []( int a, int b ) { return a + b; };

        auto c = ( ( ( a, b )->*summ ), ( ( a, make_var<D>( ctx, 100 ) )->*summ ) )->*&myfunc;
        auto d = c->*&myfunc2;
        auto e = ( d, d )->*&myfunc3;
        auto f = make_signal<D>( e, []( float value ) { return -value + 100; } );

        CHECK_EQ( c(), 103 );
        CHECK_EQ( d(), 51.5f );
        CHECK_EQ( e(), 2652.25f );
        CHECK_EQ( f(), -2552.25 );

        a <<= 10;

        CHECK_EQ( c(), 121 );
        CHECK_EQ( d(), 60.5f );
        CHECK_EQ( e(), 3660.25f );
        CHECK_EQ( f(), -3560.25f );
    }

    TEST_CASE( "Flatten1" )
    {
        context ctx;

        auto inner1 = make_var<D>( ctx, 123 );
        auto inner2 = make_var<D>( ctx, 789 );

        auto outer = make_var<D>( ctx, inner1 );

        auto flattened = flatten( outer );

        std::queue<int> results;

        observe( flattened, [&]( int v ) { results.push( v ); } );

        CHECK( outer().equals( inner1 ) );
        CHECK_EQ( flattened(), 123 );

        inner1 <<= 456;

        CHECK_EQ( flattened(), 456 );

        CHECK_EQ( results.front(), 456 );
        results.pop();
        CHECK( results.empty() );

        outer <<= inner2;

        CHECK( outer().equals( inner2 ) );
        CHECK_EQ( flattened(), 789 );

        CHECK_EQ( results.front(), 789 );
        results.pop();
        CHECK( results.empty() );
    }

    TEST_CASE( "Flatten2" )
    {
        context ctx;

        auto a0 = make_var<D>( ctx, 100 );

        auto inner1 = make_var<D>( ctx, 200 );

        auto plus0 = []( int value ) { return value + 0; };

        auto a1 = make_var<D>( ctx, 300 );
        auto a2 = make_signal<D>( a1, plus0 );
        auto a3 = make_signal<D>( a2, plus0 );
        auto a4 = make_signal<D>( a3, plus0 );
        auto a5 = make_signal<D>( a4, plus0 );
        auto a6 = make_signal<D>( a5, plus0 );
        auto inner2 = make_signal<D>( a6, plus0 );

        CHECK_EQ( inner1(), 200 );
        CHECK_EQ( inner2(), 300 );

        auto outer = make_var<D>( ctx, inner1 );

        auto flattened = flatten( outer );

        CHECK_EQ( flattened(), 200 );

        int observeCount = 0;

        observe( flattened, [&observeCount]( int v ) { observeCount++; } );

        auto o1 = make_signal<D>( ( a0, flattened ), []( int a, int b ) { return a + b; } );
        auto o2 = make_signal<D>( o1, plus0 );
        auto o3 = make_signal<D>( o2, plus0 );
        auto result = make_signal<D>( o3, plus0 );

        CHECK_EQ( result(), 100 + 200 );

        inner1 <<= 1234;

        CHECK_EQ( result(), 100 + 1234 );
        CHECK_EQ( observeCount, 1 );

        outer <<= inner2;

        CHECK_EQ( result(), 100 + 300 );
        CHECK_EQ( observeCount, 2 );

        do_transaction<D>( [&] {
            a0 <<= 5000;
            a1 <<= 6000;
        } );

        CHECK_EQ( result(), 5000 + 6000 );
        CHECK_EQ( observeCount, 3 );
    }

    TEST_CASE( "Flatten3" )
    {
        auto plus0 = []( int value ) { return value + 0; };
        auto summ = []( int a, int b ) { return a + b; };

        context ctx;

        auto inner1 = make_var<D>( ctx, 10 );

        auto a1 = make_var<D>( ctx, 20 );
        auto a2 = a1->*plus0;
        auto a3 = a2->*plus0;
        auto inner2 = a3->*plus0;

        auto outer = make_var<D>( ctx, inner1 );

        auto a0 = make_var<D>( ctx, 30 );

        auto flattened = flatten( outer );

        int observeCount = 0;

        observe( flattened, [&observeCount]( int v ) { observeCount++; } );

        auto result = ( flattened, a0 )->*summ;

        CHECK_EQ( result(), 10 + 30 );
        CHECK_EQ( observeCount, 0 );

        do_transaction<D>( [&] {
            inner1 <<= 1000;
            a0 <<= 200000;
            a1 <<= 50000;
            outer <<= inner2;
        } );

        CHECK_EQ( result(), 50000 + 200000 );
        CHECK_EQ( observeCount, 1 );

        do_transaction<D>( [&] {
            a0 <<= 667;
            a1 <<= 776;
        } );

        CHECK_EQ( result(), 776 + 667 );
        CHECK_EQ( observeCount, 2 );

        do_transaction<D>( [&] {
            inner1 <<= 999;
            a0 <<= 888;
        } );

        CHECK_EQ( result(), 776 + 888 );
        CHECK_EQ( observeCount, 2 );
    }

    TEST_CASE( "Flatten4" )
    {
        std::vector<int> results;

        auto plus0 = []( int value ) { return value + 0; };
        auto summ = []( int a, int b ) { return a + b; };

        context ctx;

        auto a1 = make_var<D>( ctx, 100 );
        auto inner1 = a1->*plus0;

        auto a2 = make_var<D>( ctx, 200 );
        auto inner2 = a2;

        auto a3 = make_var<D>( ctx, 200 );

        auto outer = make_var<D>( ctx, inner1 );

        auto flattened = flatten( outer );

        auto result = ( flattened, a3 )->*summ;

        observe( result, [&]( int v ) { results.push_back( v ); } );

        do_transaction<D>( [&] {
            a3 <<= 400;
            outer <<= inner2;
        } );

        CHECK_EQ( results.size(), 1 );

        CHECK( std::find( results.begin(), results.end(), 600 ) != results.end() );
    }

    TEST_CASE( "Member1" )
    {
        context ctx;

        auto outer = make_var<D>( ctx, 10 );
        auto inner = make_var<D>( ctx, outer );

        auto flattened = flatten( inner );

        observe( flattened, []( int v ) { CHECK_EQ( v, 30 ); } );

        outer <<= 30;
    }

    TEST_CASE( "Modify1" )
    {
        context ctx;

        auto v = make_var<D>( ctx, std::vector<int>{} );

        int obsCount = 0;

        observe( v, [&]( const std::vector<int>& v ) {
            CHECK_EQ( v, std::vector<int>{ 30, 50, 70 } );
            obsCount++;
        } );

        v.modify( []( std::vector<int>& v ) {
            v.push_back( 30 );
            v.push_back( 50 );
            v.push_back( 70 );
        } );

        CHECK_EQ( obsCount, 1 );
    }

    TEST_CASE( "Modify2" )
    {
        context ctx;

        auto v = make_var<D>( ctx, std::vector<int>{} );

        int obsCount = 0;

        observe( v, [&]( const std::vector<int>& v ) {
            CHECK_EQ( v, std::vector<int>{ 30, 50, 70 } );
            obsCount++;
        } );

        do_transaction<D>( [&] {
            v.modify( []( std::vector<int>& v ) { v.push_back( 30 ); } );

            v.modify( []( std::vector<int>& v ) { v.push_back( 50 ); } );

            v.modify( []( std::vector<int>& v ) { v.push_back( 70 ); } );
        } );


        CHECK_EQ( obsCount, 1 );
    }

    TEST_CASE( "Modify3" )
    {
        context ctx;

        auto vect = make_var<D>( ctx, std::vector<int>{} );

        int obsCount = 0;

        observe( vect, [&]( const std::vector<int>& v ) {
            CHECK_EQ( v, std::vector<int>{ 30, 50, 70 } );
            obsCount++;
        } );

        // Also terrible
        do_transaction<D>( [&] {
            vect.set( std::vector<int>{ 30, 50 } );

            vect.modify( []( std::vector<int>& v ) { v.push_back( 70 ); } );
        } );

        CHECK_EQ( obsCount, 1 );
    }

    TEST_CASE( "Signals of references" )
    {
        context ctx;

        Company company1( ctx, 1, "MetroTec" );
        Company company2( ctx, 2, "ACME" );

        Employee Bob( ctx, company1 );

        CHECK( Bob.company.value().get() == Company( ctx, 1, "MetroTec" ) );

        std::vector<std::string> bob_company_names;

        observe( Bob.company, [&]( const Company& company ) {
            bob_company_names.emplace_back( company.name.value() );
        } );

        // Changing ref of Bob's company to company2
        Bob.company <<= company2;

        CHECK( Bob.company.value().get() == Company( ctx, 2, "ACME" ) );

        CHECK( bob_company_names == std::vector<std::string>{ "ACME" } );
    }

    TEST_CASE( "Dynamic signal references" )
    {
        context ctx;

        Company company1( ctx, 1, "MetroTec" );
        Company company2( ctx, 2, "ACME" );

        Employee Alice( ctx, company1 );

        CHECK( Alice.company.value() == Company( ctx, 1, "MetroTec" ) );

        std::vector<std::string> alice_company_names;

        // Observer isn't bound to long term lived signal. So we need to keep it alive
        auto obs = observe( REACTIVE_REF( Alice.company, name ),
            [&]( const std::string& name ) { alice_company_names.emplace_back( name ); } );

        company1.name <<= std::string( "ModernTec" );
        Alice.company <<= std::ref( company2 );
        company2.name <<= std::string( "A.C.M.E." );

        CHECK( alice_company_names == std::vector<std::string>{ "ModernTec", "ACME", "A.C.M.E." } );
    }

    TEST_CASE( "Signal of events" )
    {
        context ctx;

        auto in1 = make_event_source<D, int>( ctx );
        auto in2 = make_event_source<D, int>( ctx );

        auto sig = make_var<D>( ctx, in1 );

        int reassign_count = 0;

        observe( sig, [&]( const events<D, int>& ) { ++reassign_count; } );

        auto f = flatten( sig );

        std::vector<int> saved_events;

        observe( f, [&]( const int value ) { saved_events.push_back( value ); } );

        in1 << -1;
        in2 << 1;

        sig <<= in2;

        in1 << -2;
        in2 << 2;

        CHECK( saved_events == std::vector<int>{ -1, 2 } );
        CHECK( reassign_count == 1 );
    }
}

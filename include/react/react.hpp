
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iostream>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace react
{
namespace detail
{

#if( defined( __cplusplus ) && __cplusplus >= 201703L )                                            \
    || ( defined( _HAS_CXX17 ) && _HAS_CXX17 == 1 )
using std::apply;
#else

// Code based on code at
/// http://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments

template <size_t N>
struct apply_helper
{
    template <typename F, typename T, typename... A>
    static inline auto apply( F&& f, T&& t, A&&... a )
        -> decltype( apply_helper<N - 1>::apply( std::forward<F>( f ),
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... ) )
    {
        return apply_helper<N - 1>::apply( std::forward<F>( f ),
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... );
    }
};

template <>
struct apply_helper<0>
{
    template <typename F, typename T, typename... A>
    static inline auto apply( F&& f, T&& /*unused*/, A&&... a )
        -> decltype( std::forward<F>( f )( std::forward<A>( a )... ) )
    {
        return std::forward<F>( f )( std::forward<A>( a )... );
    }
};

template <typename F, typename T>
inline auto apply( F&& f, T&& t )
    -> decltype( apply_helper<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        std::forward<F>( f ), std::forward<T>( t ) ) )
{
    return apply_helper<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        std::forward<F>( f ), std::forward<T>( t ) );
}

#endif

/// Helper to enable calling a function on each element of an argument pack.
/// We can't do f(args) ...; because ... expands with a comma.
/// But we can do nop_func(f(args) ...);
template <typename... args_t>
inline void pass( args_t&&... /*unused*/ )
{}

/// type_identity (workaround to enable enable decltype()::X)
/// See https://en.cppreference.com/w/cpp/types/type_identity
template <typename T>
struct type_identity
{
    using type = T;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DontMove!
///////////////////////////////////////////////////////////////////////////////////////////////////
struct dont_move
{};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// disable_if_same
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename U>
struct disable_if_same
    : std::enable_if<
          !std::is_same<typename std::decay<T>::type, typename std::decay<U>::type>::value>
{};

template <typename T1, typename T2>
using is_same_decay = std::is_same<typename std::decay<T1>::type, typename std::decay<T2>::type>;

/// Special wrapper to add specific return type to the void function
template <typename F, typename ret_t, ret_t return_value>
struct add_default_return_value_wrapper
{
    add_default_return_value_wrapper( const add_default_return_value_wrapper& ) = default;

    add_default_return_value_wrapper( add_default_return_value_wrapper&& other ) noexcept
        : m_func( std::move( other.m_func ) )
    {}

    template <typename in_f,
        class = typename std::enable_if<
            !is_same_decay<in_f, add_default_return_value_wrapper>::value>::type>
    explicit add_default_return_value_wrapper( in_f&& func )
        : m_func( std::forward<in_f>( func ) )
    {}

    template <typename... args_t>
    ret_t operator()( args_t&&... args )
    {
        m_func( std::forward<args_t>( args )... );
        return return_value;
    }

    F m_func;
};

template <typename F, typename ret_t, typename... args_t>
class is_callable_with
{
private:
    using NoT = char[1];
    using YesT = char[2];

    template <typename U,
        class = decltype(
            static_cast<ret_t>( ( std::declval<U>() )( std::declval<args_t>()... ) ) )>
    static YesT& check( void* );

    template <typename U>
    static NoT& check( ... );

public:
    enum
    {
        value = sizeof( check<F>( nullptr ) ) == sizeof( YesT )
    };
};

// Expand args by wrapping them in a dummy function
// Use comma operator to replace potential void return value with 0
#define REACT_EXPAND_PACK( ... ) ::react::detail::pass( ( __VA_ARGS__, 0 )... )

class reactive_node;

class topological_queue
{
public:
    using value_type = reactive_node*;

    topological_queue() = default;

    void push( const value_type& value, const int level )
    {
        m_queue_data.emplace_back( value, level );
    }

    bool fetch_next()
    {
        // Throw away previous values
        m_next_data.clear();

        // Find min level of nodes in queue data
        auto minimal_level = std::numeric_limits<int>::max();
        for( const auto& e : m_queue_data )
        {
            if( minimal_level > e.second )
            {
                minimal_level = e.second;
            }
        }

        // Swap entries with min level to the end
        auto p = std::partition( m_queue_data.begin(),
            m_queue_data.end(),
            [minimal_level]( const entry& e ) { return e.second != minimal_level; } );

        // Reserve once to avoid multiple re-allocations
        const auto to_reserve = static_cast<size_t>( std::distance( p, m_queue_data.end() ) );
        m_next_data.reserve( to_reserve );

        // Move min level values to next data
        for( auto it = p, ite = m_queue_data.end(); it != ite; ++it )
        {
            m_next_data.push_back( it->first );
        }

        // Truncate moved entries
        const auto to_resize = static_cast<size_t>( std::distance( m_queue_data.begin(), p ) );
        m_queue_data.resize( to_resize );

        return !m_next_data.empty();
    }

    const std::vector<value_type>& next_values() const
    {
        return m_next_data;
    }

private:
    using entry = std::pair<value_type, int>;

    std::vector<value_type> m_next_data;
    std::vector<entry> m_queue_data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// node_vector
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_type>
class node_vector
{
private:
    using data_type = std::vector<node_type*>;

public:
    void add( node_type& node )
    {
        m_data.push_back( &node );
    }

    void remove( const node_type& node )
    {
        m_data.erase( std::find( m_data.begin(), m_data.end(), &node ) );
    }

    typedef typename data_type::iterator iterator;
    typedef typename data_type::const_iterator const_iterator;

    iterator begin()
    {
        return m_data.begin();
    }

    iterator end()
    {
        return m_data.end();
    }

    const_iterator begin() const
    {
        return m_data.begin();
    }

    const_iterator end() const
    {
        return m_data.end();
    }

private:
    data_type m_data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// reactive_node
///////////////////////////////////////////////////////////////////////////////////////////////////
class reactive_node
{
public:
    int level{ 0 };
    int new_level{ 0 };
    bool queued{ false };

    node_vector<reactive_node> successors;

    virtual ~reactive_node() = default;

    // Note: Could get rid of this ugly ptr by adding a template parameter to the interface
    // But that would mean all engine nodes need that template parameter too - so rather cast
    virtual void tick( void* turn_ptr ) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////
using turn_id_t = unsigned;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// turn
///////////////////////////////////////////////////////////////////////////////////////////////////
class turn_type
{
public:
    inline turn_type( turn_id_t id )
        : id_( id )
    {}

    inline turn_id_t id() const
    {
        return id_;
    }

private:
    turn_id_t id_;
};

class topological_sort_engine
{
public:
    using node_t = reactive_node;
    using turn_t = turn_type;

    void propagate( turn_type& turn );

    static void on_node_attach( reactive_node& node, reactive_node& parent )
    {
        parent.successors.add( node );

        if( node.level <= parent.level )
        {
            node.level = parent.level + 1;
        }
    }

    static void on_node_detach( reactive_node& node, reactive_node& parent )
    {
        parent.successors.remove( node );
    }

    void on_input_change( reactive_node& node )
    {
        process_children( node );
    }

    void on_node_pulse( reactive_node& node )
    {
        process_children( node );
    }

    void on_dynamic_node_attach( reactive_node& node, reactive_node& parent )
    {
        on_node_attach( node, parent );

        invalidate_successors( node );

        // Re-schedule this node
        node.queued = true;
        m_scheduled_nodes.push( &node, node.level );
    }

    static void on_dynamic_node_detach( reactive_node& node, reactive_node& parent )
    {
        on_node_detach( node, parent );
    }

private:
    static void invalidate_successors( reactive_node& node )
    {
        for( auto* successor : node.successors )
        {
            if( successor->new_level <= node.level )
            {
                successor->new_level = node.level + 1;
            }
        }
    }

    void process_children( reactive_node& node );

    topological_queue m_scheduled_nodes;
};

template <typename D, typename engine_t>
struct engine_interface
{
    using node_t = typename engine_t::node_t;
    using turn_t = typename engine_t::turn_t;

    static engine_t& instance()
    {
        static engine_t engine;
        return engine;
    }

    static void on_input_change( node_t& node )
    {
        instance().on_input_change( node );
    }

    static void propagate( turn_t& turn )
    {
        instance().propagate( turn );
    }

    static void on_node_destroy( node_t& node )
    {
        instance().on_node_destroy( node );
    }

    static void on_node_attach( node_t& node, node_t& parent )
    {
        instance().on_node_attach( node, parent );
    }

    static void on_node_detach( node_t& node, node_t& parent )
    {
        instance().on_node_detach( node, parent );
    }

    static void on_node_pulse( node_t& node )
    {
        instance().on_node_pulse( node );
    }

    static void on_dynamic_node_attach( node_t& node, node_t& parent )
    {
        instance().on_dynamic_node_attach( node, parent );
    }

    static void on_dynamic_node_detach( node_t& node, node_t& parent )
    {
        instance().on_dynamic_node_detach( node, parent );
    }
};

struct input_node_interface
{
    virtual ~input_node_interface() = default;

    virtual bool apply_input( void* turn_ptr ) = 0;
};

class observer_interface
{
public:
    virtual ~observer_interface() = default;

    virtual void unregister_self() = 0;

private:
    virtual void detach_observer() = 0;

    template <typename D>
    friend class observable;
};

template <typename D>
class observable
{
public:
    observable() = default;

    observable( const observable& ) = delete;
    observable& operator=( const observable& ) = delete;
    observable( observable&& ) noexcept = delete;
    observable& operator=( observable&& ) noexcept = delete;

    ~observable()
    {
        for( const auto& p : m_observers )
        {
            if( p != nullptr )
            {
                p->detach_observer();
            }
        }
    }

    void register_observer( std::unique_ptr<observer_interface>&& obs_ptr )
    {
        m_observers.push_back( std::move( obs_ptr ) );
    }

    void unregister_observer( observer_interface* raw_obs_ptr )
    {
        for( auto it = m_observers.begin(); it != m_observers.end(); ++it )
        {
            if( it->get() == raw_obs_ptr )
            {
                it->get()->detach_observer();
                m_observers.erase( it );
                break;
            }
        }
    }

private:
    std::vector<std::unique_ptr<observer_interface>> m_observers;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// node_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class node_base : public reactive_node
{
public:
    using domain_t = D;
    using engine = typename D::engine;
    using node_t = reactive_node;
    using turn_t = typename engine::turn_t;

    node_base() = default;

    // Nodes can't be copied
    node_base( const node_base& ) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observable_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class observable_node
    : public node_base<D>
    , public observable<D>
{
public:
    observable_node() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// attach/detach helper functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename node_t, typename... deps_t>
struct attach_functor
{
    explicit attach_functor( node_t& node )
        : node( node )
    {}

    void operator()( const deps_t&... deps ) const
    {
        REACT_EXPAND_PACK( attach( deps ) );
    }

    template <typename T>
    void attach( const T& op ) const
    {
        op.template attach_rec<D, node_t>( *this );
    }

    template <typename T>
    void attach( const std::shared_ptr<T>& dep_ptr ) const
    {
        D::engine::on_node_attach( node, *dep_ptr );
    }

    node_t& node;
};

template <typename D, typename node_t, typename... deps_t>
struct detach_functor
{
    detach_functor( node_t& node )
        : node( node )
    {}

    void operator()( const deps_t&... deps ) const
    {
        REACT_EXPAND_PACK( detach( deps ) );
    }

    template <typename T>
    void detach( const T& op ) const
    {
        op.template detach_rec<D, node_t>( *this );
    }

    template <typename T>
    void detach( const std::shared_ptr<T>& dep_ptr ) const
    {
        D::engine::on_node_detach( node, *dep_ptr );
    }

    node_t& node;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// reactive_op_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename... deps_t>
class reactive_op_base
{
public:
    using dep_holder_t = std::tuple<deps_t...>;

    template <typename... deps_in_t>
    reactive_op_base( dont_move, deps_in_t&&... deps )
        : m_deps( std::forward<deps_in_t>( deps )... )
    {}

    reactive_op_base( reactive_op_base&& other ) noexcept
        : m_deps( std::move( other.m_deps ) )
    {}

    // Can't be copied, only moved
    reactive_op_base( const reactive_op_base& other ) = delete;

    template <typename D, typename node_t>
    void attach( node_t& node ) const
    {
        apply( attach_functor<D, node_t, deps_t...>{ node }, m_deps );
    }

    template <typename D, typename node_t>
    void detach( node_t& node ) const
    {
        apply( detach_functor<D, node_t, deps_t...>{ node }, m_deps );
    }

    template <typename D, typename node_t, typename functor_t>
    void attach_rec( const functor_t& functor ) const
    {
        // Same memory layout, different func
        apply( reinterpret_cast<const attach_functor<D, node_t, deps_t...>&>( functor ), m_deps );
    }

    template <typename D, typename node_t, typename functor_t>
    void detach_rec( const functor_t& functor ) const
    {
        apply( reinterpret_cast<const detach_functor<D, node_t, deps_t...>&>( functor ), m_deps );
    }

protected:
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterators for event processing
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class event_range
{
public:
    using const_iterator = typename std::vector<E>::const_iterator;
    using size_type = typename std::vector<E>::size_type;

    // Copy ctor
    event_range( const event_range& ) = default;

    // Copy assignment
    event_range& operator=( const event_range& ) = default;

    const_iterator begin() const
    {
        return data_.begin();
    }

    const_iterator end() const
    {
        return data_.end();
    }

    size_type size() const
    {
        return data_.size();
    }

    bool is_empty() const
    {
        return data_.empty();
    }

    explicit event_range( const std::vector<E>& data )
        : data_( data )
    {}

private:
    const std::vector<E>& data_;
};

template <typename E>
using event_emitter = std::back_insert_iterator<std::vector<E>>;

template <typename L, typename R>
bool equals( const L& lhs, const R& rhs )
{
    return lhs == rhs;
}

template <typename L, typename R>
bool equals( const std::reference_wrapper<L>& lhs, const std::reference_wrapper<R>& rhs )
{
    return lhs.get() == rhs.get();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// reactive_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
class reactive_base
{
public:
    using domain_t = typename node_t::domain_t;

    // Default ctor
    reactive_base() = default;

    // Copy ctor
    reactive_base( const reactive_base& ) = default;

    // Move ctor (VS2013 doesn't default generate that yet)
    reactive_base( reactive_base&& other ) noexcept
        : m_ptr( std::move( other.m_ptr ) )
    {}

    // Explicit node ctor
    explicit reactive_base( std::shared_ptr<node_t>&& ptr )
        : m_ptr( std::move( ptr ) )
    {}

    // Copy assignment
    reactive_base& operator=( const reactive_base& ) = default;

    // Move assignment
    reactive_base& operator=( reactive_base&& other ) noexcept
    {
        m_ptr.reset( std::move( other ) );
        return *this;
    }

    virtual bool is_valid() const
    {
        return m_ptr != nullptr;
    }

protected:
    std::shared_ptr<node_t> m_ptr;

    template <typename node_t_>
    friend const std::shared_ptr<node_t_>& get_node_ptr( const reactive_base<node_t_>& node );
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// copyable_reactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
class copyable_reactive : public reactive_base<node_t>
{
public:
    copyable_reactive() = default;

    copyable_reactive( const copyable_reactive& ) = default;

    copyable_reactive( copyable_reactive&& other ) noexcept
        : copyable_reactive::reactive_base( std::move( other ) )
    {}

    explicit copyable_reactive( std::shared_ptr<node_t>&& ptr )
        : copyable_reactive::reactive_base( std::move( ptr ) )
    {}

    copyable_reactive& operator=( const copyable_reactive& ) = default;

    copyable_reactive& operator=( copyable_reactive&& other ) noexcept
    {
        copyable_reactive::reactive_base::operator=( std::move( other ) );
        return *this;
    }

    bool equals( const copyable_reactive& other ) const
    {
        return this->m_ptr == other.m_ptr;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// get_node_ptr
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
const std::shared_ptr<node_t>& get_node_ptr( const reactive_base<node_t>& node )
{
    return node.m_ptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
struct input_node_interface;

class observer_interface;

template <typename D>
class input_manager;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// detached_observers_manager
///////////////////////////////////////////////////////////////////////////////////////////////////
class detached_observers_manager
{
public:
    void queue_observer_for_detach( observer_interface& obs )
    {
        m_detached_observers.push_back( &obs );
    }

    template <typename D>
    void detach_queued_observers()
    {
        for( auto* o : m_detached_observers )
        {
            o->unregister_self();
        }
        m_detached_observers.clear();
    }

private:
    std::vector<observer_interface*> m_detached_observers;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// input_manager
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class input_manager
{
public:
    using turn_t = typename D::turn_t;
    using engine = typename D::engine;

    input_manager() = default;

    template <typename F>
    void do_transaction( F&& func )
    {
        bool should_propagate = false;

        // Phase 1 - Input admission
        m_is_transaction_active = true;

        turn_t turn( next_turn_id() );

        func();

        m_is_transaction_active = false;

        // Phase 2 - Apply input node changes
        for( auto* p : m_changed_inputs )
        {
            if( p->apply_input( &turn ) )
            {
                should_propagate = true;
            }
        }
        m_changed_inputs.clear();

        // Phase 3 - propagate changes
        if( should_propagate )
        {
            engine::propagate( turn );
        }

        finalize_sync_transaction();
    }

    template <typename R, typename V>
    void add_input( R& r, V&& v )
    {
        if( m_is_transaction_active )
        {
            add_transaction_input( r, std::forward<V>( v ) );
        }
        else
        {
            add_simple_input( r, std::forward<V>( v ) );
        }
    }

    template <typename R, typename F>
    void modify_input( R& r, const F& func )
    {
        if( m_is_transaction_active )
        {
            modify_transaction_input( r, func );
        }
        else
        {
            modify_simple_input( r, func );
        }
    }

    void queue_observer_for_detach( observer_interface& obs )
    {
        m_detached_observers_manager.queue_observer_for_detach( obs );
    }

private:
    turn_id_t next_turn_id()
    {
        return m_next_turn_id++;
    }

    // Create a turn with a single input
    template <typename R, typename V>
    void add_simple_input( R& r, V&& v )
    {
        turn_t turn( next_turn_id() );
        r.add_input( std::forward<V>( v ) );

        if( r.apply_input( &turn ) )
        {
            engine::propagate( turn );
        }

        finalize_sync_transaction();
    }

    template <typename R, typename F>
    void modify_simple_input( R& r, const F& func )
    {
        turn_t turn( next_turn_id() );

        r.modify_input( func );


        // Return value, will always be true
        r.apply_input( &turn );

        engine::propagate( turn );

        finalize_sync_transaction();
    }

    void finalize_sync_transaction()
    {
        m_detached_observers_manager.template detach_queued_observers<D>();
    }

    // This input is part of an active transaction
    template <typename R, typename V>
    void add_transaction_input( R& r, V&& v )
    {
        r.add_input( std::forward<V>( v ) );
        m_changed_inputs.push_back( &r );
    }

    template <typename R, typename F>
    void modify_transaction_input( R& r, const F& func )
    {
        r.modify_input( func );
        m_changed_inputs.push_back( &r );
    }

    detached_observers_manager m_detached_observers_manager;

    turn_id_t m_next_turn_id{ 0 };

    bool m_is_transaction_active = false;

    std::vector<input_node_interface*> m_changed_inputs;
};

template <typename D>
class domain_specific_input_manager
{
public:
    domain_specific_input_manager() = delete;

    static input_manager<D>& instance()
    {
        static input_manager<D> instance;
        return instance;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal_node;

template <typename D, typename E>
class event_stream_node;

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal;

template <typename D, typename S>
class var_signal;

template <typename D, typename E>
class events;

template <typename D, typename E>
class event_source;

enum class token;

template <typename D>
class observer;

template <typename D>
class scoped_observer;

template <typename D, typename S, typename op_t>
class temp_signal;

template <typename D, typename E, typename op_t>
class temp_events;

template <typename D, typename... values_t>
class signal_pack;

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
/// domain_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class domain_base
{
public:
    using turn_t = turn_type;

    domain_base() = delete;

    using engine = ::react::detail::engine_interface<D, topological_sort_engine>;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Aliases for reactives of this domain
    ///////////////////////////////////////////////////////////////////////////////////////////////
    template <typename S>
    using signal_t = signal<D, S>;

    template <typename S>
    using var_signal_t = var_signal<D, S>;

    template <typename E = token>
    using events_t = events<D, E>;

    template <typename E = token>
    using event_source_t = event_source<D, E>;

    using observer_t = observer<D>;

    using scoped_observer_t = scoped_observer<D>;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Ensure singletons are created immediately after domain declaration (TODO hax)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class domain_initializer
{
public:
    domain_initializer()
    {
        D::engine::instance();
        domain_specific_input_manager<D>::instance();
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////
/// do_transaction
///////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename F>
void do_transaction( F&& func )
{
    using ::react::detail::domain_specific_input_manager;
    domain_specific_input_manager<D>::instance().do_transaction( std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Domain definition macro
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACTIVE_DOMAIN( name )                                                                    \
    struct name : public ::react::detail::domain_base<name>                                        \
    {};                                                                                            \
    static ::react::detail::domain_initializer<name> name##_initializer_;

/*
    A brief reminder why the domain initializer is here:
    Each domain has a couple of singletons (debug log, engine, input manager) which are
    currently implemented as meyer singletons. From what I understand, these are thread-safe
    in C++11, but not all compilers implement that yet. That's why a static initializer has
    been added to make sure singleton creation happens before any multi-threaded access.
    This implementation is obviously inconsequential.
 */

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Define type aliases for given domain
///////////////////////////////////////////////////////////////////////////////////////////////////
#define USING_REACTIVE_DOMAIN( name )                                                              \
    template <typename S>                                                                          \
    using signal_t = signal<name, S>;                                                              \
                                                                                                   \
    template <typename S>                                                                          \
    using var_signal_t = var_signal<name, S>;                                                      \
                                                                                                   \
    template <typename E = token>                                                                  \
    using events_t = events<name, E>;                                                              \
                                                                                                   \
    template <typename E = token>                                                                  \
    using event_source_t = event_source<name, E>;                                                  \
                                                                                                   \
    using observer_t = observer<name>;                                                             \
                                                                                                   \
    using scoped_observer_t = scoped_observer<name>;

#if _MSC_VER && !__INTEL_COMPILER
#    pragma warning( disable : 4503 )
#endif

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observer_action
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class observer_action
{
    next,
    stop_and_detach
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// add_observer_range_wrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename F, typename... args_t>
struct add_observer_range_wrapper
{
    add_observer_range_wrapper( const add_observer_range_wrapper& other ) = default;

    add_observer_range_wrapper( add_observer_range_wrapper&& other ) noexcept
        : m_func( std::move( other.m_func ) )
    {}

    template <typename f_in_t,
        class = typename disable_if_same<f_in_t, add_observer_range_wrapper>::type>
    explicit add_observer_range_wrapper( f_in_t&& func )
        : m_func( std::forward<f_in_t>( func ) )
    {}

    observer_action operator()( event_range<E> range, const args_t&... args )
    {
        for( const auto& e : range )
        {
            if( m_func( e, args... ) == observer_action::stop_and_detach )
            {
                return observer_action::stop_and_detach;
            }
        }

        return observer_action::next;
    }

    F m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observer_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class observer_node
    : public node_base<D>
    , public observer_interface
{
public:
    observer_node() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_observer_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename func_t>
class signal_observer_node : public observer_node<D>
{
    using engine = typename signal_observer_node::engine;

public:
    template <typename F>
    signal_observer_node( const std::shared_ptr<signal_node<D, S>>& subject, F&& func )
        : signal_observer_node::observer_node()
        , m_subject( subject )
        , m_func( std::forward<F>( func ) )
    {
        engine::on_node_attach( *this, *subject );
    }

    ~signal_observer_node() = default;

    void tick( void* ) override
    {
        bool should_detach = false;

        if( auto p = m_subject.lock() )
        {
            if( m_func( p->value_ref() ) == observer_action::stop_and_detach )
            {
                should_detach = true;
            }
        }

        if( should_detach )
        {
            domain_specific_input_manager<D>::instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = m_subject.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    void detach_observer() override
    {
        if( auto p = m_subject.lock() )
        {
            engine::on_node_detach( *this, *p );
            m_subject.reset();
        }
    }

    std::weak_ptr<signal_node<D, S>> m_subject;
    func_t m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_observer_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename func_t>
class event_observer_node : public observer_node<D>
{
    using engine = typename event_observer_node::engine;

public:
    template <typename F>
    event_observer_node( const std::shared_ptr<event_stream_node<D, E>>& subject, F&& func )
        : event_observer_node::observer_node()
        , m_subject( subject )
        , m_func( std::forward<F>( func ) )
    {
        engine::on_node_attach( *this, *subject );
    }

    ~event_observer_node() = default;

    void tick( void* ) override
    {
        bool should_detach = false;

        if( auto p = m_subject.lock() )
        {
            should_detach
                = m_func( event_range<E>( p->events() ) ) == observer_action::stop_and_detach;
        }

        if( should_detach )
        {
            domain_specific_input_manager<D>::instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = m_subject.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    std::weak_ptr<event_stream_node<D, E>> m_subject;

    func_t m_func;

    virtual void detach_observer()
    {
        if( auto p = m_subject.lock() )
        {
            engine::on_node_detach( *this, *p );
            m_subject.reset();
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_observer_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename func_t, typename... dep_values_t>
class synced_observer_node : public observer_node<D>
{
    using engine = typename synced_observer_node::engine;

public:
    template <typename F>
    synced_observer_node( const std::shared_ptr<event_stream_node<D, E>>& subject,
        F&& func,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_observer_node::observer_node()
        , m_subject( subject )
        , m_func( std::forward<F>( func ) )
        , m_deps( deps... )
    {
        engine::on_node_attach( *this, *subject );

        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_observer_node() = default;

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        bool should_detach = false;

        if( auto p = m_subject.lock() )
        {
            // Update of this node could be triggered from deps,
            // so make sure source doesnt contain events from last turn
            p->set_current_turn( turn );

            {
                should_detach
                    = apply(
                          [this, &p](
                              const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                              return m_func( event_range<E>( p->events() ), args->value_ref()... );
                          },
                          m_deps )
                   == observer_action::stop_and_detach;
            }
        }

        if( should_detach )
        {
            domain_specific_input_manager<D>::instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = m_subject.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::weak_ptr<event_stream_node<D, E>> m_subject;

    func_t m_func;
    dep_holder_t m_deps;

    virtual void detach_observer()
    {
        if( auto p = m_subject.lock() )
        {
            engine::on_node_detach( *this, *p );

            apply( detach_functor<D,
                       synced_observer_node,
                       std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
                m_deps );

            m_subject.reset();
        }
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
using ::react::detail::observer_action;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class observer
{
private:
    using subject_ptr_t = std::shared_ptr<::react::detail::observable_node<D>>;
    using node_t = ::react::detail::observer_node<D>;

public:
    // Default ctor
    observer()
        : m_node_ptr( nullptr )
        , m_subject_ptr( nullptr )
    {}

    // Move ctor
    observer( observer&& other ) noexcept
        : m_node_ptr( other.m_node_ptr )
        , m_subject_ptr( std::move( other.m_subject_ptr ) )
    {
        other.m_node_ptr = nullptr;
        other.m_subject_ptr.reset();
    }

    // Node ctor
    observer( node_t* node_ptr, const subject_ptr_t& subject_ptr )
        : m_node_ptr( node_ptr )
        , m_subject_ptr( subject_ptr )
    {}

    // Move assignment
    observer& operator=( observer&& other ) noexcept
    {
        m_node_ptr = other.m_node_ptr;
        m_subject_ptr = std::move( other.m_subject_ptr );

        other.m_node_ptr = nullptr;
        other.m_subject_ptr.reset();

        return *this;
    }

    // Deleted copy ctor and assignment
    observer( const observer& ) = delete;
    observer& operator=( const observer& ) = delete;

    void detach()
    {
        assert( is_valid() );
        m_subject_ptr->unregister_observer( m_node_ptr );
    }

    bool is_valid() const
    {
        return m_node_ptr != nullptr;
    }

private:
    // Owned by subject
    node_t* m_node_ptr;

    // While the observer handle exists, the subject is not destroyed
    subject_ptr_t m_subject_ptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// scoped_observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class scoped_observer
{
public:
    // Move ctor
    scoped_observer( scoped_observer&& other ) noexcept
        : m_obs( std::move( other.m_obs ) )
    {}

    // Construct from observer
    scoped_observer( observer<D>&& obs )
        : m_obs( std::move( obs ) )
    {}

    // Move assignment
    scoped_observer& operator=( scoped_observer&& other ) noexcept
    {
        m_obs = std::move( other.m_obs );
    }

    // Deleted default ctor, copy ctor and assignment
    scoped_observer() = delete;
    scoped_observer( const scoped_observer& ) = delete;
    scoped_observer& operator=( const scoped_observer& ) = delete;

    ~scoped_observer()
    {
        m_obs.detach();
    }

    bool is_valid() const
    {
        return m_obs.is_valid();
    }

private:
    observer<D> m_obs;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observe - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename f_in_t, typename S>
auto observe( const signal<D, S>& subject, f_in_t&& func ) -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::observer_interface;
    using ::react::detail::observer_node;
    using ::react::detail::signal_observer_node;

    using F = typename std::decay<f_in_t>::type;
    using R = typename std::result_of<f_in_t( S )>::type;
    using wrapper_t = add_default_return_value_wrapper<F, observer_action, observer_action::next>;

    // If return value of passed function is void, add observer_action::next as
    // default return value.
    using node_t = typename std::conditional<std::is_same<void, R>::value,
        signal_observer_node<D, S, wrapper_t>,
        signal_observer_node<D, S, F>>::type;

    const auto& subject_ptr = get_node_ptr( subject );

    std::unique_ptr<observer_node<D>> node_ptr(
        new node_t( subject_ptr, std::forward<f_in_t>( func ) ) );
    observer_node<D>* raw_node_ptr = node_ptr.get();

    subject_ptr->register_observer( std::move( node_ptr ) );

    return observer<D>( raw_node_ptr, subject_ptr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observe - events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename f_in_t, typename E>
auto observe( const events<D, E>& subject, f_in_t&& func ) -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::add_observer_range_wrapper;
    using ::react::detail::event_observer_node;
    using ::react::detail::event_range;
    using ::react::detail::is_callable_with;
    using ::react::detail::observer_interface;
    using ::react::detail::observer_node;

    using F = typename std::decay<f_in_t>::type;

    using wrapper_t =
        typename std::conditional<is_callable_with<F, observer_action, event_range<E>>::value,
            F,
            typename std::conditional<is_callable_with<F, observer_action, E>::value,
                add_observer_range_wrapper<E, F>,
                typename std::conditional<is_callable_with<F, void, event_range<E>>::value,
                    add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                    typename std::conditional<is_callable_with<F, void, E>::value,
                        add_observer_range_wrapper<E,
                            add_default_return_value_wrapper<F,
                                observer_action,
                                observer_action::next>>,
                        void>::type>::type>::type>::type;

    static_assert( !std::is_same<wrapper_t, void>::value,
        "observe: Passed function does not match any of the supported signatures." );

    using node_t = event_observer_node<D, E, wrapper_t>;

    const auto& subject_ptr = get_node_ptr( subject );

    std::unique_ptr<observer_node<D>> node_ptr(
        new node_t( subject_ptr, std::forward<f_in_t>( func ) ) );
    observer_node<D>* raw_node_ptr = node_ptr.get();

    subject_ptr->register_observer( std::move( node_ptr ) );

    return observer<D>( raw_node_ptr, subject_ptr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// observe - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename f_in_t, typename E, typename... dep_values_t>
auto observe(
    const events<D, E>& subject, const signal_pack<D, dep_values_t...>& dep_pack, f_in_t&& func )
    -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::add_observer_range_wrapper;
    using ::react::detail::event_range;
    using ::react::detail::is_callable_with;
    using ::react::detail::observer_interface;
    using ::react::detail::observer_node;
    using ::react::detail::synced_observer_node;

    using F = typename std::decay<f_in_t>::type;

    using wrapper_t = typename std::conditional<
        is_callable_with<F, observer_action, event_range<E>, dep_values_t...>::value,
        F,
        typename std::conditional<is_callable_with<F, observer_action, E, dep_values_t...>::value,
            add_observer_range_wrapper<E, F, dep_values_t...>,
            typename std::conditional<
                is_callable_with<F, void, event_range<E>, dep_values_t...>::value,
                add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                typename std::conditional<is_callable_with<F, void, E, dep_values_t...>::value,
                    add_observer_range_wrapper<E,
                        add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                        dep_values_t...>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<wrapper_t, void>::value,
        "observe: Passed function does not match any of the supported signatures." );

    using node_t = synced_observer_node<D, E, wrapper_t, dep_values_t...>;

    struct node_builder
    {
        node_builder( const events<D, E>& subject, f_in_t&& func )
            : m_subject( subject )
            , m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, dep_values_t>&... deps ) -> observer_node<D>*
        {
            return new node_t( get_node_ptr( m_subject ),
                std::forward<f_in_t>( m_func ),
                get_node_ptr( deps )... );
        }

        const events<D, E>& m_subject;
        f_in_t m_func;
    };

    const auto& subject_ptr = get_node_ptr( subject );

    std::unique_ptr<observer_node<D>> node_ptr( ::react::detail::apply(
        node_builder( subject, std::forward<f_in_t>( func ) ), dep_pack.Data ) );

    observer_node<D>* raw_node_ptr = node_ptr.get();

    subject_ptr->register_observer( std::move( node_ptr ) );

    return observer<D>( raw_node_ptr, subject_ptr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// is_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct is_signal
{
    static const bool value = false;
};

template <typename D, typename T>
struct is_signal<signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct is_signal<var_signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename op_t>
struct is_signal<temp_signal<D, T, op_t>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// is_event
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct is_event
{
    static const bool value = false;
};

template <typename D, typename T>
struct is_event<events<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct is_event<event_source<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename op_t>
struct is_event<temp_events<D, T, op_t>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// decay_input
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct decay_input
{
    using type = T;
};

template <typename D, typename T>
struct decay_input<var_signal<D, T>>
{
    using type = signal<D, T>;
};

template <typename D, typename T>
struct decay_input<event_source<D, T>>
{
    using type = events<D, T>;
};

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename L, typename R>
bool equals( const L& lhs, const R& rhs );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal_node : public observable_node<D>
{
public:
    signal_node() = default;

    template <typename T>
    explicit signal_node( T&& value )
        : signal_node::observable_node()
        , m_value( std::forward<T>( value ) )
    {}

    const S& value_ref() const
    {
        return m_value;
    }

protected:
    S m_value;
};

template <typename D, typename S>
using signal_node_ptr_t = std::shared_ptr<signal_node<D, S>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// var_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class var_node
    : public signal_node<D, S>
    , public input_node_interface
{
    using engine = typename var_node::engine;

public:
    template <typename T>
    var_node( T&& value )
        : var_node::signal_node( std::forward<T>( value ) )
        , m_new_value( value )
    {}

    ~var_node() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked var_node" );
    }

    template <typename V>
    void add_input( V&& new_value )
    {
        m_new_value = std::forward<V>( new_value );

        m_is_input_added = true;

        // m_is_input_added takes precedences over m_is_input_modified
        // the only difference between the two is that m_is_input_modified doesn't/can't compare
        m_is_input_modified = false;
    }

    // This is signal-specific
    template <typename F>
    void modify_input( F& func )
    {
        // There hasn't been any Set(...) input yet, modify.
        if( !m_is_input_added )
        {
            func( this->m_value );

            m_is_input_modified = true;
        }
        // There's a new_value, modify new_value instead.
        // The modified new_value will handled like before, i.e. it'll be compared to m_value
        // in apply_input
        else
        {
            func( m_new_value );
        }
    }

    bool apply_input( void* ) override
    {
        if( m_is_input_added )
        {
            m_is_input_added = false;

            if( !equals( this->m_value, m_new_value ) )
            {
                this->m_value = std::move( m_new_value );
                engine::on_input_change( *this );
                return true;
            }
            else
            {
                return false;
            }
        }
        else if( m_is_input_modified )
        {
            m_is_input_modified = false;

            engine::on_input_change( *this );
            return true;
        }

        else
        {
            return false;
        }
    }

private:
    S m_new_value;
    bool m_is_input_added = false;
    bool m_is_input_modified = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// function_op
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename... deps_t>
class function_op : public reactive_op_base<deps_t...>
{
public:
    template <typename f_in_t, typename... deps_in_t>
    function_op( f_in_t&& func, deps_in_t&&... deps )
        : function_op::reactive_op_base( dont_move(), std::forward<deps_in_t>( deps )... )
        , m_func( std::forward<f_in_t>( func ) )
    {}

    function_op( function_op&& other ) noexcept
        : function_op::reactive_op_base( std::move( other ) )
        , m_func( std::move( other.m_func ) )
    {}

    S evaluate()
    {
        return apply( eval_functor( m_func ), this->m_deps );
    }

private:
    // Eval
    struct eval_functor
    {
        eval_functor( F& f )
            : m_func( f )
        {}

        template <typename... T>
        S operator()( T&&... args )
        {
            return m_func( eval( args )... );
        }

        template <typename T>
        static auto eval( T& op ) -> decltype( op.evaluate() )
        {
            return op.evaluate();
        }

        template <typename T>
        static auto eval( const std::shared_ptr<T>& dep_ptr ) -> decltype( dep_ptr->value_ref() )
        {
            return dep_ptr->value_ref();
        }

        F& m_func;
    };

private:
    F m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_op_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename op_t>
class signal_op_node : public signal_node<D, S>
{
    using engine = typename signal_op_node::engine;

public:
    template <typename... args_t>
    signal_op_node( args_t&&... args )
        : signal_op_node::signal_node()
        , m_op( std::forward<args_t>( args )... )
    {
        this->m_value = m_op.evaluate();


        m_op.template attach<D>( *this );
    }

    ~signal_op_node()
    {
        if( !m_was_op_stolen )
        {
            m_op.template detach<D>( *this );
        }
    }

    void tick( void* ) override
    {
        bool changed = false;

        {
            S new_value = m_op.evaluate();

            if( !equals( this->m_value, new_value ) )
            {
                this->m_value = std::move( new_value );
                changed = true;
            }
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

    op_t steal_op()
    {
        assert( !m_was_op_stolen && "Op was already stolen." );
        m_was_op_stolen = true;
        m_op.template detach<D>( *this );
        return std::move( m_op );
    }

private:
    op_t m_op;
    bool m_was_op_stolen = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// flatten_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename outer_t, typename inner_t>
class flatten_node : public signal_node<D, inner_t>
{
    using engine = typename flatten_node::engine;

public:
    flatten_node( const std::shared_ptr<signal_node<D, outer_t>>& outer,
        const std::shared_ptr<signal_node<D, inner_t>>& inner )
        : flatten_node::signal_node( inner->value_ref() )
        , m_outer( outer )
        , m_inner( inner )
    {
        engine::on_node_attach( *this, *m_outer );
        engine::on_node_attach( *this, *m_inner );
    }

    ~flatten_node()
    {
        engine::on_node_detach( *this, *m_inner );
        engine::on_node_detach( *this, *m_outer );
    }

    void tick( void* ) override
    {
        auto new_inner = get_node_ptr( m_outer->value_ref() );

        if( new_inner != m_inner )
        {
            // Topology has been changed
            auto m_old_inner = m_inner;
            m_inner = new_inner;

            engine::on_dynamic_node_detach( *this, *m_old_inner );
            engine::on_dynamic_node_attach( *this, *new_inner );

            return;
        }

        if( !equals( this->m_value, m_inner->value_ref() ) )
        {
            this->m_value = m_inner->value_ref();
            engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<signal_node<D, outer_t>> m_outer;
    std::shared_ptr<signal_node<D, inner_t>> m_inner;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal_base : public copyable_reactive<signal_node<D, S>>
{
public:
    signal_base() = default;
    signal_base( const signal_base& ) = default;

    template <typename T>
    signal_base( T&& t )
        : signal_base::copyable_reactive( std::forward<T>( t ) )
    {}

protected:
    const S& get_value() const
    {
        return this->m_ptr->value_ref();
    }

    template <typename T>
    void set_value( T&& new_value ) const
    {
        domain_specific_input_manager<D>::instance().add_input(
            *reinterpret_cast<var_node<D, S>*>( this->m_ptr.get() ), std::forward<T>( new_value ) );
    }

    template <typename F>
    void modify_value( const F& func ) const
    {
        domain_specific_input_manager<D>::instance().modify_input(
            *reinterpret_cast<var_node<D, S>*>( this->m_ptr.get() ), func );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_pack - Wraps several nodes in a tuple. Create with comma operator.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... values_t>
class signal_pack
{
public:
    signal_pack( const signal<D, values_t>&... deps )
        : Data( std::tie( deps... ) )
    {}

    template <typename... cur_values_t, typename append_value_t>
    signal_pack(
        const signal_pack<D, cur_values_t...>& cur_args, const signal<D, append_value_t>& new_arg )
        : Data( std::tuple_cat( cur_args.Data, std::tie( new_arg ) ) )
    {}

    std::tuple<const signal<D, values_t>&...> Data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// with - Utility function to create a signal_pack
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... values_t>
auto with( const signal<D, values_t>&... deps ) -> signal_pack<D, values_t...>
{
    return signal_pack<D, values_t...>( deps... );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// make_var
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    class = typename std::enable_if<!is_signal<S>::value>::type,
    class = typename std::enable_if<!is_event<S>::value>::type>
auto make_var( V&& value ) -> var_signal<D, S>
{
    return var_signal<D, S>(
        std::make_shared<::react::detail::var_node<D, S>>( std::forward<V>( value ) ) );
}

template <typename D, typename S>
auto make_var( std::reference_wrapper<S> value ) -> var_signal<D, S&>
{
    return var_signal<D, S&>(
        std::make_shared<::react::detail::var_node<D, std::reference_wrapper<S>>>( value ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// make_var (higher order reactives)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename inner_t = typename S::value_t,
    class = typename std::enable_if<is_signal<S>::value>::type>
auto make_var( V&& value ) -> var_signal<D, signal<D, inner_t>>
{
    return var_signal<D, signal<D, inner_t>>(
        std::make_shared<::react::detail::var_node<D, signal<D, inner_t>>>(
            std::forward<V>( value ) ) );
}

template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename inner_t = typename S::value_t,
    class = typename std::enable_if<is_event<S>::value>::type>
auto make_var( V&& value ) -> var_signal<D, events<D, inner_t>>
{
    return var_signal<D, events<D, inner_t>>(
        std::make_shared<::react::detail::var_node<D, events<D, inner_t>>>(
            std::forward<V>( value ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// make_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
// Single arg
template <typename D,
    typename TValue,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename S = typename std::result_of<F( TValue )>::type,
    typename op_t
    = ::react::detail::function_op<S, F, ::react::detail::signal_node_ptr_t<D, TValue>>>
auto make_signal( const signal<D, TValue>& arg, f_in_t&& func ) -> temp_signal<D, S, op_t>
{
    return temp_signal<D, S, op_t>( std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(
        std::forward<f_in_t>( func ), get_node_ptr( arg ) ) );
}

// Multiple args
template <typename D,
    typename... values_t,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename S = typename std::result_of<F( values_t... )>::type,
    typename op_t
    = ::react::detail::function_op<S, F, ::react::detail::signal_node_ptr_t<D, values_t>...>>
auto make_signal( const signal_pack<D, values_t...>& arg_pack, f_in_t&& func )
    -> temp_signal<D, S, op_t>
{
    using ::react::detail::signal_op_node;

    struct node_builder
    {
        node_builder( f_in_t&& func )
            : m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, values_t>&... args ) -> temp_signal<D, S, op_t>
        {
            return temp_signal<D, S, op_t>( std::make_shared<signal_op_node<D, S, op_t>>(
                std::forward<f_in_t>( m_func ), get_node_ptr( args )... ) );
        }

        f_in_t m_func;
    };

    return ::react::detail::apply( node_builder( std::forward<f_in_t>( func ) ), arg_pack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Unary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP( op, name )                                                               \
    template <typename T>                                                                          \
    struct name##_op_functor                                                                       \
    {                                                                                              \
        T operator()( const T& v ) const                                                           \
        {                                                                                          \
            return op v;                                                                           \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename TSignal,                                                                    \
        typename D = typename TSignal::domain_t,                                                   \
        typename TVal = typename TSignal::value_t,                                                 \
        class = typename std::enable_if<is_signal<TSignal>::value>::type,                          \
        typename F = name##_op_functor<TVal>,                                                      \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename op_t                                                                              \
        = ::react::detail::function_op<S, F, ::react::detail::signal_node_ptr_t<D, TVal>>>         \
    auto operator op( const TSignal& arg )->temp_signal<D, S, op_t>                                \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), get_node_ptr( arg ) ) );                                                      \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TVal,                                                                             \
        typename op_in_t,                                                                          \
        typename F = name##_op_functor<TVal>,                                                      \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename op_t = ::react::detail::function_op<S, F, op_in_t>>                               \
    auto operator op( temp_signal<D, TVal, op_in_t>&& arg )->temp_signal<D, S, op_t>               \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), arg.steal_op() ) );                                                           \
    }

REACT_DECLARE_OP( +, unary_plus )

REACT_DECLARE_OP( -, unary_minus )

REACT_DECLARE_OP( !, logical_negation )

REACT_DECLARE_OP( ~, bitwise_complement )

REACT_DECLARE_OP( ++, increment )

REACT_DECLARE_OP( --, decrement )

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Binary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP( op, name )                                                               \
    template <typename L, typename R>                                                              \
    struct name##_op_functor                                                                       \
    {                                                                                              \
        auto operator()( const L& lhs, const R& rhs ) const                                        \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return lhs op rhs;                                                                     \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename L, typename R>                                                              \
    struct name##_op_r_functor                                                                     \
    {                                                                                              \
        name##_op_r_functor( name##_op_r_functor&& other ) noexcept                                \
            : m_left_val( std::move( other.m_left_val ) )                                          \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        explicit name##_op_r_functor( T&& val )                                                    \
            : m_left_val( std::forward<T>( val ) )                                                 \
        {}                                                                                         \
                                                                                                   \
        name##_op_r_functor( const name##_op_r_functor& other ) = delete;                          \
                                                                                                   \
        auto operator()( const R& rhs ) const                                                      \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return m_left_val op rhs;                                                              \
        }                                                                                          \
                                                                                                   \
        L m_left_val;                                                                              \
    };                                                                                             \
                                                                                                   \
    template <typename L, typename R>                                                              \
    struct name##_op_l_functor                                                                     \
    {                                                                                              \
        name##_op_l_functor( name##_op_l_functor&& other ) noexcept                                \
            : m_right_val( std::move( other.m_right_val ) )                                        \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        explicit name##_op_l_functor( T&& val )                                                    \
            : m_right_val( std::forward<T>( val ) )                                                \
        {}                                                                                         \
                                                                                                   \
        name##_op_l_functor( const name##_op_l_functor& other ) = delete;                          \
                                                                                                   \
        auto operator()( const L& lhs ) const                                                      \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return lhs op m_right_val;                                                             \
        }                                                                                          \
                                                                                                   \
        R m_right_val;                                                                             \
    };                                                                                             \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename TRightSignal,                                                                     \
        typename D = typename TLeftSignal::domain_t,                                               \
        typename left_val_t = typename TLeftSignal::value_t,                                       \
        typename right_val_t = typename TRightSignal::value_t,                                     \
        class = typename std::enable_if<is_signal<TLeftSignal>::value>::type,                      \
        class = typename std::enable_if<is_signal<TRightSignal>::value>::type,                     \
        typename F = name##_op_functor<left_val_t, right_val_t>,                                   \
        typename S = typename std::result_of<F( left_val_t, right_val_t )>::type,                  \
        typename op_t = ::react::detail::function_op<S,                                            \
            F,                                                                                     \
            ::react::detail::signal_node_ptr_t<D, left_val_t>,                                     \
            ::react::detail::signal_node_ptr_t<D, right_val_t>>>                                   \
    auto operator op( const TLeftSignal& lhs, const TRightSignal& rhs )->temp_signal<D, S, op_t>   \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), get_node_ptr( lhs ), get_node_ptr( rhs ) ) );                                 \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename Tm_right_valIn,                                                                   \
        typename D = typename TLeftSignal::domain_t,                                               \
        typename left_val_t = typename TLeftSignal::value_t,                                       \
        typename right_val_t = typename std::decay<Tm_right_valIn>::type,                          \
        class = typename std::enable_if<is_signal<TLeftSignal>::value>::type,                      \
        class = typename std::enable_if<!is_signal<right_val_t>::value>::type,                     \
        typename F = name##_op_l_functor<left_val_t, right_val_t>,                                 \
        typename S = typename std::result_of<F( left_val_t )>::type,                               \
        typename op_t                                                                              \
        = ::react::detail::function_op<S, F, ::react::detail::signal_node_ptr_t<D, left_val_t>>>   \
    auto operator op( const TLeftSignal& lhs, Tm_right_valIn&& rhs )->temp_signal<D, S, op_t>      \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F( std::forward<Tm_right_valIn>( rhs ) ), get_node_ptr( lhs ) ) );                 \
    }                                                                                              \
                                                                                                   \
    template <typename Tm_left_valIn,                                                              \
        typename TRightSignal,                                                                     \
        typename D = typename TRightSignal::domain_t,                                              \
        typename left_val_t = typename std::decay<Tm_left_valIn>::type,                            \
        typename right_val_t = typename TRightSignal::value_t,                                     \
        class = typename std::enable_if<!is_signal<left_val_t>::value>::type,                      \
        class = typename std::enable_if<is_signal<TRightSignal>::value>::type,                     \
        typename F = name##_op_r_functor<left_val_t, right_val_t>,                                 \
        typename S = typename std::result_of<F( right_val_t )>::type,                              \
        typename op_t                                                                              \
        = ::react::detail::function_op<S, F, ::react::detail::signal_node_ptr_t<D, right_val_t>>>  \
    auto operator op( Tm_left_valIn&& lhs, const TRightSignal& rhs )->temp_signal<D, S, op_t>      \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F( std::forward<Tm_left_valIn>( lhs ) ), get_node_ptr( rhs ) ) );                  \
    }                                                                                              \
    template <typename D,                                                                          \
        typename left_val_t,                                                                       \
        typename left_op_t,                                                                        \
        typename right_val_t,                                                                      \
        typename right_op_t,                                                                       \
        typename F = name##_op_functor<left_val_t, right_val_t>,                                   \
        typename S = typename std::result_of<F( left_val_t, right_val_t )>::type,                  \
        typename op_t = ::react::detail::function_op<S, F, left_op_t, right_op_t>>                 \
    auto operator op( temp_signal<D, left_val_t, left_op_t>&& lhs,                                 \
        temp_signal<D, right_val_t, right_op_t>&& rhs )                                            \
        ->temp_signal<D, S, op_t>                                                                  \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), lhs.steal_op(), rhs.steal_op() ) );                                           \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename left_val_t,                                                                       \
        typename left_op_t,                                                                        \
        typename TRightSignal,                                                                     \
        typename right_val_t = typename TRightSignal::value_t,                                     \
        class = typename std::enable_if<is_signal<TRightSignal>::value>::type,                     \
        typename F = name##_op_functor<left_val_t, right_val_t>,                                   \
        typename S = typename std::result_of<F( left_val_t, right_val_t )>::type,                  \
        typename op_t = ::react::detail::                                                          \
            function_op<S, F, left_op_t, ::react::detail::signal_node_ptr_t<D, right_val_t>>>      \
    auto operator op( temp_signal<D, left_val_t, left_op_t>&& lhs, const TRightSignal& rhs )       \
        ->temp_signal<D, S, op_t>                                                                  \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), lhs.steal_op(), get_node_ptr( rhs ) ) );                                      \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename D,                                                                                \
        typename right_val_t,                                                                      \
        typename right_op_t,                                                                       \
        typename left_val_t = typename TLeftSignal::value_t,                                       \
        class = typename std::enable_if<is_signal<TLeftSignal>::value>::type,                      \
        typename F = name##_op_functor<left_val_t, right_val_t>,                                   \
        typename S = typename std::result_of<F( left_val_t, right_val_t )>::type,                  \
        typename op_t = ::react::detail::                                                          \
            function_op<S, F, ::react::detail::signal_node_ptr_t<D, left_val_t>, right_op_t>>      \
    auto operator op( const TLeftSignal& lhs, temp_signal<D, right_val_t, right_op_t>&& rhs )      \
        ->temp_signal<D, S, op_t>                                                                  \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F(), get_node_ptr( lhs ), rhs.steal_op() ) );                                      \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename left_val_t,                                                                       \
        typename left_op_t,                                                                        \
        typename Tm_right_valIn,                                                                   \
        typename right_val_t = typename std::decay<Tm_right_valIn>::type,                          \
        class = typename std::enable_if<!is_signal<right_val_t>::value>::type,                     \
        typename F = name##_op_l_functor<left_val_t, right_val_t>,                                 \
        typename S = typename std::result_of<F( left_val_t )>::type,                               \
        typename op_t = ::react::detail::function_op<S, F, left_op_t>>                             \
    auto operator op( temp_signal<D, left_val_t, left_op_t>&& lhs, Tm_right_valIn&& rhs )          \
        ->temp_signal<D, S, op_t>                                                                  \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F( std::forward<Tm_right_valIn>( rhs ) ), lhs.steal_op() ) );                      \
    }                                                                                              \
                                                                                                   \
    template <typename Tm_left_valIn,                                                              \
        typename D,                                                                                \
        typename right_val_t,                                                                      \
        typename right_op_t,                                                                       \
        typename left_val_t = typename std::decay<Tm_left_valIn>::type,                            \
        class = typename std::enable_if<!is_signal<left_val_t>::value>::type,                      \
        typename F = name##_op_r_functor<left_val_t, right_val_t>,                                 \
        typename S = typename std::result_of<F( right_val_t )>::type,                              \
        typename op_t = ::react::detail::function_op<S, F, right_op_t>>                            \
    auto operator op( Tm_left_valIn&& lhs, temp_signal<D, right_val_t, right_op_t>&& rhs )         \
        ->temp_signal<D, S, op_t>                                                                  \
    {                                                                                              \
        return temp_signal<D, S, op_t>(                                                            \
            std::make_shared<::react::detail::signal_op_node<D, S, op_t>>(                         \
                F( std::forward<Tm_left_valIn>( lhs ) ), rhs.steal_op() ) );                       \
    }

REACT_DECLARE_OP( +, addition )

REACT_DECLARE_OP( -, subtraction )

REACT_DECLARE_OP( *, multiplication )

REACT_DECLARE_OP( /, division )

REACT_DECLARE_OP( %, modulo )

REACT_DECLARE_OP( ==, equal )

REACT_DECLARE_OP( !=, not_equal )

REACT_DECLARE_OP( <, less )

REACT_DECLARE_OP( <=, less_equal )

REACT_DECLARE_OP( >, greater )

REACT_DECLARE_OP( >=, greater_equal )

REACT_DECLARE_OP( &&, logical_and )

REACT_DECLARE_OP( ||, logical_or )

REACT_DECLARE_OP( &, bitwise_and )

REACT_DECLARE_OP( |, bitwise_or )

REACT_DECLARE_OP( ^, bitwise_xor )

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to create signal pack from 2 signals.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename left_val_t, typename right_val_t>
auto operator,( const signal<D, left_val_t>& a, const signal<D, right_val_t>& b )
                  -> signal_pack<D, left_val_t, right_val_t>
{
    return signal_pack<D, left_val_t, right_val_t>( a, b );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to append node to existing signal pack.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... cur_values_t, typename append_value_t>
auto operator,( const signal_pack<D, cur_values_t...>& cur,
    const signal<D, append_value_t>& append ) -> signal_pack<D, cur_values_t..., append_value_t>
{
    return signal_pack<D, cur_values_t..., append_value_t>( cur, append );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// operator->* overload to connect signals to a function and return the resulting signal.
///////////////////////////////////////////////////////////////////////////////////////////////////
// Single arg
template <typename D,
    typename F,
    template <typename D_, typename V_>
    class TSignal,
    typename TValue,
    class = typename std::enable_if<is_signal<TSignal<D, TValue>>::value>::type>
auto operator->*( const TSignal<D, TValue>& arg, F&& func )
    -> signal<D, typename std::result_of<F( TValue )>::type>
{
    return ::react::make_signal( arg, std::forward<F>( func ) );
}

// Multiple args
template <typename D, typename F, typename... values_t>
auto operator->*( const signal_pack<D, values_t...>& arg_pack, F&& func )
    -> signal<D, typename std::result_of<F( values_t... )>::type>
{
    return ::react::make_signal( arg_pack, std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename inner_value_t>
auto flatten( const signal<D, signal<D, inner_value_t>>& outer ) -> signal<D, inner_value_t>
{
    return signal<D, inner_value_t>(
        std::make_shared<::react::detail::flatten_node<D, signal<D, inner_value_t>, inner_value_t>>(
            get_node_ptr( outer ), get_node_ptr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal : public ::react::detail::signal_base<D, S>
{
private:
    using node_t = ::react::detail::signal_node<D, S>;

public:
    using value_t = S;

    // Default ctor
    signal() = default;

    // Copy ctor
    signal( const signal& ) = default;

    // Move ctor
    signal( signal&& other ) noexcept
        : signal::signal_base( std::move( other ) )
    {}

    // Node ctor
    explicit signal( std::shared_ptr<node_t>&& node_ptr )
        : signal::signal_base( std::move( node_ptr ) )
    {}

    // Copy assignment
    signal& operator=( const signal& ) = default;

    // Move assignment
    signal& operator=( signal&& other ) noexcept
    {
        signal::signal_base::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return signal::signal_base::get_value();
    }

    const S& operator()() const
    {
        return signal::signal_base::get_value();
    }

    bool equals( const signal& other ) const
    {
        return signal::signal_base::equals( other );
    }

    bool is_valid() const
    {
        return signal::signal_base::is_valid();
    }

    S flatten() const
    {
        static_assert( is_signal<S>::value || is_event<S>::value,
            "flatten requires a signal or events value type." );
        return ::react::flatten( *this );
    }
};

// Specialize for references
template <typename D, typename S>
class signal<D, S&> : public ::react::detail::signal_base<D, std::reference_wrapper<S>>
{
private:
    using node_t = ::react::detail::signal_node<D, std::reference_wrapper<S>>;

public:
    using value_t = S;

    // Default ctor
    signal() = default;

    // Copy ctor
    signal( const signal& ) = default;

    // Move ctor
    signal( signal&& other ) noexcept
        : signal::signal_base( std::move( other ) )
    {}

    // Node ctor
    explicit signal( std::shared_ptr<node_t>&& node_ptr )
        : signal::signal_base( std::move( node_ptr ) )
    {}

    // Copy assignment
    signal& operator=( const signal& ) = default;

    // Move assignment
    signal& operator=( signal&& other ) noexcept
    {
        signal::signal_base::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return signal::signal_base::get_value();
    }

    const S& operator()() const
    {
        return signal::signal_base::get_value();
    }

    bool equals( const signal& other ) const
    {
        return signal::signal_base::equals( other );
    }

    bool is_valid() const
    {
        return signal::signal_base::is_valid();
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// var_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class var_signal : public signal<D, S>
{
private:
    using node_t = ::react::detail::var_node<D, S>;

public:
    // Default ctor
    var_signal() = default;

    // Copy ctor
    var_signal( const var_signal& ) = default;

    // Move ctor
    var_signal( var_signal&& other ) noexcept
        : var_signal::signal( std::move( other ) )
    {}

    // Node ctor
    explicit var_signal( std::shared_ptr<node_t>&& node_ptr )
        : var_signal::signal( std::move( node_ptr ) )
    {}

    // Copy assignment
    var_signal& operator=( const var_signal& ) = default;

    // Move assignment
    var_signal& operator=( var_signal&& other ) noexcept
    {
        var_signal::signal_base::operator=( std::move( other ) );
        return *this;
    }

    void Set( const S& new_value ) const
    {
        var_signal::signal_base::set_value( new_value );
    }

    void Set( S&& new_value ) const
    {
        var_signal::signal_base::set_value( std::move( new_value ) );
    }

    const var_signal& operator<<=( const S& new_value ) const
    {
        var_signal::signal_base::set_value( new_value );
        return *this;
    }

    const var_signal& operator<<=( S&& new_value ) const
    {
        var_signal::signal_base::set_value( std::move( new_value ) );
        return *this;
    }

    template <typename F>
    void Modify( const F& func ) const
    {
        var_signal::signal_base::modify_value( func );
    }
};

// Specialize for references
template <typename D, typename S>
class var_signal<D, S&> : public signal<D, std::reference_wrapper<S>>
{
private:
    using node_t = ::react::detail::var_node<D, std::reference_wrapper<S>>;

public:
    using value_t = S;

    // Default ctor
    var_signal() = default;

    // Copy ctor
    var_signal( const var_signal& ) = default;

    // Move ctor
    var_signal( var_signal&& other ) noexcept
        : var_signal::signal( std::move( other ) )
    {}

    // Node ctor
    explicit var_signal( std::shared_ptr<node_t>&& node_ptr )
        : var_signal::signal( std::move( node_ptr ) )
    {}

    // Copy assignment
    var_signal& operator=( const var_signal& ) = default;

    // Move assignment
    var_signal& operator=( var_signal&& other ) noexcept
    {
        var_signal::signal::operator=( std::move( other ) );
        return *this;
    }

    void Set( std::reference_wrapper<S> new_value ) const
    {
        var_signal::signal_base::set_value( new_value );
    }

    const var_signal& operator<<=( std::reference_wrapper<S> new_value ) const
    {
        var_signal::signal_base::set_value( new_value );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// temp_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename op_t>
class temp_signal : public signal<D, S>
{
private:
    using node_t = ::react::detail::signal_op_node<D, S, op_t>;

public:
    // Default ctor
    temp_signal() = default;

    // Copy ctor
    temp_signal( const temp_signal& ) = default;

    // Move ctor
    temp_signal( temp_signal&& other ) noexcept
        : temp_signal::signal( std::move( other ) )
    {}

    // Node ctor
    explicit temp_signal( std::shared_ptr<node_t>&& ptr )
        : temp_signal::signal( std::move( ptr ) )
    {}

    // Copy assignment
    temp_signal& operator=( const temp_signal& ) = default;

    // Move assignment
    temp_signal& operator=( temp_signal&& other ) noexcept
    {
        temp_signal::signal::operator=( std::move( other ) );
        return *this;
    }

    op_t steal_op()
    {
        return std::move( reinterpret_cast<node_t*>( this->m_ptr.get() )->steal_op() );
    }
};

namespace detail
{

template <typename D, typename L, typename R>
bool equals( const signal<D, L>& lhs, const signal<D, R>& rhs )
{
    return lhs.equals( rhs );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// flatten macros
///////////////////////////////////////////////////////////////////////////////////////////////////
// Note: Using static_cast rather than -> return type, because when using lambda for inline
// class initialization, decltype did not recognize the parameter r
// Note2: MSVC doesn't like typename in the lambda
#if _MSC_VER && !__INTEL_COMPILER
#    define REACT_MSVC_NO_TYPENAME
#else
#    define REACT_MSVC_NO_TYPENAME typename
#endif

#define REACTIVE_REF( obj, name )                                                                  \
    flatten( make_signal( obj,                                                                     \
        []( const REACT_MSVC_NO_TYPENAME ::react::detail::type_identity<decltype(                  \
                obj )>::type::value_t& r ) {                                                       \
            using T = decltype( r.name );                                                          \
            using S = REACT_MSVC_NO_TYPENAME ::react::decay_input<T>::type;                        \
            return static_cast<S>( r.name );                                                       \
        } ) )

#define REACTIVE_PTR( obj, name )                                                                  \
    flatten( make_signal( obj,                                                                     \
        []( REACT_MSVC_NO_TYPENAME ::react::detail::type_identity<decltype( obj )>::type::value_t  \
                r ) {                                                                              \
            assert( r != nullptr );                                                                \
            using T = decltype( r->name );                                                         \
            using S = REACT_MSVC_NO_TYPENAME ::react::decay_input<T>::type;                        \
            return static_cast<S>( r->name );                                                      \
        } ) )

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_stream_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class event_stream_node : public observable_node<D>
{
public:
    using data_t = std::vector<E>;
    using engine_t = typename D::engine;
    using turn_t = typename engine_t::turn_t;

    event_stream_node() = default;

    void set_current_turn( const turn_t& turn, bool force_update = false, bool no_clear = false )
    {
        if( m_cur_turn_id != turn.id() || force_update )
        {
            m_cur_turn_id = turn.id();
            if( !no_clear )
            {
                m_events.clear();
            }
        }
    }

    data_t& events()
    {
        return m_events;
    }

protected:
    data_t m_events;

private:
    unsigned m_cur_turn_id{ ( std::numeric_limits<unsigned>::max )() };
};

template <typename D, typename E>
using event_stream_node_ptr_t = std::shared_ptr<event_stream_node<D, E>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_source_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class event_source_node
    : public event_stream_node<D, E>
    , public input_node_interface
{
    using engine = typename event_source_node::engine;

public:
    event_source_node()
        : event_source_node::event_stream_node{}
    {}

    ~event_source_node() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked event_source_node" );
    }

    template <typename V>
    void add_input( V&& v )
    {
        // Clear input from previous turn
        if( m_changed_flag )
        {
            m_changed_flag = false;
            this->m_events.clear();
        }

        this->m_events.push_back( std::forward<V>( v ) );
    }

    bool apply_input( void* turn_ptr ) override
    {
        if( this->m_events.size() > 0 && !m_changed_flag )
        {
            using turn_t = typename D::engine::turn_t;
            turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

            this->set_current_turn( turn, true, true );
            m_changed_flag = true;
            engine::on_input_change( *this );
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    bool m_changed_flag = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_merge_op
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename... deps_t>
class event_merge_op : public reactive_op_base<deps_t...>
{
public:
    template <typename... deps_in_t>
    event_merge_op( deps_in_t&&... deps )
        : event_merge_op::reactive_op_base( dont_move(), std::forward<deps_in_t>( deps )... )
    {}

    event_merge_op( event_merge_op&& other ) noexcept
        : event_merge_op::reactive_op_base( std::move( other ) )
    {}

    template <typename turn_t_, typename collector_t>
    void collect( const turn_t_& turn, const collector_t& collector ) const
    {
        apply( collect_functor<turn_t_, collector_t>( turn, collector ), this->m_deps );
    }

    template <typename turn_t_, typename collector_t, typename functor_t>
    void collect_rec( const functor_t& functor ) const
    {
        apply( reinterpret_cast<const collect_functor<turn_t_, collector_t>&>( functor ),
            this->m_deps );
    }

private:
    template <typename turn_t_, typename collector_t>
    struct collect_functor
    {
        collect_functor( const turn_t_& turn, const collector_t& collector )
            : m_turn( turn )
            , m_collector( collector )
        {}

        void operator()( const deps_t&... deps ) const
        {
            REACT_EXPAND_PACK( collect( deps ) );
        }

        template <typename T>
        void collect( const T& op ) const
        {
            op.template collect_rec<turn_t_, collector_t>( *this );
        }

        template <typename T>
        void collect( const std::shared_ptr<T>& dep_ptr ) const
        {
            dep_ptr->set_current_turn( m_turn );

            for( const auto& v : dep_ptr->events() )
            {
                m_collector( v );
            }
        }

        const turn_t_& m_turn;
        const collector_t& m_collector;
    };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_filter_op
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename filter_t, typename dep_t>
class event_filter_op : public reactive_op_base<dep_t>
{
public:
    template <typename filter_in_t, typename dep_in_t>
    event_filter_op( filter_in_t&& filter, dep_in_t&& dep )
        : event_filter_op::reactive_op_base{ dont_move(), std::forward<dep_in_t>( dep ) }
        , m_filter( std::forward<filter_in_t>( filter ) )
    {}

    event_filter_op( event_filter_op&& other ) noexcept
        : event_filter_op::reactive_op_base{ std::move( other ) }
        , m_filter( std::move( other.m_filter ) )
    {}

    template <typename turn_t_, typename collector_t>
    void collect( const turn_t_& turn, const collector_t& collector ) const
    {
        collect_impl(
            turn, filtered_event_collector<collector_t>{ m_filter, collector }, get_dep() );
    }

    template <typename turn_t_, typename collector_t, typename functor_t>
    void collect_rec( const functor_t& functor ) const
    {
        // Can't recycle functor because m_func needs replacing
        collect<turn_t_, collector_t>( functor.m_turn, functor.m_collector );
    }

private:
    const dep_t& get_dep() const
    {
        return std::get<0>( this->m_deps );
    }

    template <typename collector_t>
    struct filtered_event_collector
    {
        filtered_event_collector( const filter_t& filter, const collector_t& collector )
            : m_filter( filter )
            , m_collector( collector )
        {}

        void operator()( const E& e ) const
        {
            // Accepted?
            if( m_filter( e ) )
            {
                m_collector( e );
            }
        }

        const filter_t& m_filter;
        const collector_t& m_collector; // The wrapped collector
    };

    template <typename turn_t_, typename collector_t, typename T>
    static void collect_impl( const turn_t_& turn, const collector_t& collector, const T& op )
    {
        op.collect( turn, collector );
    }

    template <typename turn_t_, typename collector_t, typename T>
    static void collect_impl(
        const turn_t_& turn, const collector_t& collector, const std::shared_ptr<T>& dep_ptr )
    {
        dep_ptr->set_current_turn( turn );

        for( const auto& v : dep_ptr->events() )
        {
            collector( v );
        }
    }

    filter_t m_filter;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_transform_op
///////////////////////////////////////////////////////////////////////////////////////////////////
// Todo: Refactor code duplication
template <typename E, typename func_t, typename dep_t>
class event_transform_op : public reactive_op_base<dep_t>
{
public:
    template <typename func_in_t, typename dep_in_t>
    event_transform_op( func_in_t&& func, dep_in_t&& dep )
        : event_transform_op::reactive_op_base( dont_move(), std::forward<dep_in_t>( dep ) )
        , m_func( std::forward<func_in_t>( func ) )
    {}

    event_transform_op( event_transform_op&& other ) noexcept
        : event_transform_op::reactive_op_base( std::move( other ) )
        , m_func( std::move( other.m_func ) )
    {}

    template <typename turn_t_, typename collector_t>
    void collect( const turn_t_& turn, const collector_t& collector ) const
    {
        collect_impl(
            turn, transform_event_collector<collector_t>( m_func, collector ), get_dep() );
    }

    template <typename turn_t_, typename collector_t, typename functor_t>
    void collect_rec( const functor_t& functor ) const
    {
        // Can't recycle functor because m_func needs replacing
        collect<turn_t_, collector_t>( functor.m_turn, functor.m_collector );
    }

private:
    const dep_t& get_dep() const
    {
        return std::get<0>( this->m_deps );
    }

    template <typename target_t>
    struct transform_event_collector
    {
        transform_event_collector( const func_t& func, const target_t& target )
            : m_func( func )
            , m_target( target )
        {}

        void operator()( const E& e ) const
        {
            m_target( m_func( e ) );
        }

        const func_t& m_func;
        const target_t& m_target;
    };

    template <typename turn_t_, typename collector_t, typename T>
    static void collect_impl( const turn_t_& turn, const collector_t& collector, const T& op )
    {
        op.collect( turn, collector );
    }

    template <typename turn_t_, typename collector_t, typename T>
    static void collect_impl(
        const turn_t_& turn, const collector_t& collector, const std::shared_ptr<T>& dep_ptr )
    {
        dep_ptr->set_current_turn( turn );

        for( const auto& v : dep_ptr->events() )
        {
            collector( v );
        }
    }

    func_t m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_op_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename op_t>
class event_op_node : public event_stream_node<D, E>
{
    using engine = typename event_op_node::engine;

public:
    template <typename... args_t>
    event_op_node( args_t&&... args )
        : event_op_node::event_stream_node()
        , m_op( std::forward<args_t>( args )... )
    {

        m_op.template attach<D>( *this );
    }

    ~event_op_node()
    {
        if( !m_was_op_stolen )
        {
            m_op.template detach<D>( *this );
        }
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );

        m_op.collect( turn, event_collector( this->m_events ) );

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

    op_t steal_op()
    {
        assert( !m_was_op_stolen && "Op was already stolen." );
        m_was_op_stolen = true;
        m_op.template detach<D>( *this );
        return std::move( m_op );
    }

private:
    struct event_collector
    {
        using data_t = typename event_op_node::data_t;

        event_collector( data_t& events )
            : m_events( events )
        {}

        void operator()( const E& e ) const
        {
            m_events.push_back( e );
        }

        data_t& m_events;
    };

    op_t m_op;
    bool m_was_op_stolen = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_flatten_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename outer_t, typename inner_t>
class event_flatten_node : public event_stream_node<D, inner_t>
{
    using engine = typename event_flatten_node::engine;

public:
    event_flatten_node( const std::shared_ptr<signal_node<D, outer_t>>& outer,
        const std::shared_ptr<event_stream_node<D, inner_t>>& inner )
        : event_flatten_node::event_stream_node()
        , m_outer( outer )
        , m_inner( inner )
    {

        engine::on_node_attach( *this, *m_outer );
        engine::on_node_attach( *this, *m_inner );
    }

    ~event_flatten_node()
    {
        engine::on_node_detach( *this, *m_outer );
        engine::on_node_detach( *this, *m_inner );
    }

    void tick( void* turn_ptr ) override
    {
        typedef typename D::engine::turn_t turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );
        m_inner->set_current_turn( turn );

        auto new_inner = get_node_ptr( m_outer->value_ref() );

        if( new_inner != m_inner )
        {
            new_inner->set_current_turn( turn );

            // Topology has been changed
            auto m_old_inner = m_inner;
            m_inner = new_inner;

            engine::on_dynamic_node_detach( *this, *m_old_inner );
            engine::on_dynamic_node_attach( *this, *new_inner );

            return;
        }

        this->m_events.insert(
            this->m_events.end(), m_inner->events().begin(), m_inner->events().end() );

        if( this->m_events.size() > 0 )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<signal_node<D, outer_t>> m_outer;
    std::shared_ptr<event_stream_node<D, inner_t>> m_inner;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_event_transform_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename in_t, typename out_t, typename func_t, typename... dep_values_t>
class synced_event_transform_node : public event_stream_node<D, out_t>
{
    using engine = typename synced_event_transform_node::engine;

public:
    template <typename F>
    synced_event_transform_node( const std::shared_ptr<event_stream_node<D, in_t>>& source,
        F&& func,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_event_transform_node::event_stream_node()
        , m_source( source )
        , m_func( std::forward<F>( func ) )
        , m_deps( deps... )
    {

        engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_event_transform_node()
    {
        engine::on_node_detach( *this, *m_source );

        apply( detach_functor<D,
                   synced_event_transform_node,
                   std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
            m_deps );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        m_source->set_current_turn( turn );

        // Don't time if there is nothing to do
        if( !m_source->events().empty() )
        {
            for( const auto& e : m_source->events() )
            {
                this->m_events.push_back( apply(
                    [this, &e]( const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                        return m_func( e, args->value_ref()... );
                    },
                    m_deps ) );
            }
        }

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::shared_ptr<event_stream_node<D, in_t>> m_source;

    func_t m_func;
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_event_filter_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename func_t, typename... dep_values_t>
class synced_event_filter_node : public event_stream_node<D, E>
{
    using engine = typename synced_event_filter_node::engine;

public:
    template <typename F>
    synced_event_filter_node( const std::shared_ptr<event_stream_node<D, E>>& source,
        F&& filter,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_event_filter_node::event_stream_node()
        , m_source( source )
        , m_filter( std::forward<F>( filter ) )
        , m_deps( deps... )
    {

        engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_event_filter_node()
    {
        engine::on_node_detach( *this, *m_source );

        apply( detach_functor<D,
                   synced_event_filter_node,
                   std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
            m_deps );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        m_source->set_current_turn( turn );

        // Don't time if there is nothing to do
        if( !m_source->events().empty() )
        {
            for( const auto& e : m_source->events() )
            {
                if( apply(
                        [this, &e]( const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                            return m_filter( e, args->value_ref()... );
                        },
                        m_deps ) )
                {
                    this->m_events.push_back( e );
                }
            }
        }

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::shared_ptr<event_stream_node<D, E>> m_source;

    func_t m_filter;
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_processing_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename in_t, typename out_t, typename func_t>
class event_processing_node : public event_stream_node<D, out_t>
{
    using engine = typename event_processing_node::engine;

public:
    template <typename F>
    event_processing_node( const std::shared_ptr<event_stream_node<D, in_t>>& source, F&& func )
        : event_processing_node::event_stream_node()
        , m_source( source )
        , m_func( std::forward<F>( func ) )
    {

        engine::on_node_attach( *this, *source );
    }

    ~event_processing_node()
    {
        engine::on_node_detach( *this, *m_source );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );

        m_func( event_range<in_t>( m_source->events() ), std::back_inserter( this->m_events ) );

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<event_stream_node<D, in_t>> m_source;

    func_t m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_event_processing_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename in_t, typename out_t, typename func_t, typename... dep_values_t>
class synced_event_processing_node : public event_stream_node<D, out_t>
{
    using engine = typename synced_event_processing_node::engine;

public:
    template <typename F>
    synced_event_processing_node( const std::shared_ptr<event_stream_node<D, in_t>>& source,
        F&& func,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_event_processing_node::event_stream_node()
        , m_source( source )
        , m_func( std::forward<F>( func ) )
        , m_deps( deps... )
    {

        engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_event_processing_node()
    {
        engine::on_node_detach( *this, *m_source );

        apply( detach_functor<D,
                   synced_event_processing_node,
                   std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
            m_deps );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        m_source->set_current_turn( turn );

        // Don't time if there is nothing to do
        if( !m_source->events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                    m_func( event_range<in_t>( m_source->events() ),
                        std::back_inserter( this->m_events ),
                        args->value_ref()... );
                },
                m_deps );
        }

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::shared_ptr<event_stream_node<D, in_t>> m_source;

    func_t m_func;
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_join_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... values_t>
class event_join_node : public event_stream_node<D, std::tuple<values_t...>>
{
    using engine = typename event_join_node::engine;
    using turn_t = typename engine::turn_t;

public:
    event_join_node( const std::shared_ptr<event_stream_node<D, values_t>>&... sources )
        : event_join_node::event_stream_node()
        , m_slots( sources... )
    {
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *sources ) );
    }

    ~event_join_node()
    {
        apply(
            [this]( slot<values_t>&... slots ) {
                REACT_EXPAND_PACK( engine::on_node_detach( *this, *slots.source ) );
            },
            m_slots );
    }

    void tick( void* turn_ptr ) override
    {
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );

        {
            // Move events into buffers
            apply(
                [this, &turn]( slot<values_t>&... slots ) {
                    REACT_EXPAND_PACK( fetch_buffer( turn, slots ) );
                },
                m_slots );

            while( true )
            {
                bool is_ready = true;

                // All slots ready?
                apply(
                    [this, &is_ready]( slot<values_t>&... slots ) {
                        // Todo: combine return values instead
                        REACT_EXPAND_PACK( check_slot( slots, is_ready ) );
                    },
                    m_slots );

                if( !is_ready )
                {
                    break;
                }

                // Pop values from buffers and emit tuple
                apply(
                    [this]( slot<values_t>&... slots ) {
                        this->m_events.emplace_back( slots.buffer.front()... );
                        REACT_EXPAND_PACK( slots.buffer.pop_front() );
                    },
                    m_slots );
            }
        }

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    template <typename T>
    struct slot
    {
        slot( const std::shared_ptr<event_stream_node<D, T>>& src )
            : source( src )
        {}

        std::shared_ptr<event_stream_node<D, T>> source;
        std::deque<T> buffer;
    };

    template <typename T>
    static void fetch_buffer( turn_t& turn, slot<T>& slot )
    {
        slot.source->set_current_turn( turn );

        slot.buffer.insert(
            slot.buffer.end(), slot.source->events().begin(), slot.source->events().end() );
    }

    template <typename T>
    static void check_slot( slot<T>& slot, bool& is_ready )
    {
        auto t = is_ready && !slot.buffer.empty();
        is_ready = t;
    }

    std::tuple<slot<values_t>...> m_slots;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_stream_base
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class event_stream_base : public copyable_reactive<event_stream_node<D, E>>
{
public:
    event_stream_base() = default;
    event_stream_base( const event_stream_base& ) = default;

    template <typename T>
    event_stream_base( T&& t )
        : event_stream_base::copyable_reactive( std::forward<T>( t ) )
    {}

protected:
    template <typename T>
    void emit( T&& e ) const
    {
        domain_specific_input_manager<D>::instance().add_input(
            *reinterpret_cast<event_source_node<D, E>*>( this->m_ptr.get() ),
            std::forward<T>( e ) );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// make_event_source
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
auto make_event_source() -> event_source<D, E>
{
    using ::react::detail::event_source_node;

    return event_source<D, E>( std::make_shared<event_source_node<D, E>>() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename TArg1,
    typename... args_t,
    typename E = TArg1,
    typename op_t = ::react::detail::event_merge_op<E,
        ::react::detail::event_stream_node_ptr_t<D, TArg1>,
        ::react::detail::event_stream_node_ptr_t<D, args_t>...>>
auto merge( const events<D, TArg1>& arg1, const events<D, args_t>&... args )
    -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    static_assert( sizeof...( args_t ) > 0, "merge: 2+ arguments are required." );

    return temp_events<D, E, op_t>( std::make_shared<event_op_node<D, E, op_t>>(
        get_node_ptr( arg1 ), get_node_ptr( args )... ) );
}

template <typename left_events_t,
    typename right_events_t,
    typename D = typename left_events_t::domain_t,
    typename left_val_t = typename left_events_t::value_t,
    typename right_val_t = typename right_events_t::value_t,
    typename E = left_val_t,
    typename op_t = ::react::detail::event_merge_op<E,
        ::react::detail::event_stream_node_ptr_t<D, left_val_t>,
        ::react::detail::event_stream_node_ptr_t<D, right_val_t>>,
    class = typename std::enable_if<is_event<left_events_t>::value>::type,
    class = typename std::enable_if<is_event<right_events_t>::value>::type>
auto operator|( const left_events_t& lhs, const right_events_t& rhs ) -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_t>(
        std::make_shared<event_op_node<D, E, op_t>>( get_node_ptr( lhs ), get_node_ptr( rhs ) ) );
}

template <typename D,
    typename left_val_t,
    typename left_op_t,
    typename right_val_t,
    typename right_op_t,
    typename E = left_val_t,
    typename op_t = ::react::detail::event_merge_op<E, left_op_t, right_op_t>>
auto operator|( temp_events<D, left_val_t, left_op_t>&& lhs,
    temp_events<D, right_val_t, right_op_t>&& rhs ) -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_t>(
        std::make_shared<event_op_node<D, E, op_t>>( lhs.steal_op(), rhs.steal_op() ) );
}

template <typename D,
    typename left_val_t,
    typename left_op_t,
    typename right_events_t,
    typename right_val_t = typename right_events_t::value_t,
    typename E = left_val_t,
    typename op_t = ::react::detail::
        event_merge_op<E, left_op_t, ::react::detail::event_stream_node_ptr_t<D, right_val_t>>,
    class = typename std::enable_if<is_event<right_events_t>::value>::type>
auto operator|( temp_events<D, left_val_t, left_op_t>&& lhs, const right_events_t& rhs )
    -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_t>(
        std::make_shared<event_op_node<D, E, op_t>>( lhs.steal_op(), get_node_ptr( rhs ) ) );
}

template <typename left_events_t,
    typename D,
    typename right_val_t,
    typename right_op_t,
    typename left_val_t = typename left_events_t::value_t,
    typename E = left_val_t,
    typename op_t = ::react::detail::
        event_merge_op<E, ::react::detail::event_stream_node_ptr_t<D, right_val_t>, right_op_t>,
    class = typename std::enable_if<is_event<left_events_t>::value>::type>
auto operator|( const left_events_t& lhs, temp_events<D, right_val_t, right_op_t>&& rhs )
    -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_t>(
        std::make_shared<event_op_node<D, E, op_t>>( get_node_ptr( lhs ), rhs.steal_op() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename op_t
    = ::react::detail::event_filter_op<E, F, ::react::detail::event_stream_node_ptr_t<D, E>>>
auto filter( const events<D, E>& src, f_in_t&& filter ) -> temp_events<D, E, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_t>( std::make_shared<event_op_node<D, E, op_t>>(
        std::forward<f_in_t>( filter ), get_node_ptr( src ) ) );
}

template <typename D,
    typename E,
    typename op_in_t,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename op_out_t = ::react::detail::event_filter_op<E, F, op_in_t>>
auto filter( temp_events<D, E, op_in_t>&& src, f_in_t&& filter ) -> temp_events<D, E, op_out_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, E, op_out_t>( std::make_shared<event_op_node<D, E, op_out_t>>(
        std::forward<f_in_t>( filter ), src.steal_op() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// filter - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename f_in_t, typename... dep_values_t>
auto filter(
    const events<D, E>& source, const signal_pack<D, dep_values_t...>& dep_pack, f_in_t&& func )
    -> events<D, E>
{
    using ::react::detail::synced_event_filter_node;

    using F = typename std::decay<f_in_t>::type;

    struct node_builder
    {
        node_builder( const events<D, E>& source, f_in_t&& func )
            : m_source( source )
            , m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, dep_values_t>&... deps ) -> events<D, E>
        {
            return events<D, E>(
                std::make_shared<synced_event_filter_node<D, E, F, dep_values_t...>>(
                    get_node_ptr( m_source ),
                    std::forward<f_in_t>( m_func ),
                    get_node_ptr( deps )... ) );
        }

        const events<D, E>& m_source;
        f_in_t m_func;
    };

    return ::react::detail::apply(
        node_builder( source, std::forward<f_in_t>( func ) ), dep_pack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename in_t,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename out_t = typename std::result_of<F( in_t )>::type,
    typename op_t = ::react::detail::
        event_transform_op<in_t, F, ::react::detail::event_stream_node_ptr_t<D, in_t>>>
auto transform( const events<D, in_t>& src, f_in_t&& func ) -> temp_events<D, out_t, op_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, out_t, op_t>( std::make_shared<event_op_node<D, out_t, op_t>>(
        std::forward<f_in_t>( func ), get_node_ptr( src ) ) );
}

template <typename D,
    typename in_t,
    typename op_in_t,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type,
    typename out_t = typename std::result_of<F( in_t )>::type,
    typename op_out_t = ::react::detail::event_transform_op<in_t, F, op_in_t>>
auto transform( temp_events<D, in_t, op_in_t>&& src, f_in_t&& func )
    -> temp_events<D, out_t, op_out_t>
{
    using ::react::detail::event_op_node;

    return temp_events<D, out_t, op_out_t>( std::make_shared<event_op_node<D, out_t, op_out_t>>(
        std::forward<f_in_t>( func ), src.steal_op() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// transform - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename in_t,
    typename f_in_t,
    typename... dep_values_t,
    typename out_t = typename std::result_of<f_in_t( in_t, dep_values_t... )>::type>
auto transform(
    const events<D, in_t>& source, const signal_pack<D, dep_values_t...>& dep_pack, f_in_t&& func )
    -> events<D, out_t>
{
    using ::react::detail::synced_event_transform_node;

    using F = typename std::decay<f_in_t>::type;

    struct node_builder
    {
        node_builder( const events<D, in_t>& source, f_in_t&& func )
            : m_source( source )
            , m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, dep_values_t>&... deps ) -> events<D, out_t>
        {
            return events<D, out_t>(
                std::make_shared<synced_event_transform_node<D, in_t, out_t, F, dep_values_t...>>(
                    get_node_ptr( m_source ),
                    std::forward<f_in_t>( m_func ),
                    get_node_ptr( deps )... ) );
        }

        const events<D, in_t>& m_source;
        f_in_t m_func;
    };

    return ::react::detail::apply(
        node_builder( source, std::forward<f_in_t>( func ) ), dep_pack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process
///////////////////////////////////////////////////////////////////////////////////////////////////
using ::react::detail::event_emitter;
using ::react::detail::event_range;

template <typename out_t,
    typename D,
    typename in_t,
    typename f_in_t,
    typename F = typename std::decay<f_in_t>::type>
auto Process( const events<D, in_t>& src, f_in_t&& func ) -> events<D, out_t>
{
    using ::react::detail::event_processing_node;

    return events<D, out_t>( std::make_shared<event_processing_node<D, in_t, out_t, F>>(
        get_node_ptr( src ), std::forward<f_in_t>( func ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename out_t, typename D, typename in_t, typename f_in_t, typename... dep_values_t>
auto Process(
    const events<D, in_t>& source, const signal_pack<D, dep_values_t...>& dep_pack, f_in_t&& func )
    -> events<D, out_t>
{
    using ::react::detail::synced_event_processing_node;

    using F = typename std::decay<f_in_t>::type;

    struct node_builder
    {
        node_builder( const events<D, in_t>& source, f_in_t&& func )
            : m_source( source )
            , m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, dep_values_t>&... deps ) -> events<D, out_t>
        {
            return events<D, out_t>(
                std::make_shared<synced_event_processing_node<D, in_t, out_t, F, dep_values_t...>>(
                    get_node_ptr( m_source ),
                    std::forward<f_in_t>( m_func ),
                    get_node_ptr( deps )... ) );
        }

        const events<D, in_t>& m_source;
        f_in_t m_func;
    };

    return ::react::detail::apply(
        node_builder( source, std::forward<f_in_t>( func ) ), dep_pack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename inner_value_t>
auto flatten( const signal<D, events<D, inner_value_t>>& outer ) -> events<D, inner_value_t>
{
    return events<D, inner_value_t>( std::make_shared<
        ::react::detail::event_flatten_node<D, events<D, inner_value_t>, inner_value_t>>(
        get_node_ptr( outer ), get_node_ptr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... args_t>
auto Join( const events<D, args_t>&... args ) -> events<D, std::tuple<args_t...>>
{
    using ::react::detail::event_join_node;

    static_assert( sizeof...( args_t ) > 1, "Join: 2+ arguments are required." );

    return events<D, std::tuple<args_t...>>(
        std::make_shared<event_join_node<D, args_t...>>( get_node_ptr( args )... ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// token
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class token
{
    value
};

struct tokenizer
{
    template <typename T>
    token operator()( const T& ) const
    {
        return token::value;
    }
};

template <typename events_t>
auto tokenize( events_t&& source ) -> decltype( transform( source, tokenizer{} ) )
{
    return transform( source, tokenizer{} );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
class events : public ::react::detail::event_stream_base<D, E>
{
private:
    using node_t = ::react::detail::event_stream_node<D, E>;
    using node_ptr_T = std::shared_ptr<node_t>;

public:
    using value_t = E;

    // Default ctor
    events() = default;

    // Copy ctor
    events( const events& ) = default;

    // Move ctor
    events( events&& other ) noexcept
        : events::event_stream_base( std::move( other ) )
    {}

    // Node ctor
    explicit events( node_ptr_T&& node_ptr )
        : events::event_stream_base( std::move( node_ptr ) )
    {}

    // Copy assignment
    events& operator=( const events& ) = default;

    // Move assignment
    events& operator=( events&& other ) noexcept
    {
        events::event_stream_base::operator=( std::move( other ) );
        return *this;
    }

    bool equals( const events& other ) const
    {
        return events::event_stream_base::equals( other );
    }

    bool is_valid() const
    {
        return events::event_stream_base::is_valid();
    }

    auto tokenize() const -> decltype( ::react::tokenize( std::declval<events>() ) )
    {
        return ::react::tokenize( *this );
    }

    template <typename... args_t>
    auto merge( args_t&&... args ) const
        -> decltype( ::react::merge( std::declval<events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto filter( F&& f ) const
        -> decltype( ::react::filter( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto transform( F&& f ) const
        -> decltype( ::react::transform( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::transform( *this, std::forward<F>( f ) );
    }
};

// Specialize for references
template <typename D, typename E>
class events<D, E&> : public ::react::detail::event_stream_base<D, std::reference_wrapper<E>>
{
private:
    using node_t = ::react::detail::event_stream_node<D, std::reference_wrapper<E>>;
    using node_ptr_T = std::shared_ptr<node_t>;

public:
    using value_t = E;

    // Default ctor
    events() = default;

    // Copy ctor
    events( const events& ) = default;

    // Move ctor
    events( events&& other ) noexcept
        : events::event_stream_base( std::move( other ) )
    {}

    // Node ctor
    explicit events( node_ptr_T&& node_ptr )
        : events::event_stream_base( std::move( node_ptr ) )
    {}

    // Copy assignment
    events& operator=( const events& ) = default;

    // Move assignment
    events& operator=( events&& other ) noexcept
    {
        events::event_stream_base::operator=( std::move( other ) );
        return *this;
    }

    bool equals( const events& other ) const
    {
        return events::event_stream_base::equals( other );
    }

    bool is_valid() const
    {
        return events::event_stream_base::is_valid();
    }

    auto tokenize() const -> decltype( ::react::tokenize( std::declval<events>() ) )
    {
        return ::react::tokenize( *this );
    }

    template <typename... args_t>
    auto merge( args_t&&... args )
        -> decltype( ::react::merge( std::declval<events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto filter( F&& f ) const
        -> decltype( ::react::filter( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto transform( F&& f ) const
        -> decltype( ::react::transform( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::transform( *this, std::forward<F>( f ) );
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_source
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
class event_source : public events<D, E>
{
private:
    using node_t = ::react::detail::event_source_node<D, E>;
    using node_ptr_T = std::shared_ptr<node_t>;

public:
    // Default ctor
    event_source() = default;

    // Copy ctor
    event_source( const event_source& ) = default;

    // Move ctor
    event_source( event_source&& other ) noexcept
        : event_source::events( std::move( other ) )
    {}

    // Node ctor
    explicit event_source( node_ptr_T&& node_ptr )
        : event_source::events( std::move( node_ptr ) )
    {}

    // Copy assignment
    event_source& operator=( const event_source& ) = default;

    // Move assignment
    event_source& operator=( event_source&& other ) noexcept
    {
        event_source::events::operator=( std::move( other ) );
        return *this;
    }

    // Explicit emit
    void emit( const E& e ) const
    {
        event_source::event_stream_base::emit( e );
    }

    void emit( E&& e ) const
    {
        event_source::event_stream_base::emit( std::move( e ) );
    }

    void emit() const
    {
        static_assert( std::is_same<E, token>::value, "Can't emit on non token stream." );
        event_source::event_stream_base::emit( token::value );
    }

    // Function object style
    void operator()( const E& e ) const
    {
        event_source::event_stream_base::emit( e );
    }

    void operator()( E&& e ) const
    {
        event_source::event_stream_base::emit( std::move( e ) );
    }

    void operator()() const
    {
        static_assert( std::is_same<E, token>::value, "Can't emit on non token stream." );
        event_source::event_stream_base::emit( token::value );
    }

    // Stream style
    const event_source& operator<<( const E& e ) const
    {
        event_source::event_stream_base::emit( e );
        return *this;
    }

    const event_source& operator<<( E&& e ) const
    {
        event_source::event_stream_base::emit( std::move( e ) );
        return *this;
    }
};

// Specialize for references
template <typename D, typename E>
class event_source<D, E&> : public events<D, std::reference_wrapper<E>>
{
private:
    using node_t = ::react::detail::event_source_node<D, std::reference_wrapper<E>>;
    using node_ptr_T = std::shared_ptr<node_t>;

public:
    // Default ctor
    event_source() = default;

    // Copy ctor
    event_source( const event_source& ) = default;

    // Move ctor
    event_source( event_source&& other ) noexcept
        : event_source::events( std::move( other ) )
    {}

    // Node ctor
    explicit event_source( node_ptr_T&& node_ptr )
        : event_source::events( std::move( node_ptr ) )
    {}

    // Copy assignment
    event_source& operator=( const event_source& ) = default;

    // Move assignment
    event_source& operator=( event_source&& other ) noexcept
    {
        event_source::events::operator=( std::move( other ) );
        return *this;
    }

    // Explicit emit
    void emit( std::reference_wrapper<E> e ) const
    {
        event_source::event_stream_base::emit( e );
    }

    // Function object style
    void operator()( std::reference_wrapper<E> e ) const
    {
        event_source::event_stream_base::emit( e );
    }

    // Stream style
    const event_source& operator<<( std::reference_wrapper<E> e ) const
    {
        event_source::event_stream_base::emit( e );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// temp_events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename op_t>
class temp_events : public events<D, E>
{
protected:
    using node_t = ::react::detail::event_op_node<D, E, op_t>;
    using node_ptr_T = std::shared_ptr<node_t>;

public:
    // Default ctor
    temp_events() = default;

    // Copy ctor
    temp_events( const temp_events& ) = default;

    // Move ctor
    temp_events( temp_events&& other ) noexcept
        : temp_events::events( std::move( other ) )
    {}

    // Node ctor
    explicit temp_events( node_ptr_T&& node_ptr )
        : temp_events::events( std::move( node_ptr ) )
    {}

    // Copy assignment
    temp_events& operator=( const temp_events& ) = default;

    // Move assignment
    temp_events& operator=( temp_events&& other ) noexcept
    {
        temp_events::event_stream_base::operator=( std::move( other ) );
        return *this;
    }

    op_t steal_op()
    {
        return std::move( reinterpret_cast<node_t*>( this->m_ptr.get() )->steal_op() );
    }

    template <typename... args_t>
    auto merge( args_t&&... args ) -> decltype(
        ::react::merge( std::declval<temp_events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto filter( F&& f ) const
        -> decltype( ::react::filter( std::declval<temp_events>(), std::forward<F>( f ) ) )
    {
        return ::react::filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto transform( F&& f ) const
        -> decltype( ::react::transform( std::declval<temp_events>(), std::forward<F>( f ) ) )
    {
        return ::react::transform( *this, std::forward<F>( f ) );
    }
};

namespace detail
{

template <typename D, typename L, typename R>
bool equals( const events<D, L>& lhs, const events<D, R>& rhs )
{
    return lhs.equals( rhs );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// add_iterate_range_wrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename S, typename F, typename... args_t>
struct add_iterate_range_wrapper
{
    add_iterate_range_wrapper( const add_iterate_range_wrapper& other ) = default;

    add_iterate_range_wrapper( add_iterate_range_wrapper&& other ) noexcept
        : m_func( std::move( other.m_func ) )
    {}

    template <typename f_in_t,
        class = typename disable_if_same<f_in_t, add_iterate_range_wrapper>::type>
    explicit add_iterate_range_wrapper( f_in_t&& func )
        : m_func( std::forward<f_in_t>( func ) )
    {}

    S operator()( event_range<E> range, S value, const args_t&... args )
    {
        for( const auto& e : range )
        {
            value = m_func( e, value, args... );
        }

        return value;
    }

    F m_func;
};

template <typename E, typename S, typename F, typename... args_t>
struct add_iterate_by_ref_range_wrapper
{
    add_iterate_by_ref_range_wrapper( const add_iterate_by_ref_range_wrapper& other ) = default;

    add_iterate_by_ref_range_wrapper( add_iterate_by_ref_range_wrapper&& other ) noexcept
        : m_func( std::move( other.m_func ) )
    {}

    template <typename f_in_t,
        class = typename disable_if_same<f_in_t, add_iterate_by_ref_range_wrapper>::type>
    explicit add_iterate_by_ref_range_wrapper( f_in_t&& func )
        : m_func( std::forward<f_in_t>( func ) )
    {}

    void operator()( event_range<E> range, S& value_ref, const args_t&... args )
    {
        for( const auto& e : range )
        {
            m_func( e, value_ref, args... );
        }
    }

    F m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// iterate_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename func_t>
class iterate_node : public signal_node<D, S>
{
    using engine = typename iterate_node::engine;

public:
    template <typename T, typename F>
    iterate_node( T&& init, const std::shared_ptr<event_stream_node<D, E>>& events, F&& func )
        : iterate_node::signal_node( std::forward<T>( init ) )
        , m_events( events )
        , m_func( std::forward<F>( func ) )
    {

        engine::on_node_attach( *this, *events );
    }

    ~iterate_node()
    {
        engine::on_node_detach( *this, *m_events );
    }

    void tick( void* ) override
    {
        bool changed = false;

        {
            S new_value = m_func( event_range<E>( m_events->events() ), this->m_value );

            if( !equals( new_value, this->m_value ) )
            {
                this->m_value = std::move( new_value );
                changed = true;
            }
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<event_stream_node<D, E>> m_events;

    func_t m_func;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// iterate_by_ref_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename func_t>
class iterate_by_ref_node : public signal_node<D, S>
{
    using engine = typename iterate_by_ref_node::engine;

public:
    template <typename T, typename F>
    iterate_by_ref_node(
        T&& init, const std::shared_ptr<event_stream_node<D, E>>& events, F&& func )
        : iterate_by_ref_node::signal_node( std::forward<T>( init ) )
        , m_func( std::forward<F>( func ) )
        , m_events( events )
    {

        engine::on_node_attach( *this, *events );
    }

    ~iterate_by_ref_node()
    {
        engine::on_node_detach( *this, *m_events );
    }

    void tick( void* ) override
    {
        m_func( event_range<E>( m_events->events() ), this->m_value );

        // Always assume change
        engine::on_node_pulse( *this );
    }

protected:
    func_t m_func;

    std::shared_ptr<event_stream_node<D, E>> m_events;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_iterate_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename func_t, typename... dep_values_t>
class synced_iterate_node : public signal_node<D, S>
{
    using engine = typename synced_iterate_node::engine;

public:
    template <typename T, typename F>
    synced_iterate_node( T&& init,
        const std::shared_ptr<event_stream_node<D, E>>& events,
        F&& func,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_iterate_node::signal_node( std::forward<T>( init ) )
        , m_events( events )
        , m_func( std::forward<F>( func ) )
        , m_deps( deps... )
    {
        engine::on_node_attach( *this, *events );
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_iterate_node()
    {
        engine::on_node_detach( *this, *m_events );

        apply( detach_functor<D,
                   synced_iterate_node,
                   std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
            m_deps );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        m_events->set_current_turn( turn );

        bool changed = false;

        if( !m_events->events().empty() )
        {
            S new_value = apply(
                [this]( const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                    return m_func(
                        event_range<E>( m_events->events() ), this->m_value, args->value_ref()... );
                },
                m_deps );

            if( !equals( new_value, this->m_value ) )
            {
                changed = true;
                this->m_value = std::move( new_value );
            }
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::shared_ptr<event_stream_node<D, E>> m_events;

    func_t m_func;
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// synced_iterate_by_ref_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename func_t, typename... dep_values_t>
class synced_iterate_by_ref_node : public signal_node<D, S>
{
    using engine = typename synced_iterate_by_ref_node::engine;

public:
    template <typename T, typename F>
    synced_iterate_by_ref_node( T&& init,
        const std::shared_ptr<event_stream_node<D, E>>& events,
        F&& func,
        const std::shared_ptr<signal_node<D, dep_values_t>>&... deps )
        : synced_iterate_by_ref_node::signal_node( std::forward<T>( init ) )
        , m_events( events )
        , m_func( std::forward<F>( func ) )
        , m_deps( deps... )
    {
        engine::on_node_attach( *this, *events );
        REACT_EXPAND_PACK( engine::on_node_attach( *this, *deps ) );
    }

    ~synced_iterate_by_ref_node()
    {
        engine::on_node_detach( *this, *m_events );

        apply( detach_functor<D,
                   synced_iterate_by_ref_node,
                   std::shared_ptr<signal_node<D, dep_values_t>>...>( *this ),
            m_deps );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        m_events->set_current_turn( turn );

        bool changed = false;

        if( !m_events->events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<signal_node<D, dep_values_t>>&... args ) {
                    m_func(
                        event_range<E>( m_events->events() ), this->m_value, args->value_ref()... );
                },
                m_deps );

            changed = true;
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, dep_values_t>>...>;

    std::shared_ptr<event_stream_node<D, E>> m_events;

    func_t m_func;
    dep_holder_t m_deps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// hold_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class hold_node : public signal_node<D, S>
{
    using engine = typename hold_node::engine;

public:
    template <typename T>
    hold_node( T&& init, const std::shared_ptr<event_stream_node<D, S>>& events )
        : hold_node::signal_node( std::forward<T>( init ) )
        , m_events( events )
    {

        engine::on_node_attach( *this, *m_events );
    }

    ~hold_node()
    {
        engine::on_node_detach( *this, *m_events );
    }

    void tick( void* ) override
    {
        bool changed = false;

        if( !m_events->events().empty() )
        {
            const S& new_value = m_events->events().back();

            if( !equals( new_value, this->m_value ) )
            {
                changed = true;
                this->m_value = new_value;
            }
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<event_stream_node<D, S>> m_events;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// snapshot_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class snapshot_node : public signal_node<D, S>
{
    using engine = typename snapshot_node::engine;

public:
    snapshot_node( const std::shared_ptr<signal_node<D, S>>& target,
        const std::shared_ptr<event_stream_node<D, E>>& trigger )
        : snapshot_node::signal_node( target->value_ref() )
        , m_target( target )
        , m_trigger( trigger )
    {
        engine::on_node_attach( *this, *m_target );
        engine::on_node_attach( *this, *m_trigger );
    }

    ~snapshot_node()
    {
        engine::on_node_detach( *this, *m_target );
        engine::on_node_detach( *this, *m_trigger );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        m_trigger->set_current_turn( turn );

        bool changed = false;

        if( !m_trigger->events().empty() )
        {
            const S& new_value = m_target->value_ref();

            if( !equals( new_value, this->m_value ) )
            {
                changed = true;
                this->m_value = new_value;
            }
        }

        if( changed )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, S>> m_target;
    const std::shared_ptr<event_stream_node<D, E>> m_trigger;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// monitor_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class monitor_node : public event_stream_node<D, E>
{
    using engine = typename monitor_node::engine;

public:
    monitor_node( const std::shared_ptr<signal_node<D, E>>& target )
        : monitor_node::event_stream_node()
        , m_target( target )
    {
        engine::on_node_attach( *this, *m_target );
    }

    ~monitor_node()
    {
        engine::on_node_detach( *this, *m_target );
    }

    void tick( void* turn_ptr ) override
    {
        using turn_t = typename D::engine::turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );

        this->m_events.push_back( m_target->value_ref() );

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, E>> m_target;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// pulse_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class pulse_node : public event_stream_node<D, S>
{
    using engine = typename pulse_node::engine;

public:
    pulse_node( const std::shared_ptr<signal_node<D, S>>& target,
        const std::shared_ptr<event_stream_node<D, E>>& trigger )
        : pulse_node::event_stream_node()
        , m_target( target )
        , m_trigger( trigger )
    {
        engine::on_node_attach( *this, *m_target );
        engine::on_node_attach( *this, *m_trigger );
    }

    ~pulse_node()
    {
        engine::on_node_detach( *this, *m_target );
        engine::on_node_detach( *this, *m_trigger );
    }

    void tick( void* turn_ptr ) override
    {
        typedef typename D::engine::turn_t turn_t;
        turn_t& turn = *reinterpret_cast<turn_t*>( turn_ptr );

        this->set_current_turn( turn, true );
        m_trigger->set_current_turn( turn );

        for( size_t i = 0, ie = m_trigger->events().size(); i < ie; ++i )
        {
            this->m_events.push_back( m_target->value_ref() );
        }

        if( !this->m_events.empty() )
        {
            engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, S>> m_target;
    const std::shared_ptr<event_stream_node<D, E>> m_trigger;
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// hold - hold the most recent event in a signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename T = typename std::decay<V>::type>
auto hold( const events<D, T>& events, V&& init ) -> signal<D, T>
{
    using ::react::detail::hold_node;

    return signal<D, T>(
        std::make_shared<hold_node<D, T>>( std::forward<V>( init ), get_node_ptr( events ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// monitor - Emits value changes of target signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto monitor( const signal<D, S>& target ) -> events<D, S>
{
    using ::react::detail::monitor_node;

    return events<D, S>( std::make_shared<monitor_node<D, S>>( get_node_ptr( target ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// iterate - Iteratively combines signal value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename V,
    typename f_in_t,
    typename S = typename std::decay<V>::type>
auto iterate( const events<D, E>& events, V&& init, f_in_t&& func ) -> signal<D, S>
{
    using ::react::detail::add_iterate_by_ref_range_wrapper;
    using ::react::detail::add_iterate_range_wrapper;
    using ::react::detail::event_range;
    using ::react::detail::is_callable_with;
    using ::react::detail::iterate_by_ref_node;
    using ::react::detail::iterate_node;

    using F = typename std::decay<f_in_t>::type;

    using node_t = typename std::conditional<is_callable_with<F, S, event_range<E>, S>::value,
        iterate_node<D, S, E, F>,
        typename std::conditional<is_callable_with<F, S, E, S>::value,
            iterate_node<D, S, E, add_iterate_range_wrapper<E, S, F>>,
            typename std::conditional<is_callable_with<F, void, event_range<E>, S&>::value,
                iterate_by_ref_node<D, S, E, F>,
                typename std::conditional<is_callable_with<F, void, E, S&>::value,
                    iterate_by_ref_node<D, S, E, add_iterate_by_ref_range_wrapper<E, S, F>>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<node_t, void>::value,
        "iterate: Passed function does not match any of the supported signatures." );

    return signal<D, S>( std::make_shared<node_t>(
        std::forward<V>( init ), get_node_ptr( events ), std::forward<f_in_t>( func ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// iterate - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename V,
    typename f_in_t,
    typename... dep_values_t,
    typename S = typename std::decay<V>::type>
auto iterate( const events<D, E>& events,
    V&& init,
    const signal_pack<D, dep_values_t...>& dep_pack,
    f_in_t&& func ) -> signal<D, S>
{
    using ::react::detail::add_iterate_by_ref_range_wrapper;
    using ::react::detail::add_iterate_range_wrapper;
    using ::react::detail::event_range;
    using ::react::detail::is_callable_with;
    using ::react::detail::synced_iterate_by_ref_node;
    using ::react::detail::synced_iterate_node;

    using F = typename std::decay<f_in_t>::type;

    using node_t = typename std::conditional<
        is_callable_with<F, S, event_range<E>, S, dep_values_t...>::value,
        synced_iterate_node<D, S, E, F, dep_values_t...>,
        typename std::conditional<is_callable_with<F, S, E, S, dep_values_t...>::value,
            synced_iterate_node<D,
                S,
                E,
                add_iterate_range_wrapper<E, S, F, dep_values_t...>,
                dep_values_t...>,
            typename std::conditional<
                is_callable_with<F, void, event_range<E>, S&, dep_values_t...>::value,
                synced_iterate_by_ref_node<D, S, E, F, dep_values_t...>,
                typename std::conditional<is_callable_with<F, void, E, S&, dep_values_t...>::value,
                    synced_iterate_by_ref_node<D,
                        S,
                        E,
                        add_iterate_by_ref_range_wrapper<E, S, F, dep_values_t...>,
                        dep_values_t...>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<node_t, void>::value,
        "iterate: Passed function does not match any of the supported signatures." );

    //static_assert(node_t::dummy_error, "DUMP MY TYPE" );

    struct node_builder
    {
        node_builder( const ::react::events<D, E>& source, V&& init, f_in_t&& func )
            : m_source( source )
            , m_init( std::forward<V>( init ) )
            , m_func( std::forward<f_in_t>( func ) )
        {}

        auto operator()( const signal<D, dep_values_t>&... deps ) -> signal<D, S>
        {
            return signal<D, S>( std::make_shared<node_t>( std::forward<V>( m_init ),
                get_node_ptr( m_source ),
                std::forward<f_in_t>( m_func ),
                get_node_ptr( deps )... ) );
        }

        const ::react::events<D, E>& m_source;
        V m_init;
        f_in_t m_func;
    };

    return ::react::detail::apply(
        node_builder( events, std::forward<V>( init ), std::forward<f_in_t>( func ) ),
        dep_pack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// snapshot - Sets signal value to value of other signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto snapshot( const events<D, E>& trigger, const signal<D, S>& target ) -> signal<D, S>
{
    using ::react::detail::snapshot_node;

    return signal<D, S>( std::make_shared<snapshot_node<D, S, E>>(
        get_node_ptr( target ), get_node_ptr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// pulse - Emits value of target signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto pulse( const events<D, E>& trigger, const signal<D, S>& target ) -> events<D, S>
{
    using ::react::detail::pulse_node;

    return events<D, S>(
        std::make_shared<pulse_node<D, S, E>>( get_node_ptr( target ), get_node_ptr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// changed - Emits token when target signal was changed
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto changed( const signal<D, S>& target ) -> events<D, token>
{
    return monitor( target ).tokenize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// changed_to - Emits token when target signal was changed to value
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename S = typename std::decay<V>::type>
auto changed_to( const signal<D, S>& target, V&& value ) -> events<D, token>
{
    return monitor( target ).filter( [=]( const S& v ) { return v == value; } ).tokenize();
}

namespace detail
{

inline void topological_sort_engine::propagate( turn_type& turn )
{
    while( m_scheduled_nodes.fetch_next() )
    {
        for( auto* cur_node : m_scheduled_nodes.next_values() )
        {
            if( cur_node->level < cur_node->new_level )
            {
                cur_node->level = cur_node->new_level;
                invalidate_successors( *cur_node );
                m_scheduled_nodes.push( cur_node, cur_node->level );
                continue;
            }

            cur_node->queued = false;
            cur_node->tick( &turn );
        }
    }
}

inline void topological_sort_engine::process_children( reactive_node& node )
{
    // Add children to queue
    for( auto* successor : node.successors )
    {
        if( !successor->queued )
        {
            successor->queued = true;
            m_scheduled_nodes.push( successor, successor->level );
        }
    }
}

} // namespace detail
} // namespace react

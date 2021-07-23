
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
struct DontMove
{};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DisableIfSame
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename U>
struct DisableIfSame
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
class turn
{
public:
    inline turn( turn_id_t id )
        : id_( id )
    {}

    inline turn_id_t Id() const
    {
        return id_;
    }

private:
    turn_id_t id_;
};

class topological_sort_engine
{
public:
    using NodeT = reactive_node;
    using TurnT = turn;

    void propagate( turn& turn );

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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineInterface - Static wrapper for IReactiveEngine
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TEngine>
struct engine_interface
{
    using NodeT = typename TEngine::NodeT;
    using TurnT = typename TEngine::TurnT;

    static TEngine& instance()
    {
        static TEngine engine;
        return engine;
    }

    static void on_input_change( NodeT& node )
    {
        instance().on_input_change( node );
    }

    static void propagate( TurnT& turn )
    {
        instance().propagate( turn );
    }

    static void on_node_destroy( NodeT& node )
    {
        instance().on_node_destroy( node );
    }

    static void on_node_attach( NodeT& node, NodeT& parent )
    {
        instance().on_node_attach( node, parent );
    }

    static void on_node_detach( NodeT& node, NodeT& parent )
    {
        instance().on_node_detach( node, parent );
    }

    static void on_node_pulse( NodeT& node )
    {
        instance().on_node_pulse( node );
    }

    static void on_dynamic_node_attach( NodeT& node, NodeT& parent )
    {
        instance().on_dynamic_node_attach( node, parent );
    }

    static void on_dynamic_node_detach( NodeT& node, NodeT& parent )
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
    using DomainT = D;
    using Engine = typename D::Engine;
    using NodeT = reactive_node;
    using TurnT = typename Engine::TurnT;

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
        D::Engine::on_node_attach( node, *dep_ptr );
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
        D::Engine::on_node_detach( node, *dep_ptr );
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

    template <typename... TDepsIn>
    reactive_op_base( DontMove, TDepsIn&&... deps )
        : m_deps( std::forward<TDepsIn>( deps )... )
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
class EventRange
{
public:
    using const_iterator = typename std::vector<E>::const_iterator;
    using size_type = typename std::vector<E>::size_type;

    // Copy ctor
    EventRange( const EventRange& ) = default;

    // Copy assignment
    EventRange& operator=( const EventRange& ) = default;

    const_iterator begin() const
    {
        return data_.begin();
    }

    const_iterator end() const
    {
        return data_.end();
    }

    size_type Size() const
    {
        return data_.size();
    }

    bool IsEmpty() const
    {
        return data_.empty();
    }

    explicit EventRange( const std::vector<E>& data )
        : data_( data )
    {}

private:
    const std::vector<E>& data_;
};

template <typename E>
using EventEmitter = std::back_insert_iterator<std::vector<E>>;

template <typename L, typename R>
bool Equals( const L& lhs, const R& rhs )
{
    return lhs == rhs;
}

template <typename L, typename R>
bool Equals( const std::reference_wrapper<L>& lhs, const std::reference_wrapper<R>& rhs )
{
    return lhs.get() == rhs.get();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ReactiveBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
class ReactiveBase
{
public:
    using DomainT = typename node_t::DomainT;

    // Default ctor
    ReactiveBase() = default;

    // Copy ctor
    ReactiveBase( const ReactiveBase& ) = default;

    // Move ctor (VS2013 doesn't default generate that yet)
    ReactiveBase( ReactiveBase&& other ) noexcept
        : ptr_( std::move( other.ptr_ ) )
    {}

    // Explicit node ctor
    explicit ReactiveBase( std::shared_ptr<node_t>&& ptr )
        : ptr_( std::move( ptr ) )
    {}

    // Copy assignment
    ReactiveBase& operator=( const ReactiveBase& ) = default;

    // Move assignment
    ReactiveBase& operator=( ReactiveBase&& other ) noexcept
    {
        ptr_.reset( std::move( other ) );
        return *this;
    }

    virtual bool IsValid() const
    {
        return ptr_ != nullptr;
    }

protected:
    std::shared_ptr<node_t> ptr_;

    template <typename TNode_>
    friend const std::shared_ptr<TNode_>& GetNodePtr( const ReactiveBase<TNode_>& node );
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CopyableReactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
class CopyableReactive : public ReactiveBase<node_t>
{
public:
    CopyableReactive() = default;

    CopyableReactive( const CopyableReactive& ) = default;

    CopyableReactive( CopyableReactive&& other ) noexcept
        : CopyableReactive::ReactiveBase( std::move( other ) )
    {}

    explicit CopyableReactive( std::shared_ptr<node_t>&& ptr )
        : CopyableReactive::ReactiveBase( std::move( ptr ) )
    {}

    CopyableReactive& operator=( const CopyableReactive& ) = default;

    CopyableReactive& operator=( CopyableReactive&& other ) noexcept
    {
        CopyableReactive::ReactiveBase::operator=( std::move( other ) );
        return *this;
    }

    bool Equals( const CopyableReactive& other ) const
    {
        return this->ptr_ == other.ptr_;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GetNodePtr
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename node_t>
const std::shared_ptr<node_t>& GetNodePtr( const ReactiveBase<node_t>& node )
{
    return node.ptr_;
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
/// InputManager
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class input_manager
{
public:
    using TurnT = typename D::TurnT;
    using Engine = typename D::Engine;

    input_manager() = default;

    template <typename F>
    void do_transaction( F&& func )
    {
        bool shouldPropagate = false;

        // Phase 1 - Input admission
        m_is_transaction_active = true;

        TurnT turn( next_turn_id() );

        func();

        m_is_transaction_active = false;

        // Phase 2 - Apply input node changes
        for( auto* p : m_changed_inputs )
        {
            if( p->apply_input( &turn ) )
            {
                shouldPropagate = true;
            }
        }
        m_changed_inputs.clear();

        // Phase 3 - propagate changes
        if( shouldPropagate )
        {
            Engine::propagate( turn );
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
        TurnT turn( next_turn_id() );
        r.add_input( std::forward<V>( v ) );

        if( r.apply_input( &turn ) )
        {
            Engine::propagate( turn );
        }

        finalize_sync_transaction();
    }

    template <typename R, typename F>
    void modify_simple_input( R& r, const F& func )
    {
        TurnT turn( next_turn_id() );

        r.modify_input( func );


        // Return value, will always be true
        r.apply_input( &turn );

        Engine::propagate( turn );

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

    static input_manager<D>& Instance()
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

template <typename D, typename S, typename TOp>
class temp_signal;

template <typename D, typename E, typename TOp>
class temp_events;

template <typename D, typename... TValues>
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
    using TurnT = turn;

    domain_base() = delete;

    using Engine = ::react::detail::engine_interface<D, topological_sort_engine>;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Aliases for reactives of this domain
    ///////////////////////////////////////////////////////////////////////////////////////////////
    template <typename S>
    using SignalT = signal<D, S>;

    template <typename S>
    using VarSignalT = var_signal<D, S>;

    template <typename E = token>
    using EventsT = events<D, E>;

    template <typename E = token>
    using EventSourceT = event_source<D, E>;

    using ObserverT = observer<D>;

    using ScopedObserverT = scoped_observer<D>;
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
        D::Engine::instance();
        domain_specific_input_manager<D>::Instance();
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
    domain_specific_input_manager<D>::Instance().do_transaction( std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Domain definition macro
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACTIVE_DOMAIN( name )                                                                    \
    struct name : public ::react::detail::domain_base<name>                                         \
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
    using SignalT = signal<name, S>;                                                               \
                                                                                                   \
    template <typename S>                                                                          \
    using VarSignalT = var_signal<name, S>;                                                         \
                                                                                                   \
    template <typename E = token>                                                                  \
    using EventsT = events<name, E>;                                                               \
                                                                                                   \
    template <typename E = token>                                                                  \
    using EventSourceT = event_source<name, E>;                                                     \
                                                                                                   \
    using ObserverT = observer<name>;                                                              \
                                                                                                   \
    using ScopedObserverT = scoped_observer<name>;

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
/// AddObserverRangeWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename F, typename... args_t>
struct AddObserverRangeWrapper
{
    AddObserverRangeWrapper( const AddObserverRangeWrapper& other ) = default;

    AddObserverRangeWrapper( AddObserverRangeWrapper&& other ) noexcept
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddObserverRangeWrapper>::type>
    explicit AddObserverRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    observer_action operator()( EventRange<E> range, const args_t&... args )
    {
        for( const auto& e : range )
        {
            if( MyFunc( e, args... ) == observer_action::stop_and_detach )
            {
                return observer_action::stop_and_detach;
            }
        }

        return observer_action::next;
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObserverNode
    : public node_base<D>
    , public observer_interface
{
public:
    ObserverNode() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename TFunc>
class SignalObserverNode : public ObserverNode<D>
{
    using Engine = typename SignalObserverNode::Engine;

public:
    template <typename F>
    SignalObserverNode( const std::shared_ptr<signal_node<D, S>>& subject, F&& func )
        : SignalObserverNode::ObserverNode()
        , subject_( subject )
        , func_( std::forward<F>( func ) )
    {

        Engine::on_node_attach( *this, *subject );
    }

    ~SignalObserverNode() = default;

    void tick( void* ) override
    {
        bool shouldDetach = false;

        if( auto p = subject_.lock() )
        {
            if( func_( p->ValueRef() ) == observer_action::stop_and_detach )
            {
                shouldDetach = true;
            }
        }

        if( shouldDetach )
        {
            domain_specific_input_manager<D>::Instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    void detach_observer() override
    {
        if( auto p = subject_.lock() )
        {
            Engine::on_node_detach( *this, *p );
            subject_.reset();
        }
    }

    std::weak_ptr<signal_node<D, S>> subject_;
    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TFunc>
class EventObserverNode : public ObserverNode<D>
{
    using Engine = typename EventObserverNode::Engine;

public:
    template <typename F>
    EventObserverNode( const std::shared_ptr<event_stream_node<D, E>>& subject, F&& func )
        : EventObserverNode::ObserverNode()
        , subject_( subject )
        , func_( std::forward<F>( func ) )
    {
        Engine::on_node_attach( *this, *subject );
    }

    ~EventObserverNode() = default;

    void tick( void* ) override
    {
        bool shouldDetach = false;

        if( auto p = subject_.lock() )
        {
            shouldDetach = func_( EventRange<E>( p->events() ) ) == observer_action::stop_and_detach;
        }

        if( shouldDetach )
        {
            domain_specific_input_manager<D>::Instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    std::weak_ptr<event_stream_node<D, E>> subject_;

    TFunc func_;

    virtual void detach_observer()
    {
        if( auto p = subject_.lock() )
        {
            Engine::on_node_detach( *this, *p );
            subject_.reset();
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TFunc, typename... TDepValues>
class SyncedObserverNode : public ObserverNode<D>
{
    using Engine = typename SyncedObserverNode::Engine;

public:
    template <typename F>
    SyncedObserverNode( const std::shared_ptr<event_stream_node<D, E>>& subject,
        F&& func,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedObserverNode::ObserverNode()
        , subject_( subject )
        , func_( std::forward<F>( func ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *subject );

        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedObserverNode() = default;

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        bool shouldDetach = false;

        if( auto p = subject_.lock() )
        {
            // Update of this node could be triggered from deps,
            // so make sure source doesnt contain events from last turn
            p->SetCurrentTurn( turn );

            {
                shouldDetach
                    = apply(
                          [this, &p]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                              return func_( EventRange<E>( p->events() ), args->ValueRef()... );
                          },
                          deps_ )
                   == observer_action::stop_and_detach;
            }
        }

        if( shouldDetach )
        {
            domain_specific_input_manager<D>::Instance().queue_observer_for_detach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->unregister_observer( this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::weak_ptr<event_stream_node<D, E>> subject_;

    TFunc func_;
    dep_holder_t deps_;

    virtual void detach_observer()
    {
        if( auto p = subject_.lock() )
        {
            Engine::on_node_detach( *this, *p );

            apply( detach_functor<D,
                       SyncedObserverNode,
                       std::shared_ptr<signal_node<D, TDepValues>>...>( *this ),
                deps_ );

            subject_.reset();
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
    using SubjectPtrT = std::shared_ptr<::react::detail::observable_node<D>>;
    using NodeT = ::react::detail::ObserverNode<D>;

public:
    // Default ctor
    observer()
        : nodePtr_( nullptr )
        , subjectPtr_( nullptr )
    {}

    // Move ctor
    observer( observer&& other ) noexcept
        : nodePtr_( other.nodePtr_ )
        , subjectPtr_( std::move( other.subjectPtr_ ) )
    {
        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();
    }

    // Node ctor
    observer( NodeT* nodePtr, const SubjectPtrT& subjectPtr )
        : nodePtr_( nodePtr )
        , subjectPtr_( subjectPtr )
    {}

    // Move assignment
    observer& operator=( observer&& other ) noexcept
    {
        nodePtr_ = other.nodePtr_;
        subjectPtr_ = std::move( other.subjectPtr_ );

        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();

        return *this;
    }

    // Deleted copy ctor and assignment
    observer( const observer& ) = delete;
    observer& operator=( const observer& ) = delete;

    void detach()
    {
        assert( IsValid() );
        subjectPtr_->unregister_observer( nodePtr_ );
    }

    bool IsValid() const
    {
        return nodePtr_ != nullptr;
    }

private:
    // Owned by subject
    NodeT* nodePtr_;

    // While the observer handle exists, the subject is not destroyed
    SubjectPtrT subjectPtr_;
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
        : obs_( std::move( other.obs_ ) )
    {}

    // Construct from observer
    scoped_observer( observer<D>&& obs )
        : obs_( std::move( obs ) )
    {}

    // Move assignment
    scoped_observer& operator=( scoped_observer&& other ) noexcept
    {
        obs_ = std::move( other.obs_ );
    }

    // Deleted default ctor, copy ctor and assignment
    scoped_observer() = delete;
    scoped_observer( const scoped_observer& ) = delete;
    scoped_observer& operator=( const scoped_observer& ) = delete;

    ~scoped_observer()
    {
        obs_.detach();
    }

    bool IsValid() const
    {
        return obs_.IsValid();
    }

private:
    observer<D> obs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename S>
auto Observe( const signal<D, S>& subject, FIn&& func ) -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::observer_interface;
    using ::react::detail::ObserverNode;
    using ::react::detail::SignalObserverNode;

    using F = typename std::decay<FIn>::type;
    using R = typename std::result_of<FIn( S )>::type;
    using WrapperT = add_default_return_value_wrapper<F, observer_action, observer_action::next>;

    // If return value of passed function is void, add observer_action::next as
    // default return value.
    using NodeT = typename std::conditional<std::is_same<void, R>::value,
        SignalObserverNode<D, S, WrapperT>,
        SignalObserverNode<D, S, F>>::type;

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT( subjectPtr, std::forward<FIn>( func ) ) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->register_observer( std::move( nodePtr ) );

    return observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename E>
auto Observe( const events<D, E>& subject, FIn&& func ) -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::AddObserverRangeWrapper;
    using ::react::detail::EventObserverNode;
    using ::react::detail::EventRange;
    using ::react::detail::is_callable_with;
    using ::react::detail::observer_interface;
    using ::react::detail::ObserverNode;

    using F = typename std::decay<FIn>::type;

    using WrapperT = typename std::conditional<
        is_callable_with<F, observer_action, EventRange<E>>::value,
        F,
        typename std::conditional<is_callable_with<F, observer_action, E>::value,
            AddObserverRangeWrapper<E, F>,
            typename std::conditional<is_callable_with<F, void, EventRange<E>>::value,
                add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                typename std::conditional<is_callable_with<F, void, E>::value,
                    AddObserverRangeWrapper<E,
                        add_default_return_value_wrapper<F, observer_action, observer_action::next>>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<WrapperT, void>::value,
        "Observe: Passed function does not match any of the supported signatures." );

    using NodeT = EventObserverNode<D, E, WrapperT>;

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT( subjectPtr, std::forward<FIn>( func ) ) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->register_observer( std::move( nodePtr ) );

    return observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename E, typename... TDepValues>
auto Observe( const events<D, E>& subject, const signal_pack<D, TDepValues...>& depPack, FIn&& func )
    -> observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::AddObserverRangeWrapper;
    using ::react::detail::EventRange;
    using ::react::detail::is_callable_with;
    using ::react::detail::observer_interface;
    using ::react::detail::ObserverNode;
    using ::react::detail::SyncedObserverNode;

    using F = typename std::decay<FIn>::type;

    using WrapperT = typename std::conditional<
        is_callable_with<F, observer_action, EventRange<E>, TDepValues...>::value,
        F,
        typename std::conditional<is_callable_with<F, observer_action, E, TDepValues...>::value,
            AddObserverRangeWrapper<E, F, TDepValues...>,
            typename std::conditional<
                is_callable_with<F, void, EventRange<E>, TDepValues...>::value,
                add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                typename std::conditional<is_callable_with<F, void, E, TDepValues...>::value,
                    AddObserverRangeWrapper<E,
                        add_default_return_value_wrapper<F, observer_action, observer_action::next>,
                        TDepValues...>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<WrapperT, void>::value,
        "Observe: Passed function does not match any of the supported signatures." );

    using NodeT = SyncedObserverNode<D, E, WrapperT, TDepValues...>;

    struct NodeBuilder_
    {
        NodeBuilder_( const events<D, E>& subject, FIn&& func )
            : MySubject( subject )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TDepValues>&... deps ) -> ObserverNode<D>*
        {
            return new NodeT(
                GetNodePtr( MySubject ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... );
        }

        const events<D, E>& MySubject;
        FIn MyFunc;
    };

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( ::react::detail::apply(
        NodeBuilder_( subject, std::forward<FIn>( func ) ), depPack.Data ) );

    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->register_observer( std::move( nodePtr ) );

    return observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsSignal
{
    static const bool value = false;
};

template <typename D, typename T>
struct IsSignal<signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsSignal<var_signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsSignal<temp_signal<D, T, TOp>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsEvent
{
    static const bool value = false;
};

template <typename D, typename T>
struct IsEvent<events<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsEvent<event_source<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsEvent<temp_events<D, T, TOp>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DecayInput
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct DecayInput
{
    using Type = T;
};

template <typename D, typename T>
struct DecayInput<var_signal<D, T>>
{
    using Type = signal<D, T>;
};

template <typename D, typename T>
struct DecayInput<event_source<D, T>>
{
    using Type = events<D, T>;
};

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename L, typename R>
bool Equals( const L& lhs, const R& rhs );

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
        , value_( std::forward<T>( value ) )
    {}

    const S& ValueRef() const
    {
        return value_;
    }

protected:
    S value_;
};

template <typename D, typename S>
using SignalNodePtrT = std::shared_ptr<signal_node<D, S>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class VarNode
    : public signal_node<D, S>
    , public input_node_interface
{
    using Engine = typename VarNode::Engine;

public:
    template <typename T>
    VarNode( T&& value )
        : VarNode::signal_node( std::forward<T>( value ) )
        , newValue_( value )
    {}

    ~VarNode() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked VarNode" );
    }

    template <typename V>
    void add_input( V&& newValue )
    {
        newValue_ = std::forward<V>( newValue );

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    // This is signal-specific
    template <typename F>
    void modify_input( F& func )
    {
        // There hasn't been any Set(...) input yet, modify.
        if( !isInputAdded_ )
        {
            func( this->value_ );

            isInputModified_ = true;
        }
        // There's a newValue, modify newValue instead.
        // The modified newValue will handled like before, i.e. it'll be compared to value_
        // in ApplyInput
        else
        {
            func( newValue_ );
        }
    }

    bool apply_input( void* ) override
    {
        if( isInputAdded_ )
        {
            isInputAdded_ = false;

            if( !Equals( this->value_, newValue_ ) )
            {
                this->value_ = std::move( newValue_ );
                Engine::on_input_change( *this );
                return true;
            }
            else
            {
                return false;
            }
        }
        else if( isInputModified_ )
        {
            isInputModified_ = false;

            Engine::on_input_change( *this );
            return true;
        }

        else
        {
            return false;
        }
    }

private:
    S newValue_;
    bool isInputAdded_ = false;
    bool isInputModified_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FunctionOp
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename... deps_t>
class FunctionOp : public reactive_op_base<deps_t...>
{
public:
    template <typename FIn, typename... TDepsIn>
    FunctionOp( FIn&& func, TDepsIn&&... deps )
        : FunctionOp::reactive_op_base( DontMove(), std::forward<TDepsIn>( deps )... )
        , func_( std::forward<FIn>( func ) )
    {}

    FunctionOp( FunctionOp&& other ) noexcept
        : FunctionOp::reactive_op_base( std::move( other ) )
        , func_( std::move( other.func_ ) )
    {}

    S Evaluate()
    {
        return apply( EvalFunctor( func_ ), this->m_deps );
    }

private:
    // Eval
    struct EvalFunctor
    {
        EvalFunctor( F& f )
            : MyFunc( f )
        {}

        template <typename... T>
        S operator()( T&&... args )
        {
            return MyFunc( eval( args )... );
        }

        template <typename T>
        static auto eval( T& op ) -> decltype( op.Evaluate() )
        {
            return op.Evaluate();
        }

        template <typename T>
        static auto eval( const std::shared_ptr<T>& dep_ptr ) -> decltype( dep_ptr->ValueRef() )
        {
            return dep_ptr->ValueRef();
        }

        F& MyFunc;
    };

private:
    F func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalOpNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename TOp>
class SignalOpNode : public signal_node<D, S>
{
    using Engine = typename SignalOpNode::Engine;

public:
    template <typename... args_t>
    SignalOpNode( args_t&&... args )
        : SignalOpNode::signal_node()
        , op_( std::forward<args_t>( args )... )
    {
        this->value_ = op_.Evaluate();


        op_.template attach<D>( *this );
    }

    ~SignalOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template detach<D>( *this );
        }
    }

    void tick( void* ) override
    {
        bool changed = false;

        {
            S newValue = op_.Evaluate();

            if( !Equals( this->value_, newValue ) )
            {
                this->value_ = std::move( newValue );
                changed = true;
            }
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

    TOp StealOp()
    {
        assert( !wasOpStolen_ && "Op was already stolen." );
        wasOpStolen_ = true;
        op_.template detach<D>( *this );
        return std::move( op_ );
    }

private:
    TOp op_;
    bool wasOpStolen_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TOuter, typename TInner>
class FlattenNode : public signal_node<D, TInner>
{
    using Engine = typename FlattenNode::Engine;

public:
    FlattenNode( const std::shared_ptr<signal_node<D, TOuter>>& outer,
        const std::shared_ptr<signal_node<D, TInner>>& inner )
        : FlattenNode::signal_node( inner->ValueRef() )
        , outer_( outer )
        , inner_( inner )
    {
        Engine::on_node_attach( *this, *outer_ );
        Engine::on_node_attach( *this, *inner_ );
    }

    ~FlattenNode()
    {
        Engine::on_node_detach( *this, *inner_ );
        Engine::on_node_detach( *this, *outer_ );
    }

    void tick( void* ) override
    {
        auto newInner = GetNodePtr( outer_->ValueRef() );

        if( newInner != inner_ )
        {
            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::on_dynamic_node_detach( *this, *oldInner );
            Engine::on_dynamic_node_attach( *this, *newInner );

            return;
        }

        if( !Equals( this->value_, inner_->ValueRef() ) )
        {
            this->value_ = inner_->ValueRef();
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<signal_node<D, TOuter>> outer_;
    std::shared_ptr<signal_node<D, TInner>> inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalBase : public CopyableReactive<signal_node<D, S>>
{
public:
    SignalBase() = default;
    SignalBase( const SignalBase& ) = default;

    template <typename T>
    SignalBase( T&& t )
        : SignalBase::CopyableReactive( std::forward<T>( t ) )
    {}

protected:
    const S& getValue() const
    {
        return this->ptr_->ValueRef();
    }

    template <typename T>
    void setValue( T&& newValue ) const
    {
        domain_specific_input_manager<D>::Instance().add_input(
            *reinterpret_cast<VarNode<D, S>*>( this->ptr_.get() ), std::forward<T>( newValue ) );
    }

    template <typename F>
    void modifyValue( const F& func ) const
    {
        domain_specific_input_manager<D>::Instance().modify_input(
            *reinterpret_cast<VarNode<D, S>*>( this->ptr_.get() ), func );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal_pack - Wraps several nodes in a tuple. Create with comma operator.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
class signal_pack
{
public:
    signal_pack( const signal<D, TValues>&... deps )
        : Data( std::tie( deps... ) )
    {}

    template <typename... TCurValues, typename TAppendValue>
    signal_pack( const signal_pack<D, TCurValues...>& curArgs, const signal<D, TAppendValue>& newArg )
        : Data( std::tuple_cat( curArgs.Data, std::tie( newArg ) ) )
    {}

    std::tuple<const signal<D, TValues>&...> Data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// With - Utility function to create a signal_pack
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
auto With( const signal<D, TValues>&... deps ) -> signal_pack<D, TValues...>
{
    return signal_pack<D, TValues...>( deps... );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVar
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    class = typename std::enable_if<!IsSignal<S>::value>::type,
    class = typename std::enable_if<!IsEvent<S>::value>::type>
auto MakeVar( V&& value ) -> var_signal<D, S>
{
    return var_signal<D, S>(
        std::make_shared<::react::detail::VarNode<D, S>>( std::forward<V>( value ) ) );
}

template <typename D, typename S>
auto MakeVar( std::reference_wrapper<S> value ) -> var_signal<D, S&>
{
    return var_signal<D, S&>(
        std::make_shared<::react::detail::VarNode<D, std::reference_wrapper<S>>>( value ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVar (higher order reactives)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename TInner = typename S::ValueT,
    class = typename std::enable_if<IsSignal<S>::value>::type>
auto MakeVar( V&& value ) -> var_signal<D, signal<D, TInner>>
{
    return var_signal<D, signal<D, TInner>>(
        std::make_shared<::react::detail::VarNode<D, signal<D, TInner>>>(
            std::forward<V>( value ) ) );
}

template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename TInner = typename S::ValueT,
    class = typename std::enable_if<IsEvent<S>::value>::type>
auto MakeVar( V&& value ) -> var_signal<D, events<D, TInner>>
{
    return var_signal<D, events<D, TInner>>(
        std::make_shared<::react::detail::VarNode<D, events<D, TInner>>>(
            std::forward<V>( value ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
// Single arg
template <typename D,
    typename TValue,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename S = typename std::result_of<F( TValue )>::type,
    typename TOp = ::react::detail::FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TValue>>>
auto MakeSignal( const signal<D, TValue>& arg, FIn&& func ) -> temp_signal<D, S, TOp>
{
    return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(
        std::forward<FIn>( func ), GetNodePtr( arg ) ) );
}

// Multiple args
template <typename D,
    typename... TValues,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename S = typename std::result_of<F( TValues... )>::type,
    typename TOp
    = ::react::detail::FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TValues>...>>
auto MakeSignal( const signal_pack<D, TValues...>& argPack, FIn&& func ) -> temp_signal<D, S, TOp>
{
    using ::react::detail::SignalOpNode;

    struct NodeBuilder_
    {
        NodeBuilder_( FIn&& func )
            : MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TValues>&... args ) -> temp_signal<D, S, TOp>
        {
            return temp_signal<D, S, TOp>( std::make_shared<SignalOpNode<D, S, TOp>>(
                std::forward<FIn>( MyFunc ), GetNodePtr( args )... ) );
        }

        FIn MyFunc;
    };

    return ::react::detail::apply( NodeBuilder_( std::forward<FIn>( func ) ), argPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Unary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP( op, name )                                                               \
    template <typename T>                                                                          \
    struct name##OpFunctor                                                                         \
    {                                                                                              \
        T operator()( const T& v ) const                                                           \
        {                                                                                          \
            return op v;                                                                           \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename TSignal,                                                                    \
        typename D = typename TSignal::DomainT,                                                    \
        typename TVal = typename TSignal::ValueT,                                                  \
        class = typename std::enable_if<IsSignal<TSignal>::value>::type,                           \
        typename F = name##OpFunctor<TVal>,                                                        \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename TOp                                                                               \
        = ::react::detail::FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TVal>>>             \
    auto operator op( const TSignal& arg )->temp_signal<D, S, TOp>                                  \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), GetNodePtr( arg ) ) );                                                            \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TVal,                                                                             \
        typename TOpIn,                                                                            \
        typename F = name##OpFunctor<TVal>,                                                        \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename TOp = ::react::detail::FunctionOp<S, F, TOpIn>>                                   \
    auto operator op( temp_signal<D, TVal, TOpIn>&& arg )->temp_signal<D, S, TOp>                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>(                                                              \
            std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>( F(), arg.StealOp() ) );    \
    }

REACT_DECLARE_OP( +, UnaryPlus )

REACT_DECLARE_OP( -, UnaryMinus )

REACT_DECLARE_OP( !, LogicalNegation )

REACT_DECLARE_OP( ~, BitwiseComplement )

REACT_DECLARE_OP( ++, Increment )

REACT_DECLARE_OP( --, Decrement )

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Binary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP( op, name )                                                               \
    template <typename L, typename R>                                                              \
    struct name##OpFunctor                                                                         \
    {                                                                                              \
        auto operator()( const L& lhs, const R& rhs ) const                                        \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return lhs op rhs;                                                                     \
        }                                                                                          \
    };                                                                                             \
                                                                                                   \
    template <typename L, typename R>                                                              \
    struct name##OpRFunctor                                                                        \
    {                                                                                              \
        name##OpRFunctor( name##OpRFunctor&& other ) noexcept                                      \
            : LeftVal( std::move( other.LeftVal ) )                                                \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        explicit name##OpRFunctor( T&& val )                                                       \
            : LeftVal( std::forward<T>( val ) )                                                    \
        {}                                                                                         \
                                                                                                   \
        name##OpRFunctor( const name##OpRFunctor& other ) = delete;                                \
                                                                                                   \
        auto operator()( const R& rhs ) const                                                      \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return LeftVal op rhs;                                                                 \
        }                                                                                          \
                                                                                                   \
        L LeftVal;                                                                                 \
    };                                                                                             \
                                                                                                   \
    template <typename L, typename R>                                                              \
    struct name##OpLFunctor                                                                        \
    {                                                                                              \
        name##OpLFunctor( name##OpLFunctor&& other ) noexcept                                      \
            : RightVal( std::move( other.RightVal ) )                                              \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        explicit name##OpLFunctor( T&& val )                                                       \
            : RightVal( std::forward<T>( val ) )                                                   \
        {}                                                                                         \
                                                                                                   \
        name##OpLFunctor( const name##OpLFunctor& other ) = delete;                                \
                                                                                                   \
        auto operator()( const L& lhs ) const                                                      \
            -> decltype( std::declval<L>() op std::declval<R>() )                                  \
        {                                                                                          \
            return lhs op RightVal;                                                                \
        }                                                                                          \
                                                                                                   \
        R RightVal;                                                                                \
    };                                                                                             \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename TRightSignal,                                                                     \
        typename D = typename TLeftSignal::DomainT,                                                \
        typename TLeftVal = typename TLeftSignal::ValueT,                                          \
        typename TRightVal = typename TRightSignal::ValueT,                                        \
        class = typename std::enable_if<IsSignal<TLeftSignal>::value>::type,                       \
        class = typename std::enable_if<IsSignal<TRightSignal>::value>::type,                      \
        typename F = name##OpFunctor<TLeftVal, TRightVal>,                                         \
        typename S = typename std::result_of<F( TLeftVal, TRightVal )>::type,                      \
        typename TOp = ::react::detail::FunctionOp<S,                                              \
            F,                                                                                     \
            ::react::detail::SignalNodePtrT<D, TLeftVal>,                                          \
            ::react::detail::SignalNodePtrT<D, TRightVal>>>                                        \
    auto operator op( const TLeftSignal& lhs, const TRightSignal& rhs )->temp_signal<D, S, TOp>     \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), GetNodePtr( lhs ), GetNodePtr( rhs ) ) );                                         \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename TRightValIn,                                                                      \
        typename D = typename TLeftSignal::DomainT,                                                \
        typename TLeftVal = typename TLeftSignal::ValueT,                                          \
        typename TRightVal = typename std::decay<TRightValIn>::type,                               \
        class = typename std::enable_if<IsSignal<TLeftSignal>::value>::type,                       \
        class = typename std::enable_if<!IsSignal<TRightVal>::value>::type,                        \
        typename F = name##OpLFunctor<TLeftVal, TRightVal>,                                        \
        typename S = typename std::result_of<F( TLeftVal )>::type,                                 \
        typename TOp                                                                               \
        = ::react::detail::FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TLeftVal>>>         \
    auto operator op( const TLeftSignal& lhs, TRightValIn&& rhs )->temp_signal<D, S, TOp>           \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F( std::forward<TRightValIn>( rhs ) ), GetNodePtr( lhs ) ) );                          \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftValIn,                                                                 \
        typename TRightSignal,                                                                     \
        typename D = typename TRightSignal::DomainT,                                               \
        typename TLeftVal = typename std::decay<TLeftValIn>::type,                                 \
        typename TRightVal = typename TRightSignal::ValueT,                                        \
        class = typename std::enable_if<!IsSignal<TLeftVal>::value>::type,                         \
        class = typename std::enable_if<IsSignal<TRightSignal>::value>::type,                      \
        typename F = name##OpRFunctor<TLeftVal, TRightVal>,                                        \
        typename S = typename std::result_of<F( TRightVal )>::type,                                \
        typename TOp                                                                               \
        = ::react::detail::FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TRightVal>>>        \
    auto operator op( TLeftValIn&& lhs, const TRightSignal& rhs )->temp_signal<D, S, TOp>           \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F( std::forward<TLeftValIn>( lhs ) ), GetNodePtr( rhs ) ) );                           \
    }                                                                                              \
    template <typename D,                                                                          \
        typename TLeftVal,                                                                         \
        typename TLeftOp,                                                                          \
        typename TRightVal,                                                                        \
        typename TRightOp,                                                                         \
        typename F = name##OpFunctor<TLeftVal, TRightVal>,                                         \
        typename S = typename std::result_of<F( TLeftVal, TRightVal )>::type,                      \
        typename TOp = ::react::detail::FunctionOp<S, F, TLeftOp, TRightOp>>                       \
    auto operator op(                                                                              \
        temp_signal<D, TLeftVal, TLeftOp>&& lhs, temp_signal<D, TRightVal, TRightOp>&& rhs )         \
        ->temp_signal<D, S, TOp>                                                                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), lhs.StealOp(), rhs.StealOp() ) );                                                 \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TLeftVal,                                                                         \
        typename TLeftOp,                                                                          \
        typename TRightSignal,                                                                     \
        typename TRightVal = typename TRightSignal::ValueT,                                        \
        class = typename std::enable_if<IsSignal<TRightSignal>::value>::type,                      \
        typename F = name##OpFunctor<TLeftVal, TRightVal>,                                         \
        typename S = typename std::result_of<F( TLeftVal, TRightVal )>::type,                      \
        typename TOp = ::react::detail::                                                           \
            FunctionOp<S, F, TLeftOp, ::react::detail::SignalNodePtrT<D, TRightVal>>>              \
    auto operator op( temp_signal<D, TLeftVal, TLeftOp>&& lhs, const TRightSignal& rhs )            \
        ->temp_signal<D, S, TOp>                                                                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), lhs.StealOp(), GetNodePtr( rhs ) ) );                                             \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftSignal,                                                                \
        typename D,                                                                                \
        typename TRightVal,                                                                        \
        typename TRightOp,                                                                         \
        typename TLeftVal = typename TLeftSignal::ValueT,                                          \
        class = typename std::enable_if<IsSignal<TLeftSignal>::value>::type,                       \
        typename F = name##OpFunctor<TLeftVal, TRightVal>,                                         \
        typename S = typename std::result_of<F( TLeftVal, TRightVal )>::type,                      \
        typename TOp = ::react::detail::                                                           \
            FunctionOp<S, F, ::react::detail::SignalNodePtrT<D, TLeftVal>, TRightOp>>              \
    auto operator op( const TLeftSignal& lhs, temp_signal<D, TRightVal, TRightOp>&& rhs )           \
        ->temp_signal<D, S, TOp>                                                                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), GetNodePtr( lhs ), rhs.StealOp() ) );                                             \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TLeftVal,                                                                         \
        typename TLeftOp,                                                                          \
        typename TRightValIn,                                                                      \
        typename TRightVal = typename std::decay<TRightValIn>::type,                               \
        class = typename std::enable_if<!IsSignal<TRightVal>::value>::type,                        \
        typename F = name##OpLFunctor<TLeftVal, TRightVal>,                                        \
        typename S = typename std::result_of<F( TLeftVal )>::type,                                 \
        typename TOp = ::react::detail::FunctionOp<S, F, TLeftOp>>                                 \
    auto operator op( temp_signal<D, TLeftVal, TLeftOp>&& lhs, TRightValIn&& rhs )                  \
        ->temp_signal<D, S, TOp>                                                                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F( std::forward<TRightValIn>( rhs ) ), lhs.StealOp() ) );                              \
    }                                                                                              \
                                                                                                   \
    template <typename TLeftValIn,                                                                 \
        typename D,                                                                                \
        typename TRightVal,                                                                        \
        typename TRightOp,                                                                         \
        typename TLeftVal = typename std::decay<TLeftValIn>::type,                                 \
        class = typename std::enable_if<!IsSignal<TLeftVal>::value>::type,                         \
        typename F = name##OpRFunctor<TLeftVal, TRightVal>,                                        \
        typename S = typename std::result_of<F( TRightVal )>::type,                                \
        typename TOp = ::react::detail::FunctionOp<S, F, TRightOp>>                                \
    auto operator op( TLeftValIn&& lhs, temp_signal<D, TRightVal, TRightOp>&& rhs )                 \
        ->temp_signal<D, S, TOp>                                                                    \
    {                                                                                              \
        return temp_signal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F( std::forward<TLeftValIn>( lhs ) ), rhs.StealOp() ) );                               \
    }

REACT_DECLARE_OP( +, Addition )

REACT_DECLARE_OP( -, Subtraction )

REACT_DECLARE_OP( *, Multiplication )

REACT_DECLARE_OP( /, Division )

REACT_DECLARE_OP( %, Modulo )

REACT_DECLARE_OP( ==, Equal )

REACT_DECLARE_OP( !=, NotEqual )

REACT_DECLARE_OP( <, Less )

REACT_DECLARE_OP( <=, LessEqual )

REACT_DECLARE_OP( >, Greater )

REACT_DECLARE_OP( >=, GreaterEqual )

REACT_DECLARE_OP( &&, LogicalAnd )

REACT_DECLARE_OP( ||, LogicalOr )

REACT_DECLARE_OP( &, BitwiseAnd )

REACT_DECLARE_OP( |, BitwiseOr )

REACT_DECLARE_OP( ^, BitwiseXor )
//REACT_DECLARE_OP(<<, BitwiseLeftShift); // MSVC: Internal compiler error
//REACT_DECLARE_OP(>>, BitwiseRightShift);

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to create signal pack from 2 signals.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TLeftVal, typename TRightVal>
auto operator,( const signal<D, TLeftVal>& a, const signal<D, TRightVal>& b )
                  -> signal_pack<D, TLeftVal, TRightVal>
{
    return signal_pack<D, TLeftVal, TRightVal>( a, b );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to append node to existing signal pack.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TCurValues, typename TAppendValue>
auto operator,( const signal_pack<D, TCurValues...>& cur, const signal<D, TAppendValue>& append )
                  -> signal_pack<D, TCurValues..., TAppendValue>
{
    return signal_pack<D, TCurValues..., TAppendValue>( cur, append );
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
    class = typename std::enable_if<IsSignal<TSignal<D, TValue>>::value>::type>
auto operator->*( const TSignal<D, TValue>& arg, F&& func )
    -> signal<D, typename std::result_of<F( TValue )>::type>
{
    return ::react::MakeSignal( arg, std::forward<F>( func ) );
}

// Multiple args
template <typename D, typename F, typename... TValues>
auto operator->*( const signal_pack<D, TValues...>& argPack, F&& func )
    -> signal<D, typename std::result_of<F( TValues... )>::type>
{
    return ::react::MakeSignal( argPack, std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TInnerValue>
auto Flatten( const signal<D, signal<D, TInnerValue>>& outer ) -> signal<D, TInnerValue>
{
    return signal<D, TInnerValue>(
        std::make_shared<::react::detail::FlattenNode<D, signal<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class signal : public ::react::detail::SignalBase<D, S>
{
private:
    using NodeT = ::react::detail::signal_node<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    signal() = default;

    // Copy ctor
    signal( const signal& ) = default;

    // Move ctor
    signal( signal&& other ) noexcept
        : signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit signal( NodePtrT&& nodePtr )
        : signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    signal& operator=( const signal& ) = default;

    // Move assignment
    signal& operator=( signal&& other ) noexcept
    {
        signal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return signal::SignalBase::getValue();
    }

    const S& operator()() const
    {
        return signal::SignalBase::getValue();
    }

    bool Equals( const signal& other ) const
    {
        return signal::SignalBase::Equals( other );
    }

    bool IsValid() const
    {
        return signal::SignalBase::IsValid();
    }

    S Flatten() const
    {
        static_assert( IsSignal<S>::value || IsEvent<S>::value,
            "Flatten requires a signal or events value type." );
        return ::react::Flatten( *this );
    }
};

// Specialize for references
template <typename D, typename S>
class signal<D, S&> : public ::react::detail::SignalBase<D, std::reference_wrapper<S>>
{
private:
    using NodeT = ::react::detail::signal_node<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    signal() = default;

    // Copy ctor
    signal( const signal& ) = default;

    // Move ctor
    signal( signal&& other ) noexcept
        : signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit signal( NodePtrT&& nodePtr )
        : signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    signal& operator=( const signal& ) = default;

    // Move assignment
    signal& operator=( signal&& other ) noexcept
    {
        signal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return signal::SignalBase::getValue();
    }

    const S& operator()() const
    {
        return signal::SignalBase::getValue();
    }

    bool Equals( const signal& other ) const
    {
        return signal::SignalBase::Equals( other );
    }

    bool IsValid() const
    {
        return signal::SignalBase::IsValid();
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// var_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class var_signal : public signal<D, S>
{
private:
    using NodeT = ::react::detail::VarNode<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

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
    explicit var_signal( NodePtrT&& nodePtr )
        : var_signal::signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    var_signal& operator=( const var_signal& ) = default;

    // Move assignment
    var_signal& operator=( var_signal&& other ) noexcept
    {
        var_signal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    void Set( const S& newValue ) const
    {
        var_signal::SignalBase::setValue( newValue );
    }

    void Set( S&& newValue ) const
    {
        var_signal::SignalBase::setValue( std::move( newValue ) );
    }

    const var_signal& operator<<=( const S& newValue ) const
    {
        var_signal::SignalBase::setValue( newValue );
        return *this;
    }

    const var_signal& operator<<=( S&& newValue ) const
    {
        var_signal::SignalBase::setValue( std::move( newValue ) );
        return *this;
    }

    template <typename F>
    void Modify( const F& func ) const
    {
        var_signal::SignalBase::modifyValue( func );
    }
};

// Specialize for references
template <typename D, typename S>
class var_signal<D, S&> : public signal<D, std::reference_wrapper<S>>
{
private:
    using NodeT = ::react::detail::VarNode<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    var_signal() = default;

    // Copy ctor
    var_signal( const var_signal& ) = default;

    // Move ctor
    var_signal( var_signal&& other ) noexcept
        : var_signal::signal( std::move( other ) )
    {}

    // Node ctor
    explicit var_signal( NodePtrT&& nodePtr )
        : var_signal::signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    var_signal& operator=( const var_signal& ) = default;

    // Move assignment
    var_signal& operator=( var_signal&& other ) noexcept
    {
        var_signal::signal::operator=( std::move( other ) );
        return *this;
    }

    void Set( std::reference_wrapper<S> newValue ) const
    {
        var_signal::SignalBase::setValue( newValue );
    }

    const var_signal& operator<<=( std::reference_wrapper<S> newValue ) const
    {
        var_signal::SignalBase::setValue( newValue );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// temp_signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename TOp>
class temp_signal : public signal<D, S>
{
private:
    using NodeT = ::react::detail::SignalOpNode<D, S, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

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
    explicit temp_signal( NodePtrT&& ptr )
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

    TOp StealOp()
    {
        return std::move( reinterpret_cast<NodeT*>( this->ptr_.get() )->StealOp() );
    }
};

namespace detail
{

template <typename D, typename L, typename R>
bool Equals( const signal<D, L>& lhs, const signal<D, R>& rhs )
{
    return lhs.Equals( rhs );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten macros
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
    Flatten( MakeSignal( obj,                                                                      \
        []( const REACT_MSVC_NO_TYPENAME ::react::detail::type_identity<decltype(                  \
                obj )>::type::ValueT& r ) {                                                        \
            using T = decltype( r.name );                                                          \
            using S = REACT_MSVC_NO_TYPENAME ::react::DecayInput<T>::Type;                         \
            return static_cast<S>( r.name );                                                       \
        } ) )

#define REACTIVE_PTR( obj, name )                                                                  \
    Flatten( MakeSignal( obj,                                                                      \
        []( REACT_MSVC_NO_TYPENAME ::react::detail::type_identity<decltype( obj )>::type::ValueT   \
                r ) {                                                                              \
            assert( r != nullptr );                                                                \
            using T = decltype( r->name );                                                         \
            using S = REACT_MSVC_NO_TYPENAME ::react::DecayInput<T>::Type;                         \
            return static_cast<S>( r->name );                                                      \
        } ) )

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_stream_node
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class event_stream_node : public observable_node<D>
{
public:
    using DataT = std::vector<E>;
    using EngineT = typename D::Engine;
    using TurnT = typename EngineT::TurnT;

    event_stream_node() = default;

    void SetCurrentTurn( const TurnT& turn, bool forceUpdate = false, bool noClear = false )
    {
        if( curTurnId_ != turn.Id() || forceUpdate )
        {
            curTurnId_ = turn.Id();
            if( !noClear )
            {
                events_.clear();
            }
        }
    }

    DataT& events()
    {
        return events_;
    }

protected:
    DataT events_;

private:
    unsigned curTurnId_{ ( std::numeric_limits<unsigned>::max )() };
};

template <typename D, typename E>
using EventStreamNodePtrT = std::shared_ptr<event_stream_node<D, E>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class EventSourceNode
    : public event_stream_node<D, E>
    , public input_node_interface
{
    using Engine = typename EventSourceNode::Engine;

public:
    EventSourceNode()
        : EventSourceNode::event_stream_node{}
    {}

    ~EventSourceNode() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked EventSourceNode" );
    }

    template <typename V>
    void add_input( V&& v )
    {
        // Clear input from previous turn
        if( changedFlag_ )
        {
            changedFlag_ = false;
            this->events_.clear();
        }

        this->events_.push_back( std::forward<V>( v ) );
    }

    bool apply_input( void* turn_ptr ) override
    {
        if( this->events_.size() > 0 && !changedFlag_ )
        {
            using TurnT = typename D::Engine::TurnT;
            TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

            this->SetCurrentTurn( turn, true, true );
            changedFlag_ = true;
            Engine::on_input_change( *this );
            return true;
        }
        else
        {
            return false;
        }
    }

private:
    bool changedFlag_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventMergeOp
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename... deps_t>
class EventMergeOp : public reactive_op_base<deps_t...>
{
public:
    template <typename... TDepsIn>
    EventMergeOp( TDepsIn&&... deps )
        : EventMergeOp::reactive_op_base( DontMove(), std::forward<TDepsIn>( deps )... )
    {}

    EventMergeOp( EventMergeOp&& other ) noexcept
        : EventMergeOp::reactive_op_base( std::move( other ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        apply( CollectFunctor<TTurn, TCollector>( turn, collector ), this->m_deps );
    }

    template <typename TTurn, typename TCollector, typename functor_t>
    void CollectRec( const functor_t& functor ) const
    {
        apply(
            reinterpret_cast<const CollectFunctor<TTurn, TCollector>&>( functor ), this->m_deps );
    }

private:
    template <typename TTurn, typename TCollector>
    struct CollectFunctor
    {
        CollectFunctor( const TTurn& turn, const TCollector& collector )
            : MyTurn( turn )
            , MyCollector( collector )
        {}

        void operator()( const deps_t&... deps ) const
        {
            REACT_EXPAND_PACK( collect( deps ) );
        }

        template <typename T>
        void collect( const T& op ) const
        {
            op.template CollectRec<TTurn, TCollector>( *this );
        }

        template <typename T>
        void collect( const std::shared_ptr<T>& dep_ptr ) const
        {
            dep_ptr->SetCurrentTurn( MyTurn );

            for( const auto& v : dep_ptr->events() )
            {
                MyCollector( v );
            }
        }

        const TTurn& MyTurn;
        const TCollector& MyCollector;
    };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventFilterOp
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename TFilter, typename TDep>
class EventFilterOp : public reactive_op_base<TDep>
{
public:
    template <typename TFilterIn, typename TDepIn>
    EventFilterOp( TFilterIn&& filter, TDepIn&& dep )
        : EventFilterOp::reactive_op_base{ DontMove(), std::forward<TDepIn>( dep ) }
        , filter_( std::forward<TFilterIn>( filter ) )
    {}

    EventFilterOp( EventFilterOp&& other ) noexcept
        : EventFilterOp::reactive_op_base{ std::move( other ) }
        , filter_( std::move( other.filter_ ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        collectImpl( turn, FilteredEventCollector<TCollector>{ filter_, collector }, getDep() );
    }

    template <typename TTurn, typename TCollector, typename functor_t>
    void CollectRec( const functor_t& functor ) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn, TCollector>( functor.MyTurn, functor.MyCollector );
    }

private:
    const TDep& getDep() const
    {
        return std::get<0>( this->m_deps );
    }

    template <typename TCollector>
    struct FilteredEventCollector
    {
        FilteredEventCollector( const TFilter& filter, const TCollector& collector )
            : MyFilter( filter )
            , MyCollector( collector )
        {}

        void operator()( const E& e ) const
        {
            // Accepted?
            if( MyFilter( e ) )
            {
                MyCollector( e );
            }
        }

        const TFilter& MyFilter;
        const TCollector& MyCollector; // The wrapped collector
    };

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl( const TTurn& turn, const TCollector& collector, const T& op )
    {
        op.Collect( turn, collector );
    }

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(
        const TTurn& turn, const TCollector& collector, const std::shared_ptr<T>& dep_ptr )
    {
        dep_ptr->SetCurrentTurn( turn );

        for( const auto& v : dep_ptr->events() )
        {
            collector( v );
        }
    }

    TFilter filter_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventTransformOp
///////////////////////////////////////////////////////////////////////////////////////////////////
// Todo: Refactor code duplication
template <typename E, typename TFunc, typename TDep>
class EventTransformOp : public reactive_op_base<TDep>
{
public:
    template <typename TFuncIn, typename TDepIn>
    EventTransformOp( TFuncIn&& func, TDepIn&& dep )
        : EventTransformOp::reactive_op_base( DontMove(), std::forward<TDepIn>( dep ) )
        , func_( std::forward<TFuncIn>( func ) )
    {}

    EventTransformOp( EventTransformOp&& other ) noexcept
        : EventTransformOp::reactive_op_base( std::move( other ) )
        , func_( std::move( other.func_ ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        collectImpl( turn, TransformEventCollector<TCollector>( func_, collector ), getDep() );
    }

    template <typename TTurn, typename TCollector, typename functor_t>
    void CollectRec( const functor_t& functor ) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn, TCollector>( functor.MyTurn, functor.MyCollector );
    }

private:
    const TDep& getDep() const
    {
        return std::get<0>( this->m_deps );
    }

    template <typename TTarget>
    struct TransformEventCollector
    {
        TransformEventCollector( const TFunc& func, const TTarget& target )
            : MyFunc( func )
            , MyTarget( target )
        {}

        void operator()( const E& e ) const
        {
            MyTarget( MyFunc( e ) );
        }

        const TFunc& MyFunc;
        const TTarget& MyTarget;
    };

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl( const TTurn& turn, const TCollector& collector, const T& op )
    {
        op.Collect( turn, collector );
    }

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(
        const TTurn& turn, const TCollector& collector, const std::shared_ptr<T>& dep_ptr )
    {
        dep_ptr->SetCurrentTurn( turn );

        for( const auto& v : dep_ptr->events() )
        {
            collector( v );
        }
    }

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventOpNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TOp>
class EventOpNode : public event_stream_node<D, E>
{
    using Engine = typename EventOpNode::Engine;

public:
    template <typename... args_t>
    EventOpNode( args_t&&... args )
        : EventOpNode::event_stream_node()
        , op_( std::forward<args_t>( args )... )
    {

        op_.template attach<D>( *this );
    }

    ~EventOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template detach<D>( *this );
        }
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );

        op_.Collect( turn, EventCollector( this->events_ ) );

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

    TOp StealOp()
    {
        assert( !wasOpStolen_ && "Op was already stolen." );
        wasOpStolen_ = true;
        op_.template detach<D>( *this );
        return std::move( op_ );
    }

private:
    struct EventCollector
    {
        using DataT = typename EventOpNode::DataT;

        EventCollector( DataT& events )
            : MyEvents( events )
        {}

        void operator()( const E& e ) const
        {
            MyEvents.push_back( e );
        }

        DataT& MyEvents;
    };

    TOp op_;
    bool wasOpStolen_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventFlattenNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TOuter, typename TInner>
class EventFlattenNode : public event_stream_node<D, TInner>
{
    using Engine = typename EventFlattenNode::Engine;

public:
    EventFlattenNode( const std::shared_ptr<signal_node<D, TOuter>>& outer,
        const std::shared_ptr<event_stream_node<D, TInner>>& inner )
        : EventFlattenNode::event_stream_node()
        , outer_( outer )
        , inner_( inner )
    {

        Engine::on_node_attach( *this, *outer_ );
        Engine::on_node_attach( *this, *inner_ );
    }

    ~EventFlattenNode()
    {
        Engine::on_node_detach( *this, *outer_ );
        Engine::on_node_detach( *this, *inner_ );
    }

    void tick( void* turn_ptr ) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );
        inner_->SetCurrentTurn( turn );

        auto newInner = GetNodePtr( outer_->ValueRef() );

        if( newInner != inner_ )
        {
            newInner->SetCurrentTurn( turn );

            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::on_dynamic_node_detach( *this, *oldInner );
            Engine::on_dynamic_node_attach( *this, *newInner );

            return;
        }

        this->events_.insert(
            this->events_.end(), inner_->events().begin(), inner_->events().end() );

        if( this->events_.size() > 0 )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<signal_node<D, TOuter>> outer_;
    std::shared_ptr<event_stream_node<D, TInner>> inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventTransformNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc, typename... TDepValues>
class SyncedEventTransformNode : public event_stream_node<D, TOut>
{
    using Engine = typename SyncedEventTransformNode::Engine;

public:
    template <typename F>
    SyncedEventTransformNode( const std::shared_ptr<event_stream_node<D, TIn>>& source,
        F&& func,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedEventTransformNode::event_stream_node()
        , source_( source )
        , func_( std::forward<F>( func ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedEventTransformNode()
    {
        Engine::on_node_detach( *this, *source_ );

        apply( detach_functor<D,
                   SyncedEventTransformNode,
                   std::shared_ptr<signal_node<D, TDepValues>>...>( *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn( turn );

        // Don't time if there is nothing to do
        if( !source_->events().empty() )
        {
            for( const auto& e : source_->events() )
            {
                this->events_.push_back( apply(
                    [this, &e]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                        return func_( e, args->ValueRef()... );
                    },
                    deps_ ) );
            }
        }

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::shared_ptr<event_stream_node<D, TIn>> source_;

    TFunc func_;
    dep_holder_t deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventFilterNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TFunc, typename... TDepValues>
class SyncedEventFilterNode : public event_stream_node<D, E>
{
    using Engine = typename SyncedEventFilterNode::Engine;

public:
    template <typename F>
    SyncedEventFilterNode( const std::shared_ptr<event_stream_node<D, E>>& source,
        F&& filter,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedEventFilterNode::event_stream_node()
        , source_( source )
        , filter_( std::forward<F>( filter ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedEventFilterNode()
    {
        Engine::on_node_detach( *this, *source_ );

        apply(
            detach_functor<D, SyncedEventFilterNode, std::shared_ptr<signal_node<D, TDepValues>>...>(
                *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn( turn );

        // Don't time if there is nothing to do
        if( !source_->events().empty() )
        {
            for( const auto& e : source_->events() )
            {
                if( apply(
                        [this, &e]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                            return filter_( e, args->ValueRef()... );
                        },
                        deps_ ) )
                {
                    this->events_.push_back( e );
                }
            }
        }

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::shared_ptr<event_stream_node<D, E>> source_;

    TFunc filter_;
    dep_holder_t deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc>
class EventProcessingNode : public event_stream_node<D, TOut>
{
    using Engine = typename EventProcessingNode::Engine;

public:
    template <typename F>
    EventProcessingNode( const std::shared_ptr<event_stream_node<D, TIn>>& source, F&& func )
        : EventProcessingNode::event_stream_node()
        , source_( source )
        , func_( std::forward<F>( func ) )
    {

        Engine::on_node_attach( *this, *source );
    }

    ~EventProcessingNode()
    {
        Engine::on_node_detach( *this, *source_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );

        func_( EventRange<TIn>( source_->events() ), std::back_inserter( this->events_ ) );

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<event_stream_node<D, TIn>> source_;

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc, typename... TDepValues>
class SyncedEventProcessingNode : public event_stream_node<D, TOut>
{
    using Engine = typename SyncedEventProcessingNode::Engine;

public:
    template <typename F>
    SyncedEventProcessingNode( const std::shared_ptr<event_stream_node<D, TIn>>& source,
        F&& func,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedEventProcessingNode::event_stream_node()
        , source_( source )
        , func_( std::forward<F>( func ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *source );
        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedEventProcessingNode()
    {
        Engine::on_node_detach( *this, *source_ );

        apply( detach_functor<D,
                   SyncedEventProcessingNode,
                   std::shared_ptr<signal_node<D, TDepValues>>...>( *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn( turn );

        // Don't time if there is nothing to do
        if( !source_->events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                    func_( EventRange<TIn>( source_->events() ),
                        std::back_inserter( this->events_ ),
                        args->ValueRef()... );
                },
                deps_ );
        }

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::shared_ptr<event_stream_node<D, TIn>> source_;

    TFunc func_;
    dep_holder_t deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventJoinNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
class EventJoinNode : public event_stream_node<D, std::tuple<TValues...>>
{
    using Engine = typename EventJoinNode::Engine;
    using TurnT = typename Engine::TurnT;

public:
    EventJoinNode( const std::shared_ptr<event_stream_node<D, TValues>>&... sources )
        : EventJoinNode::event_stream_node()
        , slots_( sources... )
    {

        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *sources ) );
    }

    ~EventJoinNode()
    {
        apply(
            [this]( Slot<TValues>&... slots ) {
                REACT_EXPAND_PACK( Engine::on_node_detach( *this, *slots.Source ) );
            },
            slots_ );
    }

    void tick( void* turn_ptr ) override
    {
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );

        // Don't time if there is nothing to do
        { // timer
            size_t count = 0;
            using TimerT = typename EventJoinNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, count );

            // Move events into buffers
            apply(
                [this, &turn](
                    Slot<TValues>&... slots ) { REACT_EXPAND_PACK( fetchBuffer( turn, slots ) ); },
                slots_ );

            while( true )
            {
                bool isReady = true;

                // All slots ready?
                apply(
                    [this, &isReady]( Slot<TValues>&... slots ) {
                        // Todo: combine return values instead
                        REACT_EXPAND_PACK( checkSlot( slots, isReady ) );
                    },
                    slots_ );

                if( !isReady )
                {
                    break;
                }

                // Pop values from buffers and emit tuple
                apply(
                    [this]( Slot<TValues>&... slots ) {
                        this->events_.emplace_back( slots.Buffer.front()... );
                        REACT_EXPAND_PACK( slots.Buffer.pop_front() );
                    },
                    slots_ );
            }

            count = this->events_.size();

        } // ~timer

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    template <typename T>
    struct Slot
    {
        Slot( const std::shared_ptr<event_stream_node<D, T>>& src )
            : Source( src )
        {}

        std::shared_ptr<event_stream_node<D, T>> Source;
        std::deque<T> Buffer;
    };

    template <typename T>
    static void fetchBuffer( TurnT& turn, Slot<T>& slot )
    {
        slot.Source->SetCurrentTurn( turn );

        slot.Buffer.insert(
            slot.Buffer.end(), slot.Source->events().begin(), slot.Source->events().end() );
    }

    template <typename T>
    static void checkSlot( Slot<T>& slot, bool& isReady )
    {
        auto t = isReady && !slot.Buffer.empty();
        isReady = t;
    }

    std::tuple<Slot<TValues>...> slots_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventStreamBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class EventStreamBase : public CopyableReactive<event_stream_node<D, E>>
{
public:
    EventStreamBase() = default;
    EventStreamBase( const EventStreamBase& ) = default;

    template <typename T>
    EventStreamBase( T&& t )
        : EventStreamBase::CopyableReactive( std::forward<T>( t ) )
    {}

protected:
    template <typename T>
    void emit( T&& e ) const
    {
        domain_specific_input_manager<D>::Instance().add_input(
            *reinterpret_cast<EventSourceNode<D, E>*>( this->ptr_.get() ), std::forward<T>( e ) );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeEventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
auto MakeEventSource() -> event_source<D, E>
{
    using ::react::detail::EventSourceNode;

    return event_source<D, E>( std::make_shared<EventSourceNode<D, E>>() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename TArg1,
    typename... args_t,
    typename E = TArg1,
    typename TOp = ::react::detail::EventMergeOp<E,
        ::react::detail::EventStreamNodePtrT<D, TArg1>,
        ::react::detail::EventStreamNodePtrT<D, args_t>...>>
auto Merge( const events<D, TArg1>& arg1, const events<D, args_t>&... args )
    -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    static_assert( sizeof...( args_t ) > 0, "Merge: 2+ arguments are required." );

    return temp_events<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( arg1 ), GetNodePtr( args )... ) );
}

template <typename TLeftEvents,
    typename TRightEvents,
    typename D = typename TLeftEvents::DomainT,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename TRightVal = typename TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = ::react::detail::EventMergeOp<E,
        ::react::detail::EventStreamNodePtrT<D, TLeftVal>,
        ::react::detail::EventStreamNodePtrT<D, TRightVal>>,
    class = typename std::enable_if<IsEvent<TLeftEvents>::value>::type,
    class = typename std::enable_if<IsEvent<TRightEvents>::value>::type>
auto operator|( const TLeftEvents& lhs, const TRightEvents& rhs ) -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( lhs ), GetNodePtr( rhs ) ) );
}

template <typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightVal,
    typename TRightOp,
    typename E = TLeftVal,
    typename TOp = ::react::detail::EventMergeOp<E, TLeftOp, TRightOp>>
auto operator|( temp_events<D, TLeftVal, TLeftOp>&& lhs, temp_events<D, TRightVal, TRightOp>&& rhs )
    -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( lhs.StealOp(), rhs.StealOp() ) );
}

template <typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightEvents,
    typename TRightVal = typename TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp
    = ::react::detail::EventMergeOp<E, TLeftOp, ::react::detail::EventStreamNodePtrT<D, TRightVal>>,
    class = typename std::enable_if<IsEvent<TRightEvents>::value>::type>
auto operator|( temp_events<D, TLeftVal, TLeftOp>&& lhs, const TRightEvents& rhs )
    -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( lhs.StealOp(), GetNodePtr( rhs ) ) );
}

template <typename TLeftEvents,
    typename D,
    typename TRightVal,
    typename TRightOp,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = ::react::detail::
        EventMergeOp<E, ::react::detail::EventStreamNodePtrT<D, TRightVal>, TRightOp>,
    class = typename std::enable_if<IsEvent<TLeftEvents>::value>::type>
auto operator|( const TLeftEvents& lhs, temp_events<D, TRightVal, TRightOp>&& rhs )
    -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( lhs ), rhs.StealOp() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOp = ::react::detail::EventFilterOp<E, F, ::react::detail::EventStreamNodePtrT<D, E>>>
auto Filter( const events<D, E>& src, FIn&& filter ) -> temp_events<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOp>( std::make_shared<EventOpNode<D, E, TOp>>(
        std::forward<FIn>( filter ), GetNodePtr( src ) ) );
}

template <typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOpOut = ::react::detail::EventFilterOp<E, F, TOpIn>>
auto Filter( temp_events<D, E, TOpIn>&& src, FIn&& filter ) -> temp_events<D, E, TOpOut>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, E, TOpOut>(
        std::make_shared<EventOpNode<D, E, TOpOut>>( std::forward<FIn>( filter ), src.StealOp() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename FIn, typename... TDepValues>
auto Filter( const events<D, E>& source, const signal_pack<D, TDepValues...>& depPack, FIn&& func )
    -> events<D, E>
{
    using ::react::detail::SyncedEventFilterNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const events<D, E>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TDepValues>&... deps ) -> events<D, E>
        {
            return events<D, E>( std::make_shared<SyncedEventFilterNode<D, E, F, TDepValues...>>(
                GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const events<D, E>& MySource;
        FIn MyFunc;
    };

    return ::react::detail::apply(
        NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename TIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F( TIn )>::type,
    typename TOp
    = ::react::detail::EventTransformOp<TIn, F, ::react::detail::EventStreamNodePtrT<D, TIn>>>
auto Transform( const events<D, TIn>& src, FIn&& func ) -> temp_events<D, TOut, TOp>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, TOut, TOp>( std::make_shared<EventOpNode<D, TOut, TOp>>(
        std::forward<FIn>( func ), GetNodePtr( src ) ) );
}

template <typename D,
    typename TIn,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F( TIn )>::type,
    typename TOpOut = ::react::detail::EventTransformOp<TIn, F, TOpIn>>
auto Transform( temp_events<D, TIn, TOpIn>&& src, FIn&& func ) -> temp_events<D, TOut, TOpOut>
{
    using ::react::detail::EventOpNode;

    return temp_events<D, TOut, TOpOut>( std::make_shared<EventOpNode<D, TOut, TOpOut>>(
        std::forward<FIn>( func ), src.StealOp() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename TIn,
    typename FIn,
    typename... TDepValues,
    typename TOut = typename std::result_of<FIn( TIn, TDepValues... )>::type>
auto Transform(
    const events<D, TIn>& source, const signal_pack<D, TDepValues...>& depPack, FIn&& func )
    -> events<D, TOut>
{
    using ::react::detail::SyncedEventTransformNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const events<D, TIn>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TDepValues>&... deps ) -> events<D, TOut>
        {
            return events<D, TOut>(
                std::make_shared<SyncedEventTransformNode<D, TIn, TOut, F, TDepValues...>>(
                    GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const events<D, TIn>& MySource;
        FIn MyFunc;
    };

    return ::react::detail::apply(
        NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process
///////////////////////////////////////////////////////////////////////////////////////////////////
using ::react::detail::EventEmitter;
using ::react::detail::EventRange;

template <typename TOut,
    typename D,
    typename TIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type>
auto Process( const events<D, TIn>& src, FIn&& func ) -> events<D, TOut>
{
    using ::react::detail::EventProcessingNode;

    return events<D, TOut>( std::make_shared<EventProcessingNode<D, TIn, TOut, F>>(
        GetNodePtr( src ), std::forward<FIn>( func ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename D, typename TIn, typename FIn, typename... TDepValues>
auto Process(
    const events<D, TIn>& source, const signal_pack<D, TDepValues...>& depPack, FIn&& func )
    -> events<D, TOut>
{
    using ::react::detail::SyncedEventProcessingNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const events<D, TIn>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TDepValues>&... deps ) -> events<D, TOut>
        {
            return events<D, TOut>(
                std::make_shared<SyncedEventProcessingNode<D, TIn, TOut, F, TDepValues...>>(
                    GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const events<D, TIn>& MySource;
        FIn MyFunc;
    };

    return ::react::detail::apply(
        NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TInnerValue>
auto Flatten( const signal<D, events<D, TInnerValue>>& outer ) -> events<D, TInnerValue>
{
    return events<D, TInnerValue>(
        std::make_shared<::react::detail::EventFlattenNode<D, events<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... args_t>
auto Join( const events<D, args_t>&... args ) -> events<D, std::tuple<args_t...>>
{
    using ::react::detail::EventJoinNode;

    static_assert( sizeof...( args_t ) > 1, "Join: 2+ arguments are required." );

    return events<D, std::tuple<args_t...>>(
        std::make_shared<EventJoinNode<D, args_t...>>( GetNodePtr( args )... ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// token
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class token
{
    value
};

struct Tokenizer
{
    template <typename T>
    token operator()( const T& ) const
    {
        return token::value;
    }
};

template <typename TEvents>
auto Tokenize( TEvents&& source ) -> decltype( Transform( source, Tokenizer{} ) )
{
    return Transform( source, Tokenizer{} );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
class events : public ::react::detail::EventStreamBase<D, E>
{
private:
    using NodeT = ::react::detail::event_stream_node<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    events() = default;

    // Copy ctor
    events( const events& ) = default;

    // Move ctor
    events( events&& other ) noexcept
        : events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit events( NodePtrT&& nodePtr )
        : events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    events& operator=( const events& ) = default;

    // Move assignment
    events& operator=( events&& other ) noexcept
    {
        events::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    bool Equals( const events& other ) const
    {
        return events::EventStreamBase::Equals( other );
    }

    bool IsValid() const
    {
        return events::EventStreamBase::IsValid();
    }

    auto Tokenize() const -> decltype( ::react::Tokenize( std::declval<events>() ) )
    {
        return ::react::Tokenize( *this );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args ) const
        -> decltype( ::react::Merge( std::declval<events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

// Specialize for references
template <typename D, typename E>
class events<D, E&> : public ::react::detail::EventStreamBase<D, std::reference_wrapper<E>>
{
private:
    using NodeT = ::react::detail::event_stream_node<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    events() = default;

    // Copy ctor
    events( const events& ) = default;

    // Move ctor
    events( events&& other ) noexcept
        : events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit events( NodePtrT&& nodePtr )
        : events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    events& operator=( const events& ) = default;

    // Move assignment
    events& operator=( events&& other ) noexcept
    {
        events::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    bool Equals( const events& other ) const
    {
        return events::EventStreamBase::Equals( other );
    }

    bool IsValid() const
    {
        return events::EventStreamBase::IsValid();
    }

    auto Tokenize() const -> decltype( ::react::Tokenize( std::declval<events>() ) )
    {
        return ::react::Tokenize( *this );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args )
        -> decltype( ::react::Merge( std::declval<events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<events>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// event_source
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = token>
class event_source : public events<D, E>
{
private:
    using NodeT = ::react::detail::EventSourceNode<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

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
    explicit event_source( NodePtrT&& nodePtr )
        : event_source::events( std::move( nodePtr ) )
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
    void Emit( const E& e ) const
    {
        event_source::EventStreamBase::emit( e );
    }

    void Emit( E&& e ) const
    {
        event_source::EventStreamBase::emit( std::move( e ) );
    }

    void Emit() const
    {
        static_assert( std::is_same<E, token>::value, "Can't emit on non token stream." );
        event_source::EventStreamBase::emit( token::value );
    }

    // Function object style
    void operator()( const E& e ) const
    {
        event_source::EventStreamBase::emit( e );
    }

    void operator()( E&& e ) const
    {
        event_source::EventStreamBase::emit( std::move( e ) );
    }

    void operator()() const
    {
        static_assert( std::is_same<E, token>::value, "Can't emit on non token stream." );
        event_source::EventStreamBase::emit( token::value );
    }

    // Stream style
    const event_source& operator<<( const E& e ) const
    {
        event_source::EventStreamBase::emit( e );
        return *this;
    }

    const event_source& operator<<( E&& e ) const
    {
        event_source::EventStreamBase::emit( std::move( e ) );
        return *this;
    }
};

// Specialize for references
template <typename D, typename E>
class event_source<D, E&> : public events<D, std::reference_wrapper<E>>
{
private:
    using NodeT = ::react::detail::EventSourceNode<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

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
    explicit event_source( NodePtrT&& nodePtr )
        : event_source::events( std::move( nodePtr ) )
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
    void Emit( std::reference_wrapper<E> e ) const
    {
        event_source::EventStreamBase::emit( e );
    }

    // Function object style
    void operator()( std::reference_wrapper<E> e ) const
    {
        event_source::EventStreamBase::emit( e );
    }

    // Stream style
    const event_source& operator<<( std::reference_wrapper<E> e ) const
    {
        event_source::EventStreamBase::emit( e );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// temp_events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TOp>
class temp_events : public events<D, E>
{
protected:
    using NodeT = ::react::detail::EventOpNode<D, E, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

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
    explicit temp_events( NodePtrT&& nodePtr )
        : temp_events::events( std::move( nodePtr ) )
    {}

    // Copy assignment
    temp_events& operator=( const temp_events& ) = default;

    // Move assignment
    temp_events& operator=( temp_events&& other ) noexcept
    {
        temp_events::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move( reinterpret_cast<NodeT*>( this->ptr_.get() )->StealOp() );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args )
        -> decltype( ::react::Merge( std::declval<temp_events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<temp_events>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<temp_events>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

namespace detail
{

template <typename D, typename L, typename R>
bool Equals( const events<D, L>& lhs, const events<D, R>& rhs )
{
    return lhs.Equals( rhs );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddIterateRangeWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename S, typename F, typename... args_t>
struct AddIterateRangeWrapper
{
    AddIterateRangeWrapper( const AddIterateRangeWrapper& other ) = default;

    AddIterateRangeWrapper( AddIterateRangeWrapper&& other ) noexcept
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddIterateRangeWrapper>::type>
    explicit AddIterateRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    S operator()( EventRange<E> range, S value, const args_t&... args )
    {
        for( const auto& e : range )
        {
            value = MyFunc( e, value, args... );
        }

        return value;
    }

    F MyFunc;
};

template <typename E, typename S, typename F, typename... args_t>
struct AddIterateByRefRangeWrapper
{
    AddIterateByRefRangeWrapper( const AddIterateByRefRangeWrapper& other ) = default;

    AddIterateByRefRangeWrapper( AddIterateByRefRangeWrapper&& other ) noexcept
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddIterateByRefRangeWrapper>::type>
    explicit AddIterateByRefRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    void operator()( EventRange<E> range, S& valueRef, const args_t&... args )
    {
        for( const auto& e : range )
        {
            MyFunc( e, valueRef, args... );
        }
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc>
class IterateNode : public signal_node<D, S>
{
    using Engine = typename IterateNode::Engine;

public:
    template <typename T, typename F>
    IterateNode( T&& init, const std::shared_ptr<event_stream_node<D, E>>& events, F&& func )
        : IterateNode::signal_node( std::forward<T>( init ) )
        , events_( events )
        , func_( std::forward<F>( func ) )
    {

        Engine::on_node_attach( *this, *events );
    }

    ~IterateNode()
    {
        Engine::on_node_detach( *this, *events_ );
    }

    void tick( void* ) override
    {
        bool changed = false;

        {
            S newValue = func_( EventRange<E>( events_->events() ), this->value_ );

            if( !Equals( newValue, this->value_ ) )
            {
                this->value_ = std::move( newValue );
                changed = true;
            }
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<event_stream_node<D, E>> events_;

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc>
class IterateByRefNode : public signal_node<D, S>
{
    using Engine = typename IterateByRefNode::Engine;

public:
    template <typename T, typename F>
    IterateByRefNode( T&& init, const std::shared_ptr<event_stream_node<D, E>>& events, F&& func )
        : IterateByRefNode::signal_node( std::forward<T>( init ) )
        , func_( std::forward<F>( func ) )
        , events_( events )
    {

        Engine::on_node_attach( *this, *events );
    }

    ~IterateByRefNode()
    {
        Engine::on_node_detach( *this, *events_ );
    }

    void tick( void* ) override
    {
        func_( EventRange<E>( events_->events() ), this->value_ );

        // Always assume change
        Engine::on_node_pulse( *this );
    }

protected:
    TFunc func_;

    std::shared_ptr<event_stream_node<D, E>> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc, typename... TDepValues>
class SyncedIterateNode : public signal_node<D, S>
{
    using Engine = typename SyncedIterateNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateNode( T&& init,
        const std::shared_ptr<event_stream_node<D, E>>& events,
        F&& func,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedIterateNode::signal_node( std::forward<T>( init ) )
        , events_( events )
        , func_( std::forward<F>( func ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *events );
        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedIterateNode()
    {
        Engine::on_node_detach( *this, *events_ );

        apply( detach_functor<D, SyncedIterateNode, std::shared_ptr<signal_node<D, TDepValues>>...>(
                   *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        events_->SetCurrentTurn( turn );

        bool changed = false;

        if( !events_->events().empty() )
        {
            S newValue = apply(
                [this]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                    return func_(
                        EventRange<E>( events_->events() ), this->value_, args->ValueRef()... );
                },
                deps_ );

            if( !Equals( newValue, this->value_ ) )
            {
                changed = true;
                this->value_ = std::move( newValue );
            }
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::shared_ptr<event_stream_node<D, E>> events_;

    TFunc func_;
    dep_holder_t deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc, typename... TDepValues>
class SyncedIterateByRefNode : public signal_node<D, S>
{
    using Engine = typename SyncedIterateByRefNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateByRefNode( T&& init,
        const std::shared_ptr<event_stream_node<D, E>>& events,
        F&& func,
        const std::shared_ptr<signal_node<D, TDepValues>>&... deps )
        : SyncedIterateByRefNode::signal_node( std::forward<T>( init ) )
        , events_( events )
        , func_( std::forward<F>( func ) )
        , deps_( deps... )
    {

        Engine::on_node_attach( *this, *events );
        REACT_EXPAND_PACK( Engine::on_node_attach( *this, *deps ) );
    }

    ~SyncedIterateByRefNode()
    {
        Engine::on_node_detach( *this, *events_ );

        apply( detach_functor<D,
                   SyncedIterateByRefNode,
                   std::shared_ptr<signal_node<D, TDepValues>>...>( *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        events_->SetCurrentTurn( turn );

        bool changed = false;

        if( !events_->events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<signal_node<D, TDepValues>>&... args ) {
                    func_( EventRange<E>( events_->events() ), this->value_, args->ValueRef()... );
                },
                deps_ );

            changed = true;
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    using dep_holder_t = std::tuple<std::shared_ptr<signal_node<D, TDepValues>>...>;

    std::shared_ptr<event_stream_node<D, E>> events_;

    TFunc func_;
    dep_holder_t deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// HoldNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class HoldNode : public signal_node<D, S>
{
    using Engine = typename HoldNode::Engine;

public:
    template <typename T>
    HoldNode( T&& init, const std::shared_ptr<event_stream_node<D, S>>& events )
        : HoldNode::signal_node( std::forward<T>( init ) )
        , events_( events )
    {

        Engine::on_node_attach( *this, *events_ );
    }

    ~HoldNode()
    {
        Engine::on_node_detach( *this, *events_ );
    }

    void tick( void* ) override
    {
        bool changed = false;

        if( !events_->events().empty() )
        {
            const S& newValue = events_->events().back();

            if( !Equals( newValue, this->value_ ) )
            {
                changed = true;
                this->value_ = newValue;
            }
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<event_stream_node<D, S>> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SnapshotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class SnapshotNode : public signal_node<D, S>
{
    using Engine = typename SnapshotNode::Engine;

public:
    SnapshotNode( const std::shared_ptr<signal_node<D, S>>& target,
        const std::shared_ptr<event_stream_node<D, E>>& trigger )
        : SnapshotNode::signal_node( target->ValueRef() )
        , target_( target )
        , trigger_( trigger )
    {

        Engine::on_node_attach( *this, *target_ );
        Engine::on_node_attach( *this, *trigger_ );
    }

    ~SnapshotNode()
    {
        Engine::on_node_detach( *this, *target_ );
        Engine::on_node_detach( *this, *trigger_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        trigger_->SetCurrentTurn( turn );

        bool changed = false;

        if( !trigger_->events().empty() )
        {
            const S& newValue = target_->ValueRef();

            if( !Equals( newValue, this->value_ ) )
            {
                changed = true;
                this->value_ = newValue;
            }
        }

        if( changed )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, S>> target_;
    const std::shared_ptr<event_stream_node<D, E>> trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MonitorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class MonitorNode : public event_stream_node<D, E>
{
    using Engine = typename MonitorNode::Engine;

public:
    MonitorNode( const std::shared_ptr<signal_node<D, E>>& target )
        : MonitorNode::event_stream_node()
        , target_( target )
    {
        Engine::on_node_attach( *this, *target_ );
    }

    ~MonitorNode()
    {
        Engine::on_node_detach( *this, *target_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );

        this->events_.push_back( target_->ValueRef() );

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, E>> target_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulseNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class PulseNode : public event_stream_node<D, S>
{
    using Engine = typename PulseNode::Engine;

public:
    PulseNode( const std::shared_ptr<signal_node<D, S>>& target,
        const std::shared_ptr<event_stream_node<D, E>>& trigger )
        : PulseNode::event_stream_node()
        , target_( target )
        , trigger_( trigger )
    {
        Engine::on_node_attach( *this, *target_ );
        Engine::on_node_attach( *this, *trigger_ );
    }

    ~PulseNode()
    {
        Engine::on_node_detach( *this, *target_ );
        Engine::on_node_detach( *this, *trigger_ );
    }

    void tick( void* turn_ptr ) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        this->SetCurrentTurn( turn, true );
        trigger_->SetCurrentTurn( turn );

        for( size_t i = 0, ie = trigger_->events().size(); i < ie; ++i )
        {
            this->events_.push_back( target_->ValueRef() );
        }

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<signal_node<D, S>> target_;
    const std::shared_ptr<event_stream_node<D, E>> trigger_;
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold - Hold the most recent event in a signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename T = typename std::decay<V>::type>
auto Hold( const events<D, T>& events, V&& init ) -> signal<D, T>
{
    using ::react::detail::HoldNode;

    return signal<D, T>(
        std::make_shared<HoldNode<D, T>>( std::forward<V>( init ), GetNodePtr( events ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Monitor - Emits value changes of target signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto Monitor( const signal<D, S>& target ) -> events<D, S>
{
    using ::react::detail::MonitorNode;

    return events<D, S>( std::make_shared<MonitorNode<D, S>>( GetNodePtr( target ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Iteratively combines signal value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename V,
    typename FIn,
    typename S = typename std::decay<V>::type>
auto Iterate( const events<D, E>& events, V&& init, FIn&& func ) -> signal<D, S>
{
    using ::react::detail::AddIterateByRefRangeWrapper;
    using ::react::detail::AddIterateRangeWrapper;
    using ::react::detail::EventRange;
    using ::react::detail::is_callable_with;
    using ::react::detail::IterateByRefNode;
    using ::react::detail::IterateNode;

    using F = typename std::decay<FIn>::type;

    using NodeT = typename std::conditional<is_callable_with<F, S, EventRange<E>, S>::value,
        IterateNode<D, S, E, F>,
        typename std::conditional<is_callable_with<F, S, E, S>::value,
            IterateNode<D, S, E, AddIterateRangeWrapper<E, S, F>>,
            typename std::conditional<is_callable_with<F, void, EventRange<E>, S&>::value,
                IterateByRefNode<D, S, E, F>,
                typename std::conditional<is_callable_with<F, void, E, S&>::value,
                    IterateByRefNode<D, S, E, AddIterateByRefRangeWrapper<E, S, F>>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<NodeT, void>::value,
        "Iterate: Passed function does not match any of the supported signatures." );

    return signal<D, S>( std::make_shared<NodeT>(
        std::forward<V>( init ), GetNodePtr( events ), std::forward<FIn>( func ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename V,
    typename FIn,
    typename... TDepValues,
    typename S = typename std::decay<V>::type>
auto Iterate(
    const events<D, E>& events, V&& init, const signal_pack<D, TDepValues...>& depPack, FIn&& func )
    -> signal<D, S>
{
    using ::react::detail::AddIterateByRefRangeWrapper;
    using ::react::detail::AddIterateRangeWrapper;
    using ::react::detail::EventRange;
    using ::react::detail::is_callable_with;
    using ::react::detail::SyncedIterateByRefNode;
    using ::react::detail::SyncedIterateNode;

    using F = typename std::decay<FIn>::type;

    using NodeT = typename std::conditional<
        is_callable_with<F, S, EventRange<E>, S, TDepValues...>::value,
        SyncedIterateNode<D, S, E, F, TDepValues...>,
        typename std::conditional<is_callable_with<F, S, E, S, TDepValues...>::value,
            SyncedIterateNode<D,
                S,
                E,
                AddIterateRangeWrapper<E, S, F, TDepValues...>,
                TDepValues...>,
            typename std::conditional<
                is_callable_with<F, void, EventRange<E>, S&, TDepValues...>::value,
                SyncedIterateByRefNode<D, S, E, F, TDepValues...>,
                typename std::conditional<is_callable_with<F, void, E, S&, TDepValues...>::value,
                    SyncedIterateByRefNode<D,
                        S,
                        E,
                        AddIterateByRefRangeWrapper<E, S, F, TDepValues...>,
                        TDepValues...>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<NodeT, void>::value,
        "Iterate: Passed function does not match any of the supported signatures." );

    //static_assert(NodeT::dummy_error, "DUMP MY TYPE" );

    struct NodeBuilder_
    {
        NodeBuilder_( const ::react::events<D, E>& source, V&& init, FIn&& func )
            : MySource( source )
            , MyInit( std::forward<V>( init ) )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const signal<D, TDepValues>&... deps ) -> signal<D, S>
        {
            return signal<D, S>( std::make_shared<NodeT>( std::forward<V>( MyInit ),
                GetNodePtr( MySource ),
                std::forward<FIn>( MyFunc ),
                GetNodePtr( deps )... ) );
        }

        const ::react::events<D, E>& MySource;
        V MyInit;
        FIn MyFunc;
    };

    return ::react::detail::apply(
        NodeBuilder_( events, std::forward<V>( init ), std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot - Sets signal value to value of other signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto Snapshot( const events<D, E>& trigger, const signal<D, S>& target ) -> signal<D, S>
{
    using ::react::detail::SnapshotNode;

    return signal<D, S>(
        std::make_shared<SnapshotNode<D, S, E>>( GetNodePtr( target ), GetNodePtr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto Pulse( const events<D, E>& trigger, const signal<D, S>& target ) -> events<D, S>
{
    using ::react::detail::PulseNode;

    return events<D, S>(
        std::make_shared<PulseNode<D, S, E>>( GetNodePtr( target ), GetNodePtr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Changed - Emits token when target signal was changed
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto Changed( const signal<D, S>& target ) -> events<D, token>
{
    return Monitor( target ).Tokenize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ChangedTo - Emits token when target signal was changed to value
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename S = typename std::decay<V>::type>
auto ChangedTo( const signal<D, S>& target, V&& value ) -> events<D, token>
{
    return Monitor( target ).Filter( [=]( const S& v ) { return v == value; } ).Tokenize();
}

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void topological_sort_engine::propagate( turn& turn )
{
    while( m_scheduled_nodes.fetch_next() )
    {
        for( auto* curNode : m_scheduled_nodes.next_values() )
        {
            if( curNode->level < curNode->new_level )
            {
                curNode->level = curNode->new_level;
                invalidate_successors( *curNode );
                m_scheduled_nodes.push( curNode, curNode->level );
                continue;
            }

            curNode->queued = false;
            curNode->tick( &turn );
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

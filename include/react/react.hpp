
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observable
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class observable
{
public:
    observable() = default;

    ~observable()
    {
        for( const auto& p : observers_ )
        {
            if( p != nullptr )
            {
                p->detach_observer();
            }
        }
    }

    void RegisterObserver( std::unique_ptr<observer_interface>&& obsPtr )
    {
        observers_.push_back( std::move( obsPtr ) );
    }

    void UnregisterObserver( observer_interface* rawObsPtr )
    {
        for( auto it = observers_.begin(); it != observers_.end(); ++it )
        {
            if( it->get() == rawObsPtr )
            {
                it->get()->detach_observer();
                observers_.erase( it );
                break;
            }
        }
    }

private:
    std::vector<std::unique_ptr<observer_interface>> observers_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class NodeBase : public D::Engine::NodeT
{
public:
    using DomainT = D;
    using Engine = typename D::Engine;
    using NodeT = typename Engine::NodeT;
    using TurnT = typename Engine::TurnT;

    NodeBase() = default;

    // Nodes can't be copied
    NodeBase( const NodeBase& ) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObservableNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObservableNode
    : public NodeBase<D>
    , public observable<D>
{
public:
    ObservableNode() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Attach/detach helper functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TNode, typename... TDeps>
struct AttachFunctor
{
    AttachFunctor( TNode& node )
        : MyNode( node )
    {}

    void operator()( const TDeps&... deps ) const
    {
        REACT_EXPAND_PACK( attach( deps ) );
    }

    template <typename T>
    void attach( const T& op ) const
    {
        op.template AttachRec<D, TNode>( *this );
    }

    template <typename T>
    void attach( const std::shared_ptr<T>& depPtr ) const
    {
        D::Engine::on_node_attach( MyNode, *depPtr );
    }

    TNode& MyNode;
};

template <typename D, typename TNode, typename... TDeps>
struct DetachFunctor
{
    DetachFunctor( TNode& node )
        : MyNode( node )
    {}

    void operator()( const TDeps&... deps ) const
    {
        REACT_EXPAND_PACK( detach( deps ) );
    }

    template <typename T>
    void detach( const T& op ) const
    {
        op.template DetachRec<D, TNode>( *this );
    }

    template <typename T>
    void detach( const std::shared_ptr<T>& depPtr ) const
    {
        D::Engine::on_node_detach( MyNode, *depPtr );
    }

    TNode& MyNode;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ReactiveOpBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename... TDeps>
class ReactiveOpBase
{
public:
    using DepHolderT = std::tuple<TDeps...>;

    template <typename... TDepsIn>
    ReactiveOpBase( DontMove, TDepsIn&&... deps )
        : deps_( std::forward<TDepsIn>( deps )... )
    {}

    ReactiveOpBase( ReactiveOpBase&& other ) noexcept
        : deps_( std::move( other.deps_ ) )
    {}

    // Can't be copied, only moved
    ReactiveOpBase( const ReactiveOpBase& other ) = delete;

    template <typename D, typename TNode>
    void Attach( TNode& node ) const
    {
        apply( AttachFunctor<D, TNode, TDeps...>{ node }, deps_ );
    }

    template <typename D, typename TNode>
    void Detach( TNode& node ) const
    {
        apply( DetachFunctor<D, TNode, TDeps...>{ node }, deps_ );
    }

    template <typename D, typename TNode, typename TFunctor>
    void AttachRec( const TFunctor& functor ) const
    {
        // Same memory layout, different func
        apply( reinterpret_cast<const AttachFunctor<D, TNode, TDeps...>&>( functor ), deps_ );
    }

    template <typename D, typename TNode, typename TFunctor>
    void DetachRec( const TFunctor& functor ) const
    {
        apply( reinterpret_cast<const DetachFunctor<D, TNode, TDeps...>&>( functor ), deps_ );
    }

protected:
    DepHolderT deps_;
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
template <typename TNode>
class ReactiveBase
{
public:
    using DomainT = typename TNode::DomainT;

    // Default ctor
    ReactiveBase() = default;

    // Copy ctor
    ReactiveBase( const ReactiveBase& ) = default;

    // Move ctor (VS2013 doesn't default generate that yet)
    ReactiveBase( ReactiveBase&& other ) noexcept
        : ptr_( std::move( other.ptr_ ) )
    {}

    // Explicit node ctor
    explicit ReactiveBase( std::shared_ptr<TNode>&& ptr )
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
    std::shared_ptr<TNode> ptr_;

    template <typename TNode_>
    friend const std::shared_ptr<TNode_>& GetNodePtr( const ReactiveBase<TNode_>& node );
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CopyableReactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
class CopyableReactive : public ReactiveBase<TNode>
{
public:
    CopyableReactive() = default;

    CopyableReactive( const CopyableReactive& ) = default;

    CopyableReactive( CopyableReactive&& other ) noexcept
        : CopyableReactive::ReactiveBase( std::move( other ) )
    {}

    explicit CopyableReactive( std::shared_ptr<TNode>&& ptr )
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
template <typename TNode>
const std::shared_ptr<TNode>& GetNodePtr( const ReactiveBase<TNode>& node )
{
    return node.ptr_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
struct input_node_interface;

class observer_interface;

template <typename D>
class InputManager;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachedObserversManager
///////////////////////////////////////////////////////////////////////////////////////////////////
class DetachedObserversManager
{
    using ObsVectT = std::vector<observer_interface*>;

public:
    void QueueObserverForDetach( observer_interface& obs )
    {
        detachedObservers_.push_back( &obs );
    }

    template <typename D>
    void DetachQueuedObservers()
    {
        for( auto* o : detachedObservers_ )
        {
            o->unregister_self();
        }
        detachedObservers_.clear();
    }

private:
    ObsVectT detachedObservers_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// InputManager
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class InputManager
{
public:
    using TurnT = typename D::TurnT;
    using Engine = typename D::Engine;

    InputManager() = default;

    template <typename F>
    void DoTransaction( F&& func )
    {
        bool shouldPropagate = false;

        // Phase 1 - Input admission
        isTransactionActive_ = true;

        TurnT turn( nextTurnId() );

        func();

        isTransactionActive_ = false;

        // Phase 2 - Apply input node changes
        for( auto* p : changedInputs_ )
        {
            if( p->apply_input( &turn ) )
            {
                shouldPropagate = true;
            }
        }
        changedInputs_.clear();

        // Phase 3 - propagate changes
        if( shouldPropagate )
        {
            Engine::propagate( turn );
        }

        finalizeSyncTransaction();
    }

    template <typename R, typename V>
    void AddInput( R& r, V&& v )
    {
        if( isTransactionActive_ )
        {
            addTransactionInput( r, std::forward<V>( v ) );
        }
        else
        {
            addSimpleInput( r, std::forward<V>( v ) );
        }
    }

    template <typename R, typename F>
    void ModifyInput( R& r, const F& func )
    {
        if( isTransactionActive_ )
        {
            modifyTransactionInput( r, func );
        }
        else
        {
            modifySimpleInput( r, func );
        }
    }

    void QueueObserverForDetach( observer_interface& obs )
    {
        detachedObserversManager_.QueueObserverForDetach( obs );
    }

private:
    turn_id_t nextTurnId()
    {
        return nextTurnId_++;
    }

    // Create a turn with a single input
    template <typename R, typename V>
    void addSimpleInput( R& r, V&& v )
    {
        TurnT turn( nextTurnId() );
        r.AddInput( std::forward<V>( v ) );

        if( r.apply_input( &turn ) )
        {
            Engine::propagate( turn );
        }

        finalizeSyncTransaction();
    }

    template <typename R, typename F>
    void modifySimpleInput( R& r, const F& func )
    {
        TurnT turn( nextTurnId() );

        r.ModifyInput( func );


        // Return value, will always be true
        r.apply_input( &turn );

        Engine::propagate( turn );

        finalizeSyncTransaction();
    }

    void finalizeSyncTransaction()
    {
        detachedObserversManager_.template DetachQueuedObservers<D>();
    }

    // This input is part of an active transaction
    template <typename R, typename V>
    void addTransactionInput( R& r, V&& v )
    {
        r.AddInput( std::forward<V>( v ) );
        changedInputs_.push_back( &r );
    }

    template <typename R, typename F>
    void modifyTransactionInput( R& r, const F& func )
    {
        r.ModifyInput( func );
        changedInputs_.push_back( &r );
    }

    DetachedObserversManager detachedObserversManager_;

    turn_id_t nextTurnId_{ 0 };

    bool isTransactionActive_ = false;

    std::vector<input_node_interface*> changedInputs_;
};

template <typename D>
class DomainSpecificInputManager
{
public:
    DomainSpecificInputManager() = delete;

    static InputManager<D>& Instance()
    {
        static InputManager<D> instance;
        return instance;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalNode;

template <typename D, typename E>
class EventStreamNode;

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename S>
class VarSignal;

template <typename D, typename E>
class Events;

template <typename D, typename E>
class EventSource;

enum class Token;

template <typename D>
class Observer;

template <typename D>
class ScopedObserver;

template <typename D, typename S, typename TOp>
class TempSignal;

template <typename D, typename E, typename TOp>
class TempEvents;

template <typename D, typename... TValues>
class SignalPack;

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DomainBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class DomainBase
{
public:
    using TurnT = typename topological_sort_engine::TurnT;

    DomainBase() = delete;

    using Engine = ::react::detail::engine_interface<D, topological_sort_engine>;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Aliases for reactives of this domain
    ///////////////////////////////////////////////////////////////////////////////////////////////
    template <typename S>
    using SignalT = Signal<D, S>;

    template <typename S>
    using VarSignalT = VarSignal<D, S>;

    template <typename E = Token>
    using EventsT = Events<D, E>;

    template <typename E = Token>
    using EventSourceT = EventSource<D, E>;

    using ObserverT = Observer<D>;

    using ScopedObserverT = ScopedObserver<D>;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Ensure singletons are created immediately after domain declaration (TODO hax)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class DomainInitializer
{
public:
    DomainInitializer()
    {
        D::Engine::instance();
        DomainSpecificInputManager<D>::Instance();
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////
/// DoTransaction
///////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename F>
void DoTransaction( F&& func )
{
    using ::react::detail::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance().DoTransaction( std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Domain definition macro
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACTIVE_DOMAIN( name )                                                                    \
    struct name : public ::react::detail::DomainBase<name>                                         \
    {};                                                                                            \
    static ::react::detail::DomainInitializer<name> name##_initializer_;

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
    using SignalT = Signal<name, S>;                                                               \
                                                                                                   \
    template <typename S>                                                                          \
    using VarSignalT = VarSignal<name, S>;                                                         \
                                                                                                   \
    template <typename E = Token>                                                                  \
    using EventsT = Events<name, E>;                                                               \
                                                                                                   \
    template <typename E = Token>                                                                  \
    using EventSourceT = EventSource<name, E>;                                                     \
                                                                                                   \
    using ObserverT = Observer<name>;                                                              \
                                                                                                   \
    using ScopedObserverT = ScopedObserver<name>;

#if _MSC_VER && !__INTEL_COMPILER
#    pragma warning( disable : 4503 )
#endif

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverAction
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class ObserverAction
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

    ObserverAction operator()( EventRange<E> range, const args_t&... args )
    {
        for( const auto& e : range )
        {
            if( MyFunc( e, args... ) == ObserverAction::stop_and_detach )
            {
                return ObserverAction::stop_and_detach;
            }
        }

        return ObserverAction::next;
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObserverNode
    : public NodeBase<D>
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
    SignalObserverNode( const std::shared_ptr<SignalNode<D, S>>& subject, F&& func )
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
            if( func_( p->ValueRef() ) == ObserverAction::stop_and_detach )
            {
                shouldDetach = true;
            }
        }

        if( shouldDetach )
        {
            DomainSpecificInputManager<D>::Instance().QueueObserverForDetach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->UnregisterObserver( this );
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

    std::weak_ptr<SignalNode<D, S>> subject_;
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
    EventObserverNode( const std::shared_ptr<EventStreamNode<D, E>>& subject, F&& func )
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
            shouldDetach = func_( EventRange<E>( p->Events() ) ) == ObserverAction::stop_and_detach;
        }

        if( shouldDetach )
        {
            DomainSpecificInputManager<D>::Instance().QueueObserverForDetach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->UnregisterObserver( this );
        }
    }

private:
    std::weak_ptr<EventStreamNode<D, E>> subject_;

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
    SyncedObserverNode( const std::shared_ptr<EventStreamNode<D, E>>& subject,
        F&& func,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
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
                          [this, &p]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
                              return func_( EventRange<E>( p->Events() ), args->ValueRef()... );
                          },
                          deps_ )
                   == ObserverAction::stop_and_detach;
            }
        }

        if( shouldDetach )
        {
            DomainSpecificInputManager<D>::Instance().QueueObserverForDetach( *this );
        }
    }

    void unregister_self() override
    {
        if( auto p = subject_.lock() )
        {
            p->UnregisterObserver( this );
        }
    }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::weak_ptr<EventStreamNode<D, E>> subject_;

    TFunc func_;
    DepHolderT deps_;

    virtual void detach_observer()
    {
        if( auto p = subject_.lock() )
        {
            Engine::on_node_detach( *this, *p );

            apply(
                DetachFunctor<D, SyncedObserverNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                    *this ),
                deps_ );

            subject_.reset();
        }
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
using ::react::detail::ObserverAction;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class Observer
{
private:
    using SubjectPtrT = std::shared_ptr<::react::detail::ObservableNode<D>>;
    using NodeT = ::react::detail::ObserverNode<D>;

public:
    // Default ctor
    Observer()
        : nodePtr_( nullptr )
        , subjectPtr_( nullptr )
    {}

    // Move ctor
    Observer( Observer&& other ) noexcept
        : nodePtr_( other.nodePtr_ )
        , subjectPtr_( std::move( other.subjectPtr_ ) )
    {
        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();
    }

    // Node ctor
    Observer( NodeT* nodePtr, const SubjectPtrT& subjectPtr )
        : nodePtr_( nodePtr )
        , subjectPtr_( subjectPtr )
    {}

    // Move assignment
    Observer& operator=( Observer&& other ) noexcept
    {
        nodePtr_ = other.nodePtr_;
        subjectPtr_ = std::move( other.subjectPtr_ );

        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();

        return *this;
    }

    // Deleted copy ctor and assignment
    Observer( const Observer& ) = delete;
    Observer& operator=( const Observer& ) = delete;

    void Detach()
    {
        assert( IsValid() );
        subjectPtr_->UnregisterObserver( nodePtr_ );
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
/// ScopedObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ScopedObserver
{
public:
    // Move ctor
    ScopedObserver( ScopedObserver&& other ) noexcept
        : obs_( std::move( other.obs_ ) )
    {}

    // Construct from observer
    ScopedObserver( Observer<D>&& obs )
        : obs_( std::move( obs ) )
    {}

    // Move assignment
    ScopedObserver& operator=( ScopedObserver&& other ) noexcept
    {
        obs_ = std::move( other.obs_ );
    }

    // Deleted default ctor, copy ctor and assignment
    ScopedObserver() = delete;
    ScopedObserver( const ScopedObserver& ) = delete;
    ScopedObserver& operator=( const ScopedObserver& ) = delete;

    ~ScopedObserver()
    {
        obs_.Detach();
    }

    bool IsValid() const
    {
        return obs_.IsValid();
    }

private:
    Observer<D> obs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename S>
auto Observe( const Signal<D, S>& subject, FIn&& func ) -> Observer<D>
{
    using ::react::detail::add_default_return_value_wrapper;
    using ::react::detail::observer_interface;
    using ::react::detail::ObserverNode;
    using ::react::detail::SignalObserverNode;

    using F = typename std::decay<FIn>::type;
    using R = typename std::result_of<FIn( S )>::type;
    using WrapperT = add_default_return_value_wrapper<F, ObserverAction, ObserverAction::next>;

    // If return value of passed function is void, add ObserverAction::next as
    // default return value.
    using NodeT = typename std::conditional<std::is_same<void, R>::value,
        SignalObserverNode<D, S, WrapperT>,
        SignalObserverNode<D, S, F>>::type;

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT( subjectPtr, std::forward<FIn>( func ) ) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver( std::move( nodePtr ) );

    return Observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename E>
auto Observe( const Events<D, E>& subject, FIn&& func ) -> Observer<D>
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
        is_callable_with<F, ObserverAction, EventRange<E>>::value,
        F,
        typename std::conditional<is_callable_with<F, ObserverAction, E>::value,
            AddObserverRangeWrapper<E, F>,
            typename std::conditional<is_callable_with<F, void, EventRange<E>>::value,
                add_default_return_value_wrapper<F, ObserverAction, ObserverAction::next>,
                typename std::conditional<is_callable_with<F, void, E>::value,
                    AddObserverRangeWrapper<E,
                        add_default_return_value_wrapper<F, ObserverAction, ObserverAction::next>>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<WrapperT, void>::value,
        "Observe: Passed function does not match any of the supported signatures." );

    using NodeT = EventObserverNode<D, E, WrapperT>;

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT( subjectPtr, std::forward<FIn>( func ) ) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver( std::move( nodePtr ) );

    return Observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename FIn, typename E, typename... TDepValues>
auto Observe( const Events<D, E>& subject, const SignalPack<D, TDepValues...>& depPack, FIn&& func )
    -> Observer<D>
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
        is_callable_with<F, ObserverAction, EventRange<E>, TDepValues...>::value,
        F,
        typename std::conditional<is_callable_with<F, ObserverAction, E, TDepValues...>::value,
            AddObserverRangeWrapper<E, F, TDepValues...>,
            typename std::conditional<
                is_callable_with<F, void, EventRange<E>, TDepValues...>::value,
                add_default_return_value_wrapper<F, ObserverAction, ObserverAction::next>,
                typename std::conditional<is_callable_with<F, void, E, TDepValues...>::value,
                    AddObserverRangeWrapper<E,
                        add_default_return_value_wrapper<F, ObserverAction, ObserverAction::next>,
                        TDepValues...>,
                    void>::type>::type>::type>::type;

    static_assert( !std::is_same<WrapperT, void>::value,
        "Observe: Passed function does not match any of the supported signatures." );

    using NodeT = SyncedObserverNode<D, E, WrapperT, TDepValues...>;

    struct NodeBuilder_
    {
        NodeBuilder_( const Events<D, E>& subject, FIn&& func )
            : MySubject( subject )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TDepValues>&... deps ) -> ObserverNode<D>*
        {
            return new NodeT(
                GetNodePtr( MySubject ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... );
        }

        const Events<D, E>& MySubject;
        FIn MyFunc;
    };

    const auto& subjectPtr = GetNodePtr( subject );

    std::unique_ptr<ObserverNode<D>> nodePtr( ::react::detail::apply(
        NodeBuilder_( subject, std::forward<FIn>( func ) ), depPack.Data ) );

    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver( std::move( nodePtr ) );

    return Observer<D>( rawNodePtr, subjectPtr );
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
struct IsSignal<Signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsSignal<VarSignal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsSignal<TempSignal<D, T, TOp>>
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
struct IsEvent<Events<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsEvent<EventSource<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsEvent<TempEvents<D, T, TOp>>
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
struct DecayInput<VarSignal<D, T>>
{
    using Type = Signal<D, T>;
};

template <typename D, typename T>
struct DecayInput<EventSource<D, T>>
{
    using Type = Events<D, T>;
};

namespace detail
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename L, typename R>
bool Equals( const L& lhs, const R& rhs );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalNode : public ObservableNode<D>
{
public:
    SignalNode() = default;

    template <typename T>
    explicit SignalNode( T&& value )
        : SignalNode::ObservableNode()
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
using SignalNodePtrT = std::shared_ptr<SignalNode<D, S>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class VarNode
    : public SignalNode<D, S>
    , public input_node_interface
{
    using Engine = typename VarNode::Engine;

public:
    template <typename T>
    VarNode( T&& value )
        : VarNode::SignalNode( std::forward<T>( value ) )
        , newValue_( value )
    {}

    ~VarNode() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked VarNode" );
    }

    template <typename V>
    void AddInput( V&& newValue )
    {
        newValue_ = std::forward<V>( newValue );

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    // This is signal-specific
    template <typename F>
    void ModifyInput( F& func )
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
template <typename S, typename F, typename... TDeps>
class FunctionOp : public ReactiveOpBase<TDeps...>
{
public:
    template <typename FIn, typename... TDepsIn>
    FunctionOp( FIn&& func, TDepsIn&&... deps )
        : FunctionOp::ReactiveOpBase( DontMove(), std::forward<TDepsIn>( deps )... )
        , func_( std::forward<FIn>( func ) )
    {}

    FunctionOp( FunctionOp&& other ) noexcept
        : FunctionOp::ReactiveOpBase( std::move( other ) )
        , func_( std::move( other.func_ ) )
    {}

    S Evaluate()
    {
        return apply( EvalFunctor( func_ ), this->deps_ );
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
        static auto eval( const std::shared_ptr<T>& depPtr ) -> decltype( depPtr->ValueRef() )
        {
            return depPtr->ValueRef();
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
class SignalOpNode : public SignalNode<D, S>
{
    using Engine = typename SignalOpNode::Engine;

public:
    template <typename... args_t>
    SignalOpNode( args_t&&... args )
        : SignalOpNode::SignalNode()
        , op_( std::forward<args_t>( args )... )
    {
        this->value_ = op_.Evaluate();


        op_.template Attach<D>( *this );
    }

    ~SignalOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template Detach<D>( *this );
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
        op_.template Detach<D>( *this );
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
class FlattenNode : public SignalNode<D, TInner>
{
    using Engine = typename FlattenNode::Engine;

public:
    FlattenNode( const std::shared_ptr<SignalNode<D, TOuter>>& outer,
        const std::shared_ptr<SignalNode<D, TInner>>& inner )
        : FlattenNode::SignalNode( inner->ValueRef() )
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
    std::shared_ptr<SignalNode<D, TOuter>> outer_;
    std::shared_ptr<SignalNode<D, TInner>> inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalBase : public CopyableReactive<SignalNode<D, S>>
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
        DomainSpecificInputManager<D>::Instance().AddInput(
            *reinterpret_cast<VarNode<D, S>*>( this->ptr_.get() ), std::forward<T>( newValue ) );
    }

    template <typename F>
    void modifyValue( const F& func ) const
    {
        DomainSpecificInputManager<D>::Instance().ModifyInput(
            *reinterpret_cast<VarNode<D, S>*>( this->ptr_.get() ), func );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalPack - Wraps several nodes in a tuple. Create with comma operator.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
class SignalPack
{
public:
    SignalPack( const Signal<D, TValues>&... deps )
        : Data( std::tie( deps... ) )
    {}

    template <typename... TCurValues, typename TAppendValue>
    SignalPack( const SignalPack<D, TCurValues...>& curArgs, const Signal<D, TAppendValue>& newArg )
        : Data( std::tuple_cat( curArgs.Data, std::tie( newArg ) ) )
    {}

    std::tuple<const Signal<D, TValues>&...> Data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// With - Utility function to create a SignalPack
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
auto With( const Signal<D, TValues>&... deps ) -> SignalPack<D, TValues...>
{
    return SignalPack<D, TValues...>( deps... );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVar
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    class = typename std::enable_if<!IsSignal<S>::value>::type,
    class = typename std::enable_if<!IsEvent<S>::value>::type>
auto MakeVar( V&& value ) -> VarSignal<D, S>
{
    return VarSignal<D, S>(
        std::make_shared<::react::detail::VarNode<D, S>>( std::forward<V>( value ) ) );
}

template <typename D, typename S>
auto MakeVar( std::reference_wrapper<S> value ) -> VarSignal<D, S&>
{
    return VarSignal<D, S&>(
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
auto MakeVar( V&& value ) -> VarSignal<D, Signal<D, TInner>>
{
    return VarSignal<D, Signal<D, TInner>>(
        std::make_shared<::react::detail::VarNode<D, Signal<D, TInner>>>(
            std::forward<V>( value ) ) );
}

template <typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename TInner = typename S::ValueT,
    class = typename std::enable_if<IsEvent<S>::value>::type>
auto MakeVar( V&& value ) -> VarSignal<D, Events<D, TInner>>
{
    return VarSignal<D, Events<D, TInner>>(
        std::make_shared<::react::detail::VarNode<D, Events<D, TInner>>>(
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
auto MakeSignal( const Signal<D, TValue>& arg, FIn&& func ) -> TempSignal<D, S, TOp>
{
    return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(
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
auto MakeSignal( const SignalPack<D, TValues...>& argPack, FIn&& func ) -> TempSignal<D, S, TOp>
{
    using ::react::detail::SignalOpNode;

    struct NodeBuilder_
    {
        NodeBuilder_( FIn&& func )
            : MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TValues>&... args ) -> TempSignal<D, S, TOp>
        {
            return TempSignal<D, S, TOp>( std::make_shared<SignalOpNode<D, S, TOp>>(
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
    auto operator op( const TSignal& arg )->TempSignal<D, S, TOp>                                  \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
            F(), GetNodePtr( arg ) ) );                                                            \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TVal,                                                                             \
        typename TOpIn,                                                                            \
        typename F = name##OpFunctor<TVal>,                                                        \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename TOp = ::react::detail::FunctionOp<S, F, TOpIn>>                                   \
    auto operator op( TempSignal<D, TVal, TOpIn>&& arg )->TempSignal<D, S, TOp>                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>(                                                              \
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
    auto operator op( const TLeftSignal& lhs, const TRightSignal& rhs )->TempSignal<D, S, TOp>     \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( const TLeftSignal& lhs, TRightValIn&& rhs )->TempSignal<D, S, TOp>           \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( TLeftValIn&& lhs, const TRightSignal& rhs )->TempSignal<D, S, TOp>           \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
        TempSignal<D, TLeftVal, TLeftOp>&& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )         \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( TempSignal<D, TLeftVal, TLeftOp>&& lhs, const TRightSignal& rhs )            \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( const TLeftSignal& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )           \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( TempSignal<D, TLeftVal, TLeftOp>&& lhs, TRightValIn&& rhs )                  \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
    auto operator op( TLeftValIn&& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )                 \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::detail::SignalOpNode<D, S, TOp>>(  \
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
auto operator,( const Signal<D, TLeftVal>& a, const Signal<D, TRightVal>& b )
                  -> SignalPack<D, TLeftVal, TRightVal>
{
    return SignalPack<D, TLeftVal, TRightVal>( a, b );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to append node to existing signal pack.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TCurValues, typename TAppendValue>
auto operator,( const SignalPack<D, TCurValues...>& cur, const Signal<D, TAppendValue>& append )
                  -> SignalPack<D, TCurValues..., TAppendValue>
{
    return SignalPack<D, TCurValues..., TAppendValue>( cur, append );
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
    -> Signal<D, typename std::result_of<F( TValue )>::type>
{
    return ::react::MakeSignal( arg, std::forward<F>( func ) );
}

// Multiple args
template <typename D, typename F, typename... TValues>
auto operator->*( const SignalPack<D, TValues...>& argPack, F&& func )
    -> Signal<D, typename std::result_of<F( TValues... )>::type>
{
    return ::react::MakeSignal( argPack, std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TInnerValue>
auto Flatten( const Signal<D, Signal<D, TInnerValue>>& outer ) -> Signal<D, TInnerValue>
{
    return Signal<D, TInnerValue>(
        std::make_shared<::react::detail::FlattenNode<D, Signal<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal : public ::react::detail::SignalBase<D, S>
{
private:
    using NodeT = ::react::detail::SignalNode<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal( const Signal& ) = default;

    // Move ctor
    Signal( Signal&& other ) noexcept
        : Signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit Signal( NodePtrT&& nodePtr )
        : Signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Signal& operator=( const Signal& ) = default;

    // Move assignment
    Signal& operator=( Signal&& other ) noexcept
    {
        Signal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return Signal::SignalBase::getValue();
    }

    const S& operator()() const
    {
        return Signal::SignalBase::getValue();
    }

    bool Equals( const Signal& other ) const
    {
        return Signal::SignalBase::Equals( other );
    }

    bool IsValid() const
    {
        return Signal::SignalBase::IsValid();
    }

    S Flatten() const
    {
        static_assert( IsSignal<S>::value || IsEvent<S>::value,
            "Flatten requires a Signal or Events value type." );
        return ::react::Flatten( *this );
    }
};

// Specialize for references
template <typename D, typename S>
class Signal<D, S&> : public ::react::detail::SignalBase<D, std::reference_wrapper<S>>
{
private:
    using NodeT = ::react::detail::SignalNode<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal( const Signal& ) = default;

    // Move ctor
    Signal( Signal&& other ) noexcept
        : Signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit Signal( NodePtrT&& nodePtr )
        : Signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Signal& operator=( const Signal& ) = default;

    // Move assignment
    Signal& operator=( Signal&& other ) noexcept
    {
        Signal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    const S& Value() const
    {
        return Signal::SignalBase::getValue();
    }

    const S& operator()() const
    {
        return Signal::SignalBase::getValue();
    }

    bool Equals( const Signal& other ) const
    {
        return Signal::SignalBase::Equals( other );
    }

    bool IsValid() const
    {
        return Signal::SignalBase::IsValid();
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class VarSignal : public Signal<D, S>
{
private:
    using NodeT = ::react::detail::VarNode<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal( const VarSignal& ) = default;

    // Move ctor
    VarSignal( VarSignal&& other ) noexcept
        : VarSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit VarSignal( NodePtrT&& nodePtr )
        : VarSignal::Signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    VarSignal& operator=( const VarSignal& ) = default;

    // Move assignment
    VarSignal& operator=( VarSignal&& other ) noexcept
    {
        VarSignal::SignalBase::operator=( std::move( other ) );
        return *this;
    }

    void Set( const S& newValue ) const
    {
        VarSignal::SignalBase::setValue( newValue );
    }

    void Set( S&& newValue ) const
    {
        VarSignal::SignalBase::setValue( std::move( newValue ) );
    }

    const VarSignal& operator<<=( const S& newValue ) const
    {
        VarSignal::SignalBase::setValue( newValue );
        return *this;
    }

    const VarSignal& operator<<=( S&& newValue ) const
    {
        VarSignal::SignalBase::setValue( std::move( newValue ) );
        return *this;
    }

    template <typename F>
    void Modify( const F& func ) const
    {
        VarSignal::SignalBase::modifyValue( func );
    }
};

// Specialize for references
template <typename D, typename S>
class VarSignal<D, S&> : public Signal<D, std::reference_wrapper<S>>
{
private:
    using NodeT = ::react::detail::VarNode<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal( const VarSignal& ) = default;

    // Move ctor
    VarSignal( VarSignal&& other ) noexcept
        : VarSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit VarSignal( NodePtrT&& nodePtr )
        : VarSignal::Signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    VarSignal& operator=( const VarSignal& ) = default;

    // Move assignment
    VarSignal& operator=( VarSignal&& other ) noexcept
    {
        VarSignal::Signal::operator=( std::move( other ) );
        return *this;
    }

    void Set( std::reference_wrapper<S> newValue ) const
    {
        VarSignal::SignalBase::setValue( newValue );
    }

    const VarSignal& operator<<=( std::reference_wrapper<S> newValue ) const
    {
        VarSignal::SignalBase::setValue( newValue );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TempSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename TOp>
class TempSignal : public Signal<D, S>
{
private:
    using NodeT = ::react::detail::SignalOpNode<D, S, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempSignal() = default;

    // Copy ctor
    TempSignal( const TempSignal& ) = default;

    // Move ctor
    TempSignal( TempSignal&& other ) noexcept
        : TempSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit TempSignal( NodePtrT&& ptr )
        : TempSignal::Signal( std::move( ptr ) )
    {}

    // Copy assignment
    TempSignal& operator=( const TempSignal& ) = default;

    // Move assignment
    TempSignal& operator=( TempSignal&& other ) noexcept
    {
        TempSignal::Signal::operator=( std::move( other ) );
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
bool Equals( const Signal<D, L>& lhs, const Signal<D, R>& rhs )
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
/// EventStreamNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class EventStreamNode : public ObservableNode<D>
{
public:
    using DataT = std::vector<E>;
    using EngineT = typename D::Engine;
    using TurnT = typename EngineT::TurnT;

    EventStreamNode() = default;

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

    DataT& Events()
    {
        return events_;
    }

protected:
    DataT events_;

private:
    unsigned curTurnId_{ ( std::numeric_limits<unsigned>::max )() };
};

template <typename D, typename E>
using EventStreamNodePtrT = std::shared_ptr<EventStreamNode<D, E>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class EventSourceNode
    : public EventStreamNode<D, E>
    , public input_node_interface
{
    using Engine = typename EventSourceNode::Engine;

public:
    EventSourceNode()
        : EventSourceNode::EventStreamNode{}
    {}

    ~EventSourceNode() override = default;

    void tick( void* ) override
    {
        assert( !"Ticked EventSourceNode" );
    }

    template <typename V>
    void AddInput( V&& v )
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
template <typename E, typename... TDeps>
class EventMergeOp : public ReactiveOpBase<TDeps...>
{
public:
    template <typename... TDepsIn>
    EventMergeOp( TDepsIn&&... deps )
        : EventMergeOp::ReactiveOpBase( DontMove(), std::forward<TDepsIn>( deps )... )
    {}

    EventMergeOp( EventMergeOp&& other ) noexcept
        : EventMergeOp::ReactiveOpBase( std::move( other ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        apply( CollectFunctor<TTurn, TCollector>( turn, collector ), this->deps_ );
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec( const TFunctor& functor ) const
    {
        apply( reinterpret_cast<const CollectFunctor<TTurn, TCollector>&>( functor ), this->deps_ );
    }

private:
    template <typename TTurn, typename TCollector>
    struct CollectFunctor
    {
        CollectFunctor( const TTurn& turn, const TCollector& collector )
            : MyTurn( turn )
            , MyCollector( collector )
        {}

        void operator()( const TDeps&... deps ) const
        {
            REACT_EXPAND_PACK( collect( deps ) );
        }

        template <typename T>
        void collect( const T& op ) const
        {
            op.template CollectRec<TTurn, TCollector>( *this );
        }

        template <typename T>
        void collect( const std::shared_ptr<T>& depPtr ) const
        {
            depPtr->SetCurrentTurn( MyTurn );

            for( const auto& v : depPtr->Events() )
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
class EventFilterOp : public ReactiveOpBase<TDep>
{
public:
    template <typename TFilterIn, typename TDepIn>
    EventFilterOp( TFilterIn&& filter, TDepIn&& dep )
        : EventFilterOp::ReactiveOpBase{ DontMove(), std::forward<TDepIn>( dep ) }
        , filter_( std::forward<TFilterIn>( filter ) )
    {}

    EventFilterOp( EventFilterOp&& other ) noexcept
        : EventFilterOp::ReactiveOpBase{ std::move( other ) }
        , filter_( std::move( other.filter_ ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        collectImpl( turn, FilteredEventCollector<TCollector>{ filter_, collector }, getDep() );
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec( const TFunctor& functor ) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn, TCollector>( functor.MyTurn, functor.MyCollector );
    }

private:
    const TDep& getDep() const
    {
        return std::get<0>( this->deps_ );
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
        const TTurn& turn, const TCollector& collector, const std::shared_ptr<T>& depPtr )
    {
        depPtr->SetCurrentTurn( turn );

        for( const auto& v : depPtr->Events() )
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
class EventTransformOp : public ReactiveOpBase<TDep>
{
public:
    template <typename TFuncIn, typename TDepIn>
    EventTransformOp( TFuncIn&& func, TDepIn&& dep )
        : EventTransformOp::ReactiveOpBase( DontMove(), std::forward<TDepIn>( dep ) )
        , func_( std::forward<TFuncIn>( func ) )
    {}

    EventTransformOp( EventTransformOp&& other ) noexcept
        : EventTransformOp::ReactiveOpBase( std::move( other ) )
        , func_( std::move( other.func_ ) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect( const TTurn& turn, const TCollector& collector ) const
    {
        collectImpl( turn, TransformEventCollector<TCollector>( func_, collector ), getDep() );
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec( const TFunctor& functor ) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn, TCollector>( functor.MyTurn, functor.MyCollector );
    }

private:
    const TDep& getDep() const
    {
        return std::get<0>( this->deps_ );
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
        const TTurn& turn, const TCollector& collector, const std::shared_ptr<T>& depPtr )
    {
        depPtr->SetCurrentTurn( turn );

        for( const auto& v : depPtr->Events() )
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
class EventOpNode : public EventStreamNode<D, E>
{
    using Engine = typename EventOpNode::Engine;

public:
    template <typename... args_t>
    EventOpNode( args_t&&... args )
        : EventOpNode::EventStreamNode()
        , op_( std::forward<args_t>( args )... )
    {

        op_.template Attach<D>( *this );
    }

    ~EventOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template Detach<D>( *this );
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
        op_.template Detach<D>( *this );
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
class EventFlattenNode : public EventStreamNode<D, TInner>
{
    using Engine = typename EventFlattenNode::Engine;

public:
    EventFlattenNode( const std::shared_ptr<SignalNode<D, TOuter>>& outer,
        const std::shared_ptr<EventStreamNode<D, TInner>>& inner )
        : EventFlattenNode::EventStreamNode()
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
            this->events_.end(), inner_->Events().begin(), inner_->Events().end() );

        if( this->events_.size() > 0 )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<SignalNode<D, TOuter>> outer_;
    std::shared_ptr<EventStreamNode<D, TInner>> inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventTransformNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc, typename... TDepValues>
class SyncedEventTransformNode : public EventStreamNode<D, TOut>
{
    using Engine = typename SyncedEventTransformNode::Engine;

public:
    template <typename F>
    SyncedEventTransformNode( const std::shared_ptr<EventStreamNode<D, TIn>>& source,
        F&& func,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
        : SyncedEventTransformNode::EventStreamNode()
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

        apply( DetachFunctor<D,
                   SyncedEventTransformNode,
                   std::shared_ptr<SignalNode<D, TDepValues>>...>( *this ),
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
        if( !source_->Events().empty() )
        {
            for( const auto& e : source_->Events() )
            {
                this->events_.push_back( apply(
                    [this, &e]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
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
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D, TIn>> source_;

    TFunc func_;
    DepHolderT deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventFilterNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TFunc, typename... TDepValues>
class SyncedEventFilterNode : public EventStreamNode<D, E>
{
    using Engine = typename SyncedEventFilterNode::Engine;

public:
    template <typename F>
    SyncedEventFilterNode( const std::shared_ptr<EventStreamNode<D, E>>& source,
        F&& filter,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
        : SyncedEventFilterNode::EventStreamNode()
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
            DetachFunctor<D, SyncedEventFilterNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
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
        if( !source_->Events().empty() )
        {
            for( const auto& e : source_->Events() )
            {
                if( apply(
                        [this, &e]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
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
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D, E>> source_;

    TFunc filter_;
    DepHolderT deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc>
class EventProcessingNode : public EventStreamNode<D, TOut>
{
    using Engine = typename EventProcessingNode::Engine;

public:
    template <typename F>
    EventProcessingNode( const std::shared_ptr<EventStreamNode<D, TIn>>& source, F&& func )
        : EventProcessingNode::EventStreamNode()
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

        func_( EventRange<TIn>( source_->Events() ), std::back_inserter( this->events_ ) );

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    std::shared_ptr<EventStreamNode<D, TIn>> source_;

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TIn, typename TOut, typename TFunc, typename... TDepValues>
class SyncedEventProcessingNode : public EventStreamNode<D, TOut>
{
    using Engine = typename SyncedEventProcessingNode::Engine;

public:
    template <typename F>
    SyncedEventProcessingNode( const std::shared_ptr<EventStreamNode<D, TIn>>& source,
        F&& func,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
        : SyncedEventProcessingNode::EventStreamNode()
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

        apply( DetachFunctor<D,
                   SyncedEventProcessingNode,
                   std::shared_ptr<SignalNode<D, TDepValues>>...>( *this ),
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
        if( !source_->Events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
                    func_( EventRange<TIn>( source_->Events() ),
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
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D, TIn>> source_;

    TFunc func_;
    DepHolderT deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventJoinNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TValues>
class EventJoinNode : public EventStreamNode<D, std::tuple<TValues...>>
{
    using Engine = typename EventJoinNode::Engine;
    using TurnT = typename Engine::TurnT;

public:
    EventJoinNode( const std::shared_ptr<EventStreamNode<D, TValues>>&... sources )
        : EventJoinNode::EventStreamNode()
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
        Slot( const std::shared_ptr<EventStreamNode<D, T>>& src )
            : Source( src )
        {}

        std::shared_ptr<EventStreamNode<D, T>> Source;
        std::deque<T> Buffer;
    };

    template <typename T>
    static void fetchBuffer( TurnT& turn, Slot<T>& slot )
    {
        slot.Source->SetCurrentTurn( turn );

        slot.Buffer.insert(
            slot.Buffer.end(), slot.Source->Events().begin(), slot.Source->Events().end() );
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
class EventStreamBase : public CopyableReactive<EventStreamNode<D, E>>
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
        DomainSpecificInputManager<D>::Instance().AddInput(
            *reinterpret_cast<EventSourceNode<D, E>*>( this->ptr_.get() ), std::forward<T>( e ) );
    }
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeEventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = Token>
auto MakeEventSource() -> EventSource<D, E>
{
    using ::react::detail::EventSourceNode;

    return EventSource<D, E>( std::make_shared<EventSourceNode<D, E>>() );
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
auto Merge( const Events<D, TArg1>& arg1, const Events<D, args_t>&... args )
    -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    static_assert( sizeof...( args_t ) > 0, "Merge: 2+ arguments are required." );

    return TempEvents<D, E, TOp>(
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
auto operator|( const TLeftEvents& lhs, const TRightEvents& rhs ) -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( lhs ), GetNodePtr( rhs ) ) );
}

template <typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightVal,
    typename TRightOp,
    typename E = TLeftVal,
    typename TOp = ::react::detail::EventMergeOp<E, TLeftOp, TRightOp>>
auto operator|( TempEvents<D, TLeftVal, TLeftOp>&& lhs, TempEvents<D, TRightVal, TRightOp>&& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOp>(
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
auto operator|( TempEvents<D, TLeftVal, TLeftOp>&& lhs, const TRightEvents& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOp>(
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
auto operator|( const TLeftEvents& lhs, TempEvents<D, TRightVal, TRightOp>&& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOp>(
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
auto Filter( const Events<D, E>& src, FIn&& filter ) -> TempEvents<D, E, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOp>( std::make_shared<EventOpNode<D, E, TOp>>(
        std::forward<FIn>( filter ), GetNodePtr( src ) ) );
}

template <typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOpOut = ::react::detail::EventFilterOp<E, F, TOpIn>>
auto Filter( TempEvents<D, E, TOpIn>&& src, FIn&& filter ) -> TempEvents<D, E, TOpOut>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, E, TOpOut>(
        std::make_shared<EventOpNode<D, E, TOpOut>>( std::forward<FIn>( filter ), src.StealOp() ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename FIn, typename... TDepValues>
auto Filter( const Events<D, E>& source, const SignalPack<D, TDepValues...>& depPack, FIn&& func )
    -> Events<D, E>
{
    using ::react::detail::SyncedEventFilterNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const Events<D, E>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TDepValues>&... deps ) -> Events<D, E>
        {
            return Events<D, E>( std::make_shared<SyncedEventFilterNode<D, E, F, TDepValues...>>(
                GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const Events<D, E>& MySource;
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
auto Transform( const Events<D, TIn>& src, FIn&& func ) -> TempEvents<D, TOut, TOp>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, TOut, TOp>( std::make_shared<EventOpNode<D, TOut, TOp>>(
        std::forward<FIn>( func ), GetNodePtr( src ) ) );
}

template <typename D,
    typename TIn,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F( TIn )>::type,
    typename TOpOut = ::react::detail::EventTransformOp<TIn, F, TOpIn>>
auto Transform( TempEvents<D, TIn, TOpIn>&& src, FIn&& func ) -> TempEvents<D, TOut, TOpOut>
{
    using ::react::detail::EventOpNode;

    return TempEvents<D, TOut, TOpOut>( std::make_shared<EventOpNode<D, TOut, TOpOut>>(
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
    const Events<D, TIn>& source, const SignalPack<D, TDepValues...>& depPack, FIn&& func )
    -> Events<D, TOut>
{
    using ::react::detail::SyncedEventTransformNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const Events<D, TIn>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TDepValues>&... deps ) -> Events<D, TOut>
        {
            return Events<D, TOut>(
                std::make_shared<SyncedEventTransformNode<D, TIn, TOut, F, TDepValues...>>(
                    GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const Events<D, TIn>& MySource;
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
auto Process( const Events<D, TIn>& src, FIn&& func ) -> Events<D, TOut>
{
    using ::react::detail::EventProcessingNode;

    return Events<D, TOut>( std::make_shared<EventProcessingNode<D, TIn, TOut, F>>(
        GetNodePtr( src ), std::forward<FIn>( func ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename D, typename TIn, typename FIn, typename... TDepValues>
auto Process(
    const Events<D, TIn>& source, const SignalPack<D, TDepValues...>& depPack, FIn&& func )
    -> Events<D, TOut>
{
    using ::react::detail::SyncedEventProcessingNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_( const Events<D, TIn>& source, FIn&& func )
            : MySource( source )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TDepValues>&... deps ) -> Events<D, TOut>
        {
            return Events<D, TOut>(
                std::make_shared<SyncedEventProcessingNode<D, TIn, TOut, F, TDepValues...>>(
                    GetNodePtr( MySource ), std::forward<FIn>( MyFunc ), GetNodePtr( deps )... ) );
        }

        const Events<D, TIn>& MySource;
        FIn MyFunc;
    };

    return ::react::detail::apply(
        NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TInnerValue>
auto Flatten( const Signal<D, Events<D, TInnerValue>>& outer ) -> Events<D, TInnerValue>
{
    return Events<D, TInnerValue>(
        std::make_shared<::react::detail::EventFlattenNode<D, Events<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... args_t>
auto Join( const Events<D, args_t>&... args ) -> Events<D, std::tuple<args_t...>>
{
    using ::react::detail::EventJoinNode;

    static_assert( sizeof...( args_t ) > 1, "Join: 2+ arguments are required." );

    return Events<D, std::tuple<args_t...>>(
        std::make_shared<EventJoinNode<D, args_t...>>( GetNodePtr( args )... ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Token
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class Token
{
    value
};

struct Tokenizer
{
    template <typename T>
    Token operator()( const T& ) const
    {
        return Token::value;
    }
};

template <typename TEvents>
auto Tokenize( TEvents&& source ) -> decltype( Transform( source, Tokenizer{} ) )
{
    return Transform( source, Tokenizer{} );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = Token>
class Events : public ::react::detail::EventStreamBase<D, E>
{
private:
    using NodeT = ::react::detail::EventStreamNode<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events( const Events& ) = default;

    // Move ctor
    Events( Events&& other ) noexcept
        : Events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit Events( NodePtrT&& nodePtr )
        : Events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Events& operator=( const Events& ) = default;

    // Move assignment
    Events& operator=( Events&& other ) noexcept
    {
        Events::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    bool Equals( const Events& other ) const
    {
        return Events::EventStreamBase::Equals( other );
    }

    bool IsValid() const
    {
        return Events::EventStreamBase::IsValid();
    }

    auto Tokenize() const -> decltype( ::react::Tokenize( std::declval<Events>() ) )
    {
        return ::react::Tokenize( *this );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args ) const
        -> decltype( ::react::Merge( std::declval<Events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<Events>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<Events>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

// Specialize for references
template <typename D, typename E>
class Events<D, E&> : public ::react::detail::EventStreamBase<D, std::reference_wrapper<E>>
{
private:
    using NodeT = ::react::detail::EventStreamNode<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events( const Events& ) = default;

    // Move ctor
    Events( Events&& other ) noexcept
        : Events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit Events( NodePtrT&& nodePtr )
        : Events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Events& operator=( const Events& ) = default;

    // Move assignment
    Events& operator=( Events&& other ) noexcept
    {
        Events::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    bool Equals( const Events& other ) const
    {
        return Events::EventStreamBase::Equals( other );
    }

    bool IsValid() const
    {
        return Events::EventStreamBase::IsValid();
    }

    auto Tokenize() const -> decltype( ::react::Tokenize( std::declval<Events>() ) )
    {
        return ::react::Tokenize( *this );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args )
        -> decltype( ::react::Merge( std::declval<Events>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<Events>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<Events>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = Token>
class EventSource : public Events<D, E>
{
private:
    using NodeT = ::react::detail::EventSourceNode<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource( const EventSource& ) = default;

    // Move ctor
    EventSource( EventSource&& other ) noexcept
        : EventSource::Events( std::move( other ) )
    {}

    // Node ctor
    explicit EventSource( NodePtrT&& nodePtr )
        : EventSource::Events( std::move( nodePtr ) )
    {}

    // Copy assignment
    EventSource& operator=( const EventSource& ) = default;

    // Move assignment
    EventSource& operator=( EventSource&& other ) noexcept
    {
        EventSource::Events::operator=( std::move( other ) );
        return *this;
    }

    // Explicit emit
    void Emit( const E& e ) const
    {
        EventSource::EventStreamBase::emit( e );
    }

    void Emit( E&& e ) const
    {
        EventSource::EventStreamBase::emit( std::move( e ) );
    }

    void Emit() const
    {
        static_assert( std::is_same<E, Token>::value, "Can't emit on non token stream." );
        EventSource::EventStreamBase::emit( Token::value );
    }

    // Function object style
    void operator()( const E& e ) const
    {
        EventSource::EventStreamBase::emit( e );
    }

    void operator()( E&& e ) const
    {
        EventSource::EventStreamBase::emit( std::move( e ) );
    }

    void operator()() const
    {
        static_assert( std::is_same<E, Token>::value, "Can't emit on non token stream." );
        EventSource::EventStreamBase::emit( Token::value );
    }

    // Stream style
    const EventSource& operator<<( const E& e ) const
    {
        EventSource::EventStreamBase::emit( e );
        return *this;
    }

    const EventSource& operator<<( E&& e ) const
    {
        EventSource::EventStreamBase::emit( std::move( e ) );
        return *this;
    }
};

// Specialize for references
template <typename D, typename E>
class EventSource<D, E&> : public Events<D, std::reference_wrapper<E>>
{
private:
    using NodeT = ::react::detail::EventSourceNode<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource( const EventSource& ) = default;

    // Move ctor
    EventSource( EventSource&& other ) noexcept
        : EventSource::Events( std::move( other ) )
    {}

    // Node ctor
    explicit EventSource( NodePtrT&& nodePtr )
        : EventSource::Events( std::move( nodePtr ) )
    {}

    // Copy assignment
    EventSource& operator=( const EventSource& ) = default;

    // Move assignment
    EventSource& operator=( EventSource&& other ) noexcept
    {
        EventSource::Events::operator=( std::move( other ) );
        return *this;
    }

    // Explicit emit
    void Emit( std::reference_wrapper<E> e ) const
    {
        EventSource::EventStreamBase::emit( e );
    }

    // Function object style
    void operator()( std::reference_wrapper<E> e ) const
    {
        EventSource::EventStreamBase::emit( e );
    }

    // Stream style
    const EventSource& operator<<( std::reference_wrapper<E> e ) const
    {
        EventSource::EventStreamBase::emit( e );
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TempEvents
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E, typename TOp>
class TempEvents : public Events<D, E>
{
protected:
    using NodeT = ::react::detail::EventOpNode<D, E, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempEvents() = default;

    // Copy ctor
    TempEvents( const TempEvents& ) = default;

    // Move ctor
    TempEvents( TempEvents&& other ) noexcept
        : TempEvents::Events( std::move( other ) )
    {}

    // Node ctor
    explicit TempEvents( NodePtrT&& nodePtr )
        : TempEvents::Events( std::move( nodePtr ) )
    {}

    // Copy assignment
    TempEvents& operator=( const TempEvents& ) = default;

    // Move assignment
    TempEvents& operator=( TempEvents&& other ) noexcept
    {
        TempEvents::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move( reinterpret_cast<NodeT*>( this->ptr_.get() )->StealOp() );
    }

    template <typename... args_t>
    auto Merge( args_t&&... args )
        -> decltype( ::react::Merge( std::declval<TempEvents>(), std::forward<args_t>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<args_t>( args )... );
    }

    template <typename F>
    auto Filter( F&& f ) const
        -> decltype( ::react::Filter( std::declval<TempEvents>(), std::forward<F>( f ) ) )
    {
        return ::react::Filter( *this, std::forward<F>( f ) );
    }

    template <typename F>
    auto Transform( F&& f ) const
        -> decltype( ::react::Transform( std::declval<TempEvents>(), std::forward<F>( f ) ) )
    {
        return ::react::Transform( *this, std::forward<F>( f ) );
    }
};

namespace detail
{

template <typename D, typename L, typename R>
bool Equals( const Events<D, L>& lhs, const Events<D, R>& rhs )
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
class IterateNode : public SignalNode<D, S>
{
    using Engine = typename IterateNode::Engine;

public:
    template <typename T, typename F>
    IterateNode( T&& init, const std::shared_ptr<EventStreamNode<D, E>>& events, F&& func )
        : IterateNode::SignalNode( std::forward<T>( init ) )
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
            S newValue = func_( EventRange<E>( events_->Events() ), this->value_ );

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
    std::shared_ptr<EventStreamNode<D, E>> events_;

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc>
class IterateByRefNode : public SignalNode<D, S>
{
    using Engine = typename IterateByRefNode::Engine;

public:
    template <typename T, typename F>
    IterateByRefNode( T&& init, const std::shared_ptr<EventStreamNode<D, E>>& events, F&& func )
        : IterateByRefNode::SignalNode( std::forward<T>( init ) )
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
        func_( EventRange<E>( events_->Events() ), this->value_ );

        // Always assume change
        Engine::on_node_pulse( *this );
    }

protected:
    TFunc func_;

    std::shared_ptr<EventStreamNode<D, E>> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc, typename... TDepValues>
class SyncedIterateNode : public SignalNode<D, S>
{
    using Engine = typename SyncedIterateNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateNode( T&& init,
        const std::shared_ptr<EventStreamNode<D, E>>& events,
        F&& func,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
        : SyncedIterateNode::SignalNode( std::forward<T>( init ) )
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

        apply( DetachFunctor<D, SyncedIterateNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                   *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        events_->SetCurrentTurn( turn );

        bool changed = false;

        if( !events_->Events().empty() )
        {
            S newValue = apply(
                [this]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
                    return func_(
                        EventRange<E>( events_->Events() ), this->value_, args->ValueRef()... );
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
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D, E>> events_;

    TFunc func_;
    DepHolderT deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E, typename TFunc, typename... TDepValues>
class SyncedIterateByRefNode : public SignalNode<D, S>
{
    using Engine = typename SyncedIterateByRefNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateByRefNode( T&& init,
        const std::shared_ptr<EventStreamNode<D, E>>& events,
        F&& func,
        const std::shared_ptr<SignalNode<D, TDepValues>>&... deps )
        : SyncedIterateByRefNode::SignalNode( std::forward<T>( init ) )
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

        apply(
            DetachFunctor<D, SyncedIterateByRefNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                *this ),
            deps_ );
    }

    void tick( void* turn_ptr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turn_ptr );

        events_->SetCurrentTurn( turn );

        bool changed = false;

        if( !events_->Events().empty() )
        {
            apply(
                [this]( const std::shared_ptr<SignalNode<D, TDepValues>>&... args ) {
                    func_( EventRange<E>( events_->Events() ), this->value_, args->ValueRef()... );
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
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D, TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D, E>> events_;

    TFunc func_;
    DepHolderT deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// HoldNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class HoldNode : public SignalNode<D, S>
{
    using Engine = typename HoldNode::Engine;

public:
    template <typename T>
    HoldNode( T&& init, const std::shared_ptr<EventStreamNode<D, S>>& events )
        : HoldNode::SignalNode( std::forward<T>( init ) )
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

        if( !events_->Events().empty() )
        {
            const S& newValue = events_->Events().back();

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
    const std::shared_ptr<EventStreamNode<D, S>> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SnapshotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class SnapshotNode : public SignalNode<D, S>
{
    using Engine = typename SnapshotNode::Engine;

public:
    SnapshotNode( const std::shared_ptr<SignalNode<D, S>>& target,
        const std::shared_ptr<EventStreamNode<D, E>>& trigger )
        : SnapshotNode::SignalNode( target->ValueRef() )
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

        if( !trigger_->Events().empty() )
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
    const std::shared_ptr<SignalNode<D, S>> target_;
    const std::shared_ptr<EventStreamNode<D, E>> trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MonitorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class MonitorNode : public EventStreamNode<D, E>
{
    using Engine = typename MonitorNode::Engine;

public:
    MonitorNode( const std::shared_ptr<SignalNode<D, E>>& target )
        : MonitorNode::EventStreamNode()
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
    const std::shared_ptr<SignalNode<D, E>> target_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulseNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
class PulseNode : public EventStreamNode<D, S>
{
    using Engine = typename PulseNode::Engine;

public:
    PulseNode( const std::shared_ptr<SignalNode<D, S>>& target,
        const std::shared_ptr<EventStreamNode<D, E>>& trigger )
        : PulseNode::EventStreamNode()
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

        for( size_t i = 0, ie = trigger_->Events().size(); i < ie; ++i )
        {
            this->events_.push_back( target_->ValueRef() );
        }

        if( !this->events_.empty() )
        {
            Engine::on_node_pulse( *this );
        }
    }

private:
    const std::shared_ptr<SignalNode<D, S>> target_;
    const std::shared_ptr<EventStreamNode<D, E>> trigger_;
};

} // namespace detail

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold - Hold the most recent event in a signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename T = typename std::decay<V>::type>
auto Hold( const Events<D, T>& events, V&& init ) -> Signal<D, T>
{
    using ::react::detail::HoldNode;

    return Signal<D, T>(
        std::make_shared<HoldNode<D, T>>( std::forward<V>( init ), GetNodePtr( events ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Monitor - Emits value changes of target signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto Monitor( const Signal<D, S>& target ) -> Events<D, S>
{
    using ::react::detail::MonitorNode;

    return Events<D, S>( std::make_shared<MonitorNode<D, S>>( GetNodePtr( target ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Iteratively combines signal value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename E,
    typename V,
    typename FIn,
    typename S = typename std::decay<V>::type>
auto Iterate( const Events<D, E>& events, V&& init, FIn&& func ) -> Signal<D, S>
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

    return Signal<D, S>( std::make_shared<NodeT>(
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
    const Events<D, E>& events, V&& init, const SignalPack<D, TDepValues...>& depPack, FIn&& func )
    -> Signal<D, S>
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
        NodeBuilder_( const Events<D, E>& source, V&& init, FIn&& func )
            : MySource( source )
            , MyInit( std::forward<V>( init ) )
            , MyFunc( std::forward<FIn>( func ) )
        {}

        auto operator()( const Signal<D, TDepValues>&... deps ) -> Signal<D, S>
        {
            return Signal<D, S>( std::make_shared<NodeT>( std::forward<V>( MyInit ),
                GetNodePtr( MySource ),
                std::forward<FIn>( MyFunc ),
                GetNodePtr( deps )... ) );
        }

        const Events<D, E>& MySource;
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
auto Snapshot( const Events<D, E>& trigger, const Signal<D, S>& target ) -> Signal<D, S>
{
    using ::react::detail::SnapshotNode;

    return Signal<D, S>(
        std::make_shared<SnapshotNode<D, S, E>>( GetNodePtr( target ), GetNodePtr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto Pulse( const Events<D, E>& trigger, const Signal<D, S>& target ) -> Events<D, S>
{
    using ::react::detail::PulseNode;

    return Events<D, S>(
        std::make_shared<PulseNode<D, S, E>>( GetNodePtr( target ), GetNodePtr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Changed - Emits token when target signal was changed
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto Changed( const Signal<D, S>& target ) -> Events<D, Token>
{
    return Monitor( target ).Tokenize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ChangedTo - Emits token when target signal was changed to value
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename S = typename std::decay<V>::type>
auto ChangedTo( const Signal<D, S>& target, V&& value ) -> Events<D, Token>
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

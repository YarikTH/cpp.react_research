
//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <algorithm>
#include <atomic>
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

// Assert with message
#define REACT_ASSERT( condition, ... )                                                             \
    for( ; !( condition ); assert( condition ) )                                                   \
    printf( __VA_ARGS__ )
#define REACT_ERROR( ... ) REACT_ASSERT( false, __VA_ARGS__ )

// Thread local storage
#if _WIN32 || _WIN64
// MSVC
#    define REACT_TLS __declspec( thread )
#elif __GNUC__
// GCC
#    define REACT_TLS __thread
#else
// Standard C++11
#    define REACT_TLS thread_local
#endif

namespace react
{
namespace impl
{

// Type aliases
using uint = unsigned int;
using uchar = unsigned char;
using std::size_t;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Unpack tuple - see
/// http://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments
///////////////////////////////////////////////////////////////////////////////////////////////////

template <size_t N>
struct Apply
{
    template <typename F, typename T, typename... A>
    static inline auto apply( F&& f, T&& t, A&&... a )
        -> decltype( Apply<N - 1>::apply( std::forward<F>( f ),
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... ) )
    {
        return Apply<N - 1>::apply( std::forward<F>( f ),
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... );
    }
};

template <>
struct Apply<0>
{
    template <typename F, typename T, typename... A>
    static inline auto apply( F&& f, T&&, A&&... a )
        -> decltype( std::forward<F>( f )( std::forward<A>( a )... ) )
    {
        return std::forward<F>( f )( std::forward<A>( a )... );
    }
};

template <typename F, typename T>
inline auto apply( F&& f, T&& t )
    -> decltype( Apply<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        std::forward<F>( f ), std::forward<T>( t ) ) )
{
    return Apply<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        std::forward<F>( f ), std::forward<T>( t ) );
}

template <size_t N>
struct ApplyMemberFn
{
    template <typename O, typename F, typename T, typename... A>
    static inline auto apply( O obj, F f, T&& t, A&&... a )
        -> decltype( ApplyMemberFn<N - 1>::apply( obj,
            f,
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... ) )
    {
        return ApplyMemberFn<N - 1>::apply( obj,
            f,
            std::forward<T>( t ),
            std::get<N - 1>( std::forward<T>( t ) ),
            std::forward<A>( a )... );
    }
};

template <>
struct ApplyMemberFn<0>
{
    template <typename O, typename F, typename T, typename... A>
    static inline auto apply( O obj, F f, T&&, A&&... a )
        -> decltype( ( obj->*f )( std::forward<A>( a )... ) )
    {
        return ( obj->*f )( std::forward<A>( a )... );
    }
};

template <typename O, typename F, typename T>
inline auto applyMemberFn( O obj, F f, T&& t )
    -> decltype( ApplyMemberFn<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        obj, f, std::forward<T>( t ) ) )
{
    return ApplyMemberFn<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        obj, f, std::forward<T>( t ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper to enable calling a function on each element of an argument pack.
/// We can't do f(args) ...; because ... expands with a comma.
/// But we can do nop_func(f(args) ...);
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename... TArgs>
inline void pass( TArgs&&... )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Identity (workaround to enable enable decltype()::X)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct Identity
{
    using Type = T;
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddDummyArgWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TArg, typename F, typename TRet, typename... TDepValues>
struct AddDummyArgWrapper
{
    AddDummyArgWrapper( const AddDummyArgWrapper& other ) = default;

    AddDummyArgWrapper( AddDummyArgWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddDummyArgWrapper>::type>
    explicit AddDummyArgWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    TRet operator()( TArg, TDepValues&... args )
    {
        return MyFunc( args... );
    }

    F MyFunc;
};

template <typename TArg, typename F, typename... TDepValues>
struct AddDummyArgWrapper<TArg, F, void, TDepValues...>
{
public:
    AddDummyArgWrapper( const AddDummyArgWrapper& other ) = default;

    AddDummyArgWrapper( AddDummyArgWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddDummyArgWrapper>::type>
    explicit AddDummyArgWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    void operator()( TArg, TDepValues&... args )
    {
        MyFunc( args... );
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddDefaultReturnValueWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename TRet, TRet return_value>
struct AddDefaultReturnValueWrapper
{
    AddDefaultReturnValueWrapper( const AddDefaultReturnValueWrapper& ) = default;

    AddDefaultReturnValueWrapper( AddDefaultReturnValueWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddDefaultReturnValueWrapper>::type>
    explicit AddDefaultReturnValueWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    template <typename... TArgs>
    TRet operator()( TArgs&&... args )
    {
        MyFunc( std::forward<TArgs>( args )... );
        return return_value;
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsCallableWith
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename TRet, typename... TArgs>
class IsCallableWith
{
private:
    using NoT = char[1];
    using YesT = char[2];

    template <typename U,
        class = decltype( static_cast<TRet>( ( std::declval<U>() )( std::declval<TArgs>()... ) ) )>
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
#define REACT_EXPAND_PACK( ... ) ::react::impl::pass( ( __VA_ARGS__, 0 )... )

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactiveEngine
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode, typename TTurn>
struct IReactiveEngine
{
    using NodeT = TNode;
    using TurnT = TTurn;

    void OnTurnAdmissionStart( TurnT& turn )
    {}

    void OnTurnAdmissionEnd( TurnT& turn )
    {}

    void OnInputChange( NodeT& node, TurnT& turn )
    {}

    void Propagate( TurnT& turn )
    {}

    void OnNodeCreate( NodeT& node )
    {}

    void OnNodeDestroy( NodeT& node )
    {}

    void OnNodeAttach( NodeT& node, NodeT& parent )
    {}

    void OnNodeDetach( NodeT& node, NodeT& parent )
    {}

    void OnNodePulse( NodeT& node, TurnT& turn )
    {}

    void OnNodeIdlePulse( NodeT& node, TurnT& turn )
    {}

    void OnDynamicNodeAttach( NodeT& node, NodeT& parent, TurnT& turn )
    {}

    void OnDynamicNodeDetach( NodeT& node, NodeT& parent, TurnT& turn )
    {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineInterface - Static wrapper for IReactiveEngine
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TEngine>
struct EngineInterface
{
    using NodeT = typename TEngine::NodeT;
    using TurnT = typename TEngine::TurnT;

    static TEngine& Instance()
    {
        static TEngine engine;
        return engine;
    }

    static void OnTurnAdmissionStart( TurnT& turn )
    {
        Instance().OnTurnAdmissionStart( turn );
    }

    static void OnTurnAdmissionEnd( TurnT& turn )
    {
        Instance().OnTurnAdmissionEnd( turn );
    }

    static void OnInputChange( NodeT& node, TurnT& turn )
    {
        Instance().OnInputChange( node, turn );
    }

    static void Propagate( TurnT& turn )
    {
        Instance().Propagate( turn );
    }

    static void OnNodeCreate( NodeT& node )
    {
        Instance().OnNodeCreate( node );
    }

    static void OnNodeDestroy( NodeT& node )
    {
        Instance().OnNodeDestroy( node );
    }

    static void OnNodeAttach( NodeT& node, NodeT& parent )
    {
        Instance().OnNodeAttach( node, parent );
    }

    static void OnNodeDetach( NodeT& node, NodeT& parent )
    {
        Instance().OnNodeDetach( node, parent );
    }

    static void OnNodePulse( NodeT& node, TurnT& turn )
    {
        Instance().OnNodePulse( node, turn );
    }

    static void OnNodeIdlePulse( NodeT& node, TurnT& turn )
    {
        Instance().OnNodeIdlePulse( node, turn );
    }

    static void OnDynamicNodeAttach( NodeT& node, NodeT& parent, TurnT& turn )
    {
        Instance().OnDynamicNodeAttach( node, parent, turn );
    }

    static void OnDynamicNodeDetach( NodeT& node, NodeT& parent, TurnT& turn )
    {
        Instance().OnDynamicNodeDetach( node, parent, turn );
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Engine traits
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename>
struct NodeUpdateTimerEnabled : std::false_type
{};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EPropagationMode
///////////////////////////////////////////////////////////////////////////////////////////////////
enum EPropagationMode
{
    sequential_propagation
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactiveNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IReactiveNode
{
    virtual ~IReactiveNode() = default;

    // Note: Could get rid of this ugly ptr by adding a template parameter to the interface
    // But that would mean all engine nodes need that template parameter too - so rather cast
    virtual void Tick( void* turnPtr ) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IInputNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IInputNode
{
    virtual ~IInputNode() = default;

    virtual bool ApplyInput( void* turnPtr ) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
class IObserver
{
public:
    virtual ~IObserver()
    {}

    virtual void UnregisterSelf() = 0;

private:
    virtual void detachObserver() = 0;

    template <typename D>
    friend class Observable;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observable
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class Observable
{
public:
    Observable() = default;

    ~Observable()
    {
        for( const auto& p : observers_ )
        {
            if( p != nullptr )
            {
                p->detachObserver();
            }
        }
    }

    void RegisterObserver( std::unique_ptr<IObserver>&& obsPtr )
    {
        observers_.push_back( std::move( obsPtr ) );
    }

    void UnregisterObserver( IObserver* rawObsPtr )
    {
        for( auto it = observers_.begin(); it != observers_.end(); ++it )
        {
            if( it->get() == rawObsPtr )
            {
                it->get()->detachObserver();
                observers_.erase( it );
                break;
            }
        }
    }

private:
    std::vector<std::unique_ptr<IObserver>> observers_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class NodeBase : public D::Policy::Engine::NodeT
{
public:
    using DomainT = D;
    using Policy = typename D::Policy;
    using Engine = typename D::Engine;
    using NodeT = typename Engine::NodeT;
    using TurnT = typename Engine::TurnT;

    NodeBase() = default;

    // Nodes can't be copied
    NodeBase( const NodeBase& ) = delete;
};

template <typename D>
using NodeBasePtrT = std::shared_ptr<NodeBase<D>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObservableNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObservableNode
    : public NodeBase<D>
    , public Observable<D>
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
        D::Engine::OnNodeAttach( MyNode, *depPtr );
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
        D::Engine::OnNodeDetach( MyNode, *depPtr );
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

    ReactiveOpBase( ReactiveOpBase&& other )
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
    ReactiveBase( ReactiveBase&& other )
        : ptr_( std::move( other.ptr_ ) )
    {}

    // Explicit node ctor
    explicit ReactiveBase( std::shared_ptr<TNode>&& ptr )
        : ptr_( std::move( ptr ) )
    {}

    // Copy assignment
    ReactiveBase& operator=( const ReactiveBase& ) = default;

    // Move assignment
    ReactiveBase& operator=( ReactiveBase&& other )
    {
        ptr_.reset( std::move( other ) );
        return *this;
    }

    bool IsValid() const
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

    CopyableReactive( CopyableReactive&& other )
        : CopyableReactive::ReactiveBase( std::move( other ) )
    {}

    explicit CopyableReactive( std::shared_ptr<TNode>&& ptr )
        : CopyableReactive::ReactiveBase( std::move( ptr ) )
    {}

    CopyableReactive& operator=( const CopyableReactive& ) = default;

    CopyableReactive& operator=( CopyableReactive&& other )
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
struct IInputNode;

class IObserver;

template <typename D>
class InputManager;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////
using TurnIdT = uint;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EInputMode
///////////////////////////////////////////////////////////////////////////////////////////////////
enum EInputMode
{
    consecutive_input
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ThreadLocalInputState
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename = void>
struct ThreadLocalInputState
{
    static REACT_TLS bool IsTransactionActive;
};

template <typename T>
REACT_TLS bool ThreadLocalInputState<T>::IsTransactionActive( false );

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DetachedObserversManager
///////////////////////////////////////////////////////////////////////////////////////////////////
class DetachedObserversManager
{
    using ObsVectT = std::vector<IObserver*>;

public:
    void QueueObserverForDetach( IObserver& obs )
    {
        detachedObservers_.push_back( &obs );
    }

    template <typename D>
    void DetachQueuedObservers()
    {
        for( auto* o : detachedObservers_ )
        {
            o->UnregisterSelf();
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

    InputManager()
    {}

    template <typename F>
    void DoTransaction( F&& func )
    {
        bool shouldPropagate = false;

        // Phase 1 - Input admission
        ThreadLocalInputState<>::IsTransactionActive = true;

        TurnT turn( nextTurnId() );
        Engine::OnTurnAdmissionStart( turn );
        func();
        Engine::OnTurnAdmissionEnd( turn );

        ThreadLocalInputState<>::IsTransactionActive = false;

        // Phase 2 - Apply input node changes
        for( auto* p : changedInputs_ )
        {
            if( p->ApplyInput( &turn ) )
            {
                shouldPropagate = true;
            }
        }
        changedInputs_.clear();

        // Phase 3 - Propagate changes
        if( shouldPropagate )
        {
            Engine::Propagate( turn );
        }

        finalizeSyncTransaction();
    }

    template <typename R, typename V>
    void AddInput( R& r, V&& v )
    {
        if( ThreadLocalInputState<>::IsTransactionActive )
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
        if( ThreadLocalInputState<>::IsTransactionActive )
        {
            modifyTransactionInput( r, func );
        }
        else
        {
            modifySimpleInput( r, func );
        }
    }

    void QueueObserverForDetach( IObserver& obs )
    {
        detachedObserversManager_.QueueObserverForDetach( obs );
    }

private:
    TurnIdT nextTurnId()
    {
        auto curId = nextTurnId_.fetch_add( 1, std::memory_order_relaxed );

        if( curId == ( std::numeric_limits<uint>::max )() )
        {
            nextTurnId_.fetch_sub( ( std::numeric_limits<int>::max )() );
        }

        return curId;
    }

    // Create a turn with a single input
    template <typename R, typename V>
    void addSimpleInput( R& r, V&& v )
    {
        TurnT turn( nextTurnId() );
        Engine::OnTurnAdmissionStart( turn );
        r.AddInput( std::forward<V>( v ) );
        Engine::OnTurnAdmissionEnd( turn );

        if( r.ApplyInput( &turn ) )
        {
            Engine::Propagate( turn );
        }

        finalizeSyncTransaction();
    }

    template <typename R, typename F>
    void modifySimpleInput( R& r, const F& func )
    {
        TurnT turn( nextTurnId() );
        Engine::OnTurnAdmissionStart( turn );
        r.ModifyInput( func );
        Engine::OnTurnAdmissionEnd( turn );

        // Return value, will always be true
        r.ApplyInput( &turn );

        Engine::Propagate( turn );

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

    std::atomic<TurnIdT> nextTurnId_{ 0 };

    std::vector<IInputNode*> changedInputs_;
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

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddContinuationRangeWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename F, typename... TArgs>
struct AddContinuationRangeWrapper
{
    AddContinuationRangeWrapper( const AddContinuationRangeWrapper& other ) = default;

    AddContinuationRangeWrapper( AddContinuationRangeWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddContinuationRangeWrapper>::type>
    explicit AddContinuationRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    void operator()( EventRange<E> range, const TArgs&... args )
    {
        for( const auto& e : range )
        {
            MyFunc( e, args... );
        }
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EnumFlags
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
class EnumFlags
{
public:
    using FlagsT = typename std::underlying_type<T>::type;

    template <T x>
    void Set()
    {
        flags_ |= 1 << x;
    }

    template <T x>
    void Clear()
    {
        flags_ &= ~( 1 << x );
    }

    template <T x>
    bool Test() const
    {
        return ( flags_ & ( 1 << x ) ) != 0;
    }

private:
    FlagsT flags_ = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeVector
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
class NodeVector
{
private:
    typedef std::vector<TNode*> DataT;

public:
    void Add( TNode& node )
    {
        data_.push_back( &node );
    }

    void Remove( const TNode& node )
    {
        data_.erase( std::find( data_.begin(), data_.end(), &node ) );
    }

    typedef typename DataT::iterator iterator;
    typedef typename DataT::const_iterator const_iterator;

    iterator begin()
    {
        return data_.begin();
    }

    iterator end()
    {
        return data_.end();
    }

    const_iterator begin() const
    {
        return data_.begin();
    }

    const_iterator end() const
    {
        return data_.end();
    }

private:
    DataT data_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TurnBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class TurnBase
{
public:
    inline TurnBase( TurnIdT id )
        : id_( id )
    {}

    inline TurnIdT Id() const
    {
        return id_;
    }

private:
    TurnIdT id_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TopoQueue - Sequential
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename TLevelFunc>
class TopoQueue
{
private:
    struct Entry;

public:
    // Store the level as part of the entry for cheap comparisons
    using QueueDataT = std::vector<Entry>;
    using NextDataT = std::vector<T>;

    TopoQueue() = default;
    TopoQueue( const TopoQueue& ) = default;

    template <typename FIn>
    TopoQueue( FIn&& levelFunc )
        : levelFunc_( std::forward<FIn>( levelFunc ) )
    {}

    void Push( const T& value )
    {
        queueData_.emplace_back( value, levelFunc_( value ) );
    }

    bool FetchNext()
    {
        // Throw away previous values
        nextData_.clear();

        // Find min level of nodes in queue data
        minLevel_ = ( std::numeric_limits<int>::max )();
        for( const auto& e : queueData_ )
        {
            if( minLevel_ > e.Level )
            {
                minLevel_ = e.Level;
            }
        }

        // Swap entries with min level to the end
        auto p
            = std::partition( queueData_.begin(), queueData_.end(), LevelCompFunctor{ minLevel_ } );

        // Reserve once to avoid multiple re-allocations
        nextData_.reserve( std::distance( p, queueData_.end() ) );

        // Move min level values to next data
        for( auto it = p; it != queueData_.end(); ++it )
        {
            nextData_.push_back( std::move( it->Value ) );
        }

        // Truncate moved entries
        queueData_.resize( std::distance( queueData_.begin(), p ) );

        return !nextData_.empty();
    }

    const NextDataT& NextValues() const
    {
        return nextData_;
    }

private:
    struct Entry
    {
        Entry() = default;
        Entry( const Entry& ) = default;

        Entry( const T& value, int level )
            : Value( value )
            , Level( level )
        {}

        T Value;
        int Level;
    };

    struct LevelCompFunctor
    {
        LevelCompFunctor( int level )
            : Level( level )
        {}

        bool operator()( const Entry& e ) const
        {
            return e.Level != Level;
        }

        const int Level;
    };

    NextDataT nextData_;
    QueueDataT queueData_;

    TLevelFunc levelFunc_;

    int minLevel_ = ( std::numeric_limits<int>::max )();
};

namespace toposort
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqNode : public IReactiveNode
{
public:
    int Level{ 0 };
    int NewLevel{ 0 };
    bool Queued{ false };

    NodeVector<SeqNode> Successors;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ExclusiveSeqTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqTurn : public TurnBase
{
public:
    SeqTurn( TurnIdT id );
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct GetLevelFunctor
{
    int operator()( const T* x ) const
    {
        return x->Level;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode, typename TTurn>
class EngineBase : public IReactiveEngine<TNode, TTurn>
{
public:
    void OnNodeAttach( TNode& node, TNode& parent );
    void OnNodeDetach( TNode& node, TNode& parent );

    void OnInputChange( TNode& node, TTurn& turn );
    void OnNodePulse( TNode& node, TTurn& turn );

protected:
    virtual void processChildren( TNode& node, TTurn& turn ) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqEngineBase : public EngineBase<SeqNode, SeqTurn>
{
public:
    using TopoQueueT = TopoQueue<SeqNode*, GetLevelFunctor<SeqNode>>;

    void Propagate( SeqTurn& turn );

    void OnDynamicNodeAttach( SeqNode& node, SeqNode& parent, SeqTurn& turn );
    void OnDynamicNodeDetach( SeqNode& node, SeqNode& parent, SeqTurn& turn );

private:
    void invalidateSuccessors( SeqNode& node );

    virtual void processChildren( SeqNode& node, SeqTurn& turn ) override;

    TopoQueueT scheduledNodes_;
};

} // namespace toposort

} // namespace impl

template <::react::impl::EPropagationMode>
class ToposortEngine;

template <>
class ToposortEngine<::react::impl::sequential_propagation>
    : public ::react::impl::toposort::SeqEngineBase
{};

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

namespace impl
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

// Domain modes
enum EDomainMode
{
    sequential,
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DomainBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TPolicy>
class DomainBase
{
public:
    using TurnT = typename TPolicy::Engine::TurnT;

    DomainBase() = delete;

    using Policy = TPolicy;
    using Engine = ::react::impl::EngineInterface<D, typename Policy::Engine>;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Domain traits
    ///////////////////////////////////////////////////////////////////////////////////////////////
    static const bool uses_node_update_timer
        = ::react::impl::NodeUpdateTimerEnabled<typename Policy::Engine>::value;

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
/// ModeSelector - Translate domain mode to individual propagation and input modes
///////////////////////////////////////////////////////////////////////////////////////////////////

template <EDomainMode>
struct ModeSelector;

template <>
struct ModeSelector<sequential>
{
    static const EInputMode input = consecutive_input;
    static const EPropagationMode propagation = sequential_propagation;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GetDefaultEngine - Get default engine type for given propagation mode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <EPropagationMode>
struct GetDefaultEngine;

template <>
struct GetDefaultEngine<sequential_propagation>
{
    using Type = ToposortEngine<sequential_propagation>;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineTypeBuilder - Instantiate the given template engine type with mode.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <EPropagationMode>
struct DefaultEnginePlaceholder;

// Concrete engine template type
template <EPropagationMode mode, template <EPropagationMode> class TTEngine>
struct EngineTypeBuilder
{
    using Type = TTEngine<mode>;
};

// Placeholder engine type - use default engine for given mode
template <EPropagationMode mode>
struct EngineTypeBuilder<mode, DefaultEnginePlaceholder>
{
    using Type = typename GetDefaultEngine<mode>::Type;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DomainPolicy
///////////////////////////////////////////////////////////////////////////////////////////////////
template <EDomainMode mode, template <EPropagationMode> class TTEngine = DefaultEnginePlaceholder>
struct DomainPolicy
{
    static const EInputMode input_mode = ModeSelector<mode>::input;
    static const EPropagationMode propagation_mode = ModeSelector<mode>::propagation;

    using Engine = typename EngineTypeBuilder<propagation_mode, TTEngine>::Type;
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
        D::Engine::Instance();
        DomainSpecificInputManager<D>::Instance();
    }
};

} // namespace impl

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename S>
class VarSignal;

template <typename D, typename S, typename TOp>
class TempSignal;

template <typename D, typename E>
class Events;

template <typename D, typename E>
class EventSource;

template <typename D, typename E, typename TOp>
class TempEvents;

enum class Token;

template <typename D>
class Observer;

template <typename D>
class ScopedObserver;

template <typename D, typename... TValues>
class SignalPack;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

// Domain modes
using ::react::impl::EDomainMode;
using ::react::impl::sequential;

// Expose enum type so aliases for engines can be declared, but don't
// expose the actual enum values as they are reserved for internal use.
using ::react::impl::EPropagationMode;

///////////////////////////////////////////////////////////////////////////////////////////////
/// DoTransaction
///////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename F>
void DoTransaction( F&& func )
{
    using ::react::impl::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance().DoTransaction( std::forward<F>( func ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Domain definition macro
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACTIVE_DOMAIN( name, ... )                                                               \
    struct name : public ::react::impl::DomainBase<name, ::react::impl::DomainPolicy<__VA_ARGS__>> \
    {};                                                                                            \
    static ::react::impl::DomainInitializer<name> name##_initializer_;

/*
    A brief reminder why the domain initializer is here:
    Each domain has a couple of singletons (debug log, engine, input manager) which are
    currently implemented as meyer singletons. From what I understand, these are thread-safe
    in C++11, but not all compilers implement that yet. That's why a static initializer has
    been added to make sure singleton creation happens before any multi-threaded access.
    This implemenation is obviously inconsequential.
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

namespace impl
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalNode;

template <typename D, typename E>
class EventStreamNode;

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
template <typename E, typename F, typename... TArgs>
struct AddObserverRangeWrapper
{
    AddObserverRangeWrapper( const AddObserverRangeWrapper& other ) = default;

    AddObserverRangeWrapper( AddObserverRangeWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddObserverRangeWrapper>::type>
    explicit AddObserverRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    ObserverAction operator()( EventRange<E> range, const TArgs&... args )
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
    , public IObserver
{
public:
    ObserverNode() = default;

    virtual bool IsOutputNode() const
    {
        return true;
    }
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *subject );
    }

    ~SignalObserverNode()
    {
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
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

    virtual void UnregisterSelf() override
    {
        if( auto p = subject_.lock() )
        {
            p->UnregisterObserver( this );
        }
    }

private:
    virtual void detachObserver() override
    {
        if( auto p = subject_.lock() )
        {
            Engine::OnNodeDetach( *this, *p );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *subject );
    }

    ~EventObserverNode()
    {
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
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

    virtual void UnregisterSelf() override
    {
        if( auto p = subject_.lock() )
        {
            p->UnregisterObserver( this );
        }
    }

private:
    std::weak_ptr<EventStreamNode<D, E>> subject_;

    TFunc func_;

    virtual void detachObserver()
    {
        if( auto p = subject_.lock() )
        {
            Engine::OnNodeDetach( *this, *p );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *subject );

        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedObserverNode()
    {
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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

    virtual void UnregisterSelf() override
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

    virtual void detachObserver()
    {
        if( auto p = subject_.lock() )
        {
            Engine::OnNodeDetach( *this, *p );

            apply(
                DetachFunctor<D, SyncedObserverNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                    *this ),
                deps_ );

            subject_.reset();
        }
    }
};

} // namespace impl

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename... TValues>
class SignalPack;

template <typename D, typename E>
class Events;

using ::react::impl::ObserverAction;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class Observer
{
private:
    using SubjectPtrT = std::shared_ptr<::react::impl::ObservableNode<D>>;
    using NodeT = ::react::impl::ObserverNode<D>;

public:
    // Default ctor
    Observer()
        : nodePtr_( nullptr )
        , subjectPtr_( nullptr )
    {}

    // Move ctor
    Observer( Observer&& other )
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
    Observer& operator=( Observer&& other )
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
    ScopedObserver( ScopedObserver&& other )
        : obs_( std::move( other.obs_ ) )
    {}

    // Construct from observer
    ScopedObserver( Observer<D>&& obs )
        : obs_( std::move( obs ) )
    {}

    // Move assignment
    ScopedObserver& operator=( ScopedObserver&& other )
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
    using ::react::impl::AddDefaultReturnValueWrapper;
    using ::react::impl::IObserver;
    using ::react::impl::ObserverNode;
    using ::react::impl::SignalObserverNode;

    using F = typename std::decay<FIn>::type;
    using R = typename std::result_of<FIn( S )>::type;
    using WrapperT = AddDefaultReturnValueWrapper<F, ObserverAction, ObserverAction::next>;

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
    using ::react::impl::AddDefaultReturnValueWrapper;
    using ::react::impl::AddObserverRangeWrapper;
    using ::react::impl::EventObserverNode;
    using ::react::impl::EventRange;
    using ::react::impl::IObserver;
    using ::react::impl::IsCallableWith;
    using ::react::impl::ObserverNode;

    using F = typename std::decay<FIn>::type;

    using WrapperT =
        typename std::conditional<IsCallableWith<F, ObserverAction, EventRange<E>>::value,
            F,
            typename std::conditional<IsCallableWith<F, ObserverAction, E>::value,
                AddObserverRangeWrapper<E, F>,
                typename std::conditional<IsCallableWith<F, void, EventRange<E>>::value,
                    AddDefaultReturnValueWrapper<F, ObserverAction, ObserverAction::next>,
                    typename std::conditional<IsCallableWith<F, void, E>::value,
                        AddObserverRangeWrapper<E,
                            AddDefaultReturnValueWrapper<F, ObserverAction, ObserverAction::next>>,
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
    using ::react::impl::AddDefaultReturnValueWrapper;
    using ::react::impl::AddObserverRangeWrapper;
    using ::react::impl::EventRange;
    using ::react::impl::IObserver;
    using ::react::impl::IsCallableWith;
    using ::react::impl::ObserverNode;
    using ::react::impl::SyncedObserverNode;

    using F = typename std::decay<FIn>::type;

    using WrapperT = typename std::conditional<
        IsCallableWith<F, ObserverAction, EventRange<E>, TDepValues...>::value,
        F,
        typename std::conditional<IsCallableWith<F, ObserverAction, E, TDepValues...>::value,
            AddObserverRangeWrapper<E, F, TDepValues...>,
            typename std::conditional<IsCallableWith<F, void, EventRange<E>, TDepValues...>::value,
                AddDefaultReturnValueWrapper<F, ObserverAction, ObserverAction::next>,
                typename std::conditional<IsCallableWith<F, void, E, TDepValues...>::value,
                    AddObserverRangeWrapper<E,
                        AddDefaultReturnValueWrapper<F, ObserverAction, ObserverAction::next>,
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

    std::unique_ptr<ObserverNode<D>> nodePtr(
        ::react::impl::apply( NodeBuilder_( subject, std::forward<FIn>( func ) ), depPack.Data ) );

    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver( std::move( nodePtr ) );

    return Observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename S>
class VarSignal;

template <typename D, typename S, typename TOp>
class TempSignal;

template <typename D, typename E>
class Events;

template <typename D, typename E>
class EventSource;

template <typename D, typename E, typename TOp>
class TempEvents;

template <typename D>
class Observer;

template <typename D>
class ScopedObserver;

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
/// IsObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsObserver
{
    static const bool value = false;
};

template <typename D>
struct IsObserver<Observer<D>>
{
    static const bool value = true;
};

template <typename D>
struct IsObserver<ScopedObserver<D>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsObservable
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsObservable
{
    static const bool value = false;
};

template <typename D, typename T>
struct IsObservable<Signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsObservable<VarSignal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsObservable<TempSignal<D, T, TOp>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsObservable<Events<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsObservable<EventSource<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsObservable<TempEvents<D, T, TOp>>
{
    static const bool value = true;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsReactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsReactive
{
    static const bool value = false;
};

template <typename D, typename T>
struct IsReactive<Signal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsReactive<VarSignal<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsReactive<TempSignal<D, T, TOp>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsReactive<Events<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T>
struct IsReactive<EventSource<D, T>>
{
    static const bool value = true;
};

template <typename D, typename T, typename TOp>
struct IsReactive<TempEvents<D, T, TOp>>
{
    static const bool value = true;
};

template <typename D>
struct IsReactive<Observer<D>>
{
    static const bool value = true;
};

template <typename D>
struct IsReactive<ScopedObserver<D>>
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

namespace impl
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
    , public IInputNode
{
    using Engine = typename VarNode::Engine;

public:
    template <typename T>
    VarNode( T&& value )
        : VarNode::SignalNode( std::forward<T>( value ) )
        , newValue_( value )
    {
        Engine::OnNodeCreate( *this );
    }

    ~VarNode()
    {
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        REACT_ASSERT( false, "Ticked VarNode\n" );
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

    virtual bool ApplyInput( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        if( isInputAdded_ )
        {
            isInputAdded_ = false;

            if( !Equals( this->value_, newValue_ ) )
            {
                this->value_ = std::move( newValue_ );
                Engine::OnInputChange( *this, turn );
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

            Engine::OnInputChange( *this, turn );
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

    FunctionOp( FunctionOp&& other )
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
    template <typename... TArgs>
    SignalOpNode( TArgs&&... args )
        : SignalOpNode::SignalNode()
        , op_( std::forward<TArgs>( args )... )
    {
        this->value_ = op_.Evaluate();

        Engine::OnNodeCreate( *this );
        op_.template Attach<D>( *this );
    }

    ~SignalOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template Detach<D>( *this );
        }
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
        }
    }

    TOp StealOp()
    {
        REACT_ASSERT( wasOpStolen_ == false, "Op was already stolen." );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *outer_ );
        Engine::OnNodeAttach( *this, *inner_ );
    }

    ~FlattenNode()
    {
        Engine::OnNodeDetach( *this, *inner_ );
        Engine::OnNodeDetach( *this, *outer_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        auto newInner = GetNodePtr( outer_->ValueRef() );

        if( newInner != inner_ )
        {
            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::OnDynamicNodeDetach( *this, *oldInner, turn );
            Engine::OnDynamicNodeAttach( *this, *newInner, turn );

            return;
        }

        if( !Equals( this->value_, inner_->ValueRef() ) )
        {
            this->value_ = inner_->ValueRef();
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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

} // namespace impl

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename S>
class VarSignal;

template <typename D, typename S, typename TOp>
class TempSignal;

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
        std::make_shared<::react::impl::VarNode<D, S>>( std::forward<V>( value ) ) );
}

template <typename D, typename S>
auto MakeVar( std::reference_wrapper<S> value ) -> VarSignal<D, S&>
{
    return VarSignal<D, S&>(
        std::make_shared<::react::impl::VarNode<D, std::reference_wrapper<S>>>( value ) );
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
        std::make_shared<::react::impl::VarNode<D, Signal<D, TInner>>>(
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
        std::make_shared<::react::impl::VarNode<D, Events<D, TInner>>>(
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
    typename TOp = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TValue>>>
auto MakeSignal( const Signal<D, TValue>& arg, FIn&& func ) -> TempSignal<D, S, TOp>
{
    return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(
        std::forward<FIn>( func ), GetNodePtr( arg ) ) );
}

// Multiple args
template <typename D,
    typename... TValues,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename S = typename std::result_of<F( TValues... )>::type,
    typename TOp = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TValues>...>>
auto MakeSignal( const SignalPack<D, TValues...>& argPack, FIn&& func ) -> TempSignal<D, S, TOp>
{
    using ::react::impl::SignalOpNode;

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

    return ::react::impl::apply( NodeBuilder_( std::forward<FIn>( func ) ), argPack.Data );
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
        typename TOp = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TVal>>>    \
    auto operator op( const TSignal& arg )->TempSignal<D, S, TOp>                                  \
    {                                                                                              \
        return TempSignal<D, S, TOp>(                                                              \
            std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>( F(), GetNodePtr( arg ) ) );  \
    }                                                                                              \
                                                                                                   \
    template <typename D,                                                                          \
        typename TVal,                                                                             \
        typename TOpIn,                                                                            \
        typename F = name##OpFunctor<TVal>,                                                        \
        typename S = typename std::result_of<F( TVal )>::type,                                     \
        typename TOp = ::react::impl::FunctionOp<S, F, TOpIn>>                                     \
    auto operator op( TempSignal<D, TVal, TOpIn>&& arg )->TempSignal<D, S, TOp>                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>(                                                              \
            std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>( F(), arg.StealOp() ) );      \
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
        name##OpRFunctor( name##OpRFunctor&& other )                                               \
            : LeftVal( std::move( other.LeftVal ) )                                                \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        name##OpRFunctor( T&& val )                                                                \
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
        name##OpLFunctor( name##OpLFunctor&& other )                                               \
            : RightVal( std::move( other.RightVal ) )                                              \
        {}                                                                                         \
                                                                                                   \
        template <typename T>                                                                      \
        name##OpLFunctor( T&& val )                                                                \
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
        typename TOp = ::react::impl::FunctionOp<S,                                                \
            F,                                                                                     \
            ::react::impl::SignalNodePtrT<D, TLeftVal>,                                            \
            ::react::impl::SignalNodePtrT<D, TRightVal>>>                                          \
    auto operator op( const TLeftSignal& lhs, const TRightSignal& rhs )->TempSignal<D, S, TOp>     \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TLeftVal>>>             \
    auto operator op( const TLeftSignal& lhs, TRightValIn&& rhs )->TempSignal<D, S, TOp>           \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TRightVal>>>            \
    auto operator op( TLeftValIn&& lhs, const TRightSignal& rhs )->TempSignal<D, S, TOp>           \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
            F( std::forward<TLeftValIn>( lhs ) ), GetNodePtr( rhs ) ) );                           \
    }                                                                                              \
    template <typename D,                                                                          \
        typename TLeftVal,                                                                         \
        typename TLeftOp,                                                                          \
        typename TRightVal,                                                                        \
        typename TRightOp,                                                                         \
        typename F = name##OpFunctor<TLeftVal, TRightVal>,                                         \
        typename S = typename std::result_of<F( TLeftVal, TRightVal )>::type,                      \
        typename TOp = ::react::impl::FunctionOp<S, F, TLeftOp, TRightOp>>                         \
    auto operator op(                                                                              \
        TempSignal<D, TLeftVal, TLeftOp>&& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )         \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        typename TOp                                                                               \
        = ::react::impl::FunctionOp<S, F, TLeftOp, ::react::impl::SignalNodePtrT<D, TRightVal>>>   \
    auto operator op( TempSignal<D, TLeftVal, TLeftOp>&& lhs, const TRightSignal& rhs )            \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        typename TOp                                                                               \
        = ::react::impl::FunctionOp<S, F, ::react::impl::SignalNodePtrT<D, TLeftVal>, TRightOp>>   \
    auto operator op( const TLeftSignal& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )           \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        typename TOp = ::react::impl::FunctionOp<S, F, TLeftOp>>                                   \
    auto operator op( TempSignal<D, TLeftVal, TLeftOp>&& lhs, TRightValIn&& rhs )                  \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        typename TOp = ::react::impl::FunctionOp<S, F, TRightOp>>                                  \
    auto operator op( TLeftValIn&& lhs, TempSignal<D, TRightVal, TRightOp>&& rhs )                 \
        ->TempSignal<D, S, TOp>                                                                    \
    {                                                                                              \
        return TempSignal<D, S, TOp>( std::make_shared<::react::impl::SignalOpNode<D, S, TOp>>(    \
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
        std::make_shared<::react::impl::FlattenNode<D, Signal<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal : public ::react::impl::SignalBase<D, S>
{
private:
    using NodeT = ::react::impl::SignalNode<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal( const Signal& ) = default;

    // Move ctor
    Signal( Signal&& other )
        : Signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit Signal( NodePtrT&& nodePtr )
        : Signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Signal& operator=( const Signal& ) = default;

    // Move assignment
    Signal& operator=( Signal&& other )
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
class Signal<D, S&> : public ::react::impl::SignalBase<D, std::reference_wrapper<S>>
{
private:
    using NodeT = ::react::impl::SignalNode<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal( const Signal& ) = default;

    // Move ctor
    Signal( Signal&& other )
        : Signal::SignalBase( std::move( other ) )
    {}

    // Node ctor
    explicit Signal( NodePtrT&& nodePtr )
        : Signal::SignalBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Signal& operator=( const Signal& ) = default;

    // Move assignment
    Signal& operator=( Signal&& other )
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
    using NodeT = ::react::impl::VarNode<D, S>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal( const VarSignal& ) = default;

    // Move ctor
    VarSignal( VarSignal&& other )
        : VarSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit VarSignal( NodePtrT&& nodePtr )
        : VarSignal::Signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    VarSignal& operator=( const VarSignal& ) = default;

    // Move assignment
    VarSignal& operator=( VarSignal&& other )
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
    using NodeT = ::react::impl::VarNode<D, std::reference_wrapper<S>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal( const VarSignal& ) = default;

    // Move ctor
    VarSignal( VarSignal&& other )
        : VarSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit VarSignal( NodePtrT&& nodePtr )
        : VarSignal::Signal( std::move( nodePtr ) )
    {}

    // Copy assignment
    VarSignal& operator=( const VarSignal& ) = default;

    // Move assignment
    VarSignal& operator=( VarSignal&& other )
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
    using NodeT = ::react::impl::SignalOpNode<D, S, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempSignal() = default;

    // Copy ctor
    TempSignal( const TempSignal& ) = default;

    // Move ctor
    TempSignal( TempSignal&& other )
        : TempSignal::Signal( std::move( other ) )
    {}

    // Node ctor
    explicit TempSignal( NodePtrT&& ptr )
        : TempSignal::Signal( std::move( ptr ) )
    {}

    // Copy assignment
    TempSignal& operator=( const TempSignal& ) = default;

    // Move assignemnt
    TempSignal& operator=( TempSignal&& other )
    {
        TempSignal::Signal::operator=( std::move( other ) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move( reinterpret_cast<NodeT*>( this->ptr_.get() )->StealOp() );
    }
};

namespace impl
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
        []( const REACT_MSVC_NO_TYPENAME ::react::impl::Identity<decltype( obj )>::Type::ValueT&   \
                r ) {                                                                              \
            using T = decltype( r.name );                                                          \
            using S = REACT_MSVC_NO_TYPENAME ::react::DecayInput<T>::Type;                         \
            return static_cast<S>( r.name );                                                       \
        } ) )

#define REACTIVE_PTR( obj, name )                                                                  \
    Flatten( MakeSignal( obj,                                                                      \
        []( REACT_MSVC_NO_TYPENAME ::react::impl::Identity<decltype( obj )>::Type::ValueT r ) {    \
            assert( r != nullptr );                                                                \
            using T = decltype( r->name );                                                         \
            using S = REACT_MSVC_NO_TYPENAME ::react::DecayInput<T>::Type;                         \
            return static_cast<S>( r->name );                                                      \
        } ) )

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalNode;

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
        [&] {
            if( curTurnId_ != turn.Id() || forceUpdate )
            {
                curTurnId_ = turn.Id();
                if( !noClear )
                {
                    events_.clear();
                }
            }
        }();
    }

    DataT& Events()
    {
        return events_;
    }

protected:
    DataT events_;

private:
    uint curTurnId_{ ( std::numeric_limits<uint>::max )() };
};

template <typename D, typename E>
using EventStreamNodePtrT = std::shared_ptr<EventStreamNode<D, E>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class EventSourceNode
    : public EventStreamNode<D, E>
    , public IInputNode
{
    using Engine = typename EventSourceNode::Engine;

public:
    EventSourceNode()
        : EventSourceNode::EventStreamNode{}
    {
        Engine::OnNodeCreate( *this );
    }

    ~EventSourceNode()
    {
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        REACT_ASSERT( false, "Ticked EventSourceNode\n" );
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

    virtual bool ApplyInput( void* turnPtr ) override
    {
        if( this->events_.size() > 0 && !changedFlag_ )
        {
            using TurnT = typename D::Engine::TurnT;
            TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

            this->SetCurrentTurn( turn, true, true );
            changedFlag_ = true;
            Engine::OnInputChange( *this, turn );
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

    EventMergeOp( EventMergeOp&& other )
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

    EventFilterOp( EventFilterOp&& other )
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

    EventTransformOp( EventTransformOp&& other )
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
    template <typename... TArgs>
    EventOpNode( TArgs&&... args )
        : EventOpNode::EventStreamNode()
        , op_( std::forward<TArgs>( args )... )
    {
        Engine::OnNodeCreate( *this );
        op_.template Attach<D>( *this );
    }

    ~EventOpNode()
    {
        if( !wasOpStolen_ )
        {
            op_.template Detach<D>( *this );
        }
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        this->SetCurrentTurn( turn, true );

        op_.Collect( turn, EventCollector( this->events_ ) );

        if( !this->events_.empty() )
        {
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
        }
    }

    TOp StealOp()
    {
        REACT_ASSERT( wasOpStolen_ == false, "Op was already stolen." );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *outer_ );
        Engine::OnNodeAttach( *this, *inner_ );
    }

    ~EventFlattenNode()
    {
        Engine::OnNodeDetach( *this, *outer_ );
        Engine::OnNodeDetach( *this, *inner_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        this->SetCurrentTurn( turn, true );
        inner_->SetCurrentTurn( turn );

        auto newInner = GetNodePtr( outer_->ValueRef() );

        if( newInner != inner_ )
        {
            newInner->SetCurrentTurn( turn );

            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::OnDynamicNodeDetach( *this, *oldInner, turn );
            Engine::OnDynamicNodeAttach( *this, *newInner, turn );

            return;
        }

        this->events_.insert(
            this->events_.end(), inner_->Events().begin(), inner_->Events().end() );

        if( this->events_.size() > 0 )
        {
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *source );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedEventTransformNode()
    {
        Engine::OnNodeDetach( *this, *source_ );

        apply( DetachFunctor<D,
                   SyncedEventTransformNode,
                   std::shared_ptr<SignalNode<D, TDepValues>>...>( *this ),
            deps_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *source );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedEventFilterNode()
    {
        Engine::OnNodeDetach( *this, *source_ );

        apply(
            DetachFunctor<D, SyncedEventFilterNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                *this ),
            deps_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *source );
    }

    ~EventProcessingNode()
    {
        Engine::OnNodeDetach( *this, *source_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        this->SetCurrentTurn( turn, true );

        func_( EventRange<TIn>( source_->Events() ), std::back_inserter( this->events_ ) );

        if( !this->events_.empty() )
        {
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *source );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedEventProcessingNode()
    {
        Engine::OnNodeDetach( *this, *source_ );

        apply( DetachFunctor<D,
                   SyncedEventProcessingNode,
                   std::shared_ptr<SignalNode<D, TDepValues>>...>( *this ),
            deps_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *sources ) );
    }

    ~EventJoinNode()
    {
        apply(
            [this]( Slot<TValues>&... slots ) {
                REACT_EXPAND_PACK( Engine::OnNodeDetach( *this, *slots.Source ) );
            },
            slots_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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

} // namespace impl

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E>
class Events;

template <typename D, typename E>
class EventSource;

template <typename D, typename E, typename TOp>
class TempEvents;

enum class Token;

template <typename D, typename S>
class Signal;

template <typename D, typename... TValues>
class SignalPack;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeEventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = Token>
auto MakeEventSource() -> EventSource<D, E>
{
    using ::react::impl::EventSourceNode;

    return EventSource<D, E>( std::make_shared<EventSourceNode<D, E>>() );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D,
    typename TArg1,
    typename... TArgs,
    typename E = TArg1,
    typename TOp = ::react::impl::EventMergeOp<E,
        ::react::impl::EventStreamNodePtrT<D, TArg1>,
        ::react::impl::EventStreamNodePtrT<D, TArgs>...>>
auto Merge( const Events<D, TArg1>& arg1, const Events<D, TArgs>&... args ) -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

    static_assert( sizeof...( TArgs ) > 0, "Merge: 2+ arguments are required." );

    return TempEvents<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( arg1 ), GetNodePtr( args )... ) );
}

template <typename TLeftEvents,
    typename TRightEvents,
    typename D = typename TLeftEvents::DomainT,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename TRightVal = typename TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = ::react::impl::EventMergeOp<E,
        ::react::impl::EventStreamNodePtrT<D, TLeftVal>,
        ::react::impl::EventStreamNodePtrT<D, TRightVal>>,
    class = typename std::enable_if<IsEvent<TLeftEvents>::value>::type,
    class = typename std::enable_if<IsEvent<TRightEvents>::value>::type>
auto operator|( const TLeftEvents& lhs, const TRightEvents& rhs ) -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

    return TempEvents<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( GetNodePtr( lhs ), GetNodePtr( rhs ) ) );
}

template <typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightVal,
    typename TRightOp,
    typename E = TLeftVal,
    typename TOp = ::react::impl::EventMergeOp<E, TLeftOp, TRightOp>>
auto operator|( TempEvents<D, TLeftVal, TLeftOp>&& lhs, TempEvents<D, TRightVal, TRightOp>&& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

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
    = ::react::impl::EventMergeOp<E, TLeftOp, ::react::impl::EventStreamNodePtrT<D, TRightVal>>,
    class = typename std::enable_if<IsEvent<TRightEvents>::value>::type>
auto operator|( TempEvents<D, TLeftVal, TLeftOp>&& lhs, const TRightEvents& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

    return TempEvents<D, E, TOp>(
        std::make_shared<EventOpNode<D, E, TOp>>( lhs.StealOp(), GetNodePtr( rhs ) ) );
}

template <typename TLeftEvents,
    typename D,
    typename TRightVal,
    typename TRightOp,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename E = TLeftVal,
    typename TOp
    = ::react::impl::EventMergeOp<E, ::react::impl::EventStreamNodePtrT<D, TRightVal>, TRightOp>,
    class = typename std::enable_if<IsEvent<TLeftEvents>::value>::type>
auto operator|( const TLeftEvents& lhs, TempEvents<D, TRightVal, TRightOp>&& rhs )
    -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

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
    typename TOp = ::react::impl::EventFilterOp<E, F, ::react::impl::EventStreamNodePtrT<D, E>>>
auto Filter( const Events<D, E>& src, FIn&& filter ) -> TempEvents<D, E, TOp>
{
    using ::react::impl::EventOpNode;

    return TempEvents<D, E, TOp>( std::make_shared<EventOpNode<D, E, TOp>>(
        std::forward<FIn>( filter ), GetNodePtr( src ) ) );
}

template <typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOpOut = ::react::impl::EventFilterOp<E, F, TOpIn>>
auto Filter( TempEvents<D, E, TOpIn>&& src, FIn&& filter ) -> TempEvents<D, E, TOpOut>
{
    using ::react::impl::EventOpNode;

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
    using ::react::impl::SyncedEventFilterNode;

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

    return ::react::impl::apply( NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
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
    = ::react::impl::EventTransformOp<TIn, F, ::react::impl::EventStreamNodePtrT<D, TIn>>>
auto Transform( const Events<D, TIn>& src, FIn&& func ) -> TempEvents<D, TOut, TOp>
{
    using ::react::impl::EventOpNode;

    return TempEvents<D, TOut, TOp>( std::make_shared<EventOpNode<D, TOut, TOp>>(
        std::forward<FIn>( func ), GetNodePtr( src ) ) );
}

template <typename D,
    typename TIn,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F( TIn )>::type,
    typename TOpOut = ::react::impl::EventTransformOp<TIn, F, TOpIn>>
auto Transform( TempEvents<D, TIn, TOpIn>&& src, FIn&& func ) -> TempEvents<D, TOut, TOpOut>
{
    using ::react::impl::EventOpNode;

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
    using ::react::impl::SyncedEventTransformNode;

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

    return ::react::impl::apply( NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process
///////////////////////////////////////////////////////////////////////////////////////////////////
using ::react::impl::EventEmitter;
using ::react::impl::EventRange;

template <typename TOut,
    typename D,
    typename TIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type>
auto Process( const Events<D, TIn>& src, FIn&& func ) -> Events<D, TOut>
{
    using ::react::impl::EventProcessingNode;

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
    using ::react::impl::SyncedEventProcessingNode;

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

    return ::react::impl::apply( NodeBuilder_( source, std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TInnerValue>
auto Flatten( const Signal<D, Events<D, TInnerValue>>& outer ) -> Events<D, TInnerValue>
{
    return Events<D, TInnerValue>(
        std::make_shared<::react::impl::EventFlattenNode<D, Events<D, TInnerValue>, TInnerValue>>(
            GetNodePtr( outer ), GetNodePtr( outer.Value() ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename... TArgs>
auto Join( const Events<D, TArgs>&... args ) -> Events<D, std::tuple<TArgs...>>
{
    using ::react::impl::EventJoinNode;

    static_assert( sizeof...( TArgs ) > 1, "Join: 2+ arguments are required." );

    return Events<D, std::tuple<TArgs...>>(
        std::make_shared<EventJoinNode<D, TArgs...>>( GetNodePtr( args )... ) );
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
class Events : public ::react::impl::EventStreamBase<D, E>
{
private:
    using NodeT = ::react::impl::EventStreamNode<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events( const Events& ) = default;

    // Move ctor
    Events( Events&& other )
        : Events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit Events( NodePtrT&& nodePtr )
        : Events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Events& operator=( const Events& ) = default;

    // Move assignment
    Events& operator=( Events&& other )
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

    template <typename... TArgs>
    auto Merge( TArgs&&... args ) const
        -> decltype( ::react::Merge( std::declval<Events>(), std::forward<TArgs>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<TArgs>( args )... );
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
class Events<D, E&> : public ::react::impl::EventStreamBase<D, std::reference_wrapper<E>>
{
private:
    using NodeT = ::react::impl::EventStreamNode<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events( const Events& ) = default;

    // Move ctor
    Events( Events&& other )
        : Events::EventStreamBase( std::move( other ) )
    {}

    // Node ctor
    explicit Events( NodePtrT&& nodePtr )
        : Events::EventStreamBase( std::move( nodePtr ) )
    {}

    // Copy assignment
    Events& operator=( const Events& ) = default;

    // Move assignment
    Events& operator=( Events&& other )
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

    template <typename... TArgs>
    auto Merge( TArgs&&... args )
        -> decltype( ::react::Merge( std::declval<Events>(), std::forward<TArgs>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<TArgs>( args )... );
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
    using NodeT = ::react::impl::EventSourceNode<D, E>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource( const EventSource& ) = default;

    // Move ctor
    EventSource( EventSource&& other )
        : EventSource::Events( std::move( other ) )
    {}

    // Node ctor
    explicit EventSource( NodePtrT&& nodePtr )
        : EventSource::Events( std::move( nodePtr ) )
    {}

    // Copy assignemnt
    EventSource& operator=( const EventSource& ) = default;

    // Move assignment
    EventSource& operator=( EventSource&& other )
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
    using NodeT = ::react::impl::EventSourceNode<D, std::reference_wrapper<E>>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource( const EventSource& ) = default;

    // Move ctor
    EventSource( EventSource&& other )
        : EventSource::Events( std::move( other ) )
    {}

    // Node ctor
    explicit EventSource( NodePtrT&& nodePtr )
        : EventSource::Events( std::move( nodePtr ) )
    {}

    // Copy assignment
    EventSource& operator=( const EventSource& ) = default;

    // Move assignment
    EventSource& operator=( EventSource&& other )
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
    using NodeT = ::react::impl::EventOpNode<D, E, TOp>;
    using NodePtrT = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempEvents() = default;

    // Copy ctor
    TempEvents( const TempEvents& ) = default;

    // Move ctor
    TempEvents( TempEvents&& other )
        : TempEvents::Events( std::move( other ) )
    {}

    // Node ctor
    explicit TempEvents( NodePtrT&& nodePtr )
        : TempEvents::Events( std::move( nodePtr ) )
    {}

    // Copy assignment
    TempEvents& operator=( const TempEvents& ) = default;

    // Move assignment
    TempEvents& operator=( TempEvents&& other )
    {
        TempEvents::EventStreamBase::operator=( std::move( other ) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move( reinterpret_cast<NodeT*>( this->ptr_.get() )->StealOp() );
    }

    template <typename... TArgs>
    auto Merge( TArgs&&... args )
        -> decltype( ::react::Merge( std::declval<TempEvents>(), std::forward<TArgs>( args )... ) )
    {
        return ::react::Merge( *this, std::forward<TArgs>( args )... );
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

namespace impl
{

template <typename D, typename L, typename R>
bool Equals( const Events<D, L>& lhs, const Events<D, R>& rhs )
{
    return lhs.Equals( rhs );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddIterateRangeWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename S, typename F, typename... TArgs>
struct AddIterateRangeWrapper
{
    AddIterateRangeWrapper( const AddIterateRangeWrapper& other ) = default;

    AddIterateRangeWrapper( AddIterateRangeWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddIterateRangeWrapper>::type>
    explicit AddIterateRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    S operator()( EventRange<E> range, S value, const TArgs&... args )
    {
        for( const auto& e : range )
        {
            value = MyFunc( e, value, args... );
        }

        return value;
    }

    F MyFunc;
};

template <typename E, typename S, typename F, typename... TArgs>
struct AddIterateByRefRangeWrapper
{
    AddIterateByRefRangeWrapper( const AddIterateByRefRangeWrapper& other ) = default;

    AddIterateByRefRangeWrapper( AddIterateByRefRangeWrapper&& other )
        : MyFunc( std::move( other.MyFunc ) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn, AddIterateByRefRangeWrapper>::type>
    explicit AddIterateByRefRangeWrapper( FIn&& func )
        : MyFunc( std::forward<FIn>( func ) )
    {}

    void operator()( EventRange<E> range, S& valueRef, const TArgs&... args )
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *events );
    }

    ~IterateNode()
    {
        Engine::OnNodeDetach( *this, *events_ );
        Engine::OnNodeDestroy( *this );
    }

    void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *events );
    }

    ~IterateByRefNode()
    {
        Engine::OnNodeDetach( *this, *events_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        func_( EventRange<E>( events_->Events() ), this->value_ );

        // Always assume change
        Engine::OnNodePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *events );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedIterateNode()
    {
        Engine::OnNodeDetach( *this, *events_ );

        apply( DetachFunctor<D, SyncedIterateNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                   *this ),
            deps_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *events );
        REACT_EXPAND_PACK( Engine::OnNodeAttach( *this, *deps ) );
    }

    ~SyncedIterateByRefNode()
    {
        Engine::OnNodeDetach( *this, *events_ );

        apply(
            DetachFunctor<D, SyncedIterateByRefNode, std::shared_ptr<SignalNode<D, TDepValues>>...>(
                *this ),
            deps_ );

        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *events_ );
    }

    ~HoldNode()
    {
        Engine::OnNodeDetach( *this, *events_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *target_ );
        Engine::OnNodeAttach( *this, *trigger_ );
    }

    ~SnapshotNode()
    {
        Engine::OnNodeDetach( *this, *target_ );
        Engine::OnNodeDetach( *this, *trigger_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

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
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *target_ );
    }

    ~MonitorNode()
    {
        Engine::OnNodeDetach( *this, *target_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        this->SetCurrentTurn( turn, true );

        this->events_.push_back( target_->ValueRef() );

        if( !this->events_.empty() )
        {
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
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
        Engine::OnNodeCreate( *this );
        Engine::OnNodeAttach( *this, *target_ );
        Engine::OnNodeAttach( *this, *trigger_ );
    }

    ~PulseNode()
    {
        Engine::OnNodeDetach( *this, *target_ );
        Engine::OnNodeDetach( *this, *trigger_ );
        Engine::OnNodeDestroy( *this );
    }

    virtual void Tick( void* turnPtr ) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>( turnPtr );

        this->SetCurrentTurn( turn, true );
        trigger_->SetCurrentTurn( turn );

        for( uint i = 0; i < trigger_->Events().size(); i++ )
        {
            this->events_.push_back( target_->ValueRef() );
        }

        if( !this->events_.empty() )
        {
            Engine::OnNodePulse( *this, turn );
        }
        else
        {
            Engine::OnNodeIdlePulse( *this, turn );
        }
    }

private:
    const std::shared_ptr<SignalNode<D, S>> target_;
    const std::shared_ptr<EventStreamNode<D, E>> trigger_;
};

} // namespace impl

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

template <typename D, typename... TValues>
class SignalPack;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold - Hold the most recent event in a signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename V, typename T = typename std::decay<V>::type>
auto Hold( const Events<D, T>& events, V&& init ) -> Signal<D, T>
{
    using ::react::impl::HoldNode;

    return Signal<D, T>(
        std::make_shared<HoldNode<D, T>>( std::forward<V>( init ), GetNodePtr( events ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Monitor - Emits value changes of target signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
auto Monitor( const Signal<D, S>& target ) -> Events<D, S>
{
    using ::react::impl::MonitorNode;

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
    using ::react::impl::AddIterateByRefRangeWrapper;
    using ::react::impl::AddIterateRangeWrapper;
    using ::react::impl::EventRange;
    using ::react::impl::IsCallableWith;
    using ::react::impl::IterateByRefNode;
    using ::react::impl::IterateNode;

    using F = typename std::decay<FIn>::type;

    using NodeT = typename std::conditional<IsCallableWith<F, S, EventRange<E>, S>::value,
        IterateNode<D, S, E, F>,
        typename std::conditional<IsCallableWith<F, S, E, S>::value,
            IterateNode<D, S, E, AddIterateRangeWrapper<E, S, F>>,
            typename std::conditional<IsCallableWith<F, void, EventRange<E>, S&>::value,
                IterateByRefNode<D, S, E, F>,
                typename std::conditional<IsCallableWith<F, void, E, S&>::value,
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
    using ::react::impl::AddIterateByRefRangeWrapper;
    using ::react::impl::AddIterateRangeWrapper;
    using ::react::impl::EventRange;
    using ::react::impl::IsCallableWith;
    using ::react::impl::SyncedIterateByRefNode;
    using ::react::impl::SyncedIterateNode;

    using F = typename std::decay<FIn>::type;

    using NodeT =
        typename std::conditional<IsCallableWith<F, S, EventRange<E>, S, TDepValues...>::value,
            SyncedIterateNode<D, S, E, F, TDepValues...>,
            typename std::conditional<IsCallableWith<F, S, E, S, TDepValues...>::value,
                SyncedIterateNode<D,
                    S,
                    E,
                    AddIterateRangeWrapper<E, S, F, TDepValues...>,
                    TDepValues...>,
                typename std::conditional<
                    IsCallableWith<F, void, EventRange<E>, S&, TDepValues...>::value,
                    SyncedIterateByRefNode<D, S, E, F, TDepValues...>,
                    typename std::conditional<IsCallableWith<F, void, E, S&, TDepValues...>::value,
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

    return ::react::impl::apply(
        NodeBuilder_( events, std::forward<V>( init ), std::forward<FIn>( func ) ), depPack.Data );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot - Sets signal value to value of other signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto Snapshot( const Events<D, E>& trigger, const Signal<D, S>& target ) -> Signal<D, S>
{
    using ::react::impl::SnapshotNode;

    return Signal<D, S>(
        std::make_shared<SnapshotNode<D, S, E>>( GetNodePtr( target ), GetNodePtr( trigger ) ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S, typename E>
auto Pulse( const Events<D, E>& trigger, const Signal<D, S>& target ) -> Events<D, S>
{
    using ::react::impl::PulseNode;

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

namespace impl
{
namespace toposort
{

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
inline SeqTurn::SeqTurn( TurnIdT id )
    : TurnBase( id )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode, typename TTurn>
void EngineBase<TNode, TTurn>::OnNodeAttach( TNode& node, TNode& parent )
{
    parent.Successors.Add( node );

    if( node.Level <= parent.Level )
    {
        node.Level = parent.Level + 1;
    }
}

template <typename TNode, typename TTurn>
void EngineBase<TNode, TTurn>::OnNodeDetach( TNode& node, TNode& parent )
{
    parent.Successors.Remove( node );
}

template <typename TNode, typename TTurn>
void EngineBase<TNode, TTurn>::OnInputChange( TNode& node, TTurn& turn )
{
    processChildren( node, turn );
}

template <typename TNode, typename TTurn>
void EngineBase<TNode, TTurn>::OnNodePulse( TNode& node, TTurn& turn )
{
    processChildren( node, turn );
}

// Explicit instantiation
template class EngineBase<SeqNode, SeqTurn>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void SeqEngineBase::Propagate( SeqTurn& turn )
{
    while( scheduledNodes_.FetchNext() )
    {
        for( auto* curNode : scheduledNodes_.NextValues() )
        {
            if( curNode->Level < curNode->NewLevel )
            {
                curNode->Level = curNode->NewLevel;
                invalidateSuccessors( *curNode );
                scheduledNodes_.Push( curNode );
                continue;
            }

            curNode->Queued = false;
            curNode->Tick( &turn );
        }
    }
}

inline void SeqEngineBase::OnDynamicNodeAttach( SeqNode& node, SeqNode& parent, SeqTurn& turn )
{
    this->OnNodeAttach( node, parent );

    invalidateSuccessors( node );

    // Re-schedule this node
    node.Queued = true;
    scheduledNodes_.Push( &node );
}

inline void SeqEngineBase::OnDynamicNodeDetach( SeqNode& node, SeqNode& parent, SeqTurn& turn )
{
    this->OnNodeDetach( node, parent );
}

inline void SeqEngineBase::processChildren( SeqNode& node, SeqTurn& turn )
{
    // Add children to queue
    for( auto* succ : node.Successors )
    {
        if( !succ->Queued )
        {
            succ->Queued = true;
            scheduledNodes_.Push( succ );
        }
    }
}

inline void SeqEngineBase::invalidateSuccessors( SeqNode& node )
{
    for( auto* succ : node.Successors )
    {
        if( succ->NewLevel <= node.Level )
        {
            succ->NewLevel = node.Level + 1;
        }
    }
}

} // namespace toposort
} // namespace impl
} // namespace react

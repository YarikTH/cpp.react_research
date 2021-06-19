// #include "react/algorithm.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_ALGORITHM_H_INCLUDED
#define REACT_ALGORITHM_H_INCLUDED



// #include "react/detail/defs.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_DEFS_H_INCLUDED
#define REACT_DETAIL_DEFS_H_INCLUDED



#include <cstddef>

///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_BEGIN     namespace react {
#define REACT_END       }
#define REACT           ::react

#define REACT_IMPL_BEGIN    REACT_BEGIN     namespace impl {
#define REACT_IMPL_END      REACT_END       }
#define REACT_IMPL          REACT           ::impl

/*****************************************/ REACT_BEGIN /*****************************************/

// Type aliases
using uint = unsigned int;
using uchar = unsigned char;
using std::size_t;

/******************************************/ REACT_END /******************************************/

#endif // REACT_DETAIL_DEFS_H_INCLUDED

#include <memory>
#include <type_traits>
#include <utility>

// #include "react/api.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_API_H_INCLUDED
#define REACT_API_H_INCLUDED



#include <type_traits>
#include <vector>

// #include "react/detail/defs.h"

// #include "react/common/utility.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_UTILITY_H_INCLUDED
#define REACT_COMMON_UTILITY_H_INCLUDED



// #include "react/detail/defs.h"


#include <tuple>
#include <type_traits>
#include <utility>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template<size_t N>
struct Apply
{
    template <typename F, typename T, typename ... A>
    static auto apply(F&& f, T&& t, A&& ... a) -> decltype(auto)
    {
        return Apply<N-1>::apply(std::forward<F>(f), std::forward<T>(t), std::get<N-1>(
            std::forward<T>(t)), std::forward<A>(a)...);
    }
};

template<>
struct Apply<0>
{
    template <typename F, typename T, typename ... A>
    static auto apply(F&& f, T&&, A&& ... a) -> decltype(auto)
    {
        return std::forward<F>(f)(std::forward<A>(a) ...);
    }
};

/// Use until C++17 std::apply is available.
template<typename F, typename T>
inline auto apply(F&& f, T&& t) -> decltype(auto)
{
    return Apply<std::tuple_size<typename std::decay<T>::type>::value>::apply(
        std::forward<F>(f), std::forward<T>(t));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper to enable calling a function on each element of an argument pack.
/// We can't do f(args) ...; because ... expands with a comma.
/// But we can do nop_func(f(args) ...);
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename... TArgs>
inline void pass(TArgs&& ...) {}

template <typename T>
bool IsBitmaskSet(T flags, T mask)
{
    return (flags & mask) != (T)0;
}

/****************************************/ REACT_IMPL_END /***************************************/

/// Expand args by wrapping them in a dummy function
/// Use comma operator to replace potential void return value with 0
/// Bware that order of calls is unspecified.
#define REACT_EXPAND_PACK(...) REACT_IMPL::pass((__VA_ARGS__ , 0) ...)

/// Bitmask helpers
#define REACT_DEFINE_BITMASK_OPERATORS(t) \
    inline t operator|(t lhs, t rhs) \
        { return static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) | static_cast<std::underlying_type<t>::type>(rhs)); } \
    inline t operator&(t lhs, t rhs) \
        { return static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) & static_cast<std::underlying_type<t>::type>(rhs)); } \
    inline t operator^(t lhs, t rhs) \
        { return static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) ^ static_cast<std::underlying_type<t>::type>(rhs)); } \
    inline t operator~(t rhs) \
        { return static_cast<t>(~static_cast<std::underlying_type<t>::type>(rhs)); } \
    inline t& operator|=(t& lhs, t rhs) \
        { lhs = static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) | static_cast<std::underlying_type<t>::type>(rhs)); return lhs; } \
    inline t& operator&=(t& lhs, t rhs) \
        { lhs = static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) & static_cast<std::underlying_type<t>::type>(rhs)); return lhs; } \
    inline t& operator^=(t& lhs, t rhs) \
        { lhs = static_cast<t>(static_cast<std::underlying_type<t>::type>(lhs) ^ static_cast<std::underlying_type<t>::type>(rhs)); return lhs; }

#endif // REACT_COMMON_UTILITY_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// API constants
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class WeightHint
{
    automatic,
    light,
    heavy
};

enum class TransactionFlags
{
    none            = 0,
    allow_merging   = 1 << 1,
    sync_linked     = 1 << 2
};

REACT_DEFINE_BITMASK_OPERATORS(TransactionFlags)

enum class Token { value };

enum class InPlaceTag
{
    value = 1
};

static constexpr InPlaceTag in_place = InPlaceTag::value;


///////////////////////////////////////////////////////////////////////////////////////////////////
/// API types
///////////////////////////////////////////////////////////////////////////////////////////////////

// Group
class Group;

// Ref
template <typename T>
using Ref = std::reference_wrapper<const T>;

// State
template <typename S>
class State;

template <typename S>
class StateVar;

template <typename S>
class StateSlot;

template <typename S>
class StateLink;

template <typename S>
using StateRef = State<Ref<S>>;

// Event
enum class Token;

template <typename E = Token>
class Event;

template <typename E = Token>
class EventSource;

template <typename E = Token>
class EventSlot;

template <typename E = Token>
using EventValueList = std::vector<E>;

template <typename E = Token>
using EventValueSink = std::back_insert_iterator<std::vector<E>>;

// Observer
class Observer;

template <typename T>
bool HasChanged(const T& a, const T& b)
    { return !(a == b); }

template <typename T>
bool HasChanged(const Ref<T>& a, const Ref<T>& b)
    { return true; }

template <typename T, typename V>
void ListInsert(T& list, V&& value)
    { list.push_back(std::forward<V>(value)); }

template <typename T, typename V>
void MapInsert(T& map, V&& value)
    { map.insert(std::forward<V>(value)); }

/******************************************/ REACT_END /******************************************/

#endif // REACT_API_H_INCLUDED
// #include "react/state.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_STATE_H_INCLUDED
#define REACT_STATE_H_INCLUDED



// #include "react/detail/defs.h"

// #include "react/api.h"

// #include "react/group.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_GROUP_H_INCLUDED
#define REACT_GROUP_H_INCLUDED



// #include "react/detail/defs.h"


#include <memory>
#include <utility>

// #include "react/api.h"

// #include "react/common/syncpoint.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_SYNCPOINT_H_INCLUDED
#define REACT_COMMON_SYNCPOINT_H_INCLUDED



// #include "react/detail/defs.h"


#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iterator>
#include <mutex>
#include <vector>


/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// A synchronization primitive that is essentially a cooperative semaphore, i.e. it blocks a
/// consumer until all producers are done.
/// Usage:
///     SyncPoint sp;
///     // Add a dependency the sync point should wait for. The depenency is released on destruction.
///     SyncPoint::Depenency dep(sp);
///     // Pass dependency to some operation. Dependencies can be copied as well.
///     SomeAsyncOperation(std::move(dep));
///     // Wait until all dependencies are released.
///     sp.Wait()
///////////////////////////////////////////////////////////////////////////////////////////////////
class SyncPoint
{
public:
    class Dependency;

private:
    class ISyncTarget
    {
    public:
        virtual ~ISyncTarget() = default;

        virtual void IncrementWaitCount() = 0;
        
        virtual void DecrementWaitCount() = 0;
    };

    class SyncPointState : public ISyncTarget
    {
    public:
        virtual void IncrementWaitCount() override
        {// mutex_
            std::lock_guard<std::mutex> scopedLock(mtx_);
            ++waitCount_;
        }// ~mutex_

        virtual void DecrementWaitCount() override
        {// mutex_
            std::lock_guard<std::mutex> scopedLock(mtx_);
            --waitCount_;

            if (waitCount_ == 0)
                cv_.notify_all();
        }// ~mutex_

        void Wait()
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return waitCount_ == 0; });
        }
        
        template <typename TRep, typename TPeriod>
        bool WaitFor(const std::chrono::duration<TRep, TPeriod>& relTime)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            return cv_.wait_for(lock, relTime, [this] { return waitCount_ == 0; });
        }

        template <typename TRep, typename TPeriod>
        bool WaitUntil(const std::chrono::duration<TRep, TPeriod>& relTime)
        {
            std::unique_lock<std::mutex> lock(mtx_);
            return cv_.wait_until(lock, relTime, [this] { return waitCount_ == 0; });
        }

    private:
        std::mutex              mtx_;
        std::condition_variable cv_;

        int waitCount_ = 0;
    };

    class SyncTargetCollection : public ISyncTarget
    {
    public:
        virtual void IncrementWaitCount() override
        {
            for (const auto& e : targets)
                e->IncrementWaitCount();
        }

        virtual void DecrementWaitCount() override
        {
            for (const auto& e : targets)
                e->DecrementWaitCount();
        }

        std::vector<std::shared_ptr<ISyncTarget>> targets;
    };

public:
    /// Creates a sync point.
    SyncPoint() :
        state_( std::make_shared<SyncPointState>() )
    { }

    SyncPoint(const SyncPoint&) = default;

    SyncPoint& operator=(const SyncPoint&) = default;

    SyncPoint(SyncPoint&&) = default;

    SyncPoint& operator=(SyncPoint&&) = default;

    /// Blocks the calling thread until all dependencies of this sync point are released.
    void Wait()
    {
        state_->Wait();
    }

    /// Like Wait, but times out after relTime. Returns false if the timeout was hit.
    template <typename TRep, typename TPeriod>
    bool WaitFor(const std::chrono::duration<TRep, TPeriod>& relTime)
    {
        return state_->WaitFor(relTime);
    }

    /// Like Wait, but times out at relTime. Returns false if the timeout was hit.
    template <typename TRep, typename TPeriod>
    bool WaitUntil(const std::chrono::duration<TRep, TPeriod>& relTime)
    {
        return state_->WaitUntil(relTime);
    }

    /// A RAII-style token object that represents a dependency of a SyncPoint.
    class Dependency
    {
    public:
        /// Constructs an unbound dependency.
        Dependency() = default;

        /// Constructs a single dependency for a sync point.
        explicit Dependency(const SyncPoint& sp) :
            target_( sp.state_ )
        {
            target_->IncrementWaitCount();
        }

        /// Merges an input range of other dependencies into a single dependency.
        /// This allows to create APIs that are agnostic of how many dependent operations they process.        
        template <typename TBegin, typename TEnd>
        Dependency(TBegin first, TEnd last)
        {
            auto count = std::distance(first, last);

            if (count == 1)
            {
                target_ = first->target_;
                if (target_)
                    target_->IncrementWaitCount();
            }
            else if (count > 1)
            {
                auto collection = std::make_shared<SyncTargetCollection>();
                collection->targets.reserve(count);
            
                for (; !(first == last); ++first)
                    if (first->target_) // There's no point in propagating released/empty dependencies.
                        collection->targets.push_back(first->target_);

                collection->IncrementWaitCount();

                target_ = std::move(collection);
            }
        }

        /// Copy constructor and assignment split a dependency.
        /// The new dependency that has the same target(s) as other.
        Dependency(const Dependency& other) :
            target_( other.target_ )
        {
            if (target_)
                target_->IncrementWaitCount();
        }

        Dependency& operator=(const Dependency& other)
        {
            if (other.target_)
                other.target_->IncrementWaitCount();

            if (target_)
                target_->DecrementWaitCount();

            target_ = other.target_;
            return *this;
        }

        /// Move constructor and assignment transfer a dependency.
        /// The moved from object is left unbound.
        Dependency(Dependency&& other) :
            target_( std::move(other.target_) )
        { }

        Dependency& operator=(Dependency&& other)
        {
            if (target_)
                target_->DecrementWaitCount();

            target_ = std::move(other.target_);
            return *this;
        }

        /// The destructor releases a dependency, if it's not unbound.
        ~Dependency()
        {
            if (target_)
                target_->DecrementWaitCount();
        }

        /// Manually releases the dependency. Afterwards it is unbound.
        void Release()
        {
            if (target_)
            {
                target_->DecrementWaitCount();
                target_ = nullptr;
            }
        }

        /// Returns if a dependency is released, i.e. if it is unbound.
        bool IsReleased() const
            { return target_ == nullptr; }

    private:
        std::shared_ptr<ISyncTarget> target_;
    };

private:
    std::shared_ptr<SyncPointState> state_;
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_COMMON_SYNCPOINT_H_INCLUDED

// #include "react/detail/graph_interface.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED
#define REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED



// #include "react/detail/defs.h"


#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

// #include "react/api.h"

// #include "react/common/utility.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Definitions
///////////////////////////////////////////////////////////////////////////////////////////////////
using NodeId = size_t;
using TurnId = size_t;
using LinkId = size_t;

static NodeId invalid_node_id = (std::numeric_limits<size_t>::max)();
static TurnId invalid_turn_id = (std::numeric_limits<size_t>::max)();
static LinkId invalid_link_id = (std::numeric_limits<size_t>::max)();

enum class UpdateResult
{
    unchanged,
    changed,
    shifted
};

enum class NodeCategory
{
    normal,
    input,
    dyninput,
    output,
    linkoutput
};

class ReactGraph;
struct IReactNode;

using LinkOutputMap = std::unordered_map<ReactGraph*, std::vector<std::function<void()>>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IReactNode
{
    virtual ~IReactNode() = default;

    virtual UpdateResult Update(TurnId turnId) noexcept = 0;
    
    virtual void Clear() noexcept
        { }

    virtual void CollectOutput(LinkOutputMap& output)
        { }
};



/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_INTERFACE_H_INCLUDED
// #include "react/detail/graph_impl.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_IMPL_H_INCLUDED
#define REACT_DETAIL_GRAPH_IMPL_H_INCLUDED



// #include "react/detail/defs.h"


#include <algorithm>
#include <atomic>
#include <type_traits>
#include <utility>
#include <vector>
#include <map>
#include <mutex>

#include <tbb/concurrent_queue.h>
#include <tbb/task.h>

// #include "react/common/ptrcache.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_PTR_CACHE_H_INCLUDED
#define REACT_COMMON_PTR_CACHE_H_INCLUDED



// #include "react/detail/defs.h"


#include <memory>
#include <mutex>
#include <unordered_map>

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// A cache to objects of type shared_ptr<V> that stores weak pointers.
/// Thread-safe.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename K, typename V>
class WeakPtrCache
{
public:
    /// Returns a shared pointer to an object that existings in the cache, indexed by key.
    /// If no hit was found, createFunc is used to construct the object managed by shared pointer.
    /// A weak pointer to the result is stored in the cache.
    template <typename F>
    std::shared_ptr<V> LookupOrCreate(const K& key, F&& createFunc)
    {
        std::lock_guard<std::mutex> scopedLock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end())
        {
            // Lock fails, if the object was cached before, but has been released already.
            // In that case we re-create it.
            if (auto ptr = it->second.lock())
            {
                return ptr;
            }
        }

        std::shared_ptr<V> v = createFunc();
        auto res = map_.emplace(key, v);
        return v;
    }

    /// Removes an entry from the cache.
    void Erase(const K& key)
    {
        std::lock_guard<std::mutex> scopedLock(mutex_);

        auto it = map_.find(key);
        if (it != map_.end())
        {
            map_.erase(it);
        }
    }

private:
    std::mutex mutex_;
    std::unordered_map<K, std::weak_ptr<V>> map_;
};


/******************************************/ REACT_END /******************************************/

#endif // REACT_COMMON_PTR_CACHE_H_INCLUDED
// #include "react/common/slotmap.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_SLOTMAP_H_INCLUDED
#define REACT_COMMON_SLOTMAP_H_INCLUDED



// #include "react/detail/defs.h"


#include <algorithm>
#include <array>
#include <iterator>
#include <memory>
#include <type_traits>

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// A simple slot map.
/// Insert returns the slot index, which stays valid until the element is erased.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
class SlotMap
{
    static const size_t initial_capacity = 8;
    static const size_t grow_factor = 2;

    using StorageType = typename std::aligned_storage<sizeof(T), alignof(T)>::type;

public:
    using ValueType = T;

    SlotMap() = default;

    SlotMap(SlotMap&&) = default;
    SlotMap& operator=(SlotMap&&) = default;

    SlotMap(const SlotMap&) = delete;
    SlotMap& operator=(const SlotMap&) = delete;

    ~SlotMap()
        { Reset(); }

    T& operator[](size_t index)
        { return reinterpret_cast<T&>(data_[index]); }

    const T& operator[](size_t index) const
        { return reinterpret_cast<T&>(data_[index]); }

    size_t Insert(T value)
    {
        if (IsAtFullCapacity())
        {
            Grow();
            return InsertAtBack(std::move(value));
        }
        else if (HasFreeIndices())
        {
            return InsertAtFreeIndex(std::move(value));
        }
        else
        {
            return InsertAtBack(std::move(value));
        }
    }

    void Erase(size_t index)
    {
        // If we erased something other than the last element, save in free index list.
        if (index != (size_ - 1))
        {
            freeIndices_[freeSize_++] = index;
        }

        reinterpret_cast<T&>(data_[index]).~T();
        --size_;
    }

    void Clear()
    {
        // Sort free indexes so we can remove check for them in linear time.
        std::sort(&freeIndices_[0], &freeIndices_[freeSize_]);
        
        const size_t totalSize = size_ + freeSize_;
        size_t index = 0;

        // Skip over free indices.
        for (size_t j = 0; j < freeSize_; ++j)
        {
            size_t freeIndex = freeIndices_[j];

            for (; index < totalSize; ++index)
            {
                if (index == freeIndex)
                {
                    ++index;
                    break;
                }
                else
                {
                    reinterpret_cast<T&>(data_[index]).~T();
                }
            }
        }

        // Rest
        for (; index < totalSize; ++index)
            reinterpret_cast<T&>(data_[index]).~T();

        size_ = 0;
        freeSize_ = 0;
    }

    void Reset()
    {
        Clear();

        data_.reset();
        freeIndices_.reset();

        capacity_ = 0;
    }

private:
    T& GetDataAt(size_t index)
        { return reinterpret_cast<T&>(data_[index]); }

    T& GetDataAt(size_t index) const
        { return reinterpret_cast<T&>(data_[index]); }

    bool IsAtFullCapacity() const
        { return capacity_ == size_; }

    bool HasFreeIndices() const
        { return freeSize_ > 0; }

    size_t CalcNextCapacity() const
        { return capacity_ == 0? initial_capacity : capacity_ * grow_factor;  }

    void Grow()
    {
        // Allocate new storage
        size_t  newCapacity = CalcNextCapacity();
        
        std::unique_ptr<StorageType[]> newData{ new StorageType[newCapacity] };
        std::unique_ptr<size_t[]> newFreeIndices{ new size_t[newCapacity] };

        // Move data to new storage
        for (size_t i = 0; i < capacity_; ++i)
        {
            new (reinterpret_cast<T*>(&newData[i])) T{ std::move(reinterpret_cast<T&>(data_[i])) };
            reinterpret_cast<T&>(data_[i]).~T();
        }

        // Free list is empty if we are at max capacity anyway

        // Use new storage
        data_           = std::move(newData);
        freeIndices_    = std::move(newFreeIndices);
        capacity_       = newCapacity;
    }

    size_t InsertAtBack(T&& value)
    {
        new (&data_[size_]) T(std::move(value));
        return size_++;
    }

    size_t InsertAtFreeIndex(T&& value)
    {
        size_t nextFreeIndex = freeIndices_[--freeSize_];
        new (&data_[nextFreeIndex]) T(std::move(value));
        ++size_;

        return nextFreeIndex;
    }

    std::unique_ptr<StorageType[]>  data_;
    std::unique_ptr<size_t[]>       freeIndices_;

    size_t  size_       = 0;
    size_t  freeSize_   = 0;
    size_t  capacity_   = 0;
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_COMMON_SLOTMAP_H_INCLUDED
// #include "react/common/syncpoint.h"

// #include "react/detail/graph_interface.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

class ReactGraph;

class TransactionQueue
{
public:
    TransactionQueue(ReactGraph& graph) :
        graph_( graph )
    { }

    template <typename F>
    void Push(F&& func, SyncPoint::Dependency dep, TransactionFlags flags)
    {
        transactions_.push(StoredTransaction{ std::forward<F>(func), std::move(dep), flags });

        if (count_.fetch_add(1, std::memory_order_release) == 0)
            tbb::task::enqueue(*new(tbb::task::allocate_root()) WorkerTask(*this));
    }

private:
    struct StoredTransaction
    {
        std::function<void()>   func;
        SyncPoint::Dependency   dep;
        TransactionFlags        flags;
    };

    class WorkerTask : public tbb::task
    {
    public:
        WorkerTask(TransactionQueue& parent) :
            parent_( parent )
        { }

        tbb::task* execute()
        {
            parent_.ProcessQueue();
            return nullptr;
        }

    private:
        TransactionQueue& parent_;
    };

    void ProcessQueue();

    size_t ProcessNextBatch();

    tbb::concurrent_queue<StoredTransaction> transactions_;

    std::atomic<size_t> count_{ 0 };

    ReactGraph& graph_;
};

class ReactGraph
{
public:
    using LinkCache = WeakPtrCache<void*, IReactNode>;

    NodeId RegisterNode(IReactNode* nodePtr, NodeCategory category);
    void UnregisterNode(NodeId nodeId);

    void AttachNode(NodeId node, NodeId parentId);
    void DetachNode(NodeId node, NodeId parentId);

    template <typename F>
    void PushInput(NodeId nodeId, F&& inputCallback);

    void AddSyncPointDependency(SyncPoint::Dependency dep, bool syncLinked);

    void AllowLinkedTransactionMerging(bool allowMerging);

    template <typename F>
    void DoTransaction(F&& transactionCallback);

    template <typename F>
    void EnqueueTransaction(F&& func, SyncPoint::Dependency dep, TransactionFlags flags);
    
    LinkCache& GetLinkCache()
        { return linkCache_; }

private:
    struct NodeData
    {
        NodeData() = default;

        NodeData(const NodeData&) = default;
        NodeData& operator=(const NodeData&) = default;

        NodeData(IReactNode* nodePtrIn, NodeCategory categoryIn) :
            nodePtr( nodePtrIn ),
            category(categoryIn)
        { }

        NodeCategory category = NodeCategory::normal;

        int     level       = 0;
        int     newLevel    = 0 ;
        bool    queued      = false;

        IReactNode*  nodePtr = nullptr;

        std::vector<NodeId> successors;
    };

    class TopoQueue
    {
    public:
        void Push(NodeId nodeId, int level)
            { queueData_.emplace_back(nodeId, level); }

        bool FetchNext();

        const std::vector<NodeId>& Next() const
            { return nextData_; }

        bool IsEmpty() const
            { return queueData_.empty(); }

    private:
        using Entry = std::pair<NodeId /*nodeId*/, int /*level*/>;

        std::vector<Entry>  queueData_;
        std::vector<NodeId> nextData_;

        int minLevel_ = (std::numeric_limits<int>::max)();
    };

    void Propagate();
    void UpdateLinkNodes();

    void ScheduleSuccessors(NodeData & node);
    void RecalculateSuccessorLevels(NodeData & node);

private:
    TransactionQueue    transactionQueue_{ *this };

    SlotMap<NodeData>   nodeData_;

    TopoQueue scheduledNodes_;

    std::vector<NodeId>         changedInputs_;
    std::vector<IReactNode*>    changedNodes_;

    LinkOutputMap scheduledLinkOutputs_;

    std::vector<SyncPoint::Dependency> localDependencies_;
    std::vector<SyncPoint::Dependency> linkDependencies_;

    LinkCache linkCache_;

    int  transactionLevel_ = 0;
    bool allowLinkedTransactionMerging_ = false;
};

template <typename F>
void ReactGraph::PushInput(NodeId nodeId, F&& inputCallback)
{
    auto& node = nodeData_[nodeId];
    auto* nodePtr = node.nodePtr;

    // This writes to the input buffer of the respective node.
    std::forward<F>(inputCallback)();
    
    changedInputs_.push_back(nodeId);

    if (transactionLevel_ == 0)
        Propagate();
}

template <typename F>
void ReactGraph::DoTransaction(F&& transactionCallback)
{
    // Transaction callback may add multiple inputs.
    ++transactionLevel_;
    std::forward<F>(transactionCallback)();
    --transactionLevel_;

    Propagate();
}

template <typename F>
void ReactGraph::EnqueueTransaction(F&& func, SyncPoint::Dependency dep, TransactionFlags flags)
{
    transactionQueue_.Push(std::forward<F>(func), std::move(dep), flags);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GroupInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
class GroupInternals
{
public:
    GroupInternals() :
        graphPtr_( std::make_shared<ReactGraph>() )
    {  }

    GroupInternals(const GroupInternals&) = default;
    GroupInternals& operator=(const GroupInternals&) = default;

    GroupInternals(GroupInternals&&) = default;
    GroupInternals& operator=(GroupInternals&&) = default;

    auto GetGraphPtr() -> std::shared_ptr<ReactGraph>&
        { return graphPtr_; }

    auto GetGraphPtr() const -> const std::shared_ptr<ReactGraph>&
        { return graphPtr_; }

private:
    std::shared_ptr<ReactGraph> graphPtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_IMPL_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Group
///////////////////////////////////////////////////////////////////////////////////////////////////
class Group : protected REACT_IMPL::GroupInternals
{
public:
    Group() = default;

    Group(const Group&) = default;
    Group& operator=(const Group&) = default;

    Group(Group&&) = default;
    Group& operator=(Group&&) = default;

    template <typename F>
    void DoTransaction(F&& func)
        { GetGraphPtr()->DoTransaction(std::forward<F>(func)); }

    template <typename F>
    void EnqueueTransaction(F&& func, TransactionFlags flags = TransactionFlags::none)
        { GetGraphPtr()->EnqueueTransaction(std::forward<F>(func), SyncPoint::Dependency{ }, flags); }

    template <typename F>
    void EnqueueTransaction(F&& func, const SyncPoint& syncPoint, TransactionFlags flags = TransactionFlags::none)
        { GetGraphPtr()->EnqueueTransaction(std::forward<F>(func), SyncPoint::Dependency{ syncPoint }, flags); }

    friend bool operator==(const Group& a, const Group& b)
        { return a.GetGraphPtr() == b.GetGraphPtr(); }

    friend bool operator!=(const Group& a, const Group& b)
        { return !(a == b); }

    friend auto GetInternals(Group& g) -> REACT_IMPL::GroupInternals&
        { return g; }

    friend auto GetInternals(const Group& g) -> const REACT_IMPL::GroupInternals&
        { return g; }
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_GROUP_H_INCLUDED
// #include "react/common/ptrcache.h"

// #include "react/detail/state_nodes.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)



#ifndef REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED

// #include "react/detail/defs.h"


#include <memory>
#include <queue>
#include <utility>
#include <vector>

// #include "node_base.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_NODE_BASE_H_INCLUDED
#define REACT_DETAIL_NODE_BASE_H_INCLUDED



// #include "react/detail/defs.h"


#include <iterator>
#include <memory>
#include <utility>

// #include "react/common/utility.h"

// #include "react/detail/graph_interface.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

class ReactGraph;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CreateWrappedNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename RET, typename NODE, typename ... ARGS>
static RET CreateWrappedNode(ARGS&& ... args)
{
    auto node = std::make_shared<NODE>(std::forward<ARGS>(args) ...);
    return RET(std::move(node));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeBase : public IReactNode
{
public:
    NodeBase(const Group& group) :
        group_( group )
    { }
    
    NodeBase(const NodeBase&) = delete;
    NodeBase& operator=(const NodeBase&) = delete;

    NodeBase(NodeBase&&) = delete;
    NodeBase& operator=(NodeBase&&) = delete;

    /*void SetWeightHint(WeightHint weight)
    {
        switch (weight)
        {
        case WeightHint::heavy :
            this->ForceUpdateThresholdExceeded(true);
            break;
        case WeightHint::light :
            this->ForceUpdateThresholdExceeded(false);
            break;
        case WeightHint::automatic :
            this->ResetUpdateThreshold();
            break;
        }
    }*/

    NodeId GetNodeId() const
        { return nodeId_; }

    auto GetGroup() const -> const Group&
        { return group_; }

    auto GetGroup() -> Group&
        { return group_; }

protected:
    auto GetGraphPtr() const -> const std::shared_ptr<ReactGraph>&
        { return GetInternals(group_).GetGraphPtr(); }

    auto GetGraphPtr() -> std::shared_ptr<ReactGraph>&
        { return GetInternals(group_).GetGraphPtr(); }

    void RegisterMe(NodeCategory category = NodeCategory::normal)
        { nodeId_ = GetGraphPtr()->RegisterNode(this, category); }
    
    void UnregisterMe()
        { GetGraphPtr()->UnregisterNode(nodeId_); }

    void AttachToMe(NodeId otherNodeId)
        { GetGraphPtr()->AttachNode(nodeId_, otherNodeId); }

    void DetachFromMe(NodeId otherNodeId)
        { GetGraphPtr()->DetachNode(nodeId_, otherNodeId); }

private:
    NodeId nodeId_;

    Group group_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_NODE_BASE_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateNode : public NodeBase
{
public:
    explicit StateNode(const Group& group) :
        StateNode::NodeBase( group ),
        value_( )
    { }

    template <typename T>
    StateNode(const Group& group, T&& value) :
        StateNode::NodeBase( group ),
        value_( std::forward<T>(value) )
    { }

    template <typename ... Ts>
    StateNode(InPlaceTag, const Group& group, Ts&& ... args) :
        StateNode::NodeBase( group ),
        value_( std::forward<Ts>(args) ... )
    { }

    S& Value()
        { return value_; }

    const S& Value() const
        { return value_; }

private:
    S value_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateVarNode : public StateNode<S>
{
public:
    explicit StateVarNode(const Group& group) :
        StateVarNode::StateNode( group ),
        newValue_( )
    {
        this->RegisterMe(NodeCategory::input);
    }

    template <typename T>
    StateVarNode(const Group& group, T&& value) :
        StateVarNode::StateNode( group, std::forward<T>(value) ),
        newValue_( value )
    {
        this->RegisterMe();
    }

    ~StateVarNode()
    {
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        if (isInputAdded_)
        {
            isInputAdded_ = false;

            if (HasChanged(this->Value(), newValue_))
            {
                this->Value() = std::move(newValue_);
                return UpdateResult::changed;
            }
            else
            {
                return UpdateResult::unchanged;
            }
        }
        else if (isInputModified_)
        {            
            isInputModified_ = false;
            return UpdateResult::changed;
        }

        else
        {
            return UpdateResult::unchanged;
        }
    }

    template <typename T>
    void SetValue(T&& newValue)
    {
        newValue_ = std::forward<T>(newValue);

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    template <typename F>
    void ModifyValue(F&& func)
    {
        // There hasn't been any Set(...) input yet, modify.
        if (! isInputAdded_)
        {
            func(this->Value());

            isInputModified_ = true;
        }
        // There's a newValue, modify newValue instead.
        // The modified newValue will handled like before, i.e. it'll be compared to value_
        // in ApplyInput
        else
        {
            func(newValue_);
        }
    }

private:
    S       newValue_;
    bool    isInputAdded_ = false;
    bool    isInputModified_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateFuncNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename ... TDeps>
class StateFuncNode : public StateNode<S>
{
public:
    template <typename FIn>
    StateFuncNode(const Group& group, FIn&& func, const State<TDeps>& ... deps) :
        StateFuncNode::StateNode( group, func(GetInternals(deps).Value() ...) ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        this->RegisterMe();
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(deps).GetNodeId()));
    }

    ~StateFuncNode()
    {
        react::impl::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(deps).GetNodePtr()->GetNodeId())); }, depHolder_);
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        S newValue = react::impl::apply([this] (const auto& ... deps)
            { return this->func_(GetInternals(deps).Value() ...); }, depHolder_);

        if (HasChanged(this->Value(), newValue))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
        }
    }

private:
    F func_;
    std::tuple<State<TDeps> ...> depHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateSlotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateSlotNode : public StateNode<S>
{
public:
    StateSlotNode(const Group& group, const State<S>& dep) :
        StateSlotNode::StateNode( group, GetInternals(dep).Value() ),
        input_( dep )
    {
        inputNodeId_ = this->GetGraphPtr()->RegisterNode(&slotInput_, NodeCategory::dyninput);
        
        this->RegisterMe();
        this->AttachToMe(inputNodeId_);
        this->AttachToMe(GetInternals(dep).GetNodeId());
    }

    ~StateSlotNode()
    {
        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->DetachFromMe(inputNodeId_);
        this->UnregisterMe();

        this->GetGraphPtr()->UnregisterNode(inputNodeId_);
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        if (! (this->Value() == GetInternals(input_).Value()))
        {
            this->Value() = GetInternals(input_).Value();
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
        }
    }

    void SetInput(const State<S>& newInput)
    {
        if (newInput == input_)
            return;

        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->AttachToMe(GetInternals(newInput).GetNodeId());

        input_ = newInput;
    }

    NodeId GetInputNodeId() const
        { return inputNodeId_; }

private:        
    struct VirtualInputNode : public IReactNode
    {
        virtual UpdateResult Update(TurnId turnId) noexcept override
            { return UpdateResult::changed; }
    };

    State<S>            input_;
    NodeId              inputNodeId_;
    VirtualInputNode    slotInput_;
    
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateLinkNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateLinkNode : public StateNode<S>
{
public:
    StateLinkNode(const Group& group, const State<S>& dep) :
        StateLinkNode::StateNode( group, GetInternals(dep).Value() ),
        dep_ ( dep ),
        srcGroup_( dep.GetGroup() )
    {
        this->RegisterMe(NodeCategory::input);

        auto& srcGraphPtr = GetInternals(srcGroup_).GetGraphPtr();
        outputNodeId_ = srcGraphPtr->RegisterNode(&linkOutput_, NodeCategory::linkoutput);
        
        srcGraphPtr->AttachNode(outputNodeId_, GetInternals(dep).GetNodeId());
    }

    ~StateLinkNode()
    {
        auto& srcGraphPtr = GetInternals(srcGroup_).GetGraphPtr();
        srcGraphPtr->DetachNode(outputNodeId_, GetInternals(dep_).GetNodeId());
        srcGraphPtr->UnregisterNode(outputNodeId_);

        auto& linkCache = this->GetGraphPtr()->GetLinkCache();
        linkCache.Erase(this);

        this->UnregisterMe();
    }

    void SetWeakSelfPtr(const std::weak_ptr<StateLinkNode>& self)
        { linkOutput_.parent = self; }

    virtual UpdateResult Update(TurnId turnId) noexcept override
        { return UpdateResult::changed; }

    void SetValue(S&& newValue)
        { this->Value() = std::move(newValue); }

private:
    struct VirtualOutputNode : public IReactNode
    {
        virtual UpdateResult Update(TurnId turnId) noexcept override
            { return UpdateResult::changed; }

        virtual void CollectOutput(LinkOutputMap& output) override
        {
            if (auto p = parent.lock())
            {
                auto* rawPtr = p->GetGraphPtr().get();
                output[rawPtr].push_back([storedParent = std::move(p), storedValue = GetInternals(p->dep_).Value()] () mutable -> void
                    {
                        NodeId nodeId = storedParent->GetNodeId();
                        auto& graphPtr = storedParent->GetGraphPtr();

                        graphPtr->PushInput(nodeId, [&storedParent, &storedValue]
                            {
                                storedParent->SetValue(std::move(storedValue));
                            });
                    });
            }
        }

        std::weak_ptr<StateLinkNode> parent;
    };

    State<S>    dep_;
    Group       srcGroup_;
    NodeId      outputNodeId_;

    VirtualOutputNode linkOutput_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateInternals
{
public:
    StateInternals() = default;

    StateInternals(const StateInternals&) = default;
    StateInternals& operator=(const StateInternals&) = default;

    StateInternals(StateInternals&&) = default;
    StateInternals& operator=(StateInternals&&) = default;

    explicit StateInternals(std::shared_ptr<StateNode<S>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto GetNodePtr() -> std::shared_ptr<StateNode<S>>&
        { return nodePtr_; }

    auto GetNodePtr() const -> const std::shared_ptr<StateNode<S>>&
        { return nodePtr_; }

    NodeId GetNodeId() const
        { return nodePtr_->GetNodeId(); }

    S& Value()
        { return nodePtr_->Value(); }

    const S& Value() const
        { return nodePtr_->Value(); }

private:
    std::shared_ptr<StateNode<S>> nodePtr_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateRefNode : public StateNode<Ref<S>>
{
public:
    StateRefNode(const Group& group, const State<S>& input) :
        StateRefNode::StateNode( group, std::cref(GetInternals(input).Value()) ),
        input_( input )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(input).GetNodeId());
    }

    ~StateRefNode()
    {
        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        this->Value() = std::cref(GetInternals(input_).Value());
        return UpdateResult::changed;
    }

private:
    State<S> input_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SameGroupOrLink
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
static State<S> SameGroupOrLink(const Group& targetGroup, const State<S>& dep)
{
    if (dep.GetGroup() == targetGroup)
        return dep;
    else
        return StateLink<S>::Create(targetGroup, dep);
}

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_STATE_NODES_H_INCLUDED

#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// State
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class State : protected REACT_IMPL::StateInternals<S>
{
public:
    // Construct with explicit group
    template <typename F, typename T1, typename ... Ts>
    static State Create(const Group& group, F&& func, const State<T1>& dep1, const State<Ts>& ... deps)
        { return State(CreateFuncNode(group, std::forward<F>(func), dep1, deps ...)); }

    // Construct with implicit group
    template <typename F, typename T1, typename ... Ts>
    static State Create(F&& func, const State<T1>& dep1, const State<Ts>& ... deps)
        { return State(CreateFuncNode(dep1.GetGroup(), std::forward<F>(func), dep1, deps ...)); }

    // Construct with constant value
    template <typename T>
    static State Create(const Group& group, T&& init)
        { return State(CreateFuncNode(group, [value = std::move(init)] { return value; })); }

    State() = default;

    State(const State&) = default;
    State& operator=(const State&) = default;

    State(State&&) = default;
    State& operator=(State&&) = default;

    auto GetGroup() const -> const Group&
        { return this->GetNodePtr()->GetGroup(); }

    auto GetGroup() -> Group&
        { return this->GetNodePtr()->GetGroup(); }

    friend bool operator==(const State<S>& a, const State<S>& b)
        { return a.GetNodePtr() == b.GetNodePtr(); }

    friend bool operator!=(const State<S>& a, const State<S>& b)
        { return !(a == b); }

    friend auto GetInternals(State<S>& s) -> REACT_IMPL::StateInternals<S>&
        { return s; }

    friend auto GetInternals(const State<S>& s) -> const REACT_IMPL::StateInternals<S>&
        { return s; }

protected:
    State(std::shared_ptr<REACT_IMPL::StateNode<S>>&& nodePtr) :
        State::StateInternals( std::move(nodePtr) )
    { }

private:
    template <typename F, typename T1, typename ... Ts>
    static auto CreateFuncNode(const Group& group, F&& func, const State<T1>& dep1, const State<Ts>& ... deps) -> decltype(auto)
    {
        using REACT_IMPL::StateFuncNode;
        using REACT_IMPL::SameGroupOrLink;

        return std::make_shared<StateFuncNode<S, typename std::decay<F>::type, T1, Ts ...>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep1), SameGroupOrLink(group, deps) ...);
    }

    template <typename RET, typename NODE, typename ... ARGS>
    friend RET impl::CreateWrappedNode(ARGS&& ... args);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateVar
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateVar : public State<S>
{
public:
    // Construct with group + default
    static StateVar Create(const Group& group)
        { return StateVar(CreateVarNode(group)); }

    // Construct with group + value
    template <typename T>
    static StateVar Create(const Group& group, T&& value)
        { return StateVar(CreateVarNode(group, std::forward<T>(value))); }

    StateVar() = default;

    StateVar(const StateVar&) = default;
    StateVar& operator=(const StateVar&) = default;

    StateVar(StateVar&&) = default;
    StateVar& operator=(StateVar&&) = default;

    void Set(const S& newValue)
        { SetValue(newValue); }

    void Set(S&& newValue)
        { SetValue(std::move(newValue)); }

    template <typename F>
    void Modify(const F& func)
        { ModifyValue(func); }

    friend bool operator==(const StateVar<S>& a, StateVar<S>& b)
        { return a.GetNodePtr() == b.GetNodePtr(); }

    friend bool operator!=(const StateVar<S>& a, StateVar<S>& b)
        { return !(a == b); }

    S* operator->()
        { return &this->Value(); }

protected:
    StateVar(std::shared_ptr<REACT_IMPL::StateNode<S>>&& nodePtr) :
        StateVar::State( std::move(nodePtr) )
    { }

private:
    static auto CreateVarNode(const Group& group) -> decltype(auto)
    {
        using REACT_IMPL::StateVarNode;
        return std::make_shared<StateVarNode<S>>(group);
    }

    template <typename T>
    static auto CreateVarNode(const Group& group, T&& value) -> decltype(auto)
    {
        using REACT_IMPL::StateVarNode;
        return std::make_shared<StateVarNode<S>>(group, std::forward<T>(value));
    }

    template <typename T>
    void SetValue(T&& newValue)
    {
        using REACT_IMPL::NodeId;
        using VarNodeType = REACT_IMPL::StateVarNode<S>;

        VarNodeType* castedPtr = static_cast<VarNodeType*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [castedPtr, &newValue] { castedPtr->SetValue(std::forward<T>(newValue)); });
    }

    template <typename F>
    void ModifyValue(const F& func)
    {
        using REACT_IMPL::NodeId;
        using VarNodeType = REACT_IMPL::StateVarNode<S>;

        VarNodeType* castedPtr = static_cast<VarNodeType*>(this->GetNodePtr().get());
        
        NodeId nodeId = castedPtr->GetNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [castedPtr, &func] { castedPtr->ModifyValue(func); });
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateSlotBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateSlot : public State<S>
{
public:
    // Construct with explicit group
    static StateSlot Create(const Group& group, const State<S>& input)
        { return StateSlot(CreateSlotNode(group, input)); }

    // Construct with implicit group
    static StateSlot Create(const State<S>& input)
        { return StateSlot(CreateSlotNode(input.GetGroup(), input)); }

    StateSlot() = default;

    StateSlot(const StateSlot&) = default;
    StateSlot& operator=(const StateSlot&) = default;

    StateSlot(StateSlot&&) = default;
    StateSlot& operator=(StateSlot&&) = default;

    void Set(const State<S>& newInput)
        { SetSlotInput(newInput); }

protected:
    StateSlot(std::shared_ptr<REACT_IMPL::StateNode<S>>&& nodePtr) :
        StateSlot::State( std::move(nodePtr) )
    { }

private:
    static auto CreateSlotNode(const Group& group, const State<S>& input) -> decltype(auto)
    {
        using REACT_IMPL::StateSlotNode;
        using REACT_IMPL::SameGroupOrLink;

        return std::make_shared<StateSlotNode<S>>(group, SameGroupOrLink(group, input));
    }

    void SetSlotInput(const State<S>& newInput)
    {
        using REACT_IMPL::NodeId;
        using REACT_IMPL::StateSlotNode;
        using REACT_IMPL::SameGroupOrLink;

        auto* castedPtr = static_cast<StateSlotNode<S>*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [this, castedPtr, &newInput] { castedPtr->SetInput(SameGroupOrLink(this->GetGroup(), newInput)); });
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateLink
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateLink : public State<S>
{
public:
    // Construct with group
    static StateLink Create(const Group& group, const State<S>& input)
        { return StateLink(GetOrCreateLinkNode(group, input)); }

    StateLink() = default;

    StateLink(const StateLink&) = default;
    StateLink& operator=(const StateLink&) = default;

    StateLink(StateLink&&) = default;
    StateLink& operator=(StateLink&&) = default;

protected:
    StateLink(std::shared_ptr<REACT_IMPL::StateNode<S>>&& nodePtr) :
        StateLink::State( std::move(nodePtr) )
    { }

private:
    static auto GetOrCreateLinkNode(const Group& group, const State<S>& input) -> decltype(auto)
    {
        using REACT_IMPL::StateLinkNode;
        using REACT_IMPL::IReactNode;
        using REACT_IMPL::ReactGraph;
        
        IReactNode* k = GetInternals(input).GetNodePtr().get();

        ReactGraph::LinkCache& linkCache = GetInternals(group).GetGraphPtr()->GetLinkCache();

        std::shared_ptr<IReactNode> nodePtr = linkCache.LookupOrCreate(k, [&]
            {
                auto nodePtr = std::make_shared<StateLinkNode<S>>(group, input);
                nodePtr->SetWeakSelfPtr(std::weak_ptr<StateLinkNode<S>>{ nodePtr });
                return std::static_pointer_cast<IReactNode>(nodePtr);
            });

        return std::static_pointer_cast<StateLinkNode<S>>(nodePtr);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CreateRef
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
auto CreateRef(const State<S>& state) -> State<Ref<S>>
{
    using REACT_IMPL::StateRefNode;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<Ref<S>>, StateRefNode<S>>(state.GetGroup(), state);
}

/******************************************/ REACT_END /******************************************/

#endif // REACT_STATE_H_INCLUDED
// #include "react/event.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_EVENT_H_INCLUDED
#define REACT_EVENT_H_INCLUDED



// #include "react/detail/defs.h"


#include <memory>
#include <type_traits>
#include <utility>

// #include "react/api.h"

// #include "react/group.h"


// #include "react/detail/state_nodes.h"

// #include "react/detail/event_nodes.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_EVENT_NODES_H_INCLUDED
#define REACT_DETAIL_EVENT_NODES_H_INCLUDED



// #include "react/detail/defs.h"


#include <atomic>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

//#include "tbb/spin_mutex.h"

// #include "node_base.h"

// #include "react/common/utility.h"


/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterators for event processing
///////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventNode : public NodeBase
{
public:
    explicit EventNode(const Group& group) :
        EventNode::NodeBase( group )
    { }

    EventValueList<E>& Events()
        { return events_; }

    const EventValueList<E>& Events() const
        { return events_; }

    virtual void Clear() noexcept override
        { events_.clear(); }

private:
    EventValueList<E> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSourceNode : public EventNode<E>
{
public:
    EventSourceNode(const Group& group) :
        EventSourceNode::EventNode( group )
    {
        this->RegisterMe(NodeCategory::input);
    }

    ~EventSourceNode()
    {
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

    template <typename U>
    void EmitValue(U&& value)
        { this->Events().push_back(std::forward<U>(value)); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventMergeNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename ... TInputs>
class EventMergeNode : public EventNode<E>
{
public:
    EventMergeNode(const Group& group, const Event<TInputs>& ... deps) :
        EventMergeNode::EventNode( group ),
        inputs_( deps ... )
    {
        this->RegisterMe();
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(deps).GetNodeId()));
    }

    ~EventMergeNode()
    {
        react::impl::apply([this] (const auto& ... inputs)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(inputs).GetNodeId())); }, inputs_);
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        react::impl::apply([this] (auto& ... inputs)
            { REACT_EXPAND_PACK(MergeFromInput(inputs)); }, inputs_);

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    template <typename U>
    void MergeFromInput(Event<U>& dep)
    {
        auto& depInternals = GetInternals(dep);
        this->Events().insert(this->Events().end(), depInternals.Events().begin(), depInternals.Events().end());
    }

    std::tuple<Event<TInputs> ...> inputs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSlotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSlotNode : public EventNode<E>
{
public:
    EventSlotNode(const Group& group) :
        EventSlotNode::EventNode( group )
    {
        inputNodeId_ = this->GetGraphPtr()->RegisterNode(&slotInput_, NodeCategory::dyninput);
        this->RegisterMe();

        this->AttachToMe(inputNodeId_);
    }

    ~EventSlotNode()
    {
        RemoveAllSlotInputs();
        this->DetachFromMe(inputNodeId_);

        this->UnregisterMe();
        this->GetGraphPtr()->UnregisterNode(inputNodeId_);
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        for (auto& e : inputs_)
        {
            const auto& events = GetInternals(e).Events();
            this->Events().insert(this->Events().end(), events.begin(), events.end());
        }

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

    void AddSlotInput(const Event<E>& input)
    {
        auto it = std::find(inputs_.begin(), inputs_.end(), input);
        if (it == inputs_.end())
        {
            inputs_.push_back(input);
            this->AttachToMe(GetInternals(input).GetNodeId());
        }
    }

    void RemoveSlotInput(const Event<E>& input)
    {
        auto it = std::find(inputs_.begin(), inputs_.end(), input);
        if (it != inputs_.end())
        {
            inputs_.erase(it);
            this->DetachFromMe(GetInternals(input).GetNodeId());
        }
    }

    void RemoveAllSlotInputs()
    {
        for (const auto& e : inputs_)
            this->DetachFromMe(GetInternals(e).GetNodeId());

        inputs_.clear();
    }

    NodeId GetInputNodeId() const
        { return inputNodeId_; }

private:        
    struct VirtualInputNode : public IReactNode
    {
        virtual UpdateResult Update(TurnId turnId) noexcept override
            { return UpdateResult::changed; }
    };

    std::vector<Event<E>> inputs_;

    NodeId              inputNodeId_;
    VirtualInputNode    slotInput_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename TIn, typename F>
class EventProcessingNode : public EventNode<TOut>
{
public:
    template <typename FIn>
    EventProcessingNode(const Group& group, FIn&& func, const Event<TIn>& dep) :
        EventProcessingNode::EventNode( group ),
        func_( std::forward<FIn>(func) ),
        dep_( dep )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(dep).GetNodeId());
    }

    ~EventProcessingNode()
    {
        this->DetachFromMe(GetInternals(dep_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        func_(GetInternals(dep_).Events(), std::back_inserter(this->Events()));

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    F func_;

    Event<TIn> dep_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TOut, typename TIn, typename F, typename ... TSyncs>
class SyncedEventProcessingNode : public EventNode<TOut>
{
public:
    template <typename FIn>
    SyncedEventProcessingNode(const Group& group, FIn&& func, const Event<TIn>& dep, const State<TSyncs>& ... syncs) :
        SyncedEventProcessingNode::EventNode( group ),
        func_( std::forward<FIn>(func) ),
        dep_( dep ),
        syncHolder_( syncs ... )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(dep).GetNodeId());
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(syncs).GetNodeId()));
    }

    ~SyncedEventProcessingNode()
    {
        react::impl::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(syncs).GetNodeId())); }, syncHolder_);
        this->DetachFromMe(GetInternals(dep_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (GetInternals(dep_).Events().empty())
            return UpdateResult::unchanged;

        react::impl::apply([this] (const auto& ... syncs)
            {
                func_(GetInternals(dep_).Events(), std::back_inserter(this->Events()), GetInternals(syncs).Value() ...);
            },
            syncHolder_);

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    F func_;

    Event<TIn> dep_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventJoinNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ... Ts>
class EventJoinNode : public EventNode<std::tuple<Ts ...>>
{
public:
    EventJoinNode(const Group& group, const Event<Ts>& ... deps) :
        EventJoinNode::EventNode( group ),
        slots_( deps ... )
    {
        this->RegisterMe();
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(deps).GetNodeId()));
    }

    ~EventJoinNode()
    {
        react::impl::apply([this] (const auto& ... slots)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(slots.source).GetNodeId())); }, slots_);
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        // Move events into buffers.
        react::impl::apply([this, turnId] (Slot<Ts>& ... slots)
            { REACT_EXPAND_PACK(FetchBuffer(turnId, slots)); }, slots_);

        while (true)
        {
            bool isReady = true;

            // All slots ready?
            react::impl::apply([this, &isReady] (Slot<Ts>& ... slots)
                {
                    // Todo: combine return values instead
                    REACT_EXPAND_PACK(CheckSlot(slots, isReady));
                },
                slots_);

            if (!isReady)
                break;

            // Pop values from buffers and emit tuple.
            react::impl::apply([this] (Slot<Ts>& ... slots)
                {
                    this->Events().emplace_back(slots.buffer.front() ...);
                    REACT_EXPAND_PACK(slots.buffer.pop_front());
                },
                slots_);
        }

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    template <typename U>
    struct Slot
    {
        Slot(const Event<U>& src) :
            source( src )
        { }

        Event<U>        source;
        std::deque<U>   buffer;
    };

    template <typename U>
    static void FetchBuffer(TurnId turnId, Slot<U>& slot)
    {
        slot.buffer.insert(slot.buffer.end(), GetInternals(slot.source).Events().begin(), GetInternals(slot.source).Events().end());
    }

    template <typename T>
    static void CheckSlot(Slot<T>& slot, bool& isReady)
    {
        bool t = isReady && !slot.buffer.empty();
        isReady = t;
    }

    std::tuple<Slot<Ts>...> slots_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventLinkNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventLinkNode : public EventNode<E>
{
public:
    EventLinkNode(const Group& group, const Event<E>& dep) :
        EventLinkNode::EventNode( group ),
        dep_( dep ),
        srcGroup_( dep.GetGroup() )
    {
        this->RegisterMe(NodeCategory::input);

        auto& srcGraphPtr = GetInternals(srcGroup_).GetGraphPtr();
        outputNodeId_ = srcGraphPtr->RegisterNode(&linkOutput_, NodeCategory::linkoutput);
        
        srcGraphPtr->AttachNode(outputNodeId_, GetInternals(dep).GetNodeId());
    }

    ~EventLinkNode()
    {
        auto& srcGraphPtr = GetInternals(srcGroup_).GetGraphPtr();
        srcGraphPtr->DetachNode(outputNodeId_, GetInternals(dep_).GetNodeId());
        srcGraphPtr->UnregisterNode(outputNodeId_);

        auto& linkCache = this->GetGraphPtr()->GetLinkCache();
        linkCache.Erase(this);

        this->UnregisterMe();
    }

    void SetWeakSelfPtr(const std::weak_ptr<EventLinkNode>& self)
        { linkOutput_.parent = self; }

    virtual UpdateResult Update(TurnId turnId) noexcept override
        { return UpdateResult::changed; }

    void SetEvents(EventValueList<E>&& events)
        { this->Events() = std::move(events); }

private:
    struct VirtualOutputNode : public IReactNode
    {
        virtual UpdateResult Update(TurnId turnId) noexcept override
            { return UpdateResult::changed; }

        virtual void CollectOutput(LinkOutputMap& output) override
        {
            if (auto p = parent.lock())
            {
                auto* rawPtr = p->GetGraphPtr().get();
                output[rawPtr].push_back(
                    [storedParent = std::move(p), storedEvents = GetInternals(p->dep_).Events()] () mutable
                    {
                        NodeId nodeId = storedParent->GetNodeId();
                        auto& graphPtr = storedParent->GetGraphPtr();

                        graphPtr->PushInput(nodeId,
                            [&storedParent, &storedEvents]
                            {
                                storedParent->SetEvents(std::move(storedEvents));
                            });
                    });
            }
        }

        std::weak_ptr<EventLinkNode> parent;  
    };

    Event<E>    dep_;
    Group       srcGroup_;
    NodeId      outputNodeId_;

    VirtualOutputNode linkOutput_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventInternals
{
public:
    EventInternals() = default;

    EventInternals(const EventInternals&) = default;
    EventInternals& operator=(const EventInternals&) = default;

    EventInternals(EventInternals&&) = default;
    EventInternals& operator=(EventInternals&&) = default;

    explicit EventInternals(std::shared_ptr<EventNode<E>>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto GetNodePtr() -> std::shared_ptr<EventNode<E>>&
        { return nodePtr_; }

    auto GetNodePtr() const -> const std::shared_ptr<EventNode<E>>&
        { return nodePtr_; }

    NodeId GetNodeId() const
        { return nodePtr_->GetNodeId(); }

    EventValueList<E>& Events()
        { return nodePtr_->Events(); }

    const EventValueList<E>& Events() const
        { return nodePtr_->Events(); }

private:
    std::shared_ptr<EventNode<E>> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_EVENT_NODES_H_INCLUDED

// #include "react/common/ptrcache.h"


/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Event
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class Event : protected REACT_IMPL::EventInternals<E>
{
public:
    // Construct with explicit group
    template <typename F, typename T>
    static Event Create(const Group& group, F&& func, const Event<T>& dep)
        { return Event(CreateProcessingNode(group, std::forward<F>(func), dep)); }

    // Construct with implicit group
    template <typename F, typename T>
    static Event Create(F&& func, const Event<T>& dep)
        { return Event(CreateProcessingNode(dep.GetGroup(), std::forward<F>(func), dep)); }

    // Construct with explicit group
    template <typename F, typename T, typename ... Us>
    static Event Create(const Group& group, F&& func, const Event<T>& dep, const State<Us>& ... states)
        { return Event(CreateSyncedProcessingNode(group, std::forward<F>(func), dep, states ...)); }

    // Construct with implicit group
    template <typename F, typename T, typename ... Us>
    static Event Create(F&& func, const Event<T>& dep, const State<Us>& ... states)
        { return Event(CreateSyncedProcessingNode(dep.GetGroup(), std::forward<F>(func), dep, states ...)); }

    Event() = default;

    Event(const Event&) = default;
    Event& operator=(const Event&) = default;

    Event(Event&&) = default;
    Event& operator=(Event&&) = default;

    auto GetGroup() const -> const Group&
        { return this->GetNodePtr()->GetGroup(); }

    auto GetGroup() -> Group&
        { return this->GetNodePtr()->GetGroup(); }

    friend bool operator==(const Event<E>& a, const Event<E>& b)
        { return a.GetNodePtr() == b.GetNodePtr(); }

    friend bool operator!=(const Event<E>& a, const Event<E>& b)
        { return !(a == b); }

    friend auto GetInternals(Event<E>& e) -> REACT_IMPL::EventInternals<E>&
        { return e; }

    friend auto GetInternals(const Event<E>& e) -> const REACT_IMPL::EventInternals<E>&
        { return e; }

protected:
    Event(std::shared_ptr<REACT_IMPL::EventNode<E>>&& nodePtr) :
        Event::EventInternals( std::move(nodePtr) )
    { }

    template <typename F, typename T>
    static auto CreateProcessingNode(const Group& group, F&& func, const Event<T>& dep) -> decltype(auto)
    {
        using REACT_IMPL::EventProcessingNode;
        using REACT_IMPL::SameGroupOrLink;

        return std::make_shared<EventProcessingNode<E, T, typename std::decay<F>::type>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep));
    }

    template <typename F, typename T, typename ... Us>
    static auto CreateSyncedProcessingNode(const Group& group, F&& func, const Event<T>& dep, const State<Us>& ... syncs) -> decltype(auto)
    {
        using REACT_IMPL::SyncedEventProcessingNode;
        using REACT_IMPL::SameGroupOrLink;

        return std::make_shared<SyncedEventProcessingNode<E, T, typename std::decay<F>::type, Us ...>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep), SameGroupOrLink(group, syncs) ...);
    }

    template <typename RET, typename NODE, typename ... ARGS>
    friend RET impl::CreateWrappedNode(ARGS&& ... args);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSource : public Event<E>
{
public:
    // Construct event source
    static EventSource Create(const Group& group)
        { return EventSource(CreateSourceNode(group)); }

    EventSource() = default;

    EventSource(const EventSource&) = default;
    EventSource& operator=(const EventSource&) = default;

    EventSource(EventSource&& other) = default;
    EventSource& operator=(EventSource&& other) = default;
    
    void Emit(const E& value)
        { EmitValue(value); }

    void Emit(E&& value)
        { EmitValue(std::move(value)); }

    template <typename T = E, typename = std::enable_if_t<std::is_same_v<T, Token>>>
    void Emit()
        { EmitValue(Token::value); }

    EventSource& operator<<(const E& value)
        { EmitValue(value); return *this; }

    EventSource& operator<<(E&& value)
        { EmitValue(std::move(value)); return *this; }

protected:
    EventSource(std::shared_ptr<REACT_IMPL::EventNode<E>>&& nodePtr) :
        EventSource::Event( std::move(nodePtr) )
    { }

private:
    static auto CreateSourceNode(const Group& group) -> decltype(auto)
    {
        using REACT_IMPL::EventSourceNode;
        return std::make_shared<EventSourceNode<E>>(group);
    }

    template <typename T>
    void EmitValue(T&& value)
    {
        using REACT_IMPL::NodeId;
        using REACT_IMPL::ReactGraph;
        using REACT_IMPL::EventSourceNode;

        auto* castedPtr = static_cast<EventSourceNode<E>*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [castedPtr, &value] { castedPtr->EmitValue(std::forward<T>(value)); });
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSlotBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventSlot : public Event<E>
{
public:
    // Construct emtpy slot
    static EventSlot Create(const Group& group)
        { return EventSlot(CreateSlotNode(group)); }

    EventSlot() = default;

    EventSlot(const EventSlot&) = default;
    EventSlot& operator=(const EventSlot&) = default;

    EventSlot(EventSlot&&) = default;
    EventSlot& operator=(EventSlot&&) = default;

    void Add(const Event<E>& input)
        { AddSlotInput(input); }

    void Remove(const Event<E>& input)
        { RemoveSlotInput(input); }

    void RemoveAll()
        { RemoveAllSlotInputs(); }

protected:
    EventSlot(std::shared_ptr<REACT_IMPL::EventNode<E>>&& nodePtr) :
        EventSlot::Event( std::move(nodePtr) )
    { }

private:
    static auto CreateSlotNode(const Group& group) -> decltype(auto)
    {
        using REACT_IMPL::EventSlotNode;
        return std::make_shared<EventSlotNode<E>>(group);
    }

    void AddSlotInput(const Event<E>& input)
    {
        using REACT_IMPL::NodeId;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [this, castedPtr, &input] { castedPtr->AddSlotInput(SameGroupOrLink(this->GetGroup(), input)); });
    }

    void RemoveSlotInput(const Event<E>& input)
    {
        using REACT_IMPL::NodeId;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [this, castedPtr, &input] { castedPtr->RemoveSlotInput(SameGroupOrLink(this->GetGroup(), input)); });
    }

    void RemoveAllSlotInputs()
    {
        using REACT_IMPL::NodeId;
        using SlotNodeType = REACT_IMPL::EventSlotNode<E>;

        SlotNodeType* castedPtr = static_cast<SlotNodeType*>(this->GetNodePtr().get());

        NodeId nodeId = castedPtr->GetInputNodeId();
        auto& graphPtr = GetInternals(this->GetGroup()).GetGraphPtr();

        graphPtr->PushInput(nodeId, [castedPtr] { castedPtr->RemoveAllSlotInputs(); });
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventLink
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E>
class EventLink : public Event<E>
{
public:
    // Construct with group
    static EventLink Create(const Group& group, const Event<E>& input)
        { return EventLink(GetOrCreateLinkNode(group, input)); }

    EventLink() = default;

    EventLink(const EventLink&) = default;
    EventLink& operator=(const EventLink&) = default;

    EventLink(EventLink&&) = default;
    EventLink& operator=(EventLink&&) = default;

protected:
    EventLink(std::shared_ptr<REACT_IMPL::EventNode<E>>&& nodePtr) :
        EventLink::Event( std::move(nodePtr) )
    { }

private:
    static auto GetOrCreateLinkNode(const Group& group, const Event<E>& input) -> decltype(auto)
    {
        using REACT_IMPL::EventLinkNode;
        using REACT_IMPL::IReactNode;
        using REACT_IMPL::ReactGraph;
        
        IReactNode* k = GetInternals(input).GetNodePtr().get();

        ReactGraph::LinkCache& linkCache = GetInternals(group).GetGraphPtr()->GetLinkCache();

        std::shared_ptr<IReactNode> nodePtr = linkCache.LookupOrCreate(k, [&]
            {
                auto nodePtr = std::make_shared<EventLinkNode<E>>(group, input);
                nodePtr->SetWeakSelfPtr(std::weak_ptr<EventLinkNode<E>>{ nodePtr });
                return std::static_pointer_cast<IReactNode>(nodePtr);
            });

        return std::static_pointer_cast<EventLinkNode<E>>(nodePtr);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename ... Us>
static auto Merge(const Group& group, const Event<E>& dep1, const Event<Us>& ... deps) -> Event<E>
{
    using REACT_IMPL::EventMergeNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<Event<E>, EventMergeNode<E, E, Us ...>>(
        group, SameGroupOrLink(group, dep1), SameGroupOrLink(group, deps) ...);
}

template <typename T = void, typename U1, typename ... Us>
static auto Merge(const Event<U1>& dep1, const Event<Us>& ... deps) -> decltype(auto)
    { return Merge(dep1.GetGroup(), dep1, deps ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E>
static auto Filter(const Group& group, F&& pred, const Event<E>& dep) -> Event<E>
{
    auto filterFunc = [capturedPred = std::forward<F>(pred)] (const EventValueList<E>& events, EventValueSink<E> out)
        { std::copy_if(events.begin(), events.end(), out, capturedPred); };

    return Event<E>::Create(group, std::move(filterFunc), dep);
}

template <typename F, typename E>
static auto Filter(F&& pred, const Event<E>& dep) -> Event<E>
    { return Filter(dep.GetGroup(), std::forward<F>(pred), dep); }

template <typename F, typename E, typename ... Ts>
static auto Filter(const Group& group, F&& pred, const Event<E>& dep, const State<Ts>& ... states) -> Event<E>
{
    auto filterFunc = [capturedPred = std::forward<F>(pred)] (const EventValueList<E>& evts, EventValueSink<E> out, const Ts& ... values)
        {
            for (const auto& v : evts)
                if (capturedPred(v, values ...))
                    *out++ = v;
        };

    return Event<E>::Create(group, std::move(filterFunc), dep, states ...);
}

template <typename F, typename E, typename ... Ts>
static auto Filter(F&& pred, const Event<E>& dep, const State<Ts>& ... states) -> Event<E>
    { return Filter(dep.GetGroup(), std::forward<F>(pred), dep, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename F, typename T>
static auto Transform(const Group& group, F&& op, const Event<T>& dep) -> Event<E>
{
    auto transformFunc = [capturedOp = std::forward<F>(op)] (const EventValueList<T>& evts, EventValueSink<E> out)
        { std::transform(evts.begin(), evts.end(), out, capturedOp); };

    return Event<E>::Create(group, std::move(transformFunc), dep);
}

template <typename E, typename F, typename T>
static auto Transform(F&& op, const Event<T>& dep) -> Event<E>
    { return Transform<E>(dep.GetGroup(), std::forward<F>(op), dep); }

template <typename E, typename F, typename T, typename ... Us>
static auto Transform(const Group& group, F&& op, const Event<T>& dep, const State<Us>& ... states) -> Event<E>
{
    auto transformFunc = [capturedOp = std::forward<F>(op)] (const EventValueList<T>& evts, EventValueSink<E> out, const Us& ... values)
        {
            for (const auto& v : evts)
                *out++ = capturedOp(v, values ...);
        };

    return Event<E>::Create(group, std::move(transformFunc), dep, states ...);
}

template <typename E, typename F, typename T, typename ... Us>
static auto Transform(F&& op, const Event<T>& dep, const State<Us>& ... states) -> Event<E>
    { return Transform<E>(dep.GetGroup(), std::forward<F>(op), dep, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename U1, typename ... Us>
static auto Join(const Group& group, const Event<U1>& dep1, const Event<Us>& ... deps) -> Event<std::tuple<U1, Us ...>>
{
    using REACT_IMPL::EventJoinNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    static_assert(sizeof...(Us) > 0, "Join requires at least 2 inputs.");

    return CreateWrappedNode<Event<std::tuple<U1, Us ...>>, EventJoinNode<U1, Us ...>>(
        group, SameGroupOrLink(group, dep1), SameGroupOrLink(group, deps) ...);
}

template <typename U1, typename ... Us>
static auto Join(const Event<U1>& dep1, const Event<Us>& ... deps) -> Event<std::tuple<U1, Us ...>>
    { return Join(dep1.GetGroup(), dep1, deps ...); }

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <typename E>
static Event<E> SameGroupOrLink(const Group& targetGroup, const Event<E>& dep)
{
    if (dep.GetGroup() == targetGroup)
        return dep;
    else
        return EventLink<E>::Create(targetGroup, dep);
}

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_EVENT_H_INCLUDED

// #include "react/detail/algorithm_nodes.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED
#define REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED



// #include "react/detail/defs.h"


#include <algorithm>
#include <memory>
#include <utility>

// #include "state_nodes.h"

// #include "event_nodes.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E>
class IterateNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    IterateNode(const Group& group, T&& init, FIn&& func, const Event<E>& evnt) :
        IterateNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(evnt).GetNodeId());
    }

    ~IterateNode()
    {
        this->DetachFromMe(GetInternals(evnt_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        S newValue = func_(GetInternals(evnt_).Events(), this->Value());

        if (! (newValue == this->Value()))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
        }
    }

private:
    F           func_;
    Event<E>    evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E>
class IterateByRefNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    IterateByRefNode(const Group& group, T&& init, FIn&& func, const Event<E>& evnt) :
        IterateByRefNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(evnt_).GetNodeId());
    }

    ~IterateByRefNode()
    {
        this->DetachFromMe(GetInternals(evnt_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        func_(GetInternals(evnt_).Events(), this->Value());

        // Always assume a change
        return UpdateResult::changed;
    }

protected:
    F           func_;
    Event<E>    evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E, typename ... TSyncs>
class SyncedIterateNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    SyncedIterateNode(const Group& group, T&& init, FIn&& func, const Event<E>& evnt, const State<TSyncs>& ... syncs) :
        SyncedIterateNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt ),
        syncHolder_( syncs ... )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(evnt).GetNodeId());
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(syncs).GetNodeId()));
    }

    ~SyncedIterateNode()
    {
        react::impl::apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(syncs).GetNodeId())); }, syncHolder_);
        this->DetachFromMe(GetInternals(evnt_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (GetInternals(evnt_).Events().empty())
            return UpdateResult::unchanged;

        S newValue = react::impl::apply([this] (const auto& ... syncs)
            {
                return func_(GetInternals(evnt_).Events(), this->Value(), GetInternals(syncs).Value() ...);
            }, syncHolder_);

        if (! (newValue == this->Value()))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }
        else
        {
            return UpdateResult::unchanged;
        }   
    }

private:
    F           func_;
    Event<E>    evnt_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename F, typename E, typename ... TSyncs>
class SyncedIterateByRefNode : public StateNode<S>
{
public:
    template <typename T, typename FIn>
    SyncedIterateByRefNode(const Group& group, T&& init, FIn&& func, const Event<E>& evnt, const State<TSyncs>& ... syncs) :
        SyncedIterateByRefNode::StateNode( group, std::forward<T>(init) ),
        func_( std::forward<FIn>(func) ),
        evnt_( evnt ),
        syncHolder_( syncs ... )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(evnt).GetNodeId());
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(syncs).GetNodeId()));
    }

    ~SyncedIterateByRefNode()
    {
        react::impl::apply([this] (const auto& ... syncs) { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(syncs).GetNodeId())); }, syncHolder_);
        this->DetachFromMe(GetInternals(evnt_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (GetInternals(evnt_).Events().empty())
            return UpdateResult::unchanged;

        react::impl::apply(
            [this] (const auto& ... args)
            {
                func_(GetInternals(evnt_).Events(), this->Value(), GetInternals(args).Value() ...);
            },
            syncHolder_);

        return UpdateResult::changed;
    }

private:
    F           func_;
    Event<E>    evnt_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// HoldNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class HoldNode : public StateNode<S>
{
public:
    template <typename T>
    HoldNode(const Group& group, T&& init, const Event<S>& evnt) :
        HoldNode::StateNode( group, std::forward<T>(init) ),
        evnt_( evnt )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(evnt).GetNodeId());
    }

    ~HoldNode()
    {
        this->DetachFromMe(GetInternals(evnt_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        bool changed = false;

        if (! GetInternals(evnt_).Events().empty())
        {
            const S& newValue = GetInternals(evnt_).Events().back();

            if (! (newValue == this->Value()))
            {
                changed = true;
                this->Value() = newValue;
            }
        }

        if (changed)
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    Event<S>  evnt_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SnapshotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
class SnapshotNode : public StateNode<S>
{
public:
    SnapshotNode(const Group& group, const State<S>& target, const Event<E>& trigger) :
        SnapshotNode::StateNode( group, GetInternals(target).Value() ),
        target_( target ),
        trigger_( trigger )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(target).GetNodeId());
        this->AttachToMe(GetInternals(trigger).GetNodeId());
    }

    ~SnapshotNode()
    {
        this->DetachFromMe(GetInternals(trigger_).GetNodeId());
        this->DetachFromMe(GetInternals(target_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        bool changed = false;
        
        if (! GetInternals(trigger_).Events().empty())
        {
            const S& newValue = GetInternals(target_).Value();

            if (! (newValue == this->Value()))
            {
                changed = true;
                this->Value() = newValue;
            }
        }

        if (changed)
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    State<S>    target_;
    Event<E>    trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MonitorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class MonitorNode : public EventNode<S>
{
public:
    MonitorNode(const Group& group, const State<S>& input) :
        MonitorNode::EventNode( group ),
        input_( input )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(input_).GetNodeId());
    }

    ~MonitorNode()
    {
        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        this->Events().push_back(GetInternals(input_).Value());
        return UpdateResult::changed;
    }

private:
    State<S>    input_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulseNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
class PulseNode : public EventNode<S>
{
public:
    PulseNode(const Group& group, const State<S>& input, const Event<E>& trigger) :
        PulseNode::EventNode( group ),
        input_( input ),
        trigger_( trigger )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(input).GetNodeId());
        this->AttachToMe(GetInternals(trigger).GetNodeId());
    }

    ~PulseNode()
    {
        this->DetachFromMe(GetInternals(trigger_).GetNodeId());
        this->DetachFromMe(GetInternals(input_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        for (size_t i = 0; i < GetInternals(trigger_).Events().size(); ++i)
            this->Events().push_back(GetInternals(input_).Value());

        if (! this->Events().empty())
            return UpdateResult::changed;
        else
            return UpdateResult::unchanged;
    }

private:
    State<S>    input_;
    Event<E>    trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, template <typename> class TState>
class FlattenStateNode : public StateNode<S>
{
public:
    FlattenStateNode(const Group& group, const State<TState<S>>& outer) :
        FlattenStateNode::StateNode( group, GetInternals(GetInternals(outer).Value()).Value() ),
        outer_( outer ),
        inner_( GetInternals(outer).Value() )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(outer_).GetNodeId());
        this->AttachToMe(GetInternals(inner_).GetNodeId());
    }

    ~FlattenStateNode()
    {
        this->DetachFromMe(GetInternals(inner_).GetNodeId());
        this->DetachFromMe(GetInternals(outer_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        const State<S>& newInner = GetInternals(outer_).Value();

        // Check if there's a new inner node.
        if (! (newInner == inner_))
        {
            this->DetachFromMe(GetInternals(inner_).GetNodeId());
            this->AttachToMe(GetInternals(newInner).GetNodeId());
            inner_ = newInner;
            return UpdateResult::shifted;
        }

        const S& newValue = GetInternals(inner_).Value();

        if (HasChanged(newValue, this->Value()))
        {
            this->Value() = newValue;
            return UpdateResult::changed;
        }

        return UpdateResult::unchanged;
    }

private:
    State<TState<S>>    outer_;
    State<S>            inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateListNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams>
class FlattenStateListNode : public StateNode<TList<V>>
{
public:
    using InputListType = TList<TState<V>, TParams ...>;
    using FlatListType = TList<V>;

    FlattenStateListNode(const Group& group, const State<InputListType>& outer) :
        FlattenStateListNode::StateNode( group, MakeFlatList(GetInternals(outer).Value()) ),
        outer_( outer ),
        inner_( GetInternals(outer).Value() )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(outer_).GetNodeId());

        for (const State<V>& state : inner_)
            this->AttachToMe(GetInternals(state).GetNodeId());
    }

    ~FlattenStateListNode()
    {
        for (const State<V>& state : inner_)
            this->DetachFromMe(GetInternals(state).GetNodeId());

        this->DetachFromMe(GetInternals(outer_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        const InputListType& newInner = GetInternals(outer_).Value();

        // Check if there's a new inner node.
        if (! (std::equal(begin(newInner), end(newInner), begin(inner_), end(inner_))))
        {
            for (const State<V>& state : inner_)
                this->DetachFromMe(GetInternals(state).GetNodeId());
            for (const State<V>& state : newInner)
                this->AttachToMe(GetInternals(state).GetNodeId());

            inner_ = newInner;
            return UpdateResult::shifted;
        }

        FlatListType newValue = MakeFlatList(inner_);
        const FlatListType& curValue = this->Value();

        if (! (std::equal(begin(newValue), end(newValue), begin(curValue), end(curValue), [] (const auto& a, const auto& b)
            { return !HasChanged(a, b); })))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }

        return UpdateResult::unchanged;
    }

private:
    static FlatListType MakeFlatList(const InputListType& list)
    {
        FlatListType res;
        for (const State<V>& state : list)
            ListInsert(res, GetInternals(state).Value());
        return res;
    }

    State<InputListType>    outer_;
    InputListType           inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenStateListNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams>
class FlattenStateMapNode : public StateNode<TMap<K, V>>
{
public:
    using InputMapType = TMap<K, TState<V>, TParams ...>;
    using FlatMapType = TMap<K, V>;

    FlattenStateMapNode(const Group& group, const State<InputMapType>& outer) :
        FlattenStateMapNode::StateNode( group, MakeFlatMap(GetInternals(outer).Value()) ),
        outer_( outer ),
        inner_( GetInternals(outer).Value() )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(outer_).GetNodeId());

        for (const auto& entry : inner_)
            this->AttachToMe(GetInternals(entry.second).GetNodeId());
    }

    ~FlattenStateMapNode()
    {
        for (const auto& entry : inner_)
            this->DetachFromMe(GetInternals(entry.second).GetNodeId());

        this->DetachFromMe(GetInternals(outer_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        const InputMapType& newInner = GetInternals(outer_).Value();

        // Check if there's a new inner node.
        if (! (std::equal(begin(newInner), end(newInner), begin(inner_), end(inner_))))
        {
            for (const auto& entry : inner_)
                this->DetachFromMe(GetInternals(entry.second).GetNodeId());
            for (const auto& entry : newInner)
                this->AttachToMe(GetInternals(entry.second).GetNodeId());

            inner_ = newInner;
            return UpdateResult::shifted;
        }

        FlatMapType newValue = MakeFlatMap(inner_);
        const FlatMapType& curValue = this->Value();

        if (! (std::equal(begin(newValue), end(newValue), begin(curValue), end(curValue), [] (const auto& a, const auto& b)
            { return !HasChanged(a.first, b.first) && !HasChanged(a.second, b.second); })))
        {
            this->Value() = std::move(newValue);
            return UpdateResult::changed;
        }

        return UpdateResult::unchanged;
    }

private:
    static FlatMapType MakeFlatMap(const InputMapType& map)
    {
        FlatMapType res;
        for (const auto& entry : map)
            MapInsert(res, typename FlatMapType::value_type{ entry.first, GetInternals(entry.second).Value() });
        return res;
    }

    State<InputMapType>    outer_;
    InputMapType           inner_;
};

struct FlattenedInitTag { };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenObjectNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename TFlat>
class FlattenObjectNode : public StateNode<TFlat>
{
public:
    FlattenObjectNode(const Group& group, const State<T>& obj) :
        StateNode<TFlat>( in_place, group, GetInternals(obj).Value(), FlattenedInitTag{ } ),
        obj_( obj )
    {
        this->RegisterMe();
        this->AttachToMe(GetInternals(obj).GetNodeId());

        for (NodeId nodeId : this->Value().memberIds_)
            this->AttachToMe(nodeId);

        this->Value().initMode_ = false;
    }

    ~FlattenObjectNode()
    {
        for (NodeId nodeId :  this->Value().memberIds_)
            this->DetachFromMe(nodeId);

        this->DetachFromMe(GetInternals(obj_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        const T& newValue = GetInternals(obj_).Value();

        if (HasChanged(newValue, static_cast<const T&>(this->Value())))
        {
            for (NodeId nodeId : this->Value().memberIds_)
                this->DetachFromMe(nodeId);

            // Steal array from old value for new value so we don't have to re-allocate.
            // The old value will freed after the assignment.
            this->Value().memberIds_.clear();
            this->Value() = TFlat { newValue, FlattenedInitTag{ }, std::move(this->Value().memberIds_) };

            for (NodeId nodeId : this->Value().memberIds_)
                this->AttachToMe(nodeId);

            return UpdateResult::shifted;
        }

        return UpdateResult::changed;
    }
private:
    State<T> obj_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ALGORITHM_NODES_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Holds the most recent event in a state
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename E>
auto Hold(const Group& group, T&& initialValue, const Event<E>& evnt) -> State<E>
{
    using REACT_IMPL::HoldNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<E>, HoldNode<E>>(
        group, std::forward<T>(initialValue), SameGroupOrLink(group, evnt));
}

template <typename T, typename E>
auto Hold(T&& initialValue, const Event<E>& evnt) -> State<E>
    { return Hold(evnt.GetGroup(), std::forward<T>(initialValue), evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Emits value changes of target state.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
auto Monitor(const Group& group, const State<S>& state) -> Event<S>
{
    using REACT_IMPL::MonitorNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<Event<S>, MonitorNode<S>>(
        group, SameGroupOrLink(group, state));
}

template <typename S>
auto Monitor(const State<S>& state) -> Event<S>
    { return Monitor(state.GetGroup(), state); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iteratively combines state value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename T, typename F, typename E>
auto Iterate(const Group& group, T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::IterateNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    using FuncType = typename std::decay<F>::type;

    return CreateWrappedNode<State<S>, IterateNode<S, FuncType, E>>(
        group, std::forward<T>(initialValue), std::forward<F>(func), SameGroupOrLink(group, evnt));
}

template <typename S, typename T, typename F, typename E>
auto IterateByRef(const Group& group, T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::IterateByRefNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    using FuncType = typename std::decay<F>::type;

    return CreateWrappedNode<State<S>, IterateByRefNode<S, FuncType, E>>(
        group, std::forward<T>(initialValue), std::forward<F>(func), SameGroupOrLink(group, evnt));
}

template <typename S, typename T, typename F, typename E>
auto Iterate(T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
    { return Iterate<S>(evnt.GetGroup(), std::forward<T>(initialValue), std::forward<F>(func), evnt); }

template <typename S, typename T, typename F, typename E>
auto IterateByRef(T&& initialValue, F&& func, const Event<E>& evnt) -> State<S>
    { return IterateByRef<S>(evnt.GetGroup(), std::forward<T>(initialValue), std::forward<F>(func), evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename T, typename F, typename E, typename ... Us>
auto Iterate(const Group& group, T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
{
    using REACT_IMPL::SyncedIterateNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    using FuncType = typename std::decay<F>::type;

    return CreateWrappedNode<State<S>, SyncedIterateNode<S, FuncType, E, Us ...>>(
        group, std::forward<T>(initialValue), std::forward<F>(func), SameGroupOrLink(group, evnt), SameGroupOrLink(group, states) ...);
}

template <typename S, typename T, typename F, typename E, typename ... Us>
auto IterateByRef(const Group& group, T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
{
    using REACT_IMPL::SyncedIterateByRefNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    using FuncType = typename std::decay<F>::type;

    return CreateWrappedNode<State<S>, SyncedIterateByRefNode<S, FuncType, E, Us ...>>(
        group, std::forward<T>(initialValue), std::forward<F>(func), SameGroupOrLink(group, evnt), SameGroupOrLink(group, states) ...);
}

template <typename S, typename T, typename F, typename E, typename ... Us>
auto Iterate(T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
    { return Iterate<S>(evnt.GetGroup(), std::forward<T>(initialValue), std::forward<F>(func), evnt, states ...); }

template <typename S, typename T, typename F, typename E, typename ... Us>
auto IterateByRef(T&& initialValue, F&& func, const Event<E>& evnt, const State<Us>& ... states) -> State<S>
    { return IterateByRef<S>(evnt.GetGroup(), std::forward<T>(initialValue), std::forward<F>(func), evnt, states ...); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot - Sets state value to value of other state when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
auto Snapshot(const Group& group, const State<S>& state, const Event<E>& evnt) -> State<S>
{
    using REACT_IMPL::SnapshotNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<S>, SnapshotNode<S, E>>(
        group, SameGroupOrLink(group, state), SameGroupOrLink(group, evnt));
}

template <typename S, typename E>
auto Snapshot(const State<S>& state, const Event<E>& evnt) -> State<S>
    { return Snapshot(state.GetGroup(), state, evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target state when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, typename E>
auto Pulse(const Group& group, const State<S>& state, const Event<E>& evnt) -> Event<S>
{
    using REACT_IMPL::PulseNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<Event<S>, PulseNode<S, E>>(
        group, SameGroupOrLink(group, state), SameGroupOrLink(group, evnt));
}

template <typename S, typename E>
auto Pulse(const State<S>& state, const Event<E>& evnt) -> Event<S>
    { return Pulse(state.GetGroup(), state, evnt); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S, template <typename> class TState,
    typename = std::enable_if_t<std::is_base_of_v<State<S>, TState<S>>>>
auto Flatten(const Group& group, const State<TState<S>>& state) -> State<S>
{
    using REACT_IMPL::FlattenStateNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<S>, FlattenStateNode<S, TState>>(group, SameGroupOrLink(group, state));
}

template <typename S, template <typename> class TState,
    typename = std::enable_if_t<std::is_base_of_v<State<S>, TState<S>>>>
auto Flatten(const State<TState<S>>& state) -> State<S>
    { return Flatten(state.GetGroup(), state); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenList
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenList(const Group& group, const State<TList<TState<V>, TParams ...>>& list) -> State<TList<V>>
{
    using REACT_IMPL::FlattenStateListNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<TList<V>>, FlattenStateListNode<TList, TState, V, TParams ...>>(
        group, SameGroupOrLink(group, list));
}

template <template <typename ...> class TList, template <typename> class TState, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenList(const State<TList<TState<V>, TParams ...>>& list) -> State<TList<V>>
    { return FlattenList(list.GetGroup(), list); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenMap
///////////////////////////////////////////////////////////////////////////////////////////////////
template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenMap(const Group& group, const State<TMap<K, TState<V>, TParams ...>>& map) -> State<TMap<K, V>>
{
    using REACT_IMPL::FlattenStateMapNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<TMap<K, V>>, FlattenStateMapNode<TMap, TState, K, V, TParams ...>>(
        group, SameGroupOrLink(group, map));
}

template <template <typename ...> class TMap, template <typename> class TState, typename K, typename V, typename ... TParams,
    typename = std::enable_if_t<std::is_base_of_v<State<V>, TState<V>>>>
auto FlattenMap(const State<TMap<K, TState<V>, TParams ...>>& map) -> State<TMap<K, V>>
    { return FlattenMap(map.GetGroup(), map); }

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flattened
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename C>
class Flattened : public C
{
public:
    using C::C;

    Flattened(const C& base) :
        C( base )
    { }

    Flattened(const C& base, REACT_IMPL::FlattenedInitTag) :
        C( base ),
        initMode_( true )
    { }

    Flattened(const C& base, REACT_IMPL::FlattenedInitTag, std::vector<REACT_IMPL::NodeId>&& emptyMemberIds) :
        C( base ),
        initMode_( true ),
        memberIds_( std::move(emptyMemberIds) ) // This will be empty, but has pre-allocated storage. It's a tweak.
    { }

    template <typename T>
    Ref<T> Flatten(State<T>& signal)
    {
        if (initMode_)
            memberIds_.push_back(GetInternals(signal).GetNodeId());
        
        return GetInternals(signal).Value();
    }

private:
    bool initMode_ = false;
    std::vector<REACT_IMPL::NodeId> memberIds_;

    template <typename T, typename TFlat>
    friend class impl::FlattenObjectNode;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenObject
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const Group& group, const State<T>& obj) -> State<TFlat>
{
    using REACT_IMPL::FlattenObjectNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<TFlat>, FlattenObjectNode<T, TFlat>>(group, obj);
}

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const State<T>& obj) -> State<TFlat>
    { return FlattenObject(obj.GetGroup(), obj); }

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const Group& group, const State<Ref<T>>& obj) -> State<TFlat>
{
    using REACT_IMPL::FlattenObjectNode;
    using REACT_IMPL::SameGroupOrLink;
    using REACT_IMPL::CreateWrappedNode;

    return CreateWrappedNode<State<TFlat>, FlattenObjectNode<Ref<T>, TFlat>>(group, obj);
}

template <typename T, typename TFlat = typename T::Flat>
auto FlattenObject(const State<Ref<T>>& obj) -> State<TFlat>
    { return FlattenObject(obj.GetGroup(), obj); }

/******************************************/ REACT_END /******************************************/

#endif // REACT_ALGORITHM_H_INCLUDED
// #include "react/api.h"

// #include "react/event.h"

// #include "react/group.h"

// #include "react/observer.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_OBSERVER_H_INCLUDED
#define REACT_OBSERVER_H_INCLUDED



// #include "react/detail/defs.h"

// #include "react/api.h"

// #include "react/group.h"


#include <memory>
#include <utility>

// #include "react/detail/observer_nodes.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_OBSERVER_NODES_H_INCLUDED
#define REACT_DETAIL_OBSERVER_NODES_H_INCLUDED



// #include "react/detail/defs.h"

// #include "react/api.h"

// #include "react/common/utility.h"


#include <memory>
#include <utility>
#include <tuple>

// #include "node_base.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename S>
class StateNode;

template <typename E>
class EventStreamNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class ObserverNode : public NodeBase
{
public:
    explicit ObserverNode(const Group& group) :
        ObserverNode::NodeBase( group )
    { }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// StateObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename ... TDeps>
class StateObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    StateObserverNode(const Group& group, FIn&& func, const State<TDeps>& ... deps) :
        StateObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        depHolder_( deps ... )
    {
        this->RegisterMe(NodeCategory::output);
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(deps).GetNodeId()));

        react::impl::apply([this] (const auto& ... deps)
            { this->func_(GetInternals(deps).Value() ...); }, depHolder_);
    }

    ~StateObserverNode()
    {
        react::impl::apply([this] (const auto& ... deps)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(deps).GetNodeId())); }, depHolder_);
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        react::impl::apply([this] (const auto& ... deps)
            { this->func_(GetInternals(deps).Value() ...); }, depHolder_);
        return UpdateResult::unchanged;
    }

private:
    F func_;

    std::tuple<State<TDeps> ...> depHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E>
class EventObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    EventObserverNode(const Group& group, FIn&& func, const Event<E>& subject) :
        EventObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject )
    {
        this->RegisterMe(NodeCategory::output);
        this->AttachToMe(GetInternals(subject).GetNodeId());
    }

    ~EventObserverNode()
    {
        this->DetachFromMe(GetInternals(subject_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        func_(GetInternals(subject_).Events());
        return UpdateResult::unchanged;
    }

private:
    F func_;

    Event<E> subject_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename F, typename E, typename ... TSyncs>
class SyncedEventObserverNode : public ObserverNode
{
public:
    template <typename FIn>
    SyncedEventObserverNode(const Group& group, FIn&& func, const Event<E>& subject, const State<TSyncs>& ... syncs) :
        SyncedEventObserverNode::ObserverNode( group ),
        func_( std::forward<FIn>(func) ),
        subject_( subject ),
        syncHolder_( syncs ... )
    {
        this->RegisterMe(NodeCategory::output);
        this->AttachToMe(GetInternals(subject).GetNodeId());
        REACT_EXPAND_PACK(this->AttachToMe(GetInternals(syncs).GetNodeId()));
    }

    ~SyncedEventObserverNode()
    {
        apply([this] (const auto& ... syncs)
            { REACT_EXPAND_PACK(this->DetachFromMe(GetInternals(syncs).GetNodeId())); }, syncHolder_);
        this->DetachFromMe(GetInternals(subject_).GetNodeId());
        this->UnregisterMe();
    }

    virtual UpdateResult Update(TurnId turnId) noexcept override
    {
        // Updates might be triggered even if only sync nodes changed. Ignore those.
        if (GetInternals(this->subject_).Events().empty())
            return UpdateResult::unchanged;

        apply([this] (const auto& ... syncs)
            { func_(GetInternals(this->subject_).Events(), GetInternals(syncs).Value() ...); }, syncHolder_);

        return UpdateResult::unchanged;
    }

private:
    F func_;

    Event<E> subject_;

    std::tuple<State<TSyncs> ...> syncHolder_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverInternals
///////////////////////////////////////////////////////////////////////////////////////////////////
class ObserverInternals
{
public:
    ObserverInternals(const ObserverInternals&) = default;
    ObserverInternals& operator=(const ObserverInternals&) = default;

    ObserverInternals(ObserverInternals&&) = default;
    ObserverInternals& operator=(ObserverInternals&&) = default;

    explicit ObserverInternals(std::shared_ptr<ObserverNode>&& nodePtr) :
        nodePtr_( std::move(nodePtr) )
    { }

    auto GetNodePtr() -> std::shared_ptr<ObserverNode>&
        { return nodePtr_; }

    auto GetNodePtr() const -> const std::shared_ptr<ObserverNode>&
        { return nodePtr_; }

    NodeId GetNodeId() const
        { return nodePtr_->GetNodeId(); }

protected:
    ObserverInternals() = default;

private:
    std::shared_ptr<ObserverNode> nodePtr_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_OBSERVER_NODES_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class Observer : protected REACT_IMPL::ObserverInternals
{
private:
    using NodeType = REACT_IMPL::ObserverNode;

public:
    // Construct state observer with explicit group
    template <typename F, typename T1, typename ... Ts>
    static Observer Create(const Group& group, F&& func, const State<T1>& subject1, const State<Ts>& ... subjects)
        { return Observer(CreateStateObserverNode(group, std::forward<F>(func), subject1, subjects ...)); }

    // Construct state observer with implicit group
    template <typename F, typename T1, typename ... Ts>
    static Observer Create(F&& func, const State<T1>& subject1, const State<Ts>& ... subjects)
        { return Observer(CreateStateObserverNode(subject1.GetGroup(), std::forward<F>(func), subject1, subjects ...)); }

    // Construct event observer with explicit group
    template <typename F, typename T>
    static Observer Create(const Group& group, F&& func, const Event<T>& subject)
        { return Observer(CreateEventObserverNode(group, std::forward<F>(func), subject)); }

    // Construct event observer with implicit group
    template <typename F, typename T>
    static Observer Create(F&& func, const Event<T>& subject)
        { return Observer(CreateEventObserverNode(subject.GetGroup(), std::forward<F>(func), subject)); }

    // Constructed synced event observer with explicit group
    template <typename F, typename T, typename ... Us>
    static Observer Create(const Group& group, F&& func, const Event<T>& subject, const State<Us>& ... states)
        { return Observer(CreateSyncedEventObserverNode(group, std::forward<F>(func), subject, states ...)); }

    // Constructed synced event observer with implicit group
    template <typename F, typename T, typename ... Us>
    static Observer Create(F&& func, const Event<T>& subject, const State<Us>& ... states)
        { return Observer(CreateSyncedEventObserverNode(subject.GetGroup(), std::forward<F>(func), subject, states ...)); }

    Observer(const Observer&) = default;
    Observer& operator=(const Observer&) = default;

    Observer(Observer&&) = default;
    Observer& operator=(Observer&&) = default;

protected: //Internal
    Observer(std::shared_ptr<NodeType>&& nodePtr) :
        nodePtr_(std::move(nodePtr))
    { }

private:
    template <typename F, typename T1, typename ... Ts>
    static auto CreateStateObserverNode(const Group& group, F&& func, const State<T1>& dep1, const State<Ts>& ... deps) -> decltype(auto)
    {
        using REACT_IMPL::StateObserverNode;
        return std::make_shared<StateObserverNode<typename std::decay<F>::type, T1, Ts ...>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep1), SameGroupOrLink(group, deps) ...);
    }

    template <typename F, typename T>
    static auto CreateEventObserverNode(const Group& group, F&& func, const Event<T>& dep) -> decltype(auto)
    {
        using REACT_IMPL::EventObserverNode;
        return std::make_shared<EventObserverNode<typename std::decay<F>::type, T>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep));
    }

    template <typename F, typename T, typename ... Us>
    static auto CreateSyncedEventObserverNode(const Group& group, F&& func, const Event<T>& dep, const State<Us>& ... syncs) -> decltype(auto)
    {
        using REACT_IMPL::SyncedEventObserverNode;
        return std::make_shared<SyncedEventObserverNode<typename std::decay<F>::type, T, Us ...>>(
            group, std::forward<F>(func), SameGroupOrLink(group, dep), SameGroupOrLink(group, syncs) ...);
    }

private:
    std::shared_ptr<NodeType> nodePtr_;
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_OBSERVER_H_INCLUDED
// #include "react/state.h"

//          Copyright Sebastian Jeckel 2017.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// #include "react/detail/defs.h"

#include <algorithm>
#include <atomic>
#include <type_traits>
#include <utility>
#include <vector>
#include <map>
#include <mutex>

#include <tbb/concurrent_queue.h>
#include <tbb/task.h>

// #include "react/detail/graph_interface.h"
// #include "react/detail/graph_impl.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

inline NodeId ReactGraph::RegisterNode(IReactNode* nodePtr, NodeCategory category)
{
    return nodeData_.Insert(NodeData{ nodePtr, category });
}

inline void ReactGraph::UnregisterNode(NodeId nodeId)
{
    nodeData_.Erase(nodeId);
}

inline void ReactGraph::AttachNode(NodeId nodeId, NodeId parentId)
{
    auto& node = nodeData_[nodeId];
    auto& parent = nodeData_[parentId];

    parent.successors.push_back(nodeId);

    if (node.level <= parent.level)
        node.level = parent.level + 1;
}

inline void ReactGraph::DetachNode(NodeId nodeId, NodeId parentId)
{
    auto& parent = nodeData_[parentId];
    auto& successors = parent.successors;

    successors.erase(std::find(successors.begin(), successors.end(), nodeId));
}

inline void ReactGraph::AddSyncPointDependency(SyncPoint::Dependency dep, bool syncLinked)
{
    if (syncLinked)
        linkDependencies_.push_back(std::move(dep));
    else
        localDependencies_.push_back(std::move(dep));
}

inline void ReactGraph::AllowLinkedTransactionMerging(bool allowMerging)
{
    allowLinkedTransactionMerging_ = true;
}

inline void ReactGraph::Propagate()
{
    // Fill update queue with successors of changed inputs.
    for (NodeId nodeId : changedInputs_)
    {
        auto& node = nodeData_[nodeId];
        auto* nodePtr = node.nodePtr;

        UpdateResult res = nodePtr->Update(0u);

        if (res == UpdateResult::changed)
        {
            changedNodes_.push_back(nodePtr);
            ScheduleSuccessors(node);
        }
    }

    // Propagate changes.
    while (scheduledNodes_.FetchNext())
    {
        for (NodeId nodeId : scheduledNodes_.Next())
        {
            auto& node = nodeData_[nodeId];
            auto* nodePtr = node.nodePtr;

            // A predecessor of this node has shifted to a lower level?
            if (node.level < node.newLevel)
            {
                // Re-schedule this node.
                node.level = node.newLevel;

                RecalculateSuccessorLevels(node);
                scheduledNodes_.Push(nodeId, node.level);
                continue;
            }

            // Special handling for link output nodes. They have no successors and they don't have to be updated.
            if (node.category == NodeCategory::linkoutput)
            {
                node.nodePtr->CollectOutput(scheduledLinkOutputs_);
                continue;
            }

            UpdateResult res = nodePtr->Update(0u);

            // Topology changed?
            if (res == UpdateResult::shifted)
            {
                // Re-schedule this node.
                RecalculateSuccessorLevels(node);
                scheduledNodes_.Push(nodeId, node.level);
                continue;
            }
            
            if (res == UpdateResult::changed)
            {
                changedNodes_.push_back(nodePtr);
                ScheduleSuccessors(node);
            }

            node.queued = false;
        }
    }

    if (!scheduledLinkOutputs_.empty())
        UpdateLinkNodes();

    // Cleanup buffers in changed nodes.
    for (IReactNode* nodePtr : changedNodes_)
        nodePtr->Clear();
    changedNodes_.clear();

    // Clean link state.
    scheduledLinkOutputs_.clear();
    localDependencies_.clear();
    linkDependencies_.clear();
    allowLinkedTransactionMerging_ = false;
}

inline void ReactGraph::UpdateLinkNodes()
{
    TransactionFlags flags = TransactionFlags::none;

    if (! linkDependencies_.empty())
        flags |= TransactionFlags::sync_linked;

    if (allowLinkedTransactionMerging_)
        flags |= TransactionFlags::allow_merging;

    SyncPoint::Dependency dep{ begin(linkDependencies_), end(linkDependencies_) };

    for (auto& e : scheduledLinkOutputs_)
    {
        e.first->EnqueueTransaction(
            [inputs = std::move(e.second)]
            {
                for (auto& callback : inputs)
                    callback();
            }, dep, flags);
    }
}

inline void ReactGraph::ScheduleSuccessors(NodeData& node)
{
    for (NodeId succId : node.successors)
    {
        auto& succ = nodeData_[succId];

        if (!succ.queued)
        {
            succ.queued = true;
            scheduledNodes_.Push(succId, succ.level);
        }
    }
}

inline void ReactGraph::RecalculateSuccessorLevels(NodeData& node)
{
    for (NodeId succId : node.successors)
    {
        auto& succ = nodeData_[succId];

        if (succ.newLevel <= node.level)
            succ.newLevel = node.level + 1;
    }
}

inline bool ReactGraph::TopoQueue::FetchNext()
{
    // Throw away previous values
    nextData_.clear();

    // Find min level of nodes in queue data
    minLevel_ = (std::numeric_limits<int>::max)();
    for (const auto& e : queueData_)
        if (minLevel_ > e.second)
            minLevel_ = e.second;

    // Swap entries with min level to the end
    auto p = std::partition(queueData_.begin(), queueData_.end(), [t = minLevel_] (const Entry& e) { return t != e.second; });

    // Move min level values to next data
    nextData_.reserve(std::distance(p, queueData_.end()));

    for (auto it = p; it != queueData_.end(); ++it)
        nextData_.push_back(it->first);

    // Truncate moved entries
    queueData_.resize(std::distance(queueData_.begin(), p));

    return !nextData_.empty();
}

inline void TransactionQueue::ProcessQueue()
{
    for (;;)
    {
        size_t popCount = ProcessNextBatch();
        if (count_.fetch_sub(popCount) == popCount)
            return;
    }
}

inline size_t TransactionQueue::ProcessNextBatch()
{
    StoredTransaction curTransaction;
    size_t popCount = 0;

    bool canMerge = false;
    bool syncLinked = false;

    bool skipPop = false;
    bool isDone = false;

    // Outer loop. One transaction per iteration.
    for (;;)
    {
        if (!skipPop)
        {
            if (!transactions_.try_pop(curTransaction))
                return popCount;

            canMerge = IsBitmaskSet(curTransaction.flags, TransactionFlags::allow_merging);
            syncLinked = IsBitmaskSet(curTransaction.flags, TransactionFlags::sync_linked);

            ++popCount;
        }
        else
        {
            skipPop = false;
        }

        graph_.DoTransaction([&]
        {
            curTransaction.func();
            graph_.AddSyncPointDependency(std::move(curTransaction.dep), syncLinked);

            if (canMerge)
            {
                graph_.AllowLinkedTransactionMerging(true);

                // Pull in additional mergeable transactions.
                for (;;)
                {
                    if (!transactions_.try_pop(curTransaction))
                        return;

                    canMerge = IsBitmaskSet(curTransaction.flags, TransactionFlags::allow_merging);
                    syncLinked = IsBitmaskSet(curTransaction.flags, TransactionFlags::sync_linked);

                    ++popCount;

                    if (!canMerge)
                    {
                        skipPop = true;
                        return;
                    }

                    curTransaction.func();
                    graph_.AddSyncPointDependency(std::move(curTransaction.dep), syncLinked);
                }
            }
        });
    }
}

/****************************************/ REACT_IMPL_END /***************************************/

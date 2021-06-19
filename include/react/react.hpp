#pragma once

#define REACT_ENABLE_LOGGING

// #include "react/logging/EventLog.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_LOGGING_EVENTLOG_H_INCLUDED
#define REACT_DETAIL_LOGGING_EVENTLOG_H_INCLUDED



// #include "react/detail/Defs.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_DEFS_H_INCLUDED
#define REACT_DETAIL_DEFS_H_INCLUDED



#include <cassert>
#include <cstddef>

///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_BEGIN     namespace react {
#define REACT_END       }
#define REACT           ::react

#define REACT_IMPL_BEGIN    REACT_BEGIN     namespace impl {
#define REACT_IMPL_END      REACT_END       }
#define REACT_IMPL          REACT           ::impl

#ifdef _DEBUG
#define REACT_MESSAGE(...) printf(__VA_ARGS__)
#else
#define REACT_MESSAGE
#endif

// Assert with message
#define REACT_ASSERT(condition, ...) for (; !(condition); assert(condition)) printf(__VA_ARGS__)
#define REACT_ERROR(...)    REACT_ASSERT(false, __VA_ARGS__)

// Logging
#ifdef REACT_ENABLE_LOGGING
    #define REACT_LOG(...) __VA_ARGS__
#else
    #define REACT_LOG(...)
#endif

// Thread local storage
#if _WIN32 || _WIN64
    // MSVC
    #define REACT_TLS   __declspec(thread)
#elif __GNUC__
    // GCC
    #define REACT_TLS   __thread
#else
    // Standard C++11
    #define REACT_TLS   thread_local
#endif

/***************************************/ REACT_IMPL_BEGIN /**************************************/

// Type aliases
using uint = unsigned int;
using uchar = unsigned char;
using std::size_t;

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_DEFS_H_INCLUDED

#include <memory>
#include <cstdint>
#include <functional>
#include <iostream>
#include <chrono>

#include "tbb/concurrent_vector.h"

// #include "Logging.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_LOGGING_LOGGING_H_INCLUDED
#define REACT_DETAIL_LOGGING_LOGGING_H_INCLUDED



// #include "react/detail/Defs.h"


#include <iostream>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IEventRecord
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IEventRecord
{
    virtual ~IEventRecord() {}

    virtual const char* EventId() const = 0;
    virtual void        Serialize(std::ostream& out) const = 0;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_LOGGING_LOGGING_H_INCLUDED
// #include "react/common/Types.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_TYPES_H_INCLUDED
#define REACT_COMMON_TYPES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <chrono>
#include <cstdint>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

using ObjectId = uintptr_t;

template <typename O>
ObjectId GetObjectId(const O& obj)
{
	return (ObjectId)&obj;
}

using UpdateDurationT = std::chrono::duration<uint, std::micro>;

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_TYPES_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventLog
///////////////////////////////////////////////////////////////////////////////////////////////////
class EventLog
{
    using TimestampT = std::chrono::time_point<std::chrono::high_resolution_clock>;

    class Entry
    {
    public:
        Entry();
        Entry(const Entry& other);

        explicit Entry(IEventRecord* ptr);

        Entry& operator=(Entry& rhs);

        inline const char* EventId() const
        {
            return data_->EventId();
        }

        inline const TimestampT& Time() const
        {
            return time_;
        }

        inline bool operator<(const Entry& other) const
        {
            return time_ < other.time_;
        }

        inline void Release()
        {
            delete data_;
        }

        void Serialize(std::ostream& out, const TimestampT& startTime) const;
        bool Equals(const Entry& other) const;

    private:
        TimestampT      time_;
        IEventRecord*   data_;
    };

public:

    EventLog();
    ~EventLog();

    void    Print();
    void    Write(std::ostream& out);
    void    Clear();

    template
    <
        typename TEventRecord,
        typename ... TArgs
    >
    void Append(TArgs ... args)
    {
        entries_.push_back(Entry(new TEventRecord(args ...)));
    }

private:
    using LogEntriesT = tbb::concurrent_vector<Entry>;

    LogEntriesT     entries_;
    TimestampT      startTime_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_LOGGING_EVENTLOG_H_INCLUDED
// #include "react/logging/EventRecords.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_LOGGING_EVENTRECORDS_H_INCLUDED
#define REACT_DETAIL_LOGGING_EVENTRECORDS_H_INCLUDED



// #include "react/detail/Defs.h"


#include <iostream>
#include <string>
#include <thread>

// #include "Logging.h"

// #include "react/common/Types.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeCreateEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeCreateEvent : public IEventRecord
{
public:
    NodeCreateEvent(ObjectId nodeId, const char* type);

    virtual const char* EventId() const { return "NodeCreate"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId        nodeId_;
    const char *    type_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeDestroyEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeDestroyEvent : public IEventRecord
{
public:
    NodeDestroyEvent(ObjectId nodeId);

    virtual const char* EventId() const { return "NodeDestroy"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeAttachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeAttachEvent : public IEventRecord
{
public:
    NodeAttachEvent(ObjectId nodeId, ObjectId parentId);

    virtual const char* EventId() const { return "NodeAttach"; }
    
    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    ObjectId    parentId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeDetachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeDetachEvent : public IEventRecord
{
public:
    NodeDetachEvent(ObjectId nodeId, ObjectId parentId);

    virtual const char* EventId() const { return "NodeDetach"; }
    
    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    ObjectId    parentId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// InputNodeAdmissionEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class InputNodeAdmissionEvent : public IEventRecord
{
public:
    InputNodeAdmissionEvent(ObjectId nodeId, int transactionId);

    virtual const char* EventId() const { return "InputNodeAdmission"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    int         transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodePulseEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodePulseEvent : public IEventRecord
{
public:
    NodePulseEvent(ObjectId nodeId, int transactionId);

    virtual const char* EventId() const { return "NodePulse"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    int         transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeIdlePulseEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeIdlePulseEvent : public IEventRecord
{
public:
    NodeIdlePulseEvent(ObjectId nodeId, int transactionId);

    virtual const char* EventId() const { return "NodeIdlePulse"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    int         transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DynamicNodeAttachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class DynamicNodeAttachEvent : public IEventRecord
{
public:
    DynamicNodeAttachEvent(ObjectId nodeId, ObjectId parentId, int transactionId);

    virtual const char* EventId() const { return "DynamicNodeAttach"; }
    
    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    ObjectId    parentId_;
    int         transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DynamicNodeDetachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class DynamicNodeDetachEvent : public IEventRecord
{
public:
    DynamicNodeDetachEvent(ObjectId nodeId, ObjectId parentId, int transactionId);

    virtual const char* EventId() const { return "DynamicNodeDetach"; }
    
    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId    nodeId_;
    ObjectId    parentId_;
    int         transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeEvaluateBeginEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeEvaluateBeginEvent : public IEventRecord
{
public:
    NodeEvaluateBeginEvent(ObjectId nodeId, int transactionId);

    virtual const char* EventId() const { return "NodeEvaluateBegin"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId        nodeId_;
    int             transactionId_;
    std::thread::id threadId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeEvaluateEndEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class NodeEvaluateEndEvent : public IEventRecord
{
public:
    NodeEvaluateEndEvent(ObjectId nodeId, int transactionId);

    virtual const char* EventId() const { return "NodeEvaluateEnd"; }

    virtual void Serialize(std::ostream& out) const;

private:
    ObjectId        nodeId_;
    int             transactionId_;
    std::thread::id threadId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionBeginEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class TransactionBeginEvent : public IEventRecord
{
public:
    TransactionBeginEvent(int transactionId);

    virtual const char* EventId() const { return "TransactionBegin"; }

    virtual void Serialize(std::ostream& out) const;

private:
    int transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionEndEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class TransactionEndEvent : public IEventRecord
{
public:
    TransactionEndEvent(int transactionId);

    virtual const char* EventId() const { return "TransactionEnd"; }

    virtual void Serialize(std::ostream& out) const;

private:
    int transactionId_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UserBreakpointEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
class UserBreakpointEvent : public IEventRecord
{
public:
    UserBreakpointEvent(const char* name);

    virtual const char* EventId() const { return "UserBreakpoint"; }

    virtual void Serialize(std::ostream& out) const;

private:
    std::string name_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_LOGGING_EVENTRECORDS_H_INCLUDED
// #include "react/logging/Logging.h"

// #include "react/Domain.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DOMAIN_H_INCLUDED
#define REACT_DOMAIN_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "react/detail/DomainBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_DOMAINBASE_H_INCLUDED
#define REACT_DETAIL_DOMAINBASE_H_INCLUDED



#include <memory>
#include <utility>

// #include "react/detail/Defs.h"


// #include "react/detail/ReactiveBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_REACTIVEBASE_H_INCLUDED
#define REACT_DETAIL_REACTIVEBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "react/detail/graph/GraphBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_GRAPHBASE_H_INCLUDED
#define REACT_DETAIL_GRAPH_GRAPHBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "react/common/Util.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_UTIL_H_INCLUDED
#define REACT_COMMON_UTIL_H_INCLUDED



// #include "react/detail/Defs.h"


#include <tuple>
#include <type_traits>
#include <utility>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Unpack tuple - see
/// http://stackoverflow.com/questions/687490/how-do-i-expand-a-tuple-into-variadic-template-functions-arguments
///////////////////////////////////////////////////////////////////////////////////////////////////

template<size_t N>
struct Apply {
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T && t, A &&... a)
        -> decltype(Apply<N-1>::apply(std::forward<F>(f), std::forward<T>(t), std::get<N-1>(std::forward<T>(t)), std::forward<A>(a)...))
    {
        return Apply<N-1>::apply(std::forward<F>(f), std::forward<T>(t), std::get<N-1>(std::forward<T>(t)), std::forward<A>(a)...);
    }
};

template<>
struct Apply<0>
{
    template<typename F, typename T, typename... A>
    static inline auto apply(F && f, T &&, A &&... a)
        -> decltype(std::forward<F>(f)(std::forward<A>(a)...))
    {
        return std::forward<F>(f)(std::forward<A>(a)...);
    }
};

template<typename F, typename T>
inline auto apply(F && f, T && t)
    -> decltype(Apply<std::tuple_size<typename std::decay<T>::type>::value>::apply(std::forward<F>(f), std::forward<T>(t)))
{
    return Apply<std::tuple_size<typename std::decay<T>::type>::value>
        ::apply(std::forward<F>(f), std::forward<T>(t));
}

template<size_t N>
struct ApplyMemberFn {
    template <typename O, typename F, typename T, typename... A>
    static inline auto apply(O obj, F f, T && t, A &&... a)
        -> decltype(ApplyMemberFn<N-1>::apply(obj, f, std::forward<T>(t), std::get<N-1>(std::forward<T>(t)), std::forward<A>(a)...))
    {
        return ApplyMemberFn<N-1>::apply(obj, f, std::forward<T>(t), std::get<N-1>(std::forward<T>(t)), std::forward<A>(a)...);
    }
};

template<>
struct ApplyMemberFn<0>
{
    template<typename O, typename F, typename T, typename... A>
    static inline auto apply(O obj, F f, T &&, A &&... a)
        -> decltype((obj->*f)(std::forward<A>(a)...))
    {
        return (obj->*f)(std::forward<A>(a)...);
    }
};

template <typename O, typename F, typename T>
inline auto applyMemberFn(O obj, F f, T&& t)
    -> decltype(ApplyMemberFn<std::tuple_size<typename std::decay<T>::type>::value>::apply(obj, f, std::forward<T>(t)))
{
    return ApplyMemberFn<std::tuple_size<typename std::decay<T>::type>::value>
        ::apply(obj, f, std::forward<T>(t));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Helper to enable calling a function on each element of an argument pack.
/// We can't do f(args) ...; because ... expands with a comma.
/// But we can do nop_func(f(args) ...);
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename... TArgs>
inline void pass(TArgs&& ...) {}

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
struct DontMove {};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DisableIfSame
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T, typename U>
struct DisableIfSame :
    std::enable_if<! std::is_same<
        typename std::decay<T>::type,
        typename std::decay<U>::type>::value> {};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddDummyArgWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TArg, typename F, typename TRet, typename ... TDepValues>
struct AddDummyArgWrapper
{
    AddDummyArgWrapper(const AddDummyArgWrapper& other) = default;

    AddDummyArgWrapper(AddDummyArgWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn,AddDummyArgWrapper>::type>
    explicit AddDummyArgWrapper(FIn&& func) : MyFunc( std::forward<FIn>(func) ) {}

    TRet operator()(TArg, TDepValues& ... args)
    {
        return MyFunc(args ...);
    }

    F MyFunc;
};

template <typename TArg, typename F, typename ... TDepValues>
struct AddDummyArgWrapper<TArg,F,void,TDepValues...>
{
public:
    AddDummyArgWrapper(const AddDummyArgWrapper& other) = default;

    AddDummyArgWrapper(AddDummyArgWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template <typename FIn, class = typename DisableIfSame<FIn,AddDummyArgWrapper>::type>
    explicit AddDummyArgWrapper(FIn&& func) : MyFunc( std::forward<FIn>(func) ) {}

    void operator()(TArg, TDepValues& ... args)
    {
        MyFunc(args ...);
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddDefaultReturnValueWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename F,
    typename TRet,
    TRet return_value
>
struct AddDefaultReturnValueWrapper
{
    AddDefaultReturnValueWrapper(const AddDefaultReturnValueWrapper&) = default;

    AddDefaultReturnValueWrapper(AddDefaultReturnValueWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template
    <
        typename FIn,
        class = typename DisableIfSame<FIn,AddDefaultReturnValueWrapper>::type
    >
    explicit AddDefaultReturnValueWrapper(FIn&& func) :
        MyFunc( std::forward<FIn>(func) )
    {}

    template <typename ... TArgs>
    TRet operator()(TArgs&& ... args)
    {
        MyFunc(std::forward<TArgs>(args) ...);
        return return_value;
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsCallableWith
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename F,
    typename TRet,
    typename ... TArgs
>
class IsCallableWith
{
private:
    using NoT = char[1];
    using YesT = char[2];

    template
    <
        typename U,
        class = decltype(
            static_cast<TRet>(
                (std::declval<U>())(std::declval<TArgs>() ...)))
    >
    static YesT& check(void*);

    template <typename U>
    static NoT& check(...);

public:
    enum { value = sizeof(check<F>(nullptr)) == sizeof(YesT) };
};

/****************************************/ REACT_IMPL_END /***************************************/

// Expand args by wrapping them in a dummy function
// Use comma operator to replace potential void return value with 0
#define REACT_EXPAND_PACK(...) REACT_IMPL::pass((__VA_ARGS__ , 0) ...)

#endif // REACT_COMMON_UTIL_H_INCLUDED
// #include "react/common/Timing.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_TIMING_H_INCLUDED
#define REACT_COMMON_TIMING_H_INCLUDED



// #include "react/detail/Defs.h"


#include <utility>

#if _WIN32 || _WIN64
    #define REACT_FIXME_CUSTOM_TIMER 1
#else
    #define REACT_FIXME_CUSTOM_TIMER 0
#endif

#if REACT_FIXME_CUSTOM_TIMER
    #include <windows.h>
#else
    #include <chrono>
#endif

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GetPerformanceFrequency
///////////////////////////////////////////////////////////////////////////////////////////////////
// Todo: Initialization not thread-safe
#if REACT_FIXME_CUSTOM_TIMER
inline const LARGE_INTEGER& GetPerformanceFrequency()
{
    static bool init = false;
    static LARGE_INTEGER frequency;

    if (init == false)
    {
        QueryPerformanceFrequency(&frequency);
        init = true;
    }

    return frequency;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ConditionalTimer
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    long long threshold,
    bool is_enabled
>
class ConditionalTimer
{
public:
    class ScopedTimer
    {
    public:
        // Note:
        // Count is passed by ref so it can be set later if it's not known at time of creation
        ScopedTimer(const ConditionalTimer&, const size_t& count);
    };

    void Reset();
    void ForceThresholdExceeded(bool isExceeded);
    bool IsThresholdExceeded() const;
};

// Disabled
template
<
    long long threshold
>
class ConditionalTimer<threshold,false>
{
public:
    // Defines scoped timer that does nothing
    class ScopedTimer
    {
    public:
        ScopedTimer(const ConditionalTimer&, const size_t& count) {}
    };

    void Reset()                                    {}
    void ForceThresholdExceeded(bool isExceeded)    {}
    bool IsThresholdExceeded() const                { return false; }
};

// Enabled
template
<
    long long threshold
>
class ConditionalTimer<threshold,true>
{
public:
#if REACT_FIXME_CUSTOM_TIMER
    using TimestampT = LARGE_INTEGER;
#else
    using ClockT = std::chrono::high_resolution_clock;
    using TimestampT = std::chrono::time_point<ClockT>;
#endif

    class ScopedTimer
    {
    public:
        ScopedTimer(ConditionalTimer& parent, const size_t& count) :
            parent_( parent ),
            count_( count )
        {
            if (!parent_.shouldMeasure_)
                return;

            startMeasure();
        }

        ~ScopedTimer()
        {
            if (!parent_.shouldMeasure_)
                return;

            parent_.shouldMeasure_ = false;
            
            endMeasure();
        }

    private:
        void startMeasure()
        {
            startTime_ = now();
        }

        void endMeasure()
        {
#if REACT_FIXME_CUSTOM_TIMER
            TimestampT endTime = now();

            LARGE_INTEGER durationUS;

            durationUS.QuadPart = endTime.QuadPart - startTime_.QuadPart;
            durationUS.QuadPart *= 1000000;
            durationUS.QuadPart /= GetPerformanceFrequency().QuadPart;

            parent_.isThresholdExceeded_ = (durationUS.QuadPart - (threshold * count_)) > 0;
#else
            using std::chrono::duration_cast;
            using std::chrono::microseconds;

            parent_.isThresholdExceeded_ =
                duration_cast<microseconds>(now() - startTime_).count() > (threshold * count_);
#endif
        }

        ConditionalTimer&   parent_;
        TimestampT          startTime_;
        const size_t&       count_;
    };

    static TimestampT now()
    {
#if REACT_FIXME_CUSTOM_TIMER
        TimestampT result;
        QueryPerformanceCounter(&result);
        return result;
#else
        return ClockT::now();
#endif
    }

    void Reset()
    {
        shouldMeasure_ = true;
        isThresholdExceeded_ = false;
    }

    void ForceThresholdExceeded(bool isExceeded)
    {
        shouldMeasure_ = false;
        isThresholdExceeded_ = isExceeded;
    }

    bool IsThresholdExceeded() const { return isThresholdExceeded_; }

private:
    // Only measure once
    bool shouldMeasure_ = true;

    // Until we have measured, assume the threshold is exceeded.
    // The cost of initially not parallelizing what should be parallelized is much higher
    // than for the other way around.
    bool isThresholdExceeded_ = true;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_TIMING_H_INCLUDED
// #include "react/common/Types.h"

// #include "react/detail/IReactiveEngine.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_IREACTIVEENGINE_H_INCLUDED
#define REACT_DETAIL_IREACTIVEENGINE_H_INCLUDED



#include <memory>
#include <utility>

// #include "react/detail/Defs.h"

// #include "react/common/Types.h"

// #include "react/logging/EventRecords.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactiveEngine
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename TNode,
    typename TTurn
>
struct IReactiveEngine
{
    using NodeT = TNode;
    using TurnT = TTurn;

    void OnTurnAdmissionStart(TurnT& turn)  {}
    void OnTurnAdmissionEnd(TurnT& turn)    {}

    void OnInputChange(NodeT& node, TurnT& turn)    {}

    void Propagate(TurnT& turn)  {}

    void OnNodeCreate(NodeT& node)  {}
    void OnNodeDestroy(NodeT& node) {}

    void OnNodeAttach(NodeT& node, NodeT& parent)   {}
    void OnNodeDetach(NodeT& node, NodeT& parent)   {}

    void OnNodePulse(NodeT& node, TurnT& turn)      {}
    void OnNodeIdlePulse(NodeT& node, TurnT& turn)  {}

    void OnDynamicNodeAttach(NodeT& node, NodeT& parent, TurnT& turn)    {}
    void OnDynamicNodeDetach(NodeT& node, NodeT& parent, TurnT& turn)    {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineInterface - Static wrapper for IReactiveEngine
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TEngine
>
struct EngineInterface
{
    using NodeT = typename TEngine::NodeT;
    using TurnT = typename TEngine::TurnT;

    static TEngine& Instance()
    {
        static TEngine engine;
        return engine;
    }

    static void OnTurnAdmissionStart(TurnT& turn)
    {
        Instance().OnTurnAdmissionStart(turn);
    }

    static void OnTurnAdmissionEnd(TurnT& turn)
    {
        Instance().OnTurnAdmissionEnd(turn);
    }

    static void OnInputChange(NodeT& node, TurnT& turn)
    {
        REACT_LOG(D::Log().template Append<InputNodeAdmissionEvent>(
            GetObjectId(node), turn.Id()));
        Instance().OnInputChange(node, turn);
    }

    static void Propagate(TurnT& turn)
    {
        Instance().Propagate(turn);
    }

    static void OnNodeCreate(NodeT& node)
    {
        REACT_LOG(D::Log().template Append<NodeCreateEvent>(
            GetObjectId(node), node.GetNodeType()));
        Instance().OnNodeCreate(node);
    }

    static void OnNodeDestroy(NodeT& node)
    {
        REACT_LOG(D::Log().template Append<NodeDestroyEvent>(
            GetObjectId(node)));
        Instance().OnNodeDestroy(node);
    }

    static void OnNodeAttach(NodeT& node, NodeT& parent)
    {
        REACT_LOG(D::Log().template Append<NodeAttachEvent>(
            GetObjectId(node), GetObjectId(parent)));
        Instance().OnNodeAttach(node, parent);
    }

    static void OnNodeDetach(NodeT& node, NodeT& parent)
    {
        REACT_LOG(D::Log().template Append<NodeDetachEvent>(
            GetObjectId(node), GetObjectId(parent)));
        Instance().OnNodeDetach(node, parent);
    }

    static void OnNodePulse(NodeT& node, TurnT& turn)
    {
        REACT_LOG(D::Log().template Append<NodePulseEvent>(
            GetObjectId(node), turn.Id()));
        Instance().OnNodePulse(node, turn);
    }

    static void OnNodeIdlePulse(NodeT& node, TurnT& turn)
    {
        REACT_LOG(D::Log().template Append<NodeIdlePulseEvent>(
            GetObjectId(node), turn.Id()));
        Instance().OnNodeIdlePulse(node, turn);
    }

    static void OnDynamicNodeAttach(NodeT& node, NodeT& parent, TurnT& turn)
    {
        REACT_LOG(D::Log().template Append<DynamicNodeAttachEvent>(
            GetObjectId(node), GetObjectId(parent), turn.Id()));
        Instance().OnDynamicNodeAttach(node, parent, turn);
    }

    static void OnDynamicNodeDetach(NodeT& node, NodeT& parent, TurnT& turn)
    {
        REACT_LOG(D::Log().template Append<DynamicNodeDetachEvent>(
            GetObjectId(node), GetObjectId(parent), turn.Id()));
        Instance().OnDynamicNodeDetach(node, parent, turn);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Engine traits
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename> struct NodeUpdateTimerEnabled : std::false_type {};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EPropagationMode
///////////////////////////////////////////////////////////////////////////////////////////////////
enum EPropagationMode
{
    sequential_propagation,
    parallel_propagation
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_IREACTIVEENGINE_H_INCLUDED
// #include "react/detail/ObserverBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_OBSERVERBASE_H_INCLUDED
#define REACT_DETAIL_OBSERVERBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <vector>
#include <utility>

// #include "IReactiveNode.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_IREACTIVENODE_H_INCLUDED
#define REACT_DETAIL_IREACTIVENODE_H_INCLUDED



// #include "react/detail/Defs.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IReactiveNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IReactiveNode
{
    virtual ~IReactiveNode() = default;

    /// Returns unique type identifier
    virtual const char* GetNodeType() const = 0;

    // Note: Could get rid of this ugly ptr by adding a template parameter to the interface
    // But that would mean all engine nodes need that template parameter too - so rather cast
    virtual void    Tick(void* turnPtr) = 0;

    /// Input nodes can be manipulated externally.
    virtual bool    IsInputNode() const = 0;

    /// Output nodes can't have any successors.
    virtual bool    IsOutputNode() const = 0;

    /// Dynamic nodes may change in topology as a result of tick.
    virtual bool    IsDynamicNode() const = 0;

    // Number of predecessors.
    // This information is statically available at compile time on the graph layer,
    // so the engine does not have to calculate it again.
    virtual int     DependencyCount() const = 0;

    // Heavyweight nodes are worth parallelizing.
    virtual bool    IsHeavyweight() const = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IInputNode
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IInputNode
{
    virtual ~IInputNode() = default;

    virtual bool ApplyInput(void* turnPtr) = 0;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif //REACT_DETAIL_IREACTIVENODE_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
class IObserver
{
public:
    virtual ~IObserver() {}

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
        for (const auto& p : observers_)
            if (p != nullptr)
                p->detachObserver();
    }

    void RegisterObserver(std::unique_ptr<IObserver>&& obsPtr)
    {
        observers_.push_back(std::move(obsPtr));
    }

    void UnregisterObserver(IObserver* rawObsPtr)
    {
        for (auto it = observers_.begin(); it != observers_.end(); ++it)
        {
            if (it->get() == rawObsPtr)
            {
                it->get()->detachObserver();
                observers_.erase(it);
                break;
            }
        }
    }

private:
    std::vector<std::unique_ptr<IObserver>> observers_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_OBSERVERBASE_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// WeightHint
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class WeightHint
{
    automatic,
    light,
    heavy
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UpdateTimingPolicy
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    long long threshold
>
class UpdateTimingPolicy :
    private ConditionalTimer<threshold, D::uses_node_update_timer>
{
    using ScopedTimer = typename UpdateTimingPolicy::ScopedTimer;

public:
    class ScopedUpdateTimer : private ScopedTimer
    {
    public:
        ScopedUpdateTimer(UpdateTimingPolicy& parent, const size_t& count) :
            ScopedTimer( parent, count )
        {}
    };

    void ResetUpdateThreshold()                 { this->Reset(); }
    void ForceUpdateThresholdExceeded(bool v)   { this->ForceThresholdExceeded(v); }
    bool IsUpdateThresholdExceeded() const      { return this->IsThresholdExceeded(); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DepCounter
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct CountHelper { static const int value = T::dependency_count; };

template <typename T>
struct CountHelper<std::shared_ptr<T>> { static const int value = 1; };

template <int N, typename... Args>
struct DepCounter;

template <>
struct DepCounter<0> { static int const value = 0; };

template <int N, typename First, typename... Args>
struct DepCounter<N, First, Args...>
{
    static int const value =
        CountHelper<typename std::decay<First>::type>::value + DepCounter<N-1,Args...>::value;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class NodeBase :
    public D::Policy::Engine::NodeT,
    public UpdateTimingPolicy<D,500>
{
public:
    using DomainT = D;
    using Policy = typename D::Policy;
    using Engine = typename D::Engine;
    using NodeT = typename Engine::NodeT;
    using TurnT = typename Engine::TurnT;

    NodeBase() = default;

    // Nodes can't be copied
    NodeBase(const NodeBase&) = delete;
    
    virtual bool    IsInputNode() const override    { return false; }
    virtual bool    IsOutputNode() const override   { return false; }
    virtual bool    IsDynamicNode() const override  { return false; }
    
    virtual bool    IsHeavyweight() const override  { return this->IsUpdateThresholdExceeded(); }

    void SetWeightHint(WeightHint weight)
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
    }
};

template <typename D>
using NodeBasePtrT = std::shared_ptr<NodeBase<D>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObservableNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObservableNode :
    public NodeBase<D>,
    public Observable<D>
{
public:
    ObservableNode() = default;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Attach/detach helper functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename TNode, typename ... TDeps>
struct AttachFunctor
{
    AttachFunctor(TNode& node) : MyNode( node ) {}

    void operator()(const TDeps& ...deps) const
    {
        REACT_EXPAND_PACK(attach(deps));
    }

    template <typename T>
    void attach(const T& op) const
    {
        op.template AttachRec<D,TNode>(*this);
    }

    template <typename T>
    void attach(const std::shared_ptr<T>& depPtr) const
    {
        D::Engine::OnNodeAttach(MyNode, *depPtr);
    }

    TNode& MyNode;
};

template <typename D, typename TNode, typename ... TDeps>
struct DetachFunctor
{
    DetachFunctor(TNode& node) : MyNode( node ) {}

    void operator()(const TDeps& ... deps) const
    {
        REACT_EXPAND_PACK(detach(deps));
    }

    template <typename T>
    void detach(const T& op) const
    {
        op.template DetachRec<D,TNode>(*this);
    }

    template <typename T>
    void detach(const std::shared_ptr<T>& depPtr) const
    {
        D::Engine::OnNodeDetach(MyNode, *depPtr);
    }

    TNode& MyNode;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ReactiveOpBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename ... TDeps>
class ReactiveOpBase
{
public:
    using DepHolderT = std::tuple<TDeps...>;

    template <typename ... TDepsIn>
    ReactiveOpBase(DontMove, TDepsIn&& ... deps) :
        deps_( std::forward<TDepsIn>(deps) ... )
    {}

    ReactiveOpBase(ReactiveOpBase&& other) :
        deps_( std::move(other.deps_) )
    {}

    // Can't be copied, only moved
    ReactiveOpBase(const ReactiveOpBase& other) = delete;

    template <typename D, typename TNode>
    void Attach(TNode& node) const
    {
        apply(AttachFunctor<D,TNode,TDeps...>{ node }, deps_);
    }

    template <typename D, typename TNode>
    void Detach(TNode& node) const
    {
        apply(DetachFunctor<D,TNode,TDeps...>{ node }, deps_);
    }

    template <typename D, typename TNode, typename TFunctor>
    void AttachRec(const TFunctor& functor) const
    {
        // Same memory layout, different func
        apply(reinterpret_cast<const AttachFunctor<D,TNode,TDeps...>&>(functor), deps_);
    }

    template <typename D, typename TNode, typename TFunctor>
    void DetachRec(const TFunctor& functor) const
    {
        apply(reinterpret_cast<const DetachFunctor<D,TNode,TDeps...>&>(functor), deps_);
    }

public:
    static const int dependency_count = DepCounter<sizeof...(TDeps), TDeps...>::value;

protected:
    DepHolderT   deps_;
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
    EventRange(const EventRange&) = default;

    // Copy assignment
    EventRange& operator=(const EventRange&) = default;

    const_iterator begin() const    { return data_.begin(); }
    const_iterator end() const      { return data_.end(); }

    size_type   Size() const        { return data_.size(); }
    bool        IsEmpty() const     { return data_.empty(); }

    explicit EventRange(const std::vector<E>& data) :
        data_( data )
    {}

private:
    const std::vector<E>&    data_;
};

template <typename E>
using EventEmitter = std::back_insert_iterator<std::vector<E>>;

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_GRAPHBASE_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <typename L, typename R>
bool Equals(const L& lhs, const R& rhs)
{
    return lhs == rhs;
}

template <typename L, typename R>
bool Equals(const std::reference_wrapper<L>& lhs, const std::reference_wrapper<R>& rhs)
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
    using DomainT   = typename TNode::DomainT;
    
    // Default ctor
    ReactiveBase() = default;

    // Copy ctor
    ReactiveBase(const ReactiveBase&) = default;

    // Move ctor (VS2013 doesn't default generate that yet)
    ReactiveBase(ReactiveBase&& other) :
        ptr_( std::move(other.ptr_) )
    {}

    // Explicit node ctor
    explicit ReactiveBase(std::shared_ptr<TNode>&& ptr) :
        ptr_( std::move(ptr) )
    {}

    // Copy assignment
    ReactiveBase& operator=(const ReactiveBase&) = default;

    // Move assignment
    ReactiveBase& operator=(ReactiveBase&& other)
    {
        ptr_.reset(std::move(other));
        return *this;
    }

    bool IsValid() const
    {
        return ptr_ != nullptr;
    }

    void SetWeightHint(WeightHint weight)
    {
        assert(IsValid());
        ptr_->SetWeightHint(weight);
    }

protected:
    std::shared_ptr<TNode>  ptr_;

    template <typename TNode_>
    friend const std::shared_ptr<TNode_>& GetNodePtr(const ReactiveBase<TNode_>& node);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// CopyableReactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
class CopyableReactive : public ReactiveBase<TNode>
{
public:
    CopyableReactive() = default;

    CopyableReactive(const CopyableReactive&) = default;

    CopyableReactive(CopyableReactive&& other) :
        CopyableReactive::ReactiveBase( std::move(other) )
    {}

    explicit CopyableReactive(std::shared_ptr<TNode>&& ptr) :
        CopyableReactive::ReactiveBase( std::move(ptr) )
    {}

    CopyableReactive& operator=(const CopyableReactive&) = default;

    CopyableReactive& operator=(CopyableReactive&& other)
    {
        CopyableReactive::ReactiveBase::operator=(std::move(other));
        return *this;
    }

    bool Equals(const CopyableReactive& other) const
    {
        return this->ptr_ == other.ptr_;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UniqueReactiveBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
class MovableReactive : public ReactiveBase<TNode>
{
public:
    MovableReactive() = default;

    MovableReactive(MovableReactive&& other) :
        MovableReactive::ReactiveBase( std::move(other) )
    {}

    explicit MovableReactive(std::shared_ptr<TNode>&& ptr) :
        MovableReactive::ReactiveBase( std::move(ptr) )
    {}

    MovableReactive& operator=(MovableReactive&& other)
    {
        MovableReactive::ReactiveBase::operator=(std::move(other));
        return *this;
    }

    // Deleted copy ctor and assignment
    MovableReactive(const MovableReactive&) = delete;
    MovableReactive& operator=(const MovableReactive&) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// GetNodePtr
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
const std::shared_ptr<TNode>& GetNodePtr(const ReactiveBase<TNode>& node)
{
    return node.ptr_;
}

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_REACTIVEBASE_H_INCLUDED
// #include "react/detail/IReactiveEngine.h"

// #include "react/detail/graph/ContinuationNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_CONTINUATIONNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_CONTINUATIONNODES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <functional>
#include <memory>
#include <utility>

// #include "GraphBase.h"


// #include "react/detail/ReactiveInput.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_REACTIVEINPUT_H_INCLUDED
#define REACT_DETAIL_REACTIVEINPUT_H_INCLUDED



// #include "react/detail/Defs.h"


#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <utility>
#include <type_traits>
#include <vector>

#include "tbb/task.h"
#include "tbb/concurrent_queue.h"
#include "tbb/enumerable_thread_specific.h"
#include "tbb/queuing_mutex.h"

// #include "IReactiveEngine.h"

// #include "ObserverBase.h"

// #include "react/common/Concurrency.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_CONCURRENCY_H_INCLUDED
#define REACT_COMMON_CONCURRENCY_H_INCLUDED



// #include "react/detail/Defs.h"


#include <condition_variable>
#include <mutex>

// #include "react/common/RefCounting.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_REF_COUNTING_H_INCLUDED
#define REACT_COMMON_REF_COUNTING_H_INCLUDED



// #include "react/detail/Defs.h"


#include <condition_variable>
#include <mutex>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

// A non-exception-safe pointer wrapper with conditional intrusive ref counting.
template <typename T>
class IntrusiveRefCountingPtr
{
    enum
    {
        tag_ref_counted = 0x1
    };

public:
    IntrusiveRefCountingPtr() :
        ptrData_( reinterpret_cast<uintptr_t>(nullptr) )
    {}

    IntrusiveRefCountingPtr(T* ptr) :
        ptrData_( reinterpret_cast<uintptr_t>(ptr) )
    {
        if (ptr != nullptr && ptr->IsRefCounted())
        {
            ptr->IncRefCount();
            ptrData_ |= tag_ref_counted;
        }
    }

    IntrusiveRefCountingPtr(const IntrusiveRefCountingPtr& other) :
        ptrData_( other.ptrData_ )
    {
        if (isValidRefCountedPtr())
            Get()->IncRefCount();
    }

    IntrusiveRefCountingPtr(IntrusiveRefCountingPtr&& other) :
        ptrData_( other.ptrData_ )
    {
        other.ptrData_ = reinterpret_cast<uintptr_t>(nullptr);
    }

    ~IntrusiveRefCountingPtr()
    {
        if (isValidRefCountedPtr())
            Get()->DecRefCount();
    }

    IntrusiveRefCountingPtr& operator=(const IntrusiveRefCountingPtr& other)
    {
        if (this != &other)
        {
            if (other.isValidRefCountedPtr())
                other.Get()->IncRefCount();

            if (isValidRefCountedPtr())
                Get()->DecRefCount();

            ptrData_ = other.ptrData_;
        }

        return *this;
    }

    IntrusiveRefCountingPtr& operator=(IntrusiveRefCountingPtr&& other)
    {
        if (this != &other)
        {
            if (isValidRefCountedPtr())
                Get()->DecRefCount();

            ptrData_ = other.ptrData_;
            other.ptrData_ = reinterpret_cast<uintptr_t>(nullptr);
        }

        return *this;
    }

    T* Get() const          { return reinterpret_cast<T*>(ptrData_ & ~tag_ref_counted); }

    T& operator*() const    { return *Get(); }
    T* operator->() const   { return Get(); }

    bool operator==(const IntrusiveRefCountingPtr& other) const { return Get() == other.Get(); }
    bool operator!=(const IntrusiveRefCountingPtr& other) const { return !(*this == other); }

private:
    bool isValidRefCountedPtr() const
    {
        return ptrData_ != reinterpret_cast<uintptr_t>(nullptr)
            && (ptrData_ & tag_ref_counted) != 0;
    }

    uintptr_t   ptrData_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_REF_COUNTING_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IWaitingState
///////////////////////////////////////////////////////////////////////////////////////////////////
class IWaitingState
{
public:
    virtual inline ~IWaitingState() {}

    virtual void Wait() = 0;

    virtual void IncWaitCount() = 0;
    virtual void DecWaitCount() = 0;

protected:
    virtual bool IsRefCounted() const = 0;
    virtual void IncRefCount() = 0;
    virtual void DecRefCount() = 0;

    friend class IntrusiveRefCountingPtr<IWaitingState>;
};

using WaitingStatePtrT = IntrusiveRefCountingPtr<IWaitingState>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// WaitingStateBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class WaitingStateBase : public IWaitingState
{
public:
    virtual inline void Wait() override
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !isWaiting_; });
    }

    virtual inline void IncWaitCount() override
    {
        auto oldVal = waitCount_.fetch_add(1, std::memory_order_relaxed);

        if (oldVal == 0)
        {// mutex_
            std::lock_guard<std::mutex> scopedLock(mutex_);
            isWaiting_ = true;
        }// ~mutex_
    }

    virtual inline void DecWaitCount() override
    {
        auto oldVal = waitCount_.fetch_sub(1, std::memory_order_relaxed);

        if (oldVal == 1)
        {// mutex_
            std::lock_guard<std::mutex> scopedLock(mutex_);
            isWaiting_ = false;
            condition_.notify_all();
        }// ~mutex_
    }

    inline bool IsWaiting()
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        return isWaiting_;
    }// ~mutex_

    template <typename F>
    auto Run(F&& func) -> decltype(func(false))
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        return func(isWaiting_);
    }// ~mutex_

    template <typename F>
    bool RunIfWaiting(F&& func)
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);

        if (!isWaiting_)
            return false;

        func();
        return true;
    }// ~mutex_

    template <typename F>
    bool RunIfNotWaiting(F&& func)
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);

        if (isWaiting_)
            return false;

        func();
        return true;
    }// ~mutex_

private:
    std::atomic<uint>           waitCount_{ 0 };
    std::condition_variable     condition_;
    std::mutex                  mutex_;
    bool                        isWaiting_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UniqueWaitingState
///////////////////////////////////////////////////////////////////////////////////////////////////
class UniqueWaitingState : public WaitingStateBase
{
protected:
    // Do nothing
    virtual inline bool IsRefCounted() const override { return false; }
    virtual inline void IncRefCount() override {}
    virtual inline void DecRefCount() override {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SharedWaitingState
///////////////////////////////////////////////////////////////////////////////////////////////////
class SharedWaitingState : public WaitingStateBase
{
private:
    SharedWaitingState() = default;

public:
    static inline WaitingStatePtrT Create()
    {
        return WaitingStatePtrT(new SharedWaitingState());
    }


protected:
    virtual inline bool IsRefCounted() const override { return true; }

    virtual inline void IncRefCount() override
    {
        refCount_.fetch_add(1, std::memory_order_relaxed);
    }

    virtual inline void DecRefCount() override
    {
        auto oldVal = refCount_.fetch_sub(1, std::memory_order_relaxed);

        if (oldVal == 1)
            delete this;
    }

private:
    std::atomic<uint>           refCount_{ 0 };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SharedWaitingStateCollection
///////////////////////////////////////////////////////////////////////////////////////////////////
class SharedWaitingStateCollection : public IWaitingState
{
private:
    SharedWaitingStateCollection(std::vector<WaitingStatePtrT>&& others) :
        others_( std::move(others) )
    {}

public:
    static inline WaitingStatePtrT Create(std::vector<WaitingStatePtrT>&& others)
    {
        return WaitingStatePtrT(new SharedWaitingStateCollection(std::move(others)));
    }

    virtual inline bool IsRefCounted() const override { return true; }

    virtual inline void Wait() override
    {
        for (const auto& e : others_)
            e->Wait();
    }

    virtual inline void IncWaitCount() override
    {
        for (const auto& e : others_)
            e->IncWaitCount();
    }

    virtual inline void DecWaitCount() override
    {
        for (const auto& e : others_)
            e->DecWaitCount();
    }

    virtual inline void IncRefCount() override
    {
        refCount_.fetch_add(1, std::memory_order_relaxed);
    }

    virtual inline void DecRefCount() override
    {
        auto oldVal = refCount_.fetch_sub(1, std::memory_order_relaxed);

        if (oldVal == 1)
            delete this;
    }

private:
    std::atomic<uint>               refCount_{ 0 };
    std::vector<WaitingStatePtrT>   others_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// BlockingCondition
///////////////////////////////////////////////////////////////////////////////////////////////////
class BlockingCondition
{
public:
    inline void Block()
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        blocked_ = true;
    }// ~mutex_

    inline void Unblock()
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        blocked_ = false;
        condition_.notify_all();
    }// ~mutex_

    inline void WaitForUnblock()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !blocked_; });
    }

    inline bool IsBlocked()
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        return blocked_;
    }// ~mutex_

    template <typename F>
    auto Run(F&& func) -> decltype(func(false))
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);
        return func(blocked_);
    }// ~mutex_

    template <typename F>
    bool RunIfBlocked(F&& func)
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);

        if (!blocked_)
            return false;

        func();
        return true;
    }// ~mutex_

    template <typename F>
    bool RunIfUnblocked(F&& func)
    {// mutex_
        std::lock_guard<std::mutex> scopedLock(mutex_);

        if (blocked_)
            return false;

        func();
        return true;
    }// ~mutex_

private:
    std::mutex                  mutex_;
    std::condition_variable     condition_;

    bool    blocked_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ConditionalCriticalSection
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TMutex, bool is_enabled>
class ConditionalCriticalSection;

template <typename TMutex>
class ConditionalCriticalSection<TMutex,false>
{
public:
    template <typename F>
    void Access(const F& f) { f(); }
};

template <typename TMutex>
class ConditionalCriticalSection<TMutex,true>
{
public:
    template <typename F>
    void Access(const F& f)
    {// mutex_
        std::lock_guard<TMutex> lock(mutex_);

        f();
    }// ~mutex_
private:
    TMutex  mutex_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_CONCURRENCY_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

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
using TransactionFuncT = std::function<void()>;

enum ETransactionFlags
{
    allow_merging    = 1 << 0
};

using TransactionFlagsT = std::underlying_type<ETransactionFlags>::type;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EInputMode
///////////////////////////////////////////////////////////////////////////////////////////////////
enum EInputMode
{
    consecutive_input,
    concurrent_input
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IContinuationTarget
///////////////////////////////////////////////////////////////////////////////////////////////////
struct IContinuationTarget
{
    virtual void AsyncContinuation(TransactionFlagsT, const WaitingStatePtrT&,
                                   TransactionFuncT&&) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ThreadLocalInputState
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename = void>
struct ThreadLocalInputState
{
    static REACT_TLS bool   IsTransactionActive;
};

template <typename T>
REACT_TLS bool ThreadLocalInputState<T>::IsTransactionActive(false);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ContinuationManager
///////////////////////////////////////////////////////////////////////////////////////////////////
// Interface
template <EPropagationMode>
class ContinuationManager
{
public:
    void StoreContinuation(IContinuationTarget& target, TransactionFlagsT flags,
                           TransactionFuncT&& cont);

    bool HasContinuations() const;
    void StartContinuations(const WaitingStatePtrT& waitingStatePtr);

    void QueueObserverForDetach(IObserver& obs);

    template <typename D>
    void DetachQueuedObservers();
};

// Non thread-safe implementation
template <>
class ContinuationManager<sequential_propagation>
{
    struct Data_
    {
        Data_(IContinuationTarget* target, TransactionFlagsT flags, TransactionFuncT&& func) :
            Target( target ),
            Flags( flags ),
            Func( func )
        {}

        IContinuationTarget*    Target;
        TransactionFlagsT       Flags;
        TransactionFuncT        Func;
    };

    using DataVectT = std::vector<Data_>;
    using ObsVectT  = std::vector<IObserver*>;

public:
    void StoreContinuation(IContinuationTarget& target, TransactionFlagsT flags,
                           TransactionFuncT&& cont)
    {
        storedContinuations_.emplace_back(&target, flags, std::move(cont));
    }

    bool HasContinuations() const
    {
        return !storedContinuations_.empty();
    }

    void StartContinuations(const WaitingStatePtrT& waitingStatePtr)
    {
        for (auto& t : storedContinuations_)
            t.Target->AsyncContinuation(t.Flags, waitingStatePtr, std::move(t.Func));

        storedContinuations_.clear();
    }

    // Todo: Move this somewhere else
    void QueueObserverForDetach(IObserver& obs)
    {
        detachedObservers_.push_back(&obs);
    }

    template <typename D>
    void DetachQueuedObservers()
    {
        for (auto* o : detachedObservers_)
            o->UnregisterSelf();
        detachedObservers_.clear();
    }

private:
    DataVectT   storedContinuations_;
    ObsVectT    detachedObservers_;
};

// Thread-safe implementation
template <>
class ContinuationManager<parallel_propagation>
{
    struct Data_
    {
        Data_(IContinuationTarget* target, TransactionFlagsT flags, TransactionFuncT&& func) :
            Target( target ),
            Flags( flags ),
            Func( func )
        {}

        IContinuationTarget*    Target;
        TransactionFlagsT       Flags;
        TransactionFuncT        Func;
    };

    using DataVecT  = std::vector<Data_>;
    using ObsVectT  = std::vector<IObserver*>;

public:
    void StoreContinuation(IContinuationTarget& target, TransactionFlagsT flags,
                           TransactionFuncT&& cont)
    {
        storedContinuations_.local().emplace_back(&target, flags, std::move(cont));
        ++contCount_;
    }

    bool HasContinuations() const
    {
        return contCount_ != 0;
    }

    void StartContinuations(const WaitingStatePtrT& waitingStatePtr)
    {
        for (auto& v : storedContinuations_)
            for (auto& t : v)
                t.Target->AsyncContinuation(t.Flags, waitingStatePtr, std::move(t.Func));

        storedContinuations_.clear();
        contCount_ = 0;
    }

    void QueueObserverForDetach(IObserver& obs)
    {
        detachedObservers_.local().push_back(&obs);
    }

    template <typename D>
    void DetachQueuedObservers()
    {
        for (auto& v : detachedObservers_)
        {
            for (auto* o : v)
                o->UnregisterSelf();
            v.clear();
        }
    }

private:
    tbb::enumerable_thread_specific<DataVecT>   storedContinuations_;
    tbb::enumerable_thread_specific<ObsVectT>   detachedObservers_;

    size_t contCount_ = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionQueue
///////////////////////////////////////////////////////////////////////////////////////////////////
// Interface
template <EInputMode>
class TransactionQueue
{
public:
    class QueueEntry
    {
    public:
        explicit QueueEntry(TransactionFlagsT flags);

        void RunMergedInputs();

        size_t  GetWaitingStatePtrCount() const;
        void    MoveWaitingStatePtrs(std::vector<WaitingStatePtrT>& out);

        void Release();

        bool IsAsync() const;
    };

    template <typename F>
    bool TryMergeSync(F&& inputFunc);

    template <typename F>
    bool TryMergeAsync(F&& inputFunc, WaitingStatePtrT&& waitingStatePtr);

    void EnterQueue(QueueEntry& turn);
    void ExitQueue(QueueEntry& turn);
};

// Non thread-safe implementation
template <>
class TransactionQueue<consecutive_input>
{
public:
    class QueueEntry
    {
    public:
        explicit QueueEntry(TransactionFlagsT flags) {}

        void RunMergedInputs()  {}

        size_t  GetWaitingStatePtrCount() const    { return 0; }
        void    MoveWaitingStatePtrs(std::vector<WaitingStatePtrT>& out)  {}

        void Release()  {}

        bool IsAsync() const { return false; }
    };

    template <typename F>
    bool TryMergeSync(F&& inputFunc) { return false; }

    template <typename F>
    bool TryMergeAsync(F&& inputFunc, WaitingStatePtrT&& waitingStatePtr) { return false; }

    void EnterQueue(QueueEntry& turn)   {}
    void ExitQueue(QueueEntry& turn)    {}
};

// Thread-safe implementation
template <>
class TransactionQueue<concurrent_input>
{
public:
    class QueueEntry
    {
    public:
        explicit QueueEntry(TransactionFlagsT flags) :
            isMergeable_( (flags & allow_merging) != 0 )
        {}

        void Append(QueueEntry& tr)
        {
            successor_ = &tr;
            tr.waitingState_.IncWaitCount();
            ++waitingStatePtrCount_;
        }

        void WaitForUnblock()
        {
            waitingState_.Wait();
        }

        void RunMergedInputs() const
        {
            for (const auto& e : merged_)
                e.InputFunc();
        }

        size_t GetWaitingStatePtrCount() const
        {
            return waitingStatePtrCount_;
        }

        void MoveWaitingStatePtrs(std::vector<WaitingStatePtrT>& out)
        {
            if (waitingStatePtrCount_ == 0)
                return;

            for (const auto& e : merged_)
                if (e.WaitingStatePtr != nullptr)
                    out.push_back(std::move(e.WaitingStatePtr));

            if (successor_)
            {
                out.push_back(WaitingStatePtrT( &successor_->waitingState_ ));

                // Ownership of successors waiting state has been transfered,
                // we no longer need it
                successor_ = nullptr;
            }

            waitingStatePtrCount_ = 0;
        }

        void Release()
        {
            if (waitingStatePtrCount_ == 0)
                return;

            // Release merged
            for (const auto& e : merged_)
                if (e.WaitingStatePtr != nullptr)
                    e.WaitingStatePtr->DecWaitCount();

            // Waiting state ptrs may point to stack of waiting threads.
            // After decwaitcount, they may start running and terminate.
            // Thus, clear merged ASAP because it now contains invalid ptrs.
            merged_.clear();

            // Release next thread in queue
            if (successor_)
                successor_->waitingState_.DecWaitCount();
        }

        template <typename F, typename P>
        bool TryMerge(F&& inputFunc, P&& waitingStatePtr)
        {
            if (!isMergeable_)
                return false;

            // Only merge if target is still waiting
            bool merged = waitingState_.RunIfWaiting([&] {
                if (waitingStatePtr != nullptr)
                {
                    waitingStatePtr->IncWaitCount();
                    ++waitingStatePtrCount_;
                }

                merged_.emplace_back(std::forward<F>(inputFunc), std::forward<P>(waitingStatePtr));
            });

            return merged;
        }

    private:
        struct MergedData
        {
            template <typename F, typename P>
            MergedData(F&& func, P&& waitingStatePtr) :
                InputFunc( std::forward<F>(func) ),
                WaitingStatePtr( std::forward<P>(waitingStatePtr) )
            {}

            std::function<void()>   InputFunc;
            WaitingStatePtrT        WaitingStatePtr;
        };

        using MergedDataVectT = std::vector<MergedData>;

        bool                isMergeable_;
        QueueEntry*         successor_      = nullptr;
        MergedDataVectT     merged_;
        UniqueWaitingState  waitingState_;
        size_t              waitingStatePtrCount_ = 0;
    };

    template <typename F>
    bool TryMergeSync(F&& inputFunc)
    {
        bool merged = false;

        UniqueWaitingState st;
        WaitingStatePtrT p( &st );

        {// seqMutex_
            SeqMutexT::scoped_lock lock(seqMutex_);

            if (tail_)
                merged = tail_->TryMerge(std::forward<F>(inputFunc), p);
        }// ~seqMutex_

        if (merged)
            p->Wait();

        return merged;
    }

    template <typename F>
    bool TryMergeAsync(F&& inputFunc, WaitingStatePtrT&& waitingStatePtr)
    {
        bool merged = false;

        {// seqMutex_
            SeqMutexT::scoped_lock lock(seqMutex_);

            if (tail_)
                merged = tail_->TryMerge(std::forward<F>(inputFunc), std::move(waitingStatePtr));
        }// ~seqMutex_

        return merged;
    }

    void EnterQueue(QueueEntry& tr)
    {
        {// seqMutex_
            SeqMutexT::scoped_lock lock(seqMutex_);

            if (tail_)
                tail_->Append(tr);

            tail_ = &tr;
        }// ~seqMutex_

        tr.WaitForUnblock();
    }

    void ExitQueue(QueueEntry& tr)
    {
        {// seqMutex_
            SeqMutexT::scoped_lock lock(seqMutex_);

            if (tail_ == &tr)
                tail_ = nullptr;
        }// ~seqMutex_

        tr.Release();
    }

private:
    using SeqMutexT = tbb::queuing_mutex;

    SeqMutexT       seqMutex_;
    QueueEntry*     tail_       = nullptr;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AsyncWorker
///////////////////////////////////////////////////////////////////////////////////////////////////
struct AsyncItem
{
    TransactionFlagsT       Flags;
    WaitingStatePtrT        WaitingStatePtr;
    TransactionFuncT        Func;
};

// Interface
template <typename D, EInputMode>
class AsyncWorker
{
public:
    AsyncWorker(InputManager<D>& mgr);

    void PushItem(AsyncItem&& item);

    void PopItem(AsyncItem& item);
    bool TryPopItem(AsyncItem& item);

    bool IncrementItemCount(size_t n);
    bool DecrementItemCount(size_t n);

    void Start();
};

// Disabled
template <typename D>
struct AsyncWorker<D, consecutive_input>
{
public:
    AsyncWorker(InputManager<D>& mgr)
    {}

    void PushItem(AsyncItem&& item)     { assert(false); }

    void PopItem(AsyncItem& item)       { assert(false); }
    bool TryPopItem(AsyncItem& item)    { assert(false); return false; }

    bool IncrementItemCount(size_t n)   { assert(false); return false; }
    bool DecrementItemCount(size_t n)   { assert(false); return false; }

    void Start()                        { assert(false); }
};

// Enabled
template <typename D>
struct AsyncWorker<D, concurrent_input>
{
    using DataT = tbb::concurrent_bounded_queue<AsyncItem>;

    class WorkerTask : public tbb::task
    {
    public:
        WorkerTask(InputManager<D>& mgr) :
            mgr_( mgr )
        {}

        tbb::task* execute()
        {
            mgr_.processAsyncQueue();
            return nullptr;
        }

    private:
        InputManager<D>& mgr_;
    };

public:
    AsyncWorker(InputManager<D>& mgr) :
        mgr_( mgr )
    {}

    void PushItem(AsyncItem&& item)
    {
        data_.push(std::move(item));
    }

    void PopItem(AsyncItem& item)
    {
        data_.pop(item);
    }

    bool TryPopItem(AsyncItem& item)
    {
        return data_.try_pop(item);
    }

    bool IncrementItemCount(size_t n)
    {
        return count_.fetch_add(n, std::memory_order_relaxed) == 0;
    }

    bool DecrementItemCount(size_t n)
    {
        return count_.fetch_sub(n, std::memory_order_relaxed) == n;
    }

    void Start()
    {
        tbb::task::enqueue(*new(tbb::task::allocate_root()) WorkerTask(mgr_));
    }

private:
    DataT               data_;
    std::atomic<size_t> count_{ 0 };

    InputManager<D>&    mgr_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// InputManager
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class InputManager :
    public IContinuationTarget
{
private:
    // Select between thread-safe and non thread-safe implementations
    using TransactionQueueT = TransactionQueue<D::Policy::input_mode>;
    using QueueEntryT = typename TransactionQueueT::QueueEntry;

    using ContinuationManagerT = ContinuationManager<D::Policy::propagation_mode>;
    using AsyncWorkerT = AsyncWorker<D, D::Policy::input_mode>;

    template <typename, EInputMode>
    friend class AsyncWorker;

public:
    using TurnT = typename D::TurnT;
    using Engine = typename D::Engine;

    InputManager() :
        asyncWorker_(*this)
    {}

    template <typename F>
    void DoTransaction(TransactionFlagsT flags, F&& func)
    {
        // Attempt to add input to another turn.
        // If successful, blocks until other turn is done and returns.
        bool canMerge = (flags & allow_merging) != 0;
        if (canMerge && transactionQueue_.TryMergeSync(std::forward<F>(func)))
            return;

        bool shouldPropagate = false;
        
        QueueEntryT tr( flags );

        transactionQueue_.EnterQueue(tr);

        // Phase 1 - Input admission
        ThreadLocalInputState<>::IsTransactionActive = true;

        TurnT turn( nextTurnId(), flags );
        Engine::OnTurnAdmissionStart(turn);
        func();
        tr.RunMergedInputs();
        Engine::OnTurnAdmissionEnd(turn);

        ThreadLocalInputState<>::IsTransactionActive = false;

        // Phase 2 - Apply input node changes
        for (auto* p : changedInputs_)
            if (p->ApplyInput(&turn))
                shouldPropagate = true;
        changedInputs_.clear();

        // Phase 3 - Propagate changes
        if (shouldPropagate)
            Engine::Propagate(turn);

        finalizeSyncTransaction(tr);
    }

    template <typename F>
    void AsyncTransaction(TransactionFlagsT flags, const WaitingStatePtrT& waitingStatePtr,
                          F&& func)
    {
        if (waitingStatePtr != nullptr)
            waitingStatePtr->IncWaitCount();

        asyncWorker_.PushItem(AsyncItem{ flags, waitingStatePtr, std::forward<F>(func) });

        if (asyncWorker_.IncrementItemCount(1))
            asyncWorker_.Start();
    }

    template <typename R, typename V>
    void AddInput(R& r, V&& v)
    {
        if (ThreadLocalInputState<>::IsTransactionActive)
        {
            addTransactionInput(r, std::forward<V>(v));
        }
        else
        {
            addSimpleInput(r, std::forward<V>(v));
        }
    }

    template <typename R, typename F>
    void ModifyInput(R& r, const F& func)
    {
        if (ThreadLocalInputState<>::IsTransactionActive)
        {
            modifyTransactionInput(r, func);
        }
        else
        {
            modifySimpleInput(r, func);
        }
    }

    //IContinuationTarget
    virtual void AsyncContinuation(TransactionFlagsT flags,
                                   const WaitingStatePtrT& waitingStatePtr,
                                   TransactionFuncT&& cont) override
    {
        AsyncTransaction(flags, waitingStatePtr, std::move(cont));
    }
    //~IContinuationTarget

    void StoreContinuation(IContinuationTarget& target, TransactionFlagsT flags,
                           TransactionFuncT&& cont)
    {
        continuationManager_.StoreContinuation(target, flags, std::move(cont));
    }

    void QueueObserverForDetach(IObserver& obs)
    {
        continuationManager_.QueueObserverForDetach(obs);
    }

private:
    TurnIdT nextTurnId()
    {
        auto curId = nextTurnId_.fetch_add(1, std::memory_order_relaxed);

        if (curId == (std::numeric_limits<int>::max)())
            nextTurnId_.fetch_sub((std::numeric_limits<int>::max)());

        return curId;
    }

    // Create a turn with a single input
    template <typename R, typename V>
    void addSimpleInput(R& r, V&& v)
    {
        QueueEntryT tr( 0 );

        transactionQueue_.EnterQueue(tr);

        TurnT turn( nextTurnId(), 0 );
        Engine::OnTurnAdmissionStart(turn);
        r.AddInput(std::forward<V>(v));
        tr.RunMergedInputs();
        Engine::OnTurnAdmissionEnd(turn);

        if (r.ApplyInput(&turn))
            Engine::Propagate(turn);

        finalizeSyncTransaction(tr);
    }

    template <typename R, typename F>
    void modifySimpleInput(R& r, const F& func)
    {
        QueueEntryT tr( 0 );

        transactionQueue_.EnterQueue(tr);

        TurnT turn( nextTurnId(), 0 );
        Engine::OnTurnAdmissionStart(turn);
        r.ModifyInput(func);
        Engine::OnTurnAdmissionEnd(turn);

        // Return value, will always be true
        r.ApplyInput(&turn);

        Engine::Propagate(turn);

        finalizeSyncTransaction(tr);
    }

    void finalizeSyncTransaction(QueueEntryT& tr)
    {
        continuationManager_.template DetachQueuedObservers<D>();

        if (continuationManager_.HasContinuations())
        {
            UniqueWaitingState st;
            WaitingStatePtrT p( &st );

            continuationManager_.StartContinuations(p);

            transactionQueue_.ExitQueue(tr);

            p->Wait();
        }
        else
        {
            transactionQueue_.ExitQueue(tr);
        }
    }

    // This input is part of an active transaction
    template <typename R, typename V>
    void addTransactionInput(R& r, V&& v)
    {
        r.AddInput(std::forward<V>(v));
        changedInputs_.push_back(&r);
    }

    template <typename R, typename F>
    void modifyTransactionInput(R& r, const F& func)
    {
        r.ModifyInput(func);
        changedInputs_.push_back(&r);
    }

    void processAsyncQueue()
    {
        AsyncItem   item;

        std::vector<WaitingStatePtrT> waitingStatePtrs;

        bool skipPop = false;

        while (true)
        {
            size_t popCount = 0;

            if (!skipPop)
            {
                // Blocks if queue is empty.
                // This should never happen,
                // and if (maybe due to some memory access internals), only briefly
                asyncWorker_.PopItem(item);
                popCount++;
            }
            else
            {
                skipPop = false;
            }

            // First try to merge to an existing synchronous item in the queue
            bool canMerge = (item.Flags & allow_merging) != 0;
            if (canMerge && transactionQueue_.TryMergeAsync(
                    std::move(item.Func),
                    std::move(item.WaitingStatePtr)))
                continue;

            bool shouldPropagate = false;
            
            QueueEntryT tr( item.Flags );

            // Blocks until turn is at the front of the queue
            transactionQueue_.EnterQueue(tr);

            TurnT turn( nextTurnId(), item.Flags );

            // Phase 1 - Input admission
            ThreadLocalInputState<>::IsTransactionActive = true;

            Engine::OnTurnAdmissionStart(turn);

            // Input of current item
            item.Func();

            // Merged sync inputs that arrived while this item was queued
            tr.RunMergedInputs();

            // Save data, item might be re-used for next input
            if (item.WaitingStatePtr != nullptr)
                waitingStatePtrs.push_back(std::move(item.WaitingStatePtr));

            // If the current item supports merging, try to add more mergeable inputs
            // to this turn
            if (canMerge)
            {
                uint extraCount = 0;
                // Todo: Make configurable
                while (extraCount < 1024 && asyncWorker_.TryPopItem(item))
                {
                    ++popCount;

                    bool canMergeNext = (item.Flags & allow_merging) != 0;
                    if (canMergeNext)
                    {
                        item.Func();

                        if (item.WaitingStatePtr != nullptr)
                            waitingStatePtrs.push_back(std::move(item.WaitingStatePtr));

                        ++extraCount;
                    }
                    else
                    {
                        // We already popped an item we could not merge
                        // Process it in the next iteration
                        skipPop = true;

                        // Break at first item that cannot be merged.
                        // We only allow merging of continuous ranges.
                        break;
                    }
                }
            }

            Engine::OnTurnAdmissionEnd(turn);

            ThreadLocalInputState<>::IsTransactionActive = false;

            // Phase 2 - Apply input node changes
            for (auto* p : changedInputs_)
                if (p->ApplyInput(&turn))
                    shouldPropagate = true;
            changedInputs_.clear();

            // Phase 3 - Propagate changes
            if (shouldPropagate)
                Engine::Propagate(turn);

            continuationManager_.template DetachQueuedObservers<D>();

            // Has continuations? If so, status ptrs have to be passed on to
            // continuation transactions
            if (continuationManager_.HasContinuations())
            {
                // Merge all waiting state ptrs for this transaction into a single vector
                tr.MoveWaitingStatePtrs(waitingStatePtrs);

                // More than 1 waiting state -> create collection from vector
                if (waitingStatePtrs.size() > 1)
                {
                    WaitingStatePtrT p
                    (
                        SharedWaitingStateCollection::Create(std::move(waitingStatePtrs))
                    );

                    continuationManager_.StartContinuations(p);

                    transactionQueue_.ExitQueue(tr);
                    p->DecWaitCount();
                }
                // Exactly one status ptr -> pass it on directly
                else if (waitingStatePtrs.size() == 1)
                {
                    WaitingStatePtrT p( std::move(waitingStatePtrs[0]) );

                    continuationManager_.StartContinuations(p);

                    transactionQueue_.ExitQueue(tr);
                    p->DecWaitCount();
                }
                // No status ptrs
                else
                {
                    continuationManager_.StartContinuations(nullptr);
                }
            }
            else
            {
                transactionQueue_.ExitQueue(tr);

                for (auto& p : waitingStatePtrs)
                    p->DecWaitCount();
            }

            waitingStatePtrs.clear();

            // Stop this task if the number of items has just been decremented zero.
            // A new task will be started by the thread that increments the item count from zero.
            if (asyncWorker_.DecrementItemCount(popCount))
                break;
        }
    }

    TransactionQueueT       transactionQueue_;
    ContinuationManagerT    continuationManager_;
    AsyncWorkerT            asyncWorker_;

    std::atomic<TurnIdT>    nextTurnId_{ 0 };

    std::vector<IInputNode*>    changedInputs_;
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

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_REACTIVEINPUT_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

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
template <typename E, typename F, typename ... TArgs>
struct AddContinuationRangeWrapper
{
    AddContinuationRangeWrapper(const AddContinuationRangeWrapper& other) = default;

    AddContinuationRangeWrapper(AddContinuationRangeWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template
    <
        typename FIn,
        class = typename DisableIfSame<FIn,AddContinuationRangeWrapper>::type
    >
    explicit AddContinuationRangeWrapper(FIn&& func) :
        MyFunc( std::forward<FIn>(func) )
    {}

    void operator()(EventRange<E> range, const TArgs& ... args)
    {
        for (const auto& e : range)
            MyFunc(e, args ...);
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ContinuationNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ContinuationNode : public NodeBase<D>
{
public:
    ContinuationNode(TransactionFlagsT turnFlags) :
        turnFlags_( turnFlags )
    {}

    virtual bool IsOutputNode() const { return true; }

protected:
    TransactionFlagsT turnFlags_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalContinuationNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut,
    typename S,
    typename TFunc
>
class SignalContinuationNode : public ContinuationNode<D>
{
    using Engine = typename SignalContinuationNode::Engine;

public:
    template <typename F>
    SignalContinuationNode(TransactionFlagsT turnFlags,
                           const std::shared_ptr<SignalNode<D,S>>& trigger, F&& func) :
        SignalContinuationNode::ContinuationNode( turnFlags ),
        trigger_( trigger ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *trigger);
    }

    ~SignalContinuationNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "SignalContinuationNode"; }
    virtual int         DependencyCount() const override    { return 1; }

    virtual void Tick(void* turnPtr) override
    {
#ifdef REACT_ENABLE_LOGGING
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);
#endif

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        auto& storedValue = trigger_->ValueRef();
        auto& storedFunc = func_;

        TransactionFuncT cont
        (
            // Copy value and func
            [storedFunc,storedValue] () mutable
            {
                storedFunc(storedValue);
            }
        );

        DomainSpecificInputManager<D>::Instance()
            .StoreContinuation(
                DomainSpecificInputManager<DOut>::Instance(),
                this->turnFlags_,
                std::move(cont));

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

private:
    std::shared_ptr<SignalNode<D,S>> trigger_;

    TFunc   func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventContinuationNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut,
    typename E,
    typename TFunc
>
class EventContinuationNode : public ContinuationNode<D>
{
    using Engine = typename EventContinuationNode::Engine;

public:
    template <typename F>
    EventContinuationNode(TransactionFlagsT turnFlags,
                          const std::shared_ptr<EventStreamNode<D,E>>& trigger, F&& func) :
        EventContinuationNode::ContinuationNode( turnFlags ),
        trigger_( trigger ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *trigger);
    }

    ~EventContinuationNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "EventContinuationNode"; }
    virtual int         DependencyCount() const override    { return 1; }

    virtual void Tick(void* turnPtr) override
    {
#ifdef REACT_ENABLE_LOGGING
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);
#endif

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        auto& storedEvents = trigger_->Events();
        auto& storedFunc = func_;

        TransactionFuncT cont
        (
            // Copy events and func
            [storedFunc,storedEvents] () mutable
            {
                storedFunc(EventRange<E>( storedEvents ));
            }
        );

        DomainSpecificInputManager<D>::Instance()
            .StoreContinuation(
                DomainSpecificInputManager<DOut>::Instance(),
                this->turnFlags_,
                std::move(cont));

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

private:
    std::shared_ptr<EventStreamNode<D,E>> trigger_;

    TFunc   func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedContinuationNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut,
    typename E,
    typename TFunc,
    typename ... TDepValues
>
class SyncedContinuationNode : public ContinuationNode<D>
{
    using Engine = typename SyncedContinuationNode::Engine;

    using ValueTupleT = std::tuple<TDepValues...>;

    struct TupleBuilder_
    {
        ValueTupleT operator()(const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps)
        {
            return ValueTupleT(deps->ValueRef() ...);
        }
    };

    struct EvalFunctor_
    {
        EvalFunctor_(const E& e, TFunc& f) :
            MyEvent( e ),
            MyFunc( f )
        {}

        void operator()(const TDepValues& ... vals)
        {
            MyFunc(MyEvent, vals ...);
        }

        const E&    MyEvent;
        TFunc&      MyFunc;
    };

public:
    template <typename F>
    SyncedContinuationNode(TransactionFlagsT turnFlags,
                           const std::shared_ptr<EventStreamNode<D,E>>& trigger, F&& func,
                           const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedContinuationNode::ContinuationNode( turnFlags ),
        trigger_( trigger ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *trigger);

        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedContinuationNode()
    {
        Engine::OnNodeDetach(*this, *trigger_);

        apply(
            DetachFunctor<D,SyncedContinuationNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "SyncedContinuationNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        trigger_->SetCurrentTurn(turn);

        auto& storedEvents = trigger_->Events();
        auto& storedFunc = func_;

        // Copy values to tuple
        ValueTupleT storedValues = apply(TupleBuilder_( ), deps_);

        // Note: MSVC error, if using () initialization.
        // Probably a compiler bug.
        TransactionFuncT cont
        {
            // Copy events, func, value tuple (note: 2x copy)
            [storedFunc,storedEvents,storedValues] () mutable
            {
                apply(
                    [&storedFunc,&storedEvents] (const TDepValues& ... vals)
                    {
                        storedFunc(EventRange<E>( storedEvents ), vals ...);
                    },
                    storedValues);
            }
        };

        DomainSpecificInputManager<D>::Instance()
            .StoreContinuation(
                DomainSpecificInputManager<DOut>::Instance(),
                this->turnFlags_,
                std::move(cont));

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,E>> trigger_;
    
    TFunc       func_;
    DepHolderT  deps_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_CONTINUATIONNODES_H_INCLUDED

// Include all engines for convenience
// #include "react/engine/PulsecountEngine.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ENGINE_PULSECOUNTENGINE_H_INCLUDED
#define REACT_DETAIL_ENGINE_PULSECOUNTENGINE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <atomic>
#include <type_traits>
#include <vector>

#include "tbb/task_group.h"
#include "tbb/spin_rw_mutex.h"
#include "tbb/task.h"

// #include "react/common/Containers.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_CONTAINERS_H_INCLUDED
#define REACT_COMMON_CONTAINERS_H_INCLUDED



// #include "react/detail/Defs.h"


#include <algorithm>
#include <array>
#include <type_traits>
#include <vector>

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EnumFlags
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
class EnumFlags
{
public:
    using FlagsT = typename std::underlying_type<T>::type;

    template <T x>
    void Set() { flags_ |= 1 << x; }

    template <T x>
    void Clear() { flags_ &= ~(1 << x); }

    template <T x>
    bool Test() const { return (flags_ & (1 << x)) != 0; }

private:
    FlagsT  flags_ = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeVector
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode>
class NodeVector
{
private:
    typedef std::vector<TNode*>    DataT;

public:
    void Add(TNode& node)
    {
        data_.push_back(&node);
    }

    void Remove(const TNode& node)
    {
        data_.erase(std::find(data_.begin(), data_.end(), &node));
    }

    typedef typename DataT::iterator        iterator;
    typedef typename DataT::const_iterator  const_iterator;

    iterator    begin() { return data_.begin(); }
    iterator    end()   { return data_.end(); }

    const_iterator begin() const    { return data_.begin(); }
    const_iterator end() const      { return data_.end(); }

private:
    DataT    data_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeBuffer
///////////////////////////////////////////////////////////////////////////////////////////////////
struct SplitTag {};

template <typename T, size_t N>
class NodeBuffer
{
public:
    using DataT = std::array<T*,N>;
    using iterator = typename DataT::iterator;
    using const_iterator = typename DataT::const_iterator;

    static const size_t split_size = N / 2;

    NodeBuffer() :
        size_( 0 ),
        front_( nodes_.begin() ),
        back_( nodes_.begin() )
    {}

    NodeBuffer(T* node) :
        size_( 1 ),
        front_( nodes_.begin() ),
        back_( nodes_.begin() + 1 )
    {
        nodes_[0] = node;
    }

    template <typename TInput>
    NodeBuffer(TInput srcBegin, TInput srcEnd) :
        size_( std::distance(srcBegin, srcEnd) ), // parentheses to allow narrowing conversion
        front_( nodes_.begin() ),
        back_( size_ != N ? nodes_.begin() + size_ : nodes_.begin() )
    {
        std::copy(srcBegin, srcEnd, front_);
    }

    // Other must be full
    NodeBuffer(NodeBuffer& other, SplitTag) :
        size_( split_size ),
        front_( nodes_.begin() ),
        back_( nodes_.begin() )
    {
        for (auto i=0; i<split_size; i++)
            *(back_++) = other.PopFront();
    }

    void PushFront(T* e)
    {
        size_++;
        decrement(front_);
        *front_ = e;
    }

    void PushBack(T* e)
    {
        size_++;
        *back_ = e;
        increment(back_);
    }

    T* PopFront()
    {
        size_--;
        auto t = *front_;
        increment(front_);
        return t;
    }

    T* PopBack()
    {
        size_--;
        decrement(back_);
        return *back_;
    }

    bool IsFull() const     { return size_ == N; }
    bool IsEmpty() const    { return size_ == 0; }

private:
    inline void increment(iterator& it)
    {
        if (++it == nodes_.end())
            it = nodes_.begin();
    }

    inline void decrement(iterator& it)
    {
        if (it == nodes_.begin())
            it = nodes_.end();
        --it;
    }

    DataT       nodes_;
    size_t      size_;
    iterator    front_;
    iterator    back_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_CONTAINERS_H_INCLUDED
// #include "react/common/Types.h"

// #include "react/detail/EngineBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ENGINEBASE_H_INCLUDED
#define REACT_DETAIL_ENGINEBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "tbb/queuing_mutex.h"

// #include "react/common/Concurrency.h"

// #include "react/common/Types.h"

// #include "react/detail/ReactiveInput.h"

// #include "react/detail/IReactiveNode.h"

// #include "react/detail/IReactiveEngine.h"

// #include "react/detail/ObserverBase.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TurnBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class TurnBase
{
public:
    inline TurnBase(TurnIdT id, TransactionFlagsT flags) :
        id_( id )
    {}

    inline TurnIdT Id() const { return id_; }

private:
    TurnIdT    id_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ENGINEBASE_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/
namespace pulsecount {

using std::atomic;
using std::vector;

using tbb::task;
using tbb::empty_task;
using tbb::spin_rw_mutex;
using tbb::task_list;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Turn
///////////////////////////////////////////////////////////////////////////////////////////////////
class Turn : public TurnBase
{
public:
    Turn(TurnIdT id, TransactionFlagsT flags);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Node
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class ENodeMark
{
    unmarked,
    visited,
    should_update
};

enum class ENodeState
{
    unchanged,
    changed,
    dyn_defer,
    dyn_repeat
};

class Node : public IReactiveNode
{
public:
    using ShiftMutexT = spin_rw_mutex;

    inline void IncCounter() { counter_.fetch_add(1, std::memory_order_relaxed); }
    inline bool DecCounter() { return counter_.fetch_sub(1, std::memory_order_relaxed) > 1; }
    inline void SetCounter(int c) { counter_.store(c, std::memory_order_relaxed); }

    inline ENodeMark Mark() const
    {
        return mark_.load(std::memory_order_relaxed);
    }

    inline void SetMark(ENodeMark mark)
    {
        mark_.store(mark, std::memory_order_relaxed);
    }

    inline bool ExchangeMark(ENodeMark mark)
    {
        return mark_.exchange(mark, std::memory_order_relaxed) != mark;
    }

    ShiftMutexT         ShiftMutex;
    NodeVector<Node>    Successors;
    
    ENodeState          State       = ENodeState::unchanged;

private:
    atomic<int>         counter_    { 0 };
    atomic<ENodeMark>   mark_       { ENodeMark::unmarked };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class EngineBase : public IReactiveEngine<Node,Turn>
{
public:
    using NodeShiftMutexT = Node::ShiftMutexT;
    using NodeVectT = vector<Node*>;

    void OnNodeAttach(Node& node, Node& parent);
    void OnNodeDetach(Node& node, Node& parent);

    void OnInputChange(Node& node, Turn& turn);
    void Propagate(Turn& turn);

    void OnNodePulse(Node& node, Turn& turn);
    void OnNodeIdlePulse(Node& node, Turn& turn);

    void OnDynamicNodeAttach(Node& node, Node& parent, Turn& turn);
    void OnDynamicNodeDetach(Node& node, Node& parent, Turn& turn);

private:
    NodeVectT       changedInputs_;
    empty_task&     rootTask_       = *new(task::allocate_root()) empty_task;
    task_list       spawnList_;
};

} // ~namespace pulsecount
/****************************************/ REACT_IMPL_END /***************************************/

/*****************************************/ REACT_BEGIN /*****************************************/

template <REACT_IMPL::EPropagationMode>
class PulsecountEngine;

template <>
class PulsecountEngine<REACT_IMPL::parallel_propagation> :
    public REACT_IMPL::pulsecount::EngineBase
{};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <>
struct NodeUpdateTimerEnabled<PulsecountEngine<parallel_propagation>> : std::true_type {};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ENGINE_PULSECOUNTENGINE_H_INCLUDED
// #include "react/engine/SubtreeEngine.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ENGINE_SUBTREEENGINE_H_INCLUDED
#define REACT_DETAIL_ENGINE_SUBTREEENGINE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <atomic>
#include <type_traits>
#include <vector>

#include "tbb/spin_rw_mutex.h"
#include "tbb/task.h"

// #include "react/common/Containers.h"

// #include "react/common/TopoQueue.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_COMMON_TOPOQUEUE_H_INCLUDED
#define REACT_COMMON_TOPOQUEUE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

#include "tbb/enumerable_thread_specific.h"
#include "tbb/tbb_stddef.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/

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
    using QueueDataT    = std::vector<Entry>;
    using NextDataT     = std::vector<T>;

    TopoQueue() = default;
    TopoQueue(const TopoQueue&) = default;

    template <typename FIn>
    TopoQueue(FIn&& levelFunc) :
        levelFunc_( std::forward<FIn>(levelFunc) )
    {}

    void Push(const T& value)
    {
        queueData_.emplace_back(value, levelFunc_(value));
    }

    bool FetchNext()
    {
        // Throw away previous values
        nextData_.clear();

        // Find min level of nodes in queue data
        minLevel_ = (std::numeric_limits<int>::max)();
        for (const auto& e : queueData_)
            if (minLevel_ > e.Level)
                minLevel_ = e.Level;

        // Swap entries with min level to the end
        auto p = std::partition(
            queueData_.begin(),
            queueData_.end(),
            LevelCompFunctor{ minLevel_ });

        // Reserve once to avoid multiple re-allocations
        nextData_.reserve(std::distance(p, queueData_.end()));

        // Move min level values to next data
        for (auto it = p; it != queueData_.end(); ++it)
            nextData_.push_back(std::move(it->Value));

        // Truncate moved entries
        queueData_.resize(std::distance(queueData_.begin(), p));

        return !nextData_.empty();
    }

    const NextDataT& NextValues() const  { return nextData_; }

private:
    struct Entry
    {
        Entry() = default;
        Entry(const Entry&) = default;

        Entry(const T& value, int level) : Value( value ), Level( level ) {}

        T       Value;
        int     Level;
    };

    struct LevelCompFunctor
    {
        LevelCompFunctor(int level) : Level( level ) {}

        bool operator()(const Entry& e) const { return e.Level != Level; }

        const int Level;
    };

    NextDataT   nextData_;
    QueueDataT  queueData_;

    TLevelFunc  levelFunc_;

    int         minLevel_ = (std::numeric_limits<int>::max)();
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// WeightedRange - Implements tbb range concept
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename TIt,
    uint grain_size
>
class WeightedRange
{
public:
    using const_iterator = TIt;
    using ValueT = typename TIt::value_type;

    WeightedRange() = default;
    WeightedRange(const WeightedRange&) = default;

    WeightedRange(const TIt& a, const TIt& b, uint weight) :
        begin_( a ),
        end_( b ),
        weight_( weight )
    {}

    WeightedRange(WeightedRange& source, tbb::split)
    {
        uint sum = 0;
        TIt p = source.begin_;
        while (p != source.end_)
        {
            // Note: assuming a pair with weight as second until more flexibility is needed
            sum += p->second;
            ++p;
            if (sum >= grain_size)
                break;
        }

        // New [p,b)
        begin_ = p;
        end_ = source.end_;
        weight_ =  source.weight_ - sum;

        // Source [a,p)
        source.end_ = p;
        source.weight_ = sum;
    }

    // tbb range interface
    bool empty() const          { return !(Size() > 0); }
    bool is_divisible() const   { return  weight_ > grain_size && Size() > 1; }

    // iteration interface
    const_iterator begin() const    { return begin_; }
    const_iterator end() const      { return end_; }

    size_t  Size() const    { return end_ - begin_; }
    uint    Weight() const  { return weight_; }

private:
    TIt         begin_;
    TIt         end_;
    uint        weight_ = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ConcurrentTopoQueue
/// Usage based on two phases:
///     1. Multiple threads push nodes to the queue concurrently.
///     2. FetchNext() prepares all nodes of the next level in NextNodes().
///         The previous contents of NextNodes() are automatically cleared.
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<   typename T,
    uint grain_size,
    typename TLevelFunc,
    typename TWeightFunc
>
class ConcurrentTopoQueue
{
private:
    struct Entry;

public:
    using QueueDataT    = std::vector<Entry>;
    using NextDataT     = std::vector<std::pair<T,uint>>;
    using NextRangeT    = WeightedRange<typename NextDataT::const_iterator, grain_size>;

    ConcurrentTopoQueue() = default;
    ConcurrentTopoQueue(const ConcurrentTopoQueue&) = default;

    template <typename FIn1, typename FIn2>
    ConcurrentTopoQueue(FIn1&& levelFunc, FIn2&& weightFunc) :
        levelFunc_( std::forward<FIn1>(levelFunc) ),
        weightFunc_( std::forward<FIn2>(weightFunc) )
    {}

    void Push(const T& value)
    {
        auto& t = collectBuffer_.local();

        auto level  = levelFunc_(value);
        auto weight = weightFunc_(value);

        t.Data.emplace_back(value,level,weight);

        t.Weight += weight;

        if (t.MinLevel > level)
            t.MinLevel = level;
    }

    bool FetchNext()
    {
        nextData_.clear();
        uint totalWeight = 0;

        // Determine current min level
        minLevel_ = (std::numeric_limits<int>::max)();
        for (const auto& buf : collectBuffer_)
            if (minLevel_ > buf.MinLevel)
                minLevel_ = buf.MinLevel;

        // For each thread local buffer...
        for (auto& buf : collectBuffer_)
        {
            auto& v = buf.Data;

            // Swap min level nodes to end of v
            auto p = std::partition(
                v.begin(),
                v.end(),
                LevelCompFunctor{ minLevel_ });

            // Reserve once to avoid multiple re-allocations
            nextData_.reserve(std::distance(p, v.end()));

            // Move min level values to global next data
            for (auto it = p; it != v.end(); ++it)
                nextData_.emplace_back(std::move(it->Value), it->Weight);

            // Truncate remaining
            v.resize(std::distance(v.begin(), p));

            // Calc new min level and weight for this buffer
            buf.MinLevel = (std::numeric_limits<int>::max)();
            int oldWeight = buf.Weight;
            buf.Weight = 0;
            for (const auto& x : v)
            {
                buf.Weight += x.Weight;

                if (buf.MinLevel > x.Level)
                    buf.MinLevel = x.Level;
            }

            // Add diff to nodes_ weight
            totalWeight += oldWeight - buf.Weight;
        }

        nextRange_ = NextRangeT{ nextData_.begin(), nextData_.end(), totalWeight };

        // Found more nodes?
        return !nextData_.empty();
    }

    const NextRangeT& NextRange() const
    {
        return nextRange_;
    }

private:
    struct Entry
    {
        Entry() = default;
        Entry(const Entry&) = default;

        Entry(const T& value, int level, uint weight) :
            Value( value ),
            Level( level ),
            Weight( weight )
        {}

        T       Value;
        int     Level;
        uint    Weight;
    };

    struct LevelCompFunctor
    {
        LevelCompFunctor(int level) : Level{ level } {}

        bool operator()(const Entry& e) const { return  e.Level != Level; }

        const int Level;
    };

    struct ThreadLocalBuffer
    {
        QueueDataT  Data;
        int         MinLevel = (std::numeric_limits<int>::max)();
        uint        Weight = 0;
    };

    int         minLevel_ = (std::numeric_limits<int>::max)();

    NextDataT   nextData_;
    NextRangeT  nextRange_;

    TLevelFunc  levelFunc_;
    TWeightFunc weightFunc_;

    tbb::enumerable_thread_specific<ThreadLocalBuffer>    collectBuffer_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_COMMON_TOPOQUEUE_H_INCLUDED
// #include "react/common/Types.h"

// #include "react/detail/EngineBase.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/
namespace subtree {

using std::atomic;
using std::vector;

using tbb::task;
using tbb::empty_task;
using tbb::task_list;
using tbb::spin_rw_mutex;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Turn
///////////////////////////////////////////////////////////////////////////////////////////////////
class Turn : public TurnBase
{
public:
    Turn(TurnIdT id, TransactionFlagsT flags);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Node
///////////////////////////////////////////////////////////////////////////////////////////////////
class Node : public IReactiveNode
{
public:
    using ShiftMutexT = spin_rw_mutex;

    inline bool IsQueued() const    { return flags_.Test<flag_queued>(); }
    inline void SetQueuedFlag()     { flags_.Set<flag_queued>(); }
    inline void ClearQueuedFlag()   { flags_.Clear<flag_queued>(); }

    inline bool IsMarked() const    { return flags_.Test<flag_marked>(); }
    inline void SetMarkedFlag()     { flags_.Set<flag_marked>(); }
    inline void ClearMarkedFlag()   { flags_.Clear<flag_marked>(); }

    inline bool IsChanged() const   { return flags_.Test<flag_changed>(); }
    inline void SetChangedFlag()    { flags_.Set<flag_changed>(); }
    inline void ClearChangedFlag()  { flags_.Clear<flag_changed>(); }

    inline bool IsDeferred() const   { return flags_.Test<flag_deferred>(); }
    inline void SetDeferredFlag()    { flags_.Set<flag_deferred>(); }
    inline void ClearDeferredFlag()  { flags_.Clear<flag_deferred>(); }

    inline bool IsRepeated() const   { return flags_.Test<flag_repeated>(); }
    inline void SetRepeatedFlag()    { flags_.Set<flag_repeated>(); }
    inline void ClearRepeatedFlag()  { flags_.Clear<flag_repeated>(); }

    inline bool IsInitial() const   { return flags_.Test<flag_initial>(); }
    inline void SetInitialFlag()    { flags_.Set<flag_initial>(); }
    inline void ClearInitialFlag()  { flags_.Clear<flag_initial>(); }

    inline bool IsRoot() const      { return flags_.Test<flag_root>(); }
    inline void SetRootFlag()       { flags_.Set<flag_root>(); }
    inline void ClearRootFlag()     { flags_.Clear<flag_root>(); }

    inline bool ShouldUpdate() const { return shouldUpdate_.load(std::memory_order_relaxed); }
    inline void SetShouldUpdate(bool b) { shouldUpdate_.store(b, std::memory_order_relaxed); }

    inline void SetReadyCount(int c)
    {
        readyCount_.store(c, std::memory_order_relaxed);
    }

    inline bool IncReadyCount()
    {
        auto t = readyCount_.fetch_add(1, std::memory_order_relaxed);
        return t < (WaitCount - 1);
    }

    inline bool DecReadyCount()
    {
        return readyCount_.fetch_sub(1, std::memory_order_relaxed) > 1;
    }

    NodeVector<Node>    Successors;
    ShiftMutexT         ShiftMutex;
    uint16_t            Level       = 0;
    uint16_t            NewLevel    = 0;
    uint16_t            WaitCount   = 0;

private:
    enum EFlags : uint16_t
    {
        flag_queued = 0,
        flag_marked,
        flag_changed,
        flag_deferred,
        flag_repeated,
        flag_initial,
        flag_root
    };

    EnumFlags<EFlags>   flags_;
    atomic<uint16_t>    readyCount_     { 0 };
    atomic<bool>        shouldUpdate_   { false };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct GetLevelFunctor
{
    int operator()(const T* x) const { return x->Level; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class EngineBase : public IReactiveEngine<Node,Turn>
{
public:
    using TopoQueueT = TopoQueue<Node*, GetLevelFunctor<Node>>;
    using NodeShiftMutexT = Node::ShiftMutexT;

    void OnNodeAttach(Node& node, Node& parent);
    void OnNodeDetach(Node& node, Node& parent);

    void OnInputChange(Node& node, Turn& turn);
    void Propagate(Turn& turn);

    void OnNodePulse(Node& node, Turn& turn);
    void OnNodeIdlePulse(Node& node, Turn& turn);

    void OnDynamicNodeAttach(Node& node, Node& parent, Turn& turn);
    void OnDynamicNodeDetach(Node& node, Node& parent, Turn& turn);

private:
    void applyAsyncDynamicAttach(Node& node, Node& parent, Turn& turn);
    void applyAsyncDynamicDetach(Node& node, Node& parent, Turn& turn);

    void invalidateSuccessors(Node& node);
    void processChildren(Node& node, Turn& turn);

    void markSubtree(Node& root);

    TopoQueueT      scheduledNodes_;
    vector<Node*>   subtreeRoots_;

    empty_task&     rootTask_   = *new(task::allocate_root()) empty_task;
    task_list       spawnList_;

    bool            isInPhase2_ = false;
};

} // ~namespace subtree
/****************************************/ REACT_IMPL_END /***************************************/

/*****************************************/ REACT_BEGIN /*****************************************/

template <REACT_IMPL::EPropagationMode>
class SubtreeEngine;

template <>
class SubtreeEngine<REACT_IMPL::parallel_propagation> :
    public REACT_IMPL::subtree::EngineBase
{};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <>
struct NodeUpdateTimerEnabled<SubtreeEngine<parallel_propagation>> : std::true_type {};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ENGINE_SUBTREEENGINE_H_INCLUDED
// #include "react/engine/ToposortEngine.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_ENGINE_TOPOSORTENGINE_H_INCLUDED
#define REACT_DETAIL_ENGINE_TOPOSORTENGINE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <atomic>
#include <utility>
#include <type_traits>
#include <vector>

#include "tbb/concurrent_vector.h"
#include "tbb/spin_mutex.h"

// #include "react/common/Containers.h"

// #include "react/common/TopoQueue.h"

// #include "react/common/Types.h"

// #include "react/detail/EngineBase.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

namespace toposort {

using std::atomic;
using std::vector;
using tbb::concurrent_vector;
using tbb::spin_mutex;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Parameters
///////////////////////////////////////////////////////////////////////////////////////////////////
static const uint min_weight = 1;
static const uint grain_size = 100;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqNode : public IReactiveNode
{
public:
    int     Level       { 0 };
    int     NewLevel    { 0 };
    bool    Queued      { false };

    NodeVector<SeqNode>     Successors;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ParNode
///////////////////////////////////////////////////////////////////////////////////////////////////
class ParNode : public IReactiveNode
{
public:
    using InvalidateMutexT = spin_mutex;

    int             Level       { 0 };
    int             NewLevel    { 0 };
    atomic<bool>    Collected   { false };

    NodeVector<ParNode> Successors;
    InvalidateMutexT    InvalidateMutex;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ShiftRequestData
///////////////////////////////////////////////////////////////////////////////////////////////////
struct DynRequestData
{
    bool        ShouldAttach;
    ParNode*    Node;
    ParNode*    Parent;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ExclusiveSeqTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqTurn : public TurnBase
{
public:
    SeqTurn(TurnIdT id, TransactionFlagsT flags);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ExclusiveParTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
class ParTurn : public TurnBase
{
public:
    ParTurn(TurnIdT id, TransactionFlagsT flags);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Functors
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct GetLevelFunctor
{
    int operator()(const T* x) const { return x->Level; }
};

template <typename T>
struct GetWeightFunctor
{
    uint operator()(T* x) const { return x->IsHeavyweight() ? grain_size : 1; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode, typename TTurn>
class EngineBase : public IReactiveEngine<TNode,TTurn>
{
public:
    void OnNodeAttach(TNode& node, TNode& parent);
    void OnNodeDetach(TNode& node, TNode& parent);

    void OnInputChange(TNode& node, TTurn& turn);
    void OnNodePulse(TNode& node, TTurn& turn);

protected:
    virtual void processChildren(TNode& node, TTurn& turn) = 0;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class SeqEngineBase : public EngineBase<SeqNode,SeqTurn>
{
public:
    using TopoQueueT = TopoQueue<SeqNode*, GetLevelFunctor<SeqNode>>;

    void Propagate(SeqTurn& turn);

    void OnDynamicNodeAttach(SeqNode& node, SeqNode& parent, SeqTurn& turn);
    void OnDynamicNodeDetach(SeqNode& node, SeqNode& parent, SeqTurn& turn);

private:
    void invalidateSuccessors(SeqNode& node);

    virtual void processChildren(SeqNode& node, SeqTurn& turn) override;

    TopoQueueT    scheduledNodes_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ParEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
class ParEngineBase : public EngineBase<ParNode,ParTurn>
{
public:
    using DynRequestVectT = concurrent_vector<DynRequestData>;
    using TopoQueueT = ConcurrentTopoQueue
    <
        ParNode*,
        grain_size,
        GetLevelFunctor<ParNode>,
        GetWeightFunctor<ParNode>
    >;

    void Propagate(ParTurn& turn);

    void OnDynamicNodeAttach(ParNode& node, ParNode& parent, ParTurn& turn);
    void OnDynamicNodeDetach(ParNode& node, ParNode& parent, ParTurn& turn);

private:
    void applyDynamicAttach(ParNode& node, ParNode& parent, ParTurn& turn);
    void applyDynamicDetach(ParNode& node, ParNode& parent, ParTurn& turn);

    void invalidateSuccessors(ParNode& node);

    virtual void processChildren(ParNode& node, ParTurn& turn) override;

    TopoQueueT          topoQueue_;
    DynRequestVectT     dynRequests_;
};

} // ~namespace toposort

/****************************************/ REACT_IMPL_END /***************************************/

/*****************************************/ REACT_BEGIN /*****************************************/

template <REACT_IMPL::EPropagationMode>
class ToposortEngine;

template <> class ToposortEngine<REACT_IMPL::sequential_propagation> :
    public REACT_IMPL::toposort::SeqEngineBase
{};

template <> class ToposortEngine<REACT_IMPL::parallel_propagation> :
    public REACT_IMPL::toposort::ParEngineBase
{};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <>
struct NodeUpdateTimerEnabled<ToposortEngine<parallel_propagation>> : std::true_type {};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_ENGINE_TOPOSORTENGINE_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

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

template <typename D>
class Reactor;

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////

// Domain modes
enum EDomainMode
{
    sequential,
    sequential_concurrent,
    parallel,
    parallel_concurrent
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
    using Engine = REACT_IMPL::EngineInterface<D, typename Policy::Engine>;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Domain traits
    ///////////////////////////////////////////////////////////////////////////////////////////////
    static const bool uses_node_update_timer =
        REACT_IMPL::NodeUpdateTimerEnabled<typename Policy::Engine>::value;

    static const bool is_concurrent =
        Policy::input_mode == REACT_IMPL::concurrent_input;

    static const bool is_parallel =
        Policy::propagation_mode == REACT_IMPL::parallel_propagation;

    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Aliases for reactives of this domain
    ///////////////////////////////////////////////////////////////////////////////////////////////
    template <typename S>
    using SignalT = Signal<D,S>;

    template <typename S>
    using VarSignalT = VarSignal<D,S>;

    template <typename E = Token>
    using EventsT = Events<D,E>;

    template <typename E = Token>
    using EventSourceT = EventSource<D,E>;

    using ObserverT = Observer<D>;

    using ScopedObserverT = ScopedObserver<D>;

    using ReactorT = Reactor<D>;

#ifdef REACT_ENABLE_LOGGING
    ///////////////////////////////////////////////////////////////////////////////////////////////
    /// Log
    ///////////////////////////////////////////////////////////////////////////////////////////////
    static EventLog& Log()
    {
        static EventLog instance;
        return instance;
    }
#endif //REACT_ENABLE_LOGGING
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ContinuationBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename D2
>
class ContinuationBase : public MovableReactive<NodeBase<D>>
{
public:
    ContinuationBase() = default;

    template <typename T>
    ContinuationBase(T&& t) :
        ContinuationBase::MovableReactive( std::forward<T>(t) )
    {}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ModeSelector - Translate domain mode to individual propagation and input modes
///////////////////////////////////////////////////////////////////////////////////////////////////

template <EDomainMode>
struct ModeSelector;

template <>
struct ModeSelector<sequential>
{
    static const EInputMode         input       = consecutive_input;
    static const EPropagationMode   propagation = sequential_propagation;
};

template <>
struct ModeSelector<sequential_concurrent>
{
    static const EInputMode         input       = concurrent_input;
    static const EPropagationMode   propagation = sequential_propagation;
};

template <>
struct ModeSelector<parallel>
{
    static const EInputMode         input       = consecutive_input;
    static const EPropagationMode   propagation = parallel_propagation;
};

template <>
struct ModeSelector<parallel_concurrent>
{
    static const EInputMode         input       = concurrent_input;
    static const EPropagationMode   propagation = parallel_propagation;
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

template <>
struct GetDefaultEngine<parallel_propagation>
{
    using Type = SubtreeEngine<parallel_propagation>;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineTypeBuilder - Instantiate the given template engine type with mode.
///////////////////////////////////////////////////////////////////////////////////////////////////
template <EPropagationMode>
struct DefaultEnginePlaceholder;

// Concrete engine template type
template
<
    EPropagationMode mode,
    template <EPropagationMode> class TTEngine
>
struct EngineTypeBuilder
{
    using Type = TTEngine<mode>;
};

// Placeholder engine type - use default engine for given mode
template
<
    EPropagationMode mode
>
struct EngineTypeBuilder<mode,DefaultEnginePlaceholder>
{
    using Type = typename GetDefaultEngine<mode>::Type;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DomainPolicy
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    EDomainMode mode,
    template <EPropagationMode> class TTEngine = DefaultEnginePlaceholder
>
struct DomainPolicy
{
    static const EInputMode         input_mode          = ModeSelector<mode>::input;
    static const EPropagationMode   propagation_mode    = ModeSelector<mode>::propagation;

    using Engine = typename EngineTypeBuilder<propagation_mode,TTEngine>::Type;
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
#ifdef REACT_ENABLE_LOGGING
        D::Log();
#endif //REACT_ENABLE_LOGGING

        D::Engine::Instance();
        DomainSpecificInputManager<D>::Instance();
    }
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_DOMAINBASE_H_INCLUDED
// #include "react/detail/ReactiveInput.h"


// #include "react/detail/graph/ContinuationNodes.h"


#ifdef REACT_ENABLE_LOGGING
    // #include "react/logging/EventLog.h"

    // #include "react/logging/EventRecords.h"

#endif //REACT_ENABLE_LOGGING

/*****************************************/ REACT_BEGIN /*****************************************/

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

template <typename D>
class Reactor;

template <typename D, typename ... TValues>
class SignalPack;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Common types & constants
///////////////////////////////////////////////////////////////////////////////////////////////////
using REACT_IMPL::TransactionFlagsT;

// ETransactionFlags
using REACT_IMPL::ETransactionFlags;
using REACT_IMPL::allow_merging;

#ifdef REACT_ENABLE_LOGGING
    using REACT_IMPL::EventLog;
#endif //REACT_ENABLE_LOGGING

// Domain modes
using REACT_IMPL::EDomainMode;
using REACT_IMPL::sequential;
using REACT_IMPL::sequential_concurrent;
using REACT_IMPL::parallel;
using REACT_IMPL::parallel_concurrent;

// Expose enum type so aliases for engines can be declared, but don't
// expose the actual enum values as they are reserved for internal use.
using REACT_IMPL::EPropagationMode;

using REACT_IMPL::WeightHint;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionStatus
///////////////////////////////////////////////////////////////////////////////////////////////////
class TransactionStatus
{
    using StateT = REACT_IMPL::SharedWaitingState;
    using PtrT = REACT_IMPL::WaitingStatePtrT;

public:
    // Default ctor
    inline TransactionStatus() :
        statePtr_( StateT::Create() )
    {}

    // Move ctor
    inline TransactionStatus(TransactionStatus&& other) :
        statePtr_( std::move(other.statePtr_) )
    {
        other.statePtr_ = StateT::Create();
    }

    // Move assignment
    inline TransactionStatus& operator=(TransactionStatus&& other)
    {
        if (this != &other)
        {
            statePtr_ = std::move(other.statePtr_);
            other.statePtr_ = StateT::Create();
        }
        return *this;
    }

    // Deleted copy ctor & assignment
    TransactionStatus(const TransactionStatus&) = delete;
    TransactionStatus& operator=(const TransactionStatus&) = delete;

    inline void Wait()
    {
        assert(statePtr_.Get() != nullptr);
        statePtr_->Wait();
    }

private:
    PtrT statePtr_;

    template <typename D, typename F>
    friend void AsyncTransaction(TransactionStatus& status, F&& func);

    template <typename D, typename F>
    friend void AsyncTransaction(TransactionFlagsT flags, TransactionStatus& status, F&& func);
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Continuation
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename D2 = D
>
class Continuation : public REACT_IMPL::ContinuationBase<D,D2>
{
private:
    using NodePtrT  = REACT_IMPL::NodeBasePtrT<D>;

public:
    using SourceDomainT = D;
    using TargetDomainT = D2;

    // Default ctor
    Continuation() = default;

    // Move ctor
    Continuation(Continuation&& other) :
        Continuation::ContinuationBase( std::move(other) )
    {}

    // Node ctor
    explicit Continuation(NodePtrT&& nodePtr) :
        Continuation::ContinuationBase( std::move(nodePtr) )
    {}

    // Move assignment
    Continuation& operator=(Continuation&& other)
    {
        Continuation::ContinuationBase::operator=( std::move(other) );
        return *this;
    }

    // Deleted copy ctor & assignment
    Continuation(const Continuation&) = delete;
    Continuation& operator=(const Continuation&) = delete;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeContinuation - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut = D,
    typename S,
    typename FIn
>
auto MakeContinuation(TransactionFlagsT flags, const Signal<D,S>& trigger, FIn&& func)
    -> Continuation<D,DOut>
{
    static_assert(DOut::is_concurrent,
        "MakeContinuation: Target domain does not support concurrent input.");

    using REACT_IMPL::SignalContinuationNode;
    using F = typename std::decay<FIn>::type;

    return Continuation<D,DOut>(
        std::make_shared<SignalContinuationNode<D,DOut,S,F>>(
            flags, GetNodePtr(trigger), std::forward<FIn>(func)));
}

template
<
    typename D,
    typename DOut = D,
    typename S,
    typename FIn
>
auto MakeContinuation(const Signal<D,S>& trigger, FIn&& func)
    -> Continuation<D,DOut>
{
    return MakeContinuation<D,DOut>(0, trigger, std::forward<FIn>(func));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeContinuation - Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut = D,
    typename E,
    typename FIn
>
auto MakeContinuation(TransactionFlagsT flags, const Events<D,E>& trigger, FIn&& func)
    -> Continuation<D,DOut>
{
    static_assert(DOut::is_concurrent,
        "MakeContinuation: Target domain does not support concurrent input.");

    using REACT_IMPL::EventContinuationNode;
    using REACT_IMPL::AddContinuationRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using WrapperT =
        typename std::conditional<
            IsCallableWith<F,void, EventRange<E>>::value,
            F,
            typename std::conditional<
                IsCallableWith<F, void, E>::value,
                AddContinuationRangeWrapper<E, F>,
                void
            >::type
        >::type;

    static_assert(! std::is_same<WrapperT,void>::value,
        "MakeContinuation: Passed function does not match any of the supported signatures.");

    return Continuation<D,DOut>(
        std::make_shared<EventContinuationNode<D,DOut,E,WrapperT>>(
            flags, GetNodePtr(trigger), std::forward<FIn>(func)));
}

template
<
    typename D,
    typename DOut = D,
    typename E,
    typename FIn
>
auto MakeContinuation(const Events<D,E>& trigger, FIn&& func)
    -> Continuation<D,DOut>
{
    return MakeContinuation<D,DOut>(0, trigger, std::forward<FIn>(func));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeContinuation - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename DOut = D,
    typename E,
    typename FIn,
    typename ... TDepValues
>
auto MakeContinuation(TransactionFlagsT flags, const Events<D,E>& trigger,
                      const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Continuation<D,DOut>
{
    static_assert(DOut::is_concurrent,
        "MakeContinuation: Target domain does not support concurrent input.");

    using REACT_IMPL::SyncedContinuationNode;
    using REACT_IMPL::AddContinuationRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using WrapperT =
        typename std::conditional<
            IsCallableWith<F, void, EventRange<E>, TDepValues ...>::value,
            F,
            typename std::conditional<
                IsCallableWith<F, void, E, TDepValues ...>::value,
                AddContinuationRangeWrapper<E, F, TDepValues ...>,
                void
            >::type
        >::type;

    static_assert(! std::is_same<WrapperT,void>::value,
        "MakeContinuation: Passed function does not match any of the supported signatures.");

    struct NodeBuilder_
    {
        NodeBuilder_(TransactionFlagsT flags, const Events<D,E>& trigger, FIn&& func) :
            MyFlags( flags ),
            MyTrigger( trigger ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> Continuation<D,DOut>
        {
            return Continuation<D,DOut>(
                std::make_shared<SyncedContinuationNode<D,DOut,E,WrapperT,TDepValues...>>(
                    MyFlags,
                    GetNodePtr(MyTrigger),
                    std::forward<FIn>(MyFunc), GetNodePtr(deps) ...));
        }

        TransactionFlagsT   MyFlags;
        const Events<D,E>&  MyTrigger;
        FIn                 MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( flags, trigger, std::forward<FIn>(func) ),
        depPack.Data);
}

template
<
    typename D,
    typename DOut = D,
    typename E,
    typename FIn,
    typename ... TDepValues
>
auto MakeContinuation(const Events<D,E>& trigger,
                      const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Continuation<D,DOut>
{
    return MakeContinuation<D,DOut>(0, trigger, depPack, std::forward<FIn>(func));
}

///////////////////////////////////////////////////////////////////////////////////////////////
/// DoTransaction
///////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename F>
void DoTransaction(F&& func)
{
    using REACT_IMPL::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance().DoTransaction(0, std::forward<F>(func));
}

template <typename D, typename F>
void DoTransaction(TransactionFlagsT flags, F&& func)
{
    using REACT_IMPL::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance().DoTransaction(flags, std::forward<F>(func));
}

///////////////////////////////////////////////////////////////////////////////////////////////
/// AsyncTransaction
///////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename F>
void AsyncTransaction(F&& func)
{
    static_assert(D::is_concurrent,
        "AsyncTransaction: Domain does not support concurrent input.");

    using REACT_IMPL::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance()
        .AsyncTransaction(0, nullptr, std::forward<F>(func));
}

template <typename D, typename F>
void AsyncTransaction(TransactionFlagsT flags, F&& func)
{
    static_assert(D::is_concurrent,
        "AsyncTransaction: Domain does not support concurrent input.");

    using REACT_IMPL::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance()
        .AsyncTransaction(flags, nullptr, std::forward<F>(func));
}

template <typename D, typename F>
void AsyncTransaction(TransactionStatus& status, F&& func)
{
    static_assert(D::is_concurrent,
        "AsyncTransaction: Domain does not support concurrent input.");

    using REACT_IMPL::DomainSpecificInputManager;

    DomainSpecificInputManager<D>::Instance()
        .AsyncTransaction(0, status.statePtr_, std::forward<F>(func));
}

template <typename D, typename F>
void AsyncTransaction(TransactionFlagsT flags, TransactionStatus& status, F&& func)
{
    static_assert(D::is_concurrent,
        "AsyncTransaction: Domain does not support concurrent input.");

    using REACT_IMPL::DomainSpecificInputManager;
    DomainSpecificInputManager<D>::Instance()
        .AsyncTransaction(flags, status.statePtr_, std::forward<F>(func));
}

/******************************************/ REACT_END /******************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Domain definition macro
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACTIVE_DOMAIN(name, ...)                                                          \
    struct name :                                                                           \
        public REACT_IMPL::DomainBase<name, REACT_IMPL::DomainPolicy< __VA_ARGS__ >> {};    \
    static REACT_IMPL::DomainInitializer<name> name ## _initializer_;

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
#define USING_REACTIVE_DOMAIN(name)                                                         \
    template <typename S>                                                                   \
    using SignalT = Signal<name,S>;                                                         \
                                                                                            \
    template <typename S>                                                                   \
    using VarSignalT = VarSignal<name,S>;                                                   \
                                                                                            \
    template <typename E = Token>                                                           \
    using EventsT = Events<name,E>;                                                         \
                                                                                            \
    template <typename E = Token>                                                           \
    using EventSourceT = EventSource<name,E>;                                               \
                                                                                            \
    using ObserverT = Observer<name>;                                                       \
                                                                                            \
    using ScopedObserverT = ScopedObserver<name>;                                           \
                                                                                            \
    using ReactorT = Reactor<name>;

#endif // REACT_DOMAIN_H_INCLUDED
// #include "react/Signal.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_SIGNAL_H_INCLUDED
#define REACT_SIGNAL_H_INCLUDED



#if _MSC_VER && !__INTEL_COMPILER
    #pragma warning(disable : 4503)
#endif

// #include "react/detail/Defs.h"


#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

// #include "react/Observer.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_OBSERVER_H_INCLUDED
#define REACT_OBSERVER_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "react/common/Util.h"

// #include "react/detail/IReactiveNode.h"

// #include "react/detail/ObserverBase.h"

// #include "react/detail/graph/ObserverNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_OBSERVERNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_OBSERVERNODES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "GraphBase.h"


// #include "react/detail/ReactiveInput.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

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
template <typename E, typename F, typename ... TArgs>
struct AddObserverRangeWrapper
{
    AddObserverRangeWrapper(const AddObserverRangeWrapper& other) = default;

    AddObserverRangeWrapper(AddObserverRangeWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template
    <
        typename FIn,
        class = typename DisableIfSame<FIn,AddObserverRangeWrapper>::type
    >
    explicit AddObserverRangeWrapper(FIn&& func) :
        MyFunc( std::forward<FIn>(func) )
    {}

    ObserverAction operator()(EventRange<E> range, const TArgs& ... args)
    {
        for (const auto& e : range)
            if (MyFunc(e, args ...) == ObserverAction::stop_and_detach)
                return ObserverAction::stop_and_detach;

        return ObserverAction::next;
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ObserverNode :
    public NodeBase<D>,
    public IObserver
{
public:
    ObserverNode() = default;

    virtual bool IsOutputNode() const { return true; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename TFunc
>
class SignalObserverNode :
    public ObserverNode<D>
{
    using Engine = typename SignalObserverNode::Engine;

public:
    template <typename F>
    SignalObserverNode(const std::shared_ptr<SignalNode<D,S>>& subject, F&& func) :
        SignalObserverNode::ObserverNode( ),
        subject_( subject ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *subject);
    }

    ~SignalObserverNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "SignalObserverNode"; }
    virtual int         DependencyCount() const override    { return 1; }

    virtual void Tick(void* turnPtr) override
    {
#ifdef REACT_ENABLE_LOGGING
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);
#endif

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        bool shouldDetach = false;

        if (auto p = subject_.lock())
        {// timer
            using TimerT = typename SignalObserverNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, 1 );
            
            if (func_(p->ValueRef()) == ObserverAction::stop_and_detach)
                shouldDetach = true;
        }// ~timer

        if (shouldDetach)
            DomainSpecificInputManager<D>::Instance()
                .QueueObserverForDetach(*this);

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

    virtual void UnregisterSelf() override
    {
        if (auto p = subject_.lock())
            p->UnregisterObserver(this);
    }

private:
    virtual void detachObserver() override
    {
        if (auto p = subject_.lock())
        {
            Engine::OnNodeDetach(*this, *p);
            subject_.reset();
        }
    }

    std::weak_ptr<SignalNode<D,S>>  subject_;
    TFunc                           func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TFunc
>
class EventObserverNode :
    public ObserverNode<D>
{
    using Engine = typename EventObserverNode::Engine;

public:
    template <typename F>
    EventObserverNode(const std::shared_ptr<EventStreamNode<D,E>>& subject, F&& func) :
        EventObserverNode::ObserverNode( ),
        subject_( subject ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *subject);
    }

    ~EventObserverNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "EventObserverNode"; }
    virtual int         DependencyCount() const override    { return 1; }

    virtual void Tick(void* turnPtr) override
    {
#ifdef REACT_ENABLE_LOGGING
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);
#endif

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        bool shouldDetach = false;

        if (auto p = subject_.lock())
        {// timer
            using TimerT = typename EventObserverNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, p->Events().size() );

            shouldDetach = func_(EventRange<E>( p->Events() )) == ObserverAction::stop_and_detach;

        }// ~timer

        if (shouldDetach)
            DomainSpecificInputManager<D>::Instance()
                .QueueObserverForDetach(*this);

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

    virtual void UnregisterSelf() override
    {
        if (auto p = subject_.lock())
            p->UnregisterObserver(this);
    }

private:
    std::weak_ptr<EventStreamNode<D,E>> subject_;

    TFunc   func_;

    virtual void detachObserver()
    {
        if (auto p = subject_.lock())
        {
            Engine::OnNodeDetach(*this, *p);
            subject_.reset();
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedObserverNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TFunc,
    typename ... TDepValues
>
class SyncedObserverNode :
    public ObserverNode<D>
{
    using Engine = typename SyncedObserverNode::Engine;

public:
    template <typename F>
    SyncedObserverNode(const std::shared_ptr<EventStreamNode<D,E>>& subject, F&& func,
                       const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedObserverNode::ObserverNode( ),
        subject_( subject ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *subject);

        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedObserverNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "SyncedObserverNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));
        
        bool shouldDetach = false;

        if (auto p = subject_.lock())
        {
            // Update of this node could be triggered from deps,
            // so make sure source doesnt contain events from last turn
            p->SetCurrentTurn(turn);

            {// timer
                using TimerT = typename SyncedObserverNode::ScopedUpdateTimer;
                TimerT scopedTimer( *this, p->Events().size() );
            
                shouldDetach = apply(
                    [this, &p] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                    {
                        return func_(EventRange<E>( p->Events() ), args->ValueRef() ...);
                    },
                    deps_) == ObserverAction::stop_and_detach;

            }// ~timer
        }

        if (shouldDetach)
            DomainSpecificInputManager<D>::Instance()
                .QueueObserverForDetach(*this);

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));
    }

    virtual void UnregisterSelf() override
    {
        if (auto p = subject_.lock())
            p->UnregisterObserver(this);
    }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::weak_ptr<EventStreamNode<D,E>> subject_;
    
    TFunc       func_;
    DepHolderT  deps_;

    virtual void detachObserver()
    {
        if (auto p = subject_.lock())
        {
            Engine::OnNodeDetach(*this, *p);

            apply(
                DetachFunctor<D,SyncedObserverNode,
                    std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
                deps_);

            subject_.reset();
        }
    }
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_OBSERVERNODES_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class Signal;

template <typename D, typename ... TValues>
class SignalPack;

template <typename D, typename E>
class Events;

using REACT_IMPL::ObserverAction;
using REACT_IMPL::WeightHint;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observer
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class Observer
{
private:
    using SubjectPtrT   = std::shared_ptr<REACT_IMPL::ObservableNode<D>>;
    using NodeT         = REACT_IMPL::ObserverNode<D>;

public:
    // Default ctor
    Observer() :
        nodePtr_( nullptr ),
        subjectPtr_( nullptr )
    {}

    // Move ctor
    Observer(Observer&& other) :
        nodePtr_( other.nodePtr_ ),
        subjectPtr_( std::move(other.subjectPtr_) )
    {
        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();
    }

    // Node ctor
    Observer(NodeT* nodePtr, const SubjectPtrT& subjectPtr) :
        nodePtr_( nodePtr ),
        subjectPtr_( subjectPtr )
    {}

    // Move assignment
    Observer& operator=(Observer&& other)
    {
        nodePtr_ = other.nodePtr_;
        subjectPtr_ = std::move(other.subjectPtr_);

        other.nodePtr_ = nullptr;
        other.subjectPtr_.reset();

        return *this;
    }

    // Deleted copy ctor and assignment
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    void Detach()
    {
        assert(IsValid());
        subjectPtr_->UnregisterObserver(nodePtr_);
    }

    bool IsValid() const
    {
        return nodePtr_ != nullptr;
    }

    void SetWeightHint(WeightHint weight)
    {
        assert(IsValid());
        nodePtr_->SetWeightHint(weight);
    }

private:
    // Owned by subject
    NodeT*          nodePtr_;

    // While the observer handle exists, the subject is not destroyed
    SubjectPtrT     subjectPtr_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ScopedObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D>
class ScopedObserver
{
public:
    // Move ctor
    ScopedObserver(ScopedObserver&& other) :
        obs_( std::move(other.obs_) )
    {}

    // Construct from observer
    ScopedObserver(Observer<D>&& obs) :
        obs_( std::move(obs) )
    {}

    // Move assignment
    ScopedObserver& operator=(ScopedObserver&& other)
    {
        obs_ = std::move(other.obs_);
    }

    // Deleted default ctor, copy ctor and assignment
    ScopedObserver() = delete;
    ScopedObserver(const ScopedObserver&) = delete;
    ScopedObserver& operator=(const ScopedObserver&) = delete;

    ~ScopedObserver()
    {
        obs_.Detach();
    }

    bool IsValid() const
    {
        return obs_.IsValid();
    }

    void SetWeightHint(WeightHint weight)
    {
        obs_.SetWeightHint(weight);
    }

private:
    Observer<D>     obs_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Signals
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename S
>
auto Observe(const Signal<D,S>& subject, FIn&& func)
    -> Observer<D>
{
    using REACT_IMPL::IObserver;
    using REACT_IMPL::ObserverNode;
    using REACT_IMPL::SignalObserverNode;
    using REACT_IMPL::AddDefaultReturnValueWrapper;

    using F = typename std::decay<FIn>::type;
    using R = typename std::result_of<FIn(S)>::type;
    using WrapperT = AddDefaultReturnValueWrapper<F,ObserverAction,ObserverAction::next>;

    // If return value of passed function is void, add ObserverAction::next as
    // default return value.
    using NodeT = typename std::conditional<
        std::is_same<void,R>::value,
        SignalObserverNode<D,S,WrapperT>,
        SignalObserverNode<D,S,F>
            >::type;

    const auto& subjectPtr = GetNodePtr(subject);

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT(subjectPtr, std::forward<FIn>(func)) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver(std::move(nodePtr));

    return Observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename E
>
auto Observe(const Events<D,E>& subject, FIn&& func)
    -> Observer<D>
{
    using REACT_IMPL::IObserver;
    using REACT_IMPL::ObserverNode;
    using REACT_IMPL::EventObserverNode;
    using REACT_IMPL::AddDefaultReturnValueWrapper;
    using REACT_IMPL::AddObserverRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using WrapperT =
        typename std::conditional<
            IsCallableWith<F, ObserverAction, EventRange<E>>::value,
            F,
            typename std::conditional<
                IsCallableWith<F, ObserverAction, E>::value,
                AddObserverRangeWrapper<E, F>,
                typename std::conditional<
                    IsCallableWith<F, void, EventRange<E>>::value,
                    AddDefaultReturnValueWrapper<F,ObserverAction,ObserverAction::next>,
                    typename std::conditional<
                        IsCallableWith<F, void, E>::value,
                        AddObserverRangeWrapper<E,
                            AddDefaultReturnValueWrapper<F,ObserverAction,ObserverAction::next>>,
                        void
                    >::type
                >::type
            >::type
        >::type;

    static_assert(
        ! std::is_same<WrapperT,void>::value,
        "Observe: Passed function does not match any of the supported signatures.");
    
    using NodeT = EventObserverNode<D,E,WrapperT>;
    
    const auto& subjectPtr = GetNodePtr(subject);

    std::unique_ptr<ObserverNode<D>> nodePtr( new NodeT(subjectPtr, std::forward<FIn>(func)) );
    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver(std::move(nodePtr));

    return Observer<D>( rawNodePtr, subjectPtr );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Observe - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename FIn,
    typename E,
    typename ... TDepValues
>
auto Observe(const Events<D,E>& subject,
             const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Observer<D>
{
    using REACT_IMPL::IObserver;
    using REACT_IMPL::ObserverNode;
    using REACT_IMPL::SyncedObserverNode;
    using REACT_IMPL::AddDefaultReturnValueWrapper;
    using REACT_IMPL::AddObserverRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using WrapperT =
        typename std::conditional<
            IsCallableWith<F, ObserverAction, EventRange<E>, TDepValues ...>::value,
            F,
            typename std::conditional<
                IsCallableWith<F, ObserverAction, E, TDepValues ...>::value,
                AddObserverRangeWrapper<E, F, TDepValues ...>,
                typename std::conditional<
                    IsCallableWith<F, void, EventRange<E>, TDepValues ...>::value,
                    AddDefaultReturnValueWrapper<F, ObserverAction ,ObserverAction::next>,
                    typename std::conditional<
                        IsCallableWith<F, void, E, TDepValues ...>::value,
                        AddObserverRangeWrapper<E,
                            AddDefaultReturnValueWrapper<F,ObserverAction,ObserverAction::next>,
                                TDepValues...>,
                            void
                        >::type
                >::type
            >::type
        >::type;

    static_assert(
        ! std::is_same<WrapperT,void>::value,
        "Observe: Passed function does not match any of the supported signatures.");

    using NodeT = SyncedObserverNode<D,E,WrapperT,TDepValues ...>;

    struct NodeBuilder_
    {
        NodeBuilder_(const Events<D,E>& subject, FIn&& func) :
            MySubject( subject ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> ObserverNode<D>*
        {
            return new NodeT(
                GetNodePtr(MySubject), std::forward<FIn>(MyFunc), GetNodePtr(deps) ... );
        }

        const Events<D,E>& MySubject;
        FIn MyFunc;
    };

    const auto& subjectPtr = GetNodePtr(subject);

    std::unique_ptr<ObserverNode<D>> nodePtr( REACT_IMPL::apply(
        NodeBuilder_( subject, std::forward<FIn>(func) ),
        depPack.Data) );

    ObserverNode<D>* rawNodePtr = nodePtr.get();

    subjectPtr->RegisterObserver(std::move(nodePtr));

    return Observer<D>( rawNodePtr, subjectPtr );
}

/******************************************/ REACT_END /******************************************/

#endif // REACT_OBSERVER_H_INCLUDED
// #include "react/TypeTraits.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_TYPETRAITS_H_INCLUDED
#define REACT_TYPETRAITS_H_INCLUDED



// #include "react/detail/Defs.h"


/*****************************************/ REACT_BEGIN /*****************************************/

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

template <typename TSourceDomain, typename TTargetDomain>
class Continuation;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsSignal { static const bool value = false; };

template <typename D, typename T>
struct IsSignal<Signal<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsSignal<VarSignal<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsSignal<TempSignal<D,T,TOp>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsEvent { static const bool value = false; };

template <typename D, typename T>
struct IsEvent<Events<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsEvent<EventSource<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsEvent<TempEvents<D,T,TOp>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsObserver
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsObserver { static const bool value = false; };

template <typename D>
struct IsObserver<Observer<D>> { static const bool value = true; };

template <typename D>
struct IsObserver<ScopedObserver<D>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsContinuation
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsContinuation { static const bool value = false; };

template <typename D, typename D2>
struct IsContinuation<Continuation<D,D2>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsObservable
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsObservable { static const bool value = false; };

template <typename D, typename T>
struct IsObservable<Signal<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsObservable<VarSignal<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsObservable<TempSignal<D,T,TOp>> { static const bool value = true; };

template <typename D, typename T>
struct IsObservable<Events<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsObservable<EventSource<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsObservable<TempEvents<D,T,TOp>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IsReactive
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct IsReactive { static const bool value = false; };

template <typename D, typename T>
struct IsReactive<Signal<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsReactive<VarSignal<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsReactive<TempSignal<D,T,TOp>> { static const bool value = true; };

template <typename D, typename T>
struct IsReactive<Events<D,T>> { static const bool value = true; };

template <typename D, typename T>
struct IsReactive<EventSource<D,T>> { static const bool value = true; };

template <typename D, typename T, typename TOp>
struct IsReactive<TempEvents<D,T,TOp>> { static const bool value = true; };

template <typename D>
struct IsReactive<Observer<D>> { static const bool value = true; };

template <typename D>
struct IsReactive<ScopedObserver<D>> { static const bool value = true; };

template <typename D, typename D2>
struct IsReactive<Continuation<D,D2>> { static const bool value = true; };

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DecayInput
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct DecayInput { using Type = T; };

template <typename D, typename T>
struct DecayInput<VarSignal<D,T>> { using Type = Signal<D,T>; };

template <typename D, typename T>
struct DecayInput<EventSource<D,T>> { using Type = Events<D,T>; };

/******************************************/ REACT_END /******************************************/

#endif // REACT_TYPETRAITS_H_INCLUDED
// #include "react/detail/SignalBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_SIGNALBASE_H_INCLUDED
#define REACT_DETAIL_SIGNALBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <utility>

// #include "react/detail/ReactiveBase.h"

// #include "react/detail/ReactiveInput.h"

// #include "react/detail/graph/SignalNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)



#ifndef REACT_DETAIL_GRAPH_SIGNALNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_SIGNALNODES_H_INCLUDED

// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "GraphBase.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename L, typename R>
bool Equals(const L& lhs, const R& rhs);

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class SignalNode : public ObservableNode<D>
{
public:
    SignalNode() = default;

    template <typename T>
    explicit SignalNode(T&& value) :
        SignalNode::ObservableNode( ),
        value_( std::forward<T>(value) )
    {}

    const S& ValueRef() const
    {
        return value_;
    }

protected:
    S value_;
};

template <typename D, typename S>
using SignalNodePtrT = std::shared_ptr<SignalNode<D,S>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class VarNode :
    public SignalNode<D,S>,
    public IInputNode
{
    using Engine = typename VarNode::Engine;

public:
    template <typename T>
    VarNode(T&& value) :
        VarNode::SignalNode( std::forward<T>(value) ),
        newValue_( value )
    {
        Engine::OnNodeCreate(*this);
    }

    ~VarNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "VarNode"; }
    virtual bool        IsInputNode() const override        { return true; }
    virtual int         DependencyCount() const override    { return 0; }

    virtual void Tick(void* turnPtr) override
    {
        REACT_ASSERT(false, "Ticked VarNode\n");
    }

    template <typename V>
    void AddInput(V&& newValue)
    {
        newValue_ = std::forward<V>(newValue);

        isInputAdded_ = true;

        // isInputAdded_ takes precedences over isInputModified_
        // the only difference between the two is that isInputModified_ doesn't/can't compare
        isInputModified_ = false;
    }

    // This is signal-specific
    template <typename F>
    void ModifyInput(F& func)
    {
        // There hasn't been any Set(...) input yet, modify.
        if (! isInputAdded_)
        {
            func(this->value_);

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

    virtual bool ApplyInput(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        if (isInputAdded_)
        {
            isInputAdded_ = false;

            if (! Equals(this->value_, newValue_))
            {
                this->value_ = std::move(newValue_);
                Engine::OnInputChange(*this, turn);
                return true;
            }
            else
            {
                return false;
            }
        }
        else if (isInputModified_)
        {
            isInputModified_ = false;

            Engine::OnInputChange(*this, turn);
            return true;
        }

        else
        {
            return false;
        }
    }

private:
    S       newValue_;
    bool    isInputAdded_ = false;
    bool    isInputModified_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FunctionOp
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename S,
    typename F,
    typename ... TDeps
>
class FunctionOp : public ReactiveOpBase<TDeps...>
{
public:
    template <typename FIn, typename ... TDepsIn>
    FunctionOp(FIn&& func, TDepsIn&& ... deps) :
        FunctionOp::ReactiveOpBase( DontMove(), std::forward<TDepsIn>(deps) ... ),
        func_( std::forward<FIn>(func) )
    {}

    FunctionOp(FunctionOp&& other) :
        FunctionOp::ReactiveOpBase( std::move(other) ),
        func_( std::move(other.func_) )
    {}

    S Evaluate()
    {
        return apply(EvalFunctor( func_ ), this->deps_);
    }

private:
    // Eval
    struct EvalFunctor
    {
        EvalFunctor(F& f) : MyFunc( f )   {}

        template <typename ... T>
        S operator()(T&& ... args)
        {
            return MyFunc(eval(args) ...);
        }

        template <typename T>
        static auto eval(T& op) -> decltype(op.Evaluate())
        {
            return op.Evaluate();
        }

        template <typename T>
        static auto eval(const std::shared_ptr<T>& depPtr) -> decltype(depPtr->ValueRef())
        {
            return depPtr->ValueRef();
        }

        F& MyFunc;
    };

private:
    F   func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalOpNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename TOp
>
class SignalOpNode :
    public SignalNode<D,S>
{
    using Engine = typename SignalOpNode::Engine;

public:
    template <typename ... TArgs>
    SignalOpNode(TArgs&& ... args) :
        SignalOpNode::SignalNode( ),
        op_( std::forward<TArgs>(args) ... )
    {
        this->value_ = op_.Evaluate();

        Engine::OnNodeCreate(*this);
        op_.template Attach<D>(*this);
    }

    ~SignalOpNode()
    {
        if (!wasOpStolen_)
            op_.template Detach<D>(*this);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));
        
        bool changed = false;

        {// timer
            using TimerT = typename SignalOpNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, 1 );

            S newValue = op_.Evaluate();

            if (! Equals(this->value_, newValue))
            {
                this->value_ = std::move(newValue);
                changed = true;
            }
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SignalOpNode"; }
    virtual int         DependencyCount() const override    { return TOp::dependency_count; }

    TOp StealOp()
    {
        REACT_ASSERT(wasOpStolen_ == false, "Op was already stolen.");
        wasOpStolen_ = true;
        op_.template Detach<D>(*this);
        return std::move(op_);
    }

private:
    TOp     op_;
    bool    wasOpStolen_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// FlattenNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TOuter,
    typename TInner
>
class FlattenNode : public SignalNode<D,TInner>
{
    using Engine = typename FlattenNode::Engine;

public:
    FlattenNode(const std::shared_ptr<SignalNode<D,TOuter>>& outer,
                const std::shared_ptr<SignalNode<D,TInner>>& inner) :
        FlattenNode::SignalNode( inner->ValueRef() ),
        outer_( outer ),
        inner_( inner )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *outer_);
        Engine::OnNodeAttach(*this, *inner_);
    }

    ~FlattenNode()
    {
        Engine::OnNodeDetach(*this, *inner_);
        Engine::OnNodeDetach(*this, *outer_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        auto newInner = GetNodePtr(outer_->ValueRef());

        if (newInner != inner_)
        {
            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::OnDynamicNodeDetach(*this, *oldInner, turn);
            Engine::OnDynamicNodeAttach(*this, *newInner, turn);

            return;
        }

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! Equals(this->value_, inner_->ValueRef()))
        {
            this->value_ = inner_->ValueRef();
            Engine::OnNodePulse(*this, turn);
        }
        else
        {
            Engine::OnNodeIdlePulse(*this, turn);
        }
    }

    virtual const char* GetNodeType() const override        { return "FlattenNode"; }
    virtual bool        IsDynamicNode() const override      { return true; }
    virtual int         DependencyCount() const override    { return 2; }

private:
    std::shared_ptr<SignalNode<D,TOuter>>   outer_;
    std::shared_ptr<SignalNode<D,TInner>>   inner_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_SIGNALNODES_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SignalBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class SignalBase : public CopyableReactive<SignalNode<D,S>>
{
public:
    SignalBase() = default;
    SignalBase(const SignalBase&) = default;
    
    template <typename T>
    SignalBase(T&& t) :
        SignalBase::CopyableReactive( std::forward<T>(t) )
    {}

protected:
    const S& getValue() const
    {
        return this->ptr_->ValueRef();
    }

    template <typename T>
    void setValue(T&& newValue) const
    {
        DomainSpecificInputManager<D>::Instance().AddInput(
            *reinterpret_cast<VarNode<D,S>*>(this->ptr_.get()),
            std::forward<T>(newValue));
    }

    template <typename F>
    void modifyValue(const F& func) const
    {
        DomainSpecificInputManager<D>::Instance().ModifyInput(
            *reinterpret_cast<VarNode<D,S>*>(this->ptr_.get()), func);
    }
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_SIGNALBASE_H_INCLUDED


/*****************************************/ REACT_BEGIN /*****************************************/

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
template
<
    typename D,
    typename ... TValues
>
class SignalPack
{
public:
    SignalPack(const Signal<D,TValues>&  ... deps) :
        Data( std::tie(deps ...) )
    {}

    template <typename ... TCurValues, typename TAppendValue>
    SignalPack(const SignalPack<D, TCurValues ...>& curArgs, const Signal<D,TAppendValue>& newArg) :
        Data( std::tuple_cat(curArgs.Data, std::tie(newArg)) )
    {}

    std::tuple<const Signal<D,TValues>& ...> Data;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// With - Utility function to create a SignalPack
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename ... TValues
>
auto With(const Signal<D,TValues>&  ... deps)
    -> SignalPack<D,TValues ...>
{
    return SignalPack<D,TValues...>(deps ...);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVar
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    class = typename std::enable_if<
        ! IsSignal<S>::value>::type,
    class = typename std::enable_if<
        ! IsEvent<S>::value>::type
>
auto MakeVar(V&& value)
    -> VarSignal<D,S>
{
    return VarSignal<D,S>(
        std::make_shared<REACT_IMPL::VarNode<D,S>>(
            std::forward<V>(value)));
}

template
<
    typename D,
    typename S
>
auto MakeVar(std::reference_wrapper<S> value)
    -> VarSignal<D,S&>
{
    return VarSignal<D,S&>(
        std::make_shared<REACT_IMPL::VarNode<D,std::reference_wrapper<S>>>(value));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeVar (higher order reactives)
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename TInner = typename S::ValueT,
    class = typename std::enable_if<
        IsSignal<S>::value>::type
>
auto MakeVar(V&& value)
    -> VarSignal<D,Signal<D,TInner>>
{
    return VarSignal<D,Signal<D,TInner>>(
        std::make_shared<REACT_IMPL::VarNode<D,Signal<D,TInner>>>(
            std::forward<V>(value)));
}

template
<
    typename D,
    typename V,
    typename S = typename std::decay<V>::type,
    typename TInner = typename S::ValueT,
    class = typename std::enable_if<
        IsEvent<S>::value>::type
>
auto MakeVar(V&& value)
    -> VarSignal<D,Events<D,TInner>>
{
    return VarSignal<D,Events<D,TInner>>(
        std::make_shared<REACT_IMPL::VarNode<D,Events<D,TInner>>>(
            std::forward<V>(value)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
// Single arg
template
<
    typename D,
    typename TValue,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename S = typename std::result_of<F(TValue)>::type,
    typename TOp = REACT_IMPL::FunctionOp<S,F, REACT_IMPL::SignalNodePtrT<D,TValue>>
>
auto MakeSignal(const Signal<D,TValue>& arg, FIn&& func)
    -> TempSignal<D,S,TOp>
{
    return TempSignal<D,S,TOp>(
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(
            std::forward<FIn>(func), GetNodePtr(arg)));
}

// Multiple args
template
<
    typename D,
    typename ... TValues,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename S = typename std::result_of<F(TValues...)>::type,
    typename TOp = REACT_IMPL::FunctionOp<S,F, REACT_IMPL::SignalNodePtrT<D,TValues> ...>
>
auto MakeSignal(const SignalPack<D,TValues...>& argPack, FIn&& func)
    -> TempSignal<D,S,TOp>
{
    using REACT_IMPL::SignalOpNode;

    struct NodeBuilder_
    {
        NodeBuilder_(FIn&& func) :
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TValues>& ... args)
            -> TempSignal<D,S,TOp>
        {
            return TempSignal<D,S,TOp>(
                std::make_shared<SignalOpNode<D,S,TOp>>(
                    std::forward<FIn>(MyFunc), GetNodePtr(args) ...));
        }

        FIn     MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( std::forward<FIn>(func) ),
        argPack.Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Unary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP(op,name)                                                   \
template <typename T>                                                               \
struct name ## OpFunctor                                                            \
{                                                                                   \
    T operator()(const T& v) const { return op v; }                                 \
};                                                                                  \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TSignal,                                                               \
    typename D = typename TSignal::DomainT,                                         \
    typename TVal = typename TSignal::ValueT,                                       \
    class = typename std::enable_if<                                                \
        IsSignal<TSignal>::value>::type,                                            \
    typename F = name ## OpFunctor<TVal>,                                           \
    typename S = typename std::result_of<F(TVal)>::type,                            \
    typename TOp = REACT_IMPL::FunctionOp<S,F,REACT_IMPL::SignalNodePtrT<D,TVal>>   \
>                                                                                   \
auto operator op(const TSignal& arg)                                                \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), GetNodePtr(arg)));                                                 \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename D,                                                                     \
    typename TVal,                                                                  \
    typename TOpIn,                                                                 \
    typename F = name ## OpFunctor<TVal>,                                           \
    typename S = typename std::result_of<F(TVal)>::type,                            \
    typename TOp = REACT_IMPL::FunctionOp<S,F,TOpIn>                                \
>                                                                                   \
auto operator op(TempSignal<D,TVal,TOpIn>&& arg)                                    \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), arg.StealOp()));                                                   \
}

REACT_DECLARE_OP(+, UnaryPlus)
REACT_DECLARE_OP(-, UnaryMinus)
REACT_DECLARE_OP(!, LogicalNegation)
REACT_DECLARE_OP(~, BitwiseComplement)
REACT_DECLARE_OP(++, Increment)
REACT_DECLARE_OP(--, Decrement)

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Binary operators
///////////////////////////////////////////////////////////////////////////////////////////////////
#define REACT_DECLARE_OP(op,name)                                                   \
template <typename L, typename R>                                                   \
struct name ## OpFunctor                                                            \
{                                                                                   \
    auto operator()(const L& lhs, const R& rhs) const                               \
        -> decltype(std::declval<L>() op std::declval<R>())                         \
    {                                                                               \
        return lhs op rhs;                                                          \
    }                                                                               \
};                                                                                  \
                                                                                    \
template <typename L, typename R>                                                   \
struct name ## OpRFunctor                                                           \
{                                                                                   \
    name ## OpRFunctor(name ## OpRFunctor&& other) :                                \
        LeftVal( std::move(other.LeftVal) )                                         \
    {}                                                                              \
                                                                                    \
    template <typename T>                                                           \
    name ## OpRFunctor(T&& val) :                                                   \
        LeftVal( std::forward<T>(val) )                                             \
    {}                                                                              \
                                                                                    \
    name ## OpRFunctor(const name ## OpRFunctor& other) = delete;                   \
                                                                                    \
    auto operator()(const R& rhs) const                                             \
        -> decltype(std::declval<L>() op std::declval<R>())                         \
    {                                                                               \
        return LeftVal op rhs;                                                      \
    }                                                                               \
                                                                                    \
    L LeftVal;                                                                      \
};                                                                                  \
                                                                                    \
template <typename L, typename R>                                                   \
struct name ## OpLFunctor                                                           \
{                                                                                   \
    name ## OpLFunctor(name ## OpLFunctor&& other) :                                \
        RightVal( std::move(other.RightVal) )                                       \
    {}                                                                              \
                                                                                    \
    template <typename T>                                                           \
    name ## OpLFunctor(T&& val) :                                                   \
        RightVal( std::forward<T>(val) )                                            \
    {}                                                                              \
                                                                                    \
    name ## OpLFunctor(const name ## OpLFunctor& other) = delete;                   \
                                                                                    \
    auto operator()(const L& lhs) const                                             \
        -> decltype(std::declval<L>() op std::declval<R>())                         \
    {                                                                               \
        return lhs op RightVal;                                                     \
    }                                                                               \
                                                                                    \
    R RightVal;                                                                     \
};                                                                                  \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TLeftSignal,                                                           \
    typename TRightSignal,                                                          \
    typename D = typename TLeftSignal::DomainT,                                     \
    typename TLeftVal = typename TLeftSignal::ValueT,                               \
    typename TRightVal = typename TRightSignal::ValueT,                             \
    class = typename std::enable_if<                                                \
        IsSignal<TLeftSignal>::value>::type,                                        \
    class = typename std::enable_if<                                                \
        IsSignal<TRightSignal>::value>::type,                                       \
    typename F = name ## OpFunctor<TLeftVal,TRightVal>,                             \
    typename S = typename std::result_of<F(TLeftVal,TRightVal)>::type,              \
    typename TOp = REACT_IMPL::FunctionOp<S,F,                                      \
        REACT_IMPL::SignalNodePtrT<D,TLeftVal>,                                     \
        REACT_IMPL::SignalNodePtrT<D,TRightVal>>                                    \
>                                                                                   \
auto operator op(const TLeftSignal& lhs, const TRightSignal& rhs)                   \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), GetNodePtr(lhs), GetNodePtr(rhs)));                                \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TLeftSignal,                                                           \
    typename TRightValIn,                                                           \
    typename D = typename TLeftSignal::DomainT,                                     \
    typename TLeftVal = typename TLeftSignal::ValueT,                               \
    typename TRightVal = typename std::decay<TRightValIn>::type,                    \
    class = typename std::enable_if<                                                \
        IsSignal<TLeftSignal>::value>::type,                                        \
    class = typename std::enable_if<                                                \
        ! IsSignal<TRightVal>::value>::type,                                        \
    typename F = name ## OpLFunctor<TLeftVal,TRightVal>,                            \
    typename S = typename std::result_of<F(TLeftVal)>::type,                        \
    typename TOp = REACT_IMPL::FunctionOp<S,F,                                      \
        REACT_IMPL::SignalNodePtrT<D,TLeftVal>>                                     \
>                                                                                   \
auto operator op(const TLeftSignal& lhs, TRightValIn&& rhs)                         \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F( std::forward<TRightValIn>(rhs) ), GetNodePtr(lhs)));                 \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TLeftValIn,                                                            \
    typename TRightSignal,                                                          \
    typename D = typename TRightSignal::DomainT,                                    \
    typename TLeftVal = typename std::decay<TLeftValIn>::type,                      \
    typename TRightVal = typename TRightSignal::ValueT,                             \
    class = typename std::enable_if<                                                \
        ! IsSignal<TLeftVal>::value>::type,                                         \
    class = typename std::enable_if<                                                \
        IsSignal<TRightSignal>::value>::type,                                       \
    typename F = name ## OpRFunctor<TLeftVal,TRightVal>,                            \
    typename S = typename std::result_of<F(TRightVal)>::type,                       \
    typename TOp = REACT_IMPL::FunctionOp<S,F,                                      \
        REACT_IMPL::SignalNodePtrT<D,TRightVal>>                                    \
>                                                                                   \
auto operator op(TLeftValIn&& lhs, const TRightSignal& rhs)                         \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F( std::forward<TLeftValIn>(lhs) ), GetNodePtr(rhs)));                  \
}                                                                                   \
template                                                                            \
<                                                                                   \
    typename D,                                                                     \
    typename TLeftVal,                                                              \
    typename TLeftOp,                                                               \
    typename TRightVal,                                                             \
    typename TRightOp,                                                              \
    typename F = name ## OpFunctor<TLeftVal,TRightVal>,                             \
    typename S = typename std::result_of<F(TLeftVal,TRightVal)>::type,              \
    typename TOp = REACT_IMPL::FunctionOp<S,F,TLeftOp,TRightOp>                     \
>                                                                                   \
auto operator op(TempSignal<D,TLeftVal,TLeftOp>&& lhs,                              \
                 TempSignal<D,TRightVal,TRightOp>&& rhs)                            \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), lhs.StealOp(), rhs.StealOp()));                                    \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename D,                                                                     \
    typename TLeftVal,                                                              \
    typename TLeftOp,                                                               \
    typename TRightSignal,                                                          \
    typename TRightVal = typename TRightSignal::ValueT,                             \
    class = typename std::enable_if<                                                \
        IsSignal<TRightSignal>::value>::type,                                       \
    typename F = name ## OpFunctor<TLeftVal,TRightVal>,                             \
    typename S = typename std::result_of<F(TLeftVal,TRightVal)>::type,              \
    typename TOp = REACT_IMPL::FunctionOp<S,F,                                      \
        TLeftOp,                                                                    \
        REACT_IMPL::SignalNodePtrT<D,TRightVal>>                                    \
>                                                                                   \
auto operator op(TempSignal<D,TLeftVal,TLeftOp>&& lhs,                              \
                 const TRightSignal& rhs)                                           \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), lhs.StealOp(), GetNodePtr(rhs)));                                  \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TLeftSignal,                                                           \
    typename D,                                                                     \
    typename TRightVal,                                                             \
    typename TRightOp,                                                              \
    typename TLeftVal = typename TLeftSignal::ValueT,                               \
    class = typename std::enable_if<                                                \
        IsSignal<TLeftSignal>::value>::type,                                        \
    typename F = name ## OpFunctor<TLeftVal,TRightVal>,                             \
    typename S = typename std::result_of<F(TLeftVal,TRightVal)>::type,              \
    typename TOp = REACT_IMPL::FunctionOp<S,F,                                      \
        REACT_IMPL::SignalNodePtrT<D,TLeftVal>,                                     \
        TRightOp>                                                                   \
>                                                                                   \
auto operator op(const TLeftSignal& lhs, TempSignal<D,TRightVal,TRightOp>&& rhs)    \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F(), GetNodePtr(lhs), rhs.StealOp()));                                  \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename D,                                                                     \
    typename TLeftVal,                                                              \
    typename TLeftOp,                                                               \
    typename TRightValIn,                                                           \
    typename TRightVal = typename std::decay<TRightValIn>::type,                    \
    class = typename std::enable_if<                                                \
        ! IsSignal<TRightVal>::value>::type,                                        \
    typename F = name ## OpLFunctor<TLeftVal,TRightVal>,                            \
    typename S = typename std::result_of<F(TLeftVal)>::type,                        \
    typename TOp = REACT_IMPL::FunctionOp<S,F,TLeftOp>                              \
>                                                                                   \
auto operator op(TempSignal<D,TLeftVal,TLeftOp>&& lhs, TRightValIn&& rhs)           \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F( std::forward<TRightValIn>(rhs) ), lhs.StealOp()));                   \
}                                                                                   \
                                                                                    \
template                                                                            \
<                                                                                   \
    typename TLeftValIn,                                                            \
    typename D,                                                                     \
    typename TRightVal,                                                             \
    typename TRightOp,                                                              \
    typename TLeftVal = typename std::decay<TLeftValIn>::type,                      \
    class = typename std::enable_if<                                                \
        ! IsSignal<TLeftVal>::value>::type,                                         \
    typename F = name ## OpRFunctor<TLeftVal,TRightVal>,                            \
    typename S = typename std::result_of<F(TRightVal)>::type,                       \
    typename TOp = REACT_IMPL::FunctionOp<S,F,TRightOp>                             \
>                                                                                   \
auto operator op(TLeftValIn&& lhs, TempSignal<D,TRightVal,TRightOp>&& rhs)          \
    -> TempSignal<D,S,TOp>                                                          \
{                                                                                   \
    return TempSignal<D,S,TOp>(                                                     \
        std::make_shared<REACT_IMPL::SignalOpNode<D,S,TOp>>(                        \
            F( std::forward<TLeftValIn>(lhs) ), rhs.StealOp()));                    \
}

REACT_DECLARE_OP(+, Addition)
REACT_DECLARE_OP(-, Subtraction)
REACT_DECLARE_OP(*, Multiplication)
REACT_DECLARE_OP(/, Division)
REACT_DECLARE_OP(%, Modulo)

REACT_DECLARE_OP(==, Equal)
REACT_DECLARE_OP(!=, NotEqual)
REACT_DECLARE_OP(<, Less)
REACT_DECLARE_OP(<=, LessEqual)
REACT_DECLARE_OP(>, Greater)
REACT_DECLARE_OP(>=, GreaterEqual)

REACT_DECLARE_OP(&&, LogicalAnd)
REACT_DECLARE_OP(||, LogicalOr)

REACT_DECLARE_OP(&, BitwiseAnd)
REACT_DECLARE_OP(|, BitwiseOr)
REACT_DECLARE_OP(^, BitwiseXor)
//REACT_DECLARE_OP(<<, BitwiseLeftShift); // MSVC: Internal compiler error
//REACT_DECLARE_OP(>>, BitwiseRightShift);

#undef REACT_DECLARE_OP

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to create signal pack from 2 signals.
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TLeftVal,
    typename TRightVal
>
auto operator,(const Signal<D,TLeftVal>& a, const Signal<D,TRightVal>& b)
    -> SignalPack<D,TLeftVal, TRightVal>
{
    return SignalPack<D, TLeftVal, TRightVal>(a, b);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Comma operator overload to append node to existing signal pack.
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename ... TCurValues,
    typename TAppendValue
>
auto operator,(const SignalPack<D, TCurValues ...>& cur, const Signal<D,TAppendValue>& append)
    -> SignalPack<D,TCurValues ... , TAppendValue>
{
    return SignalPack<D, TCurValues ... , TAppendValue>(cur, append);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// operator->* overload to connect signals to a function and return the resulting signal.
///////////////////////////////////////////////////////////////////////////////////////////////////
// Single arg
template
<
    typename D,
    typename F,
    template <typename D_, typename V_> class TSignal,
    typename TValue,
    class = typename std::enable_if<
        IsSignal<TSignal<D,TValue>>::value>::type
>
auto operator->*(const TSignal<D,TValue>& arg, F&& func)
    -> Signal<D, typename std::result_of<F(TValue)>::type>
{
    return REACT::MakeSignal(arg, std::forward<F>(func));
}

// Multiple args
template
<
    typename D,
    typename F,
    typename ... TValues
>
auto operator->*(const SignalPack<D,TValues ...>& argPack, F&& func)
    -> Signal<D, typename std::result_of<F(TValues ...)>::type>
{
    return REACT::MakeSignal(argPack, std::forward<F>(func));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TInnerValue
>
auto Flatten(const Signal<D,Signal<D,TInnerValue>>& outer)
    -> Signal<D,TInnerValue>
{
    return Signal<D,TInnerValue>(
        std::make_shared<REACT_IMPL::FlattenNode<D, Signal<D,TInnerValue>, TInnerValue>>(
            GetNodePtr(outer), GetNodePtr(outer.Value())));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class Signal : public REACT_IMPL::SignalBase<D,S>
{
private:
    using NodeT     = REACT_IMPL::SignalNode<D,S>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal(const Signal&) = default;

    // Move ctor
    Signal(Signal&& other) :
        Signal::SignalBase( std::move(other) )
    {}

    // Node ctor
    explicit Signal(NodePtrT&& nodePtr) :
        Signal::SignalBase( std::move(nodePtr) )
    {}

    // Copy assignment
    Signal& operator=(const Signal&) = default;

    // Move assignment
    Signal& operator=(Signal&& other)
    {
        Signal::SignalBase::operator=( std::move(other) );
        return *this;
    }

    const S& Value() const      { return Signal::SignalBase::getValue(); }
    const S& operator()() const { return Signal::SignalBase::getValue(); }

    bool Equals(const Signal& other) const
    {
        return Signal::SignalBase::Equals(other);
    }

    bool IsValid() const
    {
        return Signal::SignalBase::IsValid();
    }

    void SetWeightHint(WeightHint weight)
    {
        Signal::SignalBase::SetWeightHint(weight);
    }

    S Flatten() const
    {
        static_assert(IsSignal<S>::value || IsEvent<S>::value,
            "Flatten requires a Signal or Events value type.");
        return REACT::Flatten(*this);
    }
};

// Specialize for references
template
<
    typename D,
    typename S
>
class Signal<D,S&> : public REACT_IMPL::SignalBase<D,std::reference_wrapper<S>>
{
private:
    using NodeT     = REACT_IMPL::SignalNode<D,std::reference_wrapper<S>>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    Signal() = default;

    // Copy ctor
    Signal(const Signal&) = default;

    // Move ctor
    Signal(Signal&& other) :
        Signal::SignalBase( std::move(other) )
    {}

    // Node ctor
    explicit Signal(NodePtrT&& nodePtr) :
        Signal::SignalBase( std::move(nodePtr) )
    {}

    // Copy assignment
    Signal& operator=(const Signal&) = default;

    // Move assignment
    Signal& operator=(Signal&& other)
    {
        Signal::SignalBase::operator=( std::move(other) );
        return *this;
    }

    const S& Value() const      { return Signal::SignalBase::getValue(); }
    const S& operator()() const { return Signal::SignalBase::getValue(); }

    bool Equals(const Signal& other) const
    {
        return Signal::SignalBase::Equals(other);
    }

    bool IsValid() const
    {
        return Signal::SignalBase::IsValid();
    }

    void SetWeightHint(WeightHint weight)
    {
        Signal::SignalBase::SetWeightHint(weight);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// VarSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class VarSignal : public Signal<D,S>
{
private:
    using NodeT     = REACT_IMPL::VarNode<D,S>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal(const VarSignal&) = default;

    // Move ctor
    VarSignal(VarSignal&& other) :
        VarSignal::Signal( std::move(other) )
    {}

    // Node ctor
    explicit VarSignal(NodePtrT&& nodePtr) :
        VarSignal::Signal( std::move(nodePtr) )
    {}

    // Copy assignment
    VarSignal& operator=(const VarSignal&) = default;

    // Move assignment
    VarSignal& operator=(VarSignal&& other)
    {
        VarSignal::SignalBase::operator=( std::move(other) );
        return *this;
    }

    void Set(const S& newValue) const
    {
        VarSignal::SignalBase::setValue(newValue);
    }

    void Set(S&& newValue) const
    {
        VarSignal::SignalBase::setValue(std::move(newValue));
    }

    const VarSignal& operator<<=(const S& newValue) const
    {
        VarSignal::SignalBase::setValue(newValue);
        return *this;
    }

    const VarSignal& operator<<=(S&& newValue) const
    {
        VarSignal::SignalBase::setValue(std::move(newValue));
        return *this;
    }

    template <typename F>
    void Modify(const F& func) const
    {
        VarSignal::SignalBase::modifyValue(func);
    }
};

// Specialize for references
template
<
    typename D,
    typename S
>
class VarSignal<D,S&> : public Signal<D,std::reference_wrapper<S>>
{
private:
    using NodeT     = REACT_IMPL::VarNode<D,std::reference_wrapper<S>>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    using ValueT = S;

    // Default ctor
    VarSignal() = default;

    // Copy ctor
    VarSignal(const VarSignal&) = default;

    // Move ctor
    VarSignal(VarSignal&& other) :
        VarSignal::Signal( std::move(other) )
    {}

    // Node ctor
    explicit VarSignal(NodePtrT&& nodePtr) :
        VarSignal::Signal( std::move(nodePtr) )
    {}

    // Copy assignment
    VarSignal& operator=(const VarSignal&) = default;

    // Move assignment
    VarSignal& operator=(VarSignal&& other)
    {
        VarSignal::Signal::operator=( std::move(other) );
        return *this;
    }

    void Set(std::reference_wrapper<S> newValue) const
    {
        VarSignal::SignalBase::setValue(newValue);
    }

    const VarSignal& operator<<=(std::reference_wrapper<S> newValue) const
    {
        VarSignal::SignalBase::setValue(newValue);
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TempSignal
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename TOp
>
class TempSignal : public Signal<D,S>
{
private:
    using NodeT     = REACT_IMPL::SignalOpNode<D,S,TOp>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempSignal() = default;

    // Copy ctor
    TempSignal(const TempSignal&) = default;

    // Move ctor
    TempSignal(TempSignal&& other) :
        TempSignal::Signal( std::move(other) )
    {}

    // Node ctor
    explicit TempSignal(NodePtrT&& ptr) :
        TempSignal::Signal( std::move(ptr) )
    {}

    // Copy assignment
    TempSignal& operator=(const TempSignal&) = default;

    // Move assignemnt
    TempSignal& operator=(TempSignal&& other)
    {
        TempSignal::Signal::operator=( std::move(other) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move(reinterpret_cast<NodeT*>(this->ptr_.get())->StealOp());
    }
};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <typename D, typename L, typename R>
bool Equals(const Signal<D,L>& lhs, const Signal<D,R>& rhs)
{
    return lhs.Equals(rhs);
}

/****************************************/ REACT_IMPL_END /***************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten macros
///////////////////////////////////////////////////////////////////////////////////////////////////
// Note: Using static_cast rather than -> return type, because when using lambda for inline
// class initialization, decltype did not recognize the parameter r
// Note2: MSVC doesn't like typename in the lambda
#if _MSC_VER && !__INTEL_COMPILER
    #define REACT_MSVC_NO_TYPENAME
#else
    #define REACT_MSVC_NO_TYPENAME typename
#endif

#define REACTIVE_REF(obj, name)                                                             \
    Flatten(                                                                                \
        MakeSignal(                                                                         \
            obj,                                                                            \
            [] (const REACT_MSVC_NO_TYPENAME                                                \
                REACT_IMPL::Identity<decltype(obj)>::Type::ValueT& r)                       \
            {                                                                               \
                using T = decltype(r.name);                                                 \
                using S = REACT_MSVC_NO_TYPENAME REACT::DecayInput<T>::Type;                \
                return static_cast<S>(r.name);                                              \
            }))

#define REACTIVE_PTR(obj, name)                                                             \
    Flatten(                                                                                \
        MakeSignal(                                                                         \
            obj,                                                                            \
            [] (REACT_MSVC_NO_TYPENAME                                                      \
                REACT_IMPL::Identity<decltype(obj)>::Type::ValueT r)                        \
            {                                                                               \
                assert(r != nullptr);                                                       \
                using T = decltype(r->name);                                                \
                using S = REACT_MSVC_NO_TYPENAME REACT::DecayInput<T>::Type;                \
                return static_cast<S>(r->name);                                             \
            }))

#endif // REACT_SIGNAL_H_INCLUDED
// #include "react/Event.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_EVENT_H_INCLUDED
#define REACT_EVENT_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <type_traits>
#include <utility>

// #include "react/Observer.h"

// #include "react/TypeTraits.h"

// #include "react/common/Util.h"

// #include "react/detail/EventBase.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_EVENTBASE_H_INCLUDED
#define REACT_DETAIL_EVENTBASE_H_INCLUDED



// #include "react/detail/Defs.h"


#include <utility>

// #include "react/detail/ReactiveBase.h"

// #include "react/detail/ReactiveInput.h"

// #include "react/detail/graph/EventNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_EVENTNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_EVENTNODES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <atomic>
#include <deque>
#include <memory>
#include <utility>

#include "tbb/spin_mutex.h"

// #include "GraphBase.h"

// #include "react/common/Concurrency.h"

// #include "react/common/Types.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Forward declarations
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename S>
class SignalNode;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// BufferClearAccessPolicy
///
/// Provides thread safe access to clear event buffer if parallel updating is enabled.
///////////////////////////////////////////////////////////////////////////////////////////////////
// Note: Weird design due to empty base class optimization
template <typename D>
struct BufferClearAccessPolicy :
    private ConditionalCriticalSection<tbb::spin_mutex, D::is_parallel>
{
    template <typename F>
    void AccessBufferForClearing(const F& f) { this->Access(f); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventStreamNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E
>
class EventStreamNode :
    public ObservableNode<D>,
    private BufferClearAccessPolicy<D>
{
public:
    using DataT     = std::vector<E>;
    using EngineT   = typename D::Engine;
    using TurnT     = typename EngineT::TurnT;

    EventStreamNode() = default;

    void SetCurrentTurn(const TurnT& turn, bool forceUpdate = false, bool noClear = false)
    {
        this->AccessBufferForClearing([&] {
            if (curTurnId_ != turn.Id() || forceUpdate)
            {
                curTurnId_ =  turn.Id();
                if (!noClear)
                    events_.clear();
            }
        });
    }

    DataT&  Events() { return events_; }

protected:
    DataT   events_;

private:
    uint    curTurnId_  { (std::numeric_limits<uint>::max)() };
};

template <typename D, typename E>
using EventStreamNodePtrT = std::shared_ptr<EventStreamNode<D,E>>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSourceNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E
>
class EventSourceNode :
    public EventStreamNode<D,E>,
    public IInputNode
{
    using Engine = typename EventSourceNode::Engine;

public:
    EventSourceNode() :
        EventSourceNode::EventStreamNode{ }
    {
        Engine::OnNodeCreate(*this);
    }

    ~EventSourceNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "EventSourceNode"; }
    virtual bool        IsInputNode() const override        { return true; }
    virtual int         DependencyCount() const override    { return 0; }

    virtual void Tick(void* turnPtr) override
    {
        REACT_ASSERT(false, "Ticked EventSourceNode\n");
    }

    template <typename V>
    void AddInput(V&& v)
    {
        // Clear input from previous turn
        if (changedFlag_)
        {
            changedFlag_ = false;
            this->events_.clear();
        }

        this->events_.push_back(std::forward<V>(v));
    }

    virtual bool ApplyInput(void* turnPtr) override
    {
        if (this->events_.size() > 0 && !changedFlag_)
        {
            using TurnT = typename D::Engine::TurnT;
            TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

            this->SetCurrentTurn(turn, true, true);
            changedFlag_ = true;
            Engine::OnInputChange(*this, turn);
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
template
<
    typename E,
    typename ... TDeps
>
class EventMergeOp : public ReactiveOpBase<TDeps...>
{
public:
    template <typename ... TDepsIn>
    EventMergeOp(TDepsIn&& ... deps) :
        EventMergeOp::ReactiveOpBase(DontMove(), std::forward<TDepsIn>(deps) ...)
    {}

    EventMergeOp(EventMergeOp&& other) :
        EventMergeOp::ReactiveOpBase( std::move(other) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect(const TTurn& turn, const TCollector& collector) const
    {
        apply(CollectFunctor<TTurn, TCollector>( turn, collector ), this->deps_);
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec(const TFunctor& functor) const
    {
        apply(reinterpret_cast<const CollectFunctor<TTurn,TCollector>&>(functor), this->deps_);
    }

private:
    template <typename TTurn, typename TCollector>
    struct CollectFunctor
    {
        CollectFunctor(const TTurn& turn, const TCollector& collector) :
            MyTurn( turn ),
            MyCollector( collector )
        {}

        void operator()(const TDeps& ... deps) const
        {
            REACT_EXPAND_PACK(collect(deps));
        }

        template <typename T>
        void collect(const T& op) const
        {
            op.template CollectRec<TTurn,TCollector>(*this);
        }

        template <typename T>
        void collect(const std::shared_ptr<T>& depPtr) const
        {
            depPtr->SetCurrentTurn(MyTurn);

            for (const auto& v : depPtr->Events())
                MyCollector(v);
        }

        const TTurn&        MyTurn;
        const TCollector&   MyCollector;
    };
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventFilterOp
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename E,
    typename TFilter,
    typename TDep
>
class EventFilterOp : public ReactiveOpBase<TDep>
{
public:
    template <typename TFilterIn, typename TDepIn>
    EventFilterOp(TFilterIn&& filter, TDepIn&& dep) :
        EventFilterOp::ReactiveOpBase{ DontMove(), std::forward<TDepIn>(dep) },
        filter_( std::forward<TFilterIn>(filter) )
    {}

    EventFilterOp(EventFilterOp&& other) :
        EventFilterOp::ReactiveOpBase{ std::move(other) },
        filter_( std::move(other.filter_) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect(const TTurn& turn, const TCollector& collector) const
    {
        collectImpl(turn, FilteredEventCollector<TCollector>{ filter_, collector }, getDep());
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec(const TFunctor& functor) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn,TCollector>(functor.MyTurn, functor.MyCollector);
    }

private:
    const TDep& getDep() const { return std::get<0>(this->deps_); }

    template <typename TCollector>
    struct FilteredEventCollector
    {
        FilteredEventCollector(const TFilter& filter, const TCollector& collector) :
            MyFilter( filter ),
            MyCollector( collector )
        {}

        void operator()(const E& e) const
        {
            // Accepted?
            if (MyFilter(e))
                MyCollector(e);
        }

        const TFilter&      MyFilter;
        const TCollector&   MyCollector;    // The wrapped collector
    };

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(const TTurn& turn, const TCollector& collector, const T& op)
    {
       op.Collect(turn, collector);
    }

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(const TTurn& turn, const TCollector& collector,
                            const std::shared_ptr<T>& depPtr)
    {
        depPtr->SetCurrentTurn(turn);

        for (const auto& v : depPtr->Events())
            collector(v);
    }

    TFilter filter_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventTransformOp
///////////////////////////////////////////////////////////////////////////////////////////////////
// Todo: Refactor code duplication
template
<
    typename E,
    typename TFunc,
    typename TDep
>
class EventTransformOp : public ReactiveOpBase<TDep>
{
public:
    template <typename TFuncIn, typename TDepIn>
    EventTransformOp(TFuncIn&& func, TDepIn&& dep) :
        EventTransformOp::ReactiveOpBase( DontMove(), std::forward<TDepIn>(dep) ),
        func_( std::forward<TFuncIn>(func) )
    {}

    EventTransformOp(EventTransformOp&& other) :
        EventTransformOp::ReactiveOpBase( std::move(other) ),
        func_( std::move(other.func_) )
    {}

    template <typename TTurn, typename TCollector>
    void Collect(const TTurn& turn, const TCollector& collector) const
    {
        collectImpl(turn, TransformEventCollector<TCollector>( func_, collector ), getDep());
    }

    template <typename TTurn, typename TCollector, typename TFunctor>
    void CollectRec(const TFunctor& functor) const
    {
        // Can't recycle functor because MyFunc needs replacing
        Collect<TTurn,TCollector>(functor.MyTurn, functor.MyCollector);
    }

private:
    const TDep& getDep() const { return std::get<0>(this->deps_); }

    template <typename TTarget>
    struct TransformEventCollector
    {
        TransformEventCollector(const TFunc& func, const TTarget& target) :
            MyFunc( func ),
            MyTarget( target )
        {}

        void operator()(const E& e) const
        {
            MyTarget(MyFunc(e));
        }

        const TFunc&    MyFunc;
        const TTarget&  MyTarget;
    };

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(const TTurn& turn, const TCollector& collector, const T& op)
    {
       op.Collect(turn, collector);
    }

    template <typename TTurn, typename TCollector, typename T>
    static void collectImpl(const TTurn& turn, const TCollector& collector, const std::shared_ptr<T>& depPtr)
    {
        depPtr->SetCurrentTurn(turn);

        for (const auto& v : depPtr->Events())
            collector(v);
    }

    TFunc func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventOpNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TOp
>
class EventOpNode :
    public EventStreamNode<D,E>
{
    using Engine = typename EventOpNode::Engine;

public:
    template <typename ... TArgs>
    EventOpNode(TArgs&& ... args) :
        EventOpNode::EventStreamNode( ),
        op_( std::forward<TArgs>(args) ... )
    {
        Engine::OnNodeCreate(*this);
        op_.template Attach<D>(*this);
    }

    ~EventOpNode()
    {
        if (!wasOpStolen_)
            op_.template Detach<D>(*this);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        {// timer
            size_t count = 0;
            using TimerT = typename EventOpNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, count );

            op_.Collect(turn, EventCollector( this->events_ ));

            // Note: Count was passed by reference, so we can still change before the dtor
            // of the scoped timer is called
            count = this->events_.size();
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "EventOpNode"; }
    virtual int         DependencyCount() const override    { return TOp::dependency_count; }

    TOp StealOp()
    {
        REACT_ASSERT(wasOpStolen_ == false, "Op was already stolen.");
        wasOpStolen_ = true;
        op_.template Detach<D>(*this);
        return std::move(op_);
    }

private:
    struct EventCollector
    {
        using DataT = typename EventOpNode::DataT;

        EventCollector(DataT& events) : MyEvents( events )  {}

        void operator()(const E& e) const { MyEvents.push_back(e); }

        DataT& MyEvents;
    };

    TOp     op_;
    bool    wasOpStolen_ = false;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventFlattenNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TOuter,
    typename TInner
>
class EventFlattenNode : public EventStreamNode<D,TInner>
{
    using Engine = typename EventFlattenNode::Engine;

public:
    EventFlattenNode(const std::shared_ptr<SignalNode<D,TOuter>>& outer,
                     const std::shared_ptr<EventStreamNode<D,TInner>>& inner) :
        EventFlattenNode::EventStreamNode( ),
        outer_( outer ),
        inner_( inner )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *outer_);
        Engine::OnNodeAttach(*this, *inner_);
    }

    ~EventFlattenNode()
    {
        Engine::OnNodeDetach(*this, *outer_);
        Engine::OnNodeDetach(*this, *inner_);
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override        { return "EventFlattenNode"; }
    virtual bool        IsDynamicNode() const override      { return true; }
    virtual int         DependencyCount() const override    { return 2; }

    virtual void Tick(void* turnPtr) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);
        inner_->SetCurrentTurn(turn);

        auto newInner = GetNodePtr(outer_->ValueRef());

        if (newInner != inner_)
        {
            newInner->SetCurrentTurn(turn);

            // Topology has been changed
            auto oldInner = inner_;
            inner_ = newInner;

            Engine::OnDynamicNodeDetach(*this, *oldInner, turn);
            Engine::OnDynamicNodeAttach(*this, *newInner, turn);

            return;
        }

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        this->events_.insert(
            this->events_.end(),
            inner_->Events().begin(),
            inner_->Events().end());

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (this->events_.size() > 0)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

private:
    std::shared_ptr<SignalNode<D,TOuter>>       outer_;
    std::shared_ptr<EventStreamNode<D,TInner>>  inner_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventTransformNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TIn,
    typename TOut,
    typename TFunc,
    typename ... TDepValues
>
class SyncedEventTransformNode :
    public EventStreamNode<D,TOut>
{
    using Engine = typename SyncedEventTransformNode::Engine;

public:
    template <typename F>
    SyncedEventTransformNode(const std::shared_ptr<EventStreamNode<D,TIn>>& source, F&& func,
                             const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedEventTransformNode::EventStreamNode( ),
        source_( source ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *source);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedEventTransformNode()
    {
        Engine::OnNodeDetach(*this, *source_);

        apply(
            DetachFunctor<D,SyncedEventTransformNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn(turn);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        // Don't time if there is nothing to do
        if (! source_->Events().empty())
        {// timer
            using TimerT = typename SyncedEventTransformNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, source_->Events().size() );

            for (const auto& e : source_->Events())
                this->events_.push_back(apply(
                        [this, &e] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                        {
                            return func_(e, args->ValueRef() ...);
                        },
                        deps_));
                    
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SyncedEventTransformNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,TIn>>   source_;

    TFunc       func_;
    DepHolderT  deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventFilterNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TFunc,
    typename ... TDepValues
>
class SyncedEventFilterNode :
    public EventStreamNode<D,E>
{
    using Engine = typename SyncedEventFilterNode::Engine;

public:
    template <typename F>
    SyncedEventFilterNode(const std::shared_ptr<EventStreamNode<D,E>>& source, F&& filter,
                          const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedEventFilterNode::EventStreamNode( ),
        source_( source ),
        filter_( std::forward<F>(filter) ),
        deps_(deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *source);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedEventFilterNode()
    {
        Engine::OnNodeDetach(*this, *source_);

        apply(
            DetachFunctor<D,SyncedEventFilterNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn(turn);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        // Don't time if there is nothing to do
        if (! source_->Events().empty())
        {// timer
            using TimerT = typename SyncedEventFilterNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, source_->Events().size() );

            for (const auto& e : source_->Events())
                if (apply(
                        [this, &e] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                        {
                            return filter_(e, args->ValueRef() ...);
                        },
                        deps_))
                    this->events_.push_back(e);
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SyncedEventFilterNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

    virtual bool IsHeavyweight() const override
    {
        return this->IsUpdateThresholdExceeded();
    }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,E>>   source_;

    TFunc       filter_;
    DepHolderT  deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TIn,
    typename TOut,
    typename TFunc
>
class EventProcessingNode :
    public EventStreamNode<D,TOut>
{
    using Engine = typename EventProcessingNode::Engine;

public:
    template <typename F>
    EventProcessingNode(const std::shared_ptr<EventStreamNode<D,TIn>>& source, F&& func) :
        EventProcessingNode::EventStreamNode( ),
        source_( source ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *source);
    }

    ~EventProcessingNode()
    {
        Engine::OnNodeDetach(*this, *source_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        {// timer
            using TimerT = typename EventProcessingNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, source_->Events().size() );

            func_(
                EventRange<TIn>( source_->Events() ),
                std::back_inserter(this->events_));

        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "EventProcessingNode"; }
    virtual int         DependencyCount() const override    { return 1; }

private:
    std::shared_ptr<EventStreamNode<D,TIn>>   source_;

    TFunc       func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedEventProcessingNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TIn,
    typename TOut,
    typename TFunc,
    typename ... TDepValues
>
class SyncedEventProcessingNode :
    public EventStreamNode<D,TOut>
{
    using Engine = typename SyncedEventProcessingNode::Engine;

public:
    template <typename F>
    SyncedEventProcessingNode(const std::shared_ptr<EventStreamNode<D,TIn>>& source, F&& func,
                             const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedEventProcessingNode::EventStreamNode( ),
        source_( source ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *source);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedEventProcessingNode()
    {
        Engine::OnNodeDetach(*this, *source_);

        apply(
            DetachFunctor<D,SyncedEventProcessingNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);
        // Update of this node could be triggered from deps,
        // so make sure source doesnt contain events from last turn
        source_->SetCurrentTurn(turn);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        // Don't time if there is nothing to do
        if (! source_->Events().empty())
        {// timer
            using TimerT = typename SyncedEventProcessingNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, source_->Events().size() );

            apply(
                [this] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                {
                    func_(
                        EventRange<TIn>( source_->Events() ),
                        std::back_inserter(this->events_),
                        args->ValueRef() ...);
                },
                deps_);

        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SycnedEventProcessingNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,TIn>>   source_;

    TFunc       func_;
    DepHolderT  deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventJoinNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename ... TValues
>
class EventJoinNode :
    public EventStreamNode<D,std::tuple<TValues ...>>
{
    using Engine = typename EventJoinNode::Engine;
    using TurnT = typename Engine::TurnT;

public:
    EventJoinNode(const std::shared_ptr<EventStreamNode<D,TValues>>& ... sources) :
        EventJoinNode::EventStreamNode( ),
        slots_( sources ... )
    {
        Engine::OnNodeCreate(*this);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *sources));
    }

    ~EventJoinNode()
    {
        apply(
            [this] (Slot<TValues>& ... slots) {
                REACT_EXPAND_PACK(Engine::OnNodeDetach(*this, *slots.Source));
            },
            slots_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        // Don't time if there is nothing to do
        {// timer
            size_t count = 0;
            using TimerT = typename EventJoinNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, count );

            // Move events into buffers
            apply(
                [this, &turn] (Slot<TValues>& ... slots) {
                    REACT_EXPAND_PACK(fetchBuffer(turn, slots));
                },
                slots_);

            while (true)
            {
                bool isReady = true;

                // All slots ready?
                apply(
                    [this,&isReady] (Slot<TValues>& ... slots) {
                        // Todo: combine return values instead
                        REACT_EXPAND_PACK(checkSlot(slots, isReady));
                    },
                    slots_);

                if (!isReady)
                    break;

                // Pop values from buffers and emit tuple
                apply(
                    [this] (Slot<TValues>& ... slots) {
                        this->events_.emplace_back(slots.Buffer.front() ...);
                        REACT_EXPAND_PACK(slots.Buffer.pop_front());
                    },
                    slots_);
            }

            count = this->events_.size();

        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "EventJoinNode"; }
    virtual int         DependencyCount() const override    { return sizeof...(TValues); }

private:
    template <typename T>
    struct Slot
    {
        Slot(const std::shared_ptr<EventStreamNode<D,T>>& src) :
            Source( src )
        {}

        std::shared_ptr<EventStreamNode<D,T>>   Source;
        std::deque<T>                           Buffer;
    };

    template <typename T>
    static void fetchBuffer(TurnT& turn, Slot<T>& slot)
    {
        slot.Source->SetCurrentTurn(turn);

        slot.Buffer.insert(
            slot.Buffer.end(),
            slot.Source->Events().begin(),
            slot.Source->Events().end());
    }

    template <typename T>
    static void checkSlot(Slot<T>& slot, bool& isReady)
    {
        auto t = isReady && !slot.Buffer.empty();
        isReady = t;
    }

    std::tuple<Slot<TValues>...>    slots_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_EVENTNODES_H_INCLUDED

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventStreamBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E
>
class EventStreamBase : public CopyableReactive<EventStreamNode<D,E>>
{
public:
    EventStreamBase() = default;
    EventStreamBase(const EventStreamBase&) = default;

    template <typename T>
    EventStreamBase(T&& t) :
        EventStreamBase::CopyableReactive( std::forward<T>(t) )
    {}

protected:
    template <typename T>
    void emit(T&& e) const
    {
        DomainSpecificInputManager<D>::Instance().AddInput(
            *reinterpret_cast<EventSourceNode<D,E>*>(this->ptr_.get()),
            std::forward<T>(e));
    }
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_EVENTBASE_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

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

template <typename D, typename ... TValues>
class SignalPack;

using REACT_IMPL::WeightHint;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MakeEventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename D, typename E = Token>
auto MakeEventSource()
    -> EventSource<D,E>
{
    using REACT_IMPL::EventSourceNode;

    return EventSource<D,E>(
        std::make_shared<EventSourceNode<D,E>>());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Merge
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TArg1,
    typename ... TArgs,
    typename E = TArg1,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtrT<D,TArg1>,
        REACT_IMPL::EventStreamNodePtrT<D,TArgs> ...>
>
auto Merge(const Events<D,TArg1>& arg1, const Events<D,TArgs>& ... args)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    static_assert(sizeof...(TArgs) > 0,
        "Merge: 2+ arguments are required.");

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            GetNodePtr(arg1), GetNodePtr(args) ...));
}

template
<
    typename TLeftEvents,
    typename TRightEvents,
    typename D = typename TLeftEvents::DomainT,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename TRightVal = typename TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtrT<D,TLeftVal>,
        REACT_IMPL::EventStreamNodePtrT<D,TRightVal>>,
    class = typename std::enable_if<
        IsEvent<TLeftEvents>::value>::type,
    class = typename std::enable_if<
        IsEvent<TRightEvents>::value>::type
>
auto operator|(const TLeftEvents& lhs, const TRightEvents& rhs)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            GetNodePtr(lhs), GetNodePtr(rhs)));
}

template
<
    typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightVal,
    typename TRightOp,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,TLeftOp,TRightOp>
>
auto operator|(TempEvents<D,TLeftVal,TLeftOp>&& lhs, TempEvents<D,TRightVal,TRightOp>&& rhs)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            lhs.StealOp(), rhs.StealOp()));
}

template
<
    typename D,
    typename TLeftVal,
    typename TLeftOp,
    typename TRightEvents,
    typename TRightVal = typename TRightEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        TLeftOp,
        REACT_IMPL::EventStreamNodePtrT<D,TRightVal>>,
    class = typename std::enable_if<
        IsEvent<TRightEvents>::value>::type
>
auto operator|(TempEvents<D,TLeftVal,TLeftOp>&& lhs, const TRightEvents& rhs)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            lhs.StealOp(), GetNodePtr(rhs)));
}

template
<
    typename TLeftEvents,
    typename D,
    typename TRightVal,
    typename TRightOp,
    typename TLeftVal = typename TLeftEvents::ValueT,
    typename E = TLeftVal,
    typename TOp = REACT_IMPL::EventMergeOp<E,
        REACT_IMPL::EventStreamNodePtrT<D,TRightVal>,
        TRightOp>,
    class = typename std::enable_if<
        IsEvent<TLeftEvents>::value>::type
>
auto operator|(const TLeftEvents& lhs, TempEvents<D,TRightVal,TRightOp>&& rhs)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            GetNodePtr(lhs), rhs.StealOp()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOp = REACT_IMPL::EventFilterOp<E,F,
        REACT_IMPL::EventStreamNodePtrT<D,E>>
>
auto Filter(const Events<D,E>& src, FIn&& filter)
    -> TempEvents<D,E,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOp>(
        std::make_shared<EventOpNode<D,E,TOp>>(
            std::forward<FIn>(filter), GetNodePtr(src)));
}

template
<
    typename D,
    typename E,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOpOut = REACT_IMPL::EventFilterOp<E,F,TOpIn>
>
auto Filter(TempEvents<D,E,TOpIn>&& src, FIn&& filter)
    -> TempEvents<D,E,TOpOut>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,E,TOpOut>(
        std::make_shared<EventOpNode<D,E,TOpOut>>(
            std::forward<FIn>(filter), src.StealOp()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Filter - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename FIn,
    typename ... TDepValues
>
auto Filter(const Events<D,E>& source, const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Events<D,E>
{
    using REACT_IMPL::SyncedEventFilterNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_(const Events<D,E>& source, FIn&& func) :
            MySource( source ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> Events<D,E>
        {
            return Events<D,E>(
                std::make_shared<SyncedEventFilterNode<D,E,F,TDepValues ...>>(
                     GetNodePtr(MySource), std::forward<FIn>(MyFunc), GetNodePtr(deps) ...));
        }

        const Events<D,E>&      MySource;
        FIn                     MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( source, std::forward<FIn>(func) ),
        depPack.Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F(TIn)>::type,
    typename TOp = REACT_IMPL::EventTransformOp<TIn,F,
        REACT_IMPL::EventStreamNodePtrT<D,TIn>>
>
auto Transform(const Events<D,TIn>& src, FIn&& func)
    -> TempEvents<D,TOut,TOp>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,TOut,TOp>(
        std::make_shared<EventOpNode<D,TOut,TOp>>(
            std::forward<FIn>(func), GetNodePtr(src)));
}

template
<
    typename D,
    typename TIn,
    typename TOpIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type,
    typename TOut = typename std::result_of<F(TIn)>::type,
    typename TOpOut = REACT_IMPL::EventTransformOp<TIn,F,TOpIn>
>
auto Transform(TempEvents<D,TIn,TOpIn>&& src, FIn&& func)
    -> TempEvents<D,TOut,TOpOut>
{
    using REACT_IMPL::EventOpNode;

    return TempEvents<D,TOut,TOpOut>(
        std::make_shared<EventOpNode<D,TOut,TOpOut>>(
            std::forward<FIn>(func), src.StealOp()));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Transform - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TIn,
    typename FIn,
    typename ... TDepValues,
    typename TOut = typename std::result_of<FIn(TIn,TDepValues...)>::type
>
auto Transform(const Events<D,TIn>& source, const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Events<D,TOut>
{
    using REACT_IMPL::SyncedEventTransformNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_(const Events<D,TIn>& source, FIn&& func) :
            MySource( source ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> Events<D,TOut>
        {
            return Events<D,TOut>(
                std::make_shared<SyncedEventTransformNode<D,TIn,TOut,F,TDepValues ...>>(
                     GetNodePtr(MySource), std::forward<FIn>(MyFunc), GetNodePtr(deps) ...));
        }

        const Events<D,TIn>&    MySource;
        FIn                     MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( source, std::forward<FIn>(func) ),
        depPack.Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process
///////////////////////////////////////////////////////////////////////////////////////////////////
using REACT_IMPL::EventRange;
using REACT_IMPL::EventEmitter;

template
<
    typename TOut,
    typename D,
    typename TIn,
    typename FIn,
    typename F = typename std::decay<FIn>::type
>
auto Process(const Events<D,TIn>& src, FIn&& func)
    -> Events<D,TOut>
{
    using REACT_IMPL::EventProcessingNode;

    return Events<D,TOut>(
        std::make_shared<EventProcessingNode<D,TIn,TOut,F>>(
            GetNodePtr(src), std::forward<FIn>(func)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Process - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename TOut,
    typename D,
    typename TIn,
    typename FIn,
    typename ... TDepValues
>
auto Process(const Events<D,TIn>& source, const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Events<D,TOut>
{
    using REACT_IMPL::SyncedEventProcessingNode;

    using F = typename std::decay<FIn>::type;

    struct NodeBuilder_
    {
        NodeBuilder_(const Events<D,TIn>& source, FIn&& func) :
            MySource( source ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> Events<D,TOut>
        {
            return Events<D,TOut>(
                std::make_shared<SyncedEventProcessingNode<D,TIn,TOut,F,TDepValues ...>>(
                     GetNodePtr(MySource), std::forward<FIn>(MyFunc), GetNodePtr(deps) ...));
        }

        const Events<D,TIn>&    MySource;
        FIn                     MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( source, std::forward<FIn>(func) ),
        depPack.Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Flatten
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TInnerValue
>
auto Flatten(const Signal<D,Events<D,TInnerValue>>& outer)
    -> Events<D,TInnerValue>
{
    return Events<D,TInnerValue>(
        std::make_shared<REACT_IMPL::EventFlattenNode<D, Events<D,TInnerValue>, TInnerValue>>(
            GetNodePtr(outer), GetNodePtr(outer.Value())));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Join
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename ... TArgs
>
auto Join(const Events<D,TArgs>& ... args)
    -> Events<D, std::tuple<TArgs ...>>
{
    using REACT_IMPL::EventJoinNode;

    static_assert(sizeof...(TArgs) > 1,
        "Join: 2+ arguments are required.");

    return Events<D, std::tuple<TArgs ...>>(
        std::make_shared<EventJoinNode<D,TArgs...>>(
            GetNodePtr(args) ...));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Token
///////////////////////////////////////////////////////////////////////////////////////////////////
enum class Token { value };

struct Tokenizer
{
    template <typename T>
    Token operator()(const T&) const { return Token::value; }
};

template <typename TEvents>
auto Tokenize(TEvents&& source)
    -> decltype(Transform(source, Tokenizer{}))
{
    return Transform(source, Tokenizer{});
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Events
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E = Token
>
class Events : public REACT_IMPL::EventStreamBase<D,E>
{
private:
    using NodeT     = REACT_IMPL::EventStreamNode<D,E>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events(const Events&) = default;

    // Move ctor
    Events(Events&& other) :
        Events::EventStreamBase( std::move(other) )
    {}

    // Node ctor
    explicit Events(NodePtrT&& nodePtr) :
        Events::EventStreamBase( std::move(nodePtr) )
    {}

    // Copy assignment
    Events& operator=(const Events&) = default;

    // Move assignment
    Events& operator=(Events&& other)
    {
        Events::EventStreamBase::operator=( std::move(other) );
        return *this;
    }

    bool Equals(const Events& other) const
    {
        return Events::EventStreamBase::Equals(other);
    }

    bool IsValid() const
    {
        return Events::EventStreamBase::IsValid();
    }

    void SetWeightHint(WeightHint weight)
    {
        Events::EventStreamBase::SetWeightHint(weight);
    }

    auto Tokenize() const
        -> decltype(REACT::Tokenize(std::declval<Events>()))
    {
        return REACT::Tokenize(*this);
    }

    template <typename ... TArgs>
    auto Merge(TArgs&& ... args) const
        -> decltype(REACT::Merge(std::declval<Events>(), std::forward<TArgs>(args) ...))
    {
        return REACT::Merge(*this, std::forward<TArgs>(args) ...);
    }

    template <typename F>
    auto Filter(F&& f) const
        -> decltype(REACT::Filter(std::declval<Events>(), std::forward<F>(f)))
    {
        return REACT::Filter(*this, std::forward<F>(f));
    }

    template <typename F>
    auto Transform(F&& f) const
        -> decltype(REACT::Transform(std::declval<Events>(), std::forward<F>(f)))
    {
        return REACT::Transform(*this, std::forward<F>(f));
    }
};

// Specialize for references
template
<
    typename D,
    typename E
>
class Events<D,E&> : public REACT_IMPL::EventStreamBase<D,std::reference_wrapper<E>>
{
private:
    using NodeT     = REACT_IMPL::EventStreamNode<D,std::reference_wrapper<E>>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    using ValueT = E;

    // Default ctor
    Events() = default;

    // Copy ctor
    Events(const Events&) = default;

    // Move ctor
    Events(Events&& other) :
        Events::EventStreamBase( std::move(other) )
    {}

    // Node ctor
    explicit Events(NodePtrT&& nodePtr) :
        Events::EventStreamBase( std::move(nodePtr) )
    {}

    // Copy assignment
    Events& operator=(const Events&) = default;

    // Move assignment
    Events& operator=(Events&& other)
    {
        Events::EventStreamBase::operator=( std::move(other) );
        return *this;
    }

    bool Equals(const Events& other) const
    {
        return Events::EventStreamBase::Equals(other);
    }

    bool IsValid() const
    {
        return Events::EventStreamBase::IsValid();
    }

    void SetWeightHint(WeightHint weight)
    {
        Events::EventStreamBase::SetWeightHint(weight);
    }

    auto Tokenize() const
        -> decltype(REACT::Tokenize(std::declval<Events>()))
    {
        return REACT::Tokenize(*this);
    }

    template <typename ... TArgs>
    auto Merge(TArgs&& ... args)
        -> decltype(REACT::Merge(std::declval<Events>(), std::forward<TArgs>(args) ...))
    {
        return REACT::Merge(*this, std::forward<TArgs>(args) ...);
    }

    template <typename F>
    auto Filter(F&& f) const
        -> decltype(REACT::Filter(std::declval<Events>(), std::forward<F>(f)))
    {
        return REACT::Filter(*this, std::forward<F>(f));
    }

    template <typename F>
    auto Transform(F&& f) const
        -> decltype(REACT::Transform(std::declval<Events>(), std::forward<F>(f)))
    {
        return REACT::Transform(*this, std::forward<F>(f));
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventSource
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E = Token
>
class EventSource : public Events<D,E>
{
private:
    using NodeT     = REACT_IMPL::EventSourceNode<D,E>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource(const EventSource&) = default;

    // Move ctor
    EventSource(EventSource&& other) :
        EventSource::Events( std::move(other) )
    {}

    // Node ctor
    explicit EventSource(NodePtrT&& nodePtr) :
        EventSource::Events( std::move(nodePtr) )
    {}

    // Copy assignemnt
    EventSource& operator=(const EventSource&) = default;

    // Move assignment
    EventSource& operator=(EventSource&& other)
    {
        EventSource::Events::operator=( std::move(other) );
        return *this;
    }

    // Explicit emit
    void Emit(const E& e) const     { EventSource::EventStreamBase::emit(e); }
    void Emit(E&& e) const          { EventSource::EventStreamBase::emit(std::move(e)); }

    void Emit() const
    {
        static_assert(std::is_same<E,Token>::value, "Can't emit on non token stream.");
        EventSource::EventStreamBase::emit(Token::value);
    }

    // Function object style
    void operator()(const E& e) const   { EventSource::EventStreamBase::emit(e); }
    void operator()(E&& e) const        { EventSource::EventStreamBase::emit(std::move(e)); }

    void operator()() const
    {
        static_assert(std::is_same<E,Token>::value, "Can't emit on non token stream.");
        EventSource::EventStreamBase::emit(Token::value);
    }

    // Stream style
    const EventSource& operator<<(const E& e) const
    {
        EventSource::EventStreamBase::emit(e);
        return *this;
    }

    const EventSource& operator<<(E&& e) const
    {
        EventSource::EventStreamBase::emit(std::move(e));
        return *this;
    }
};

// Specialize for references
template
<
    typename D,
    typename E
>
class EventSource<D,E&> : public Events<D,std::reference_wrapper<E>>
{
private:
    using NodeT     = REACT_IMPL::EventSourceNode<D,std::reference_wrapper<E>>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    // Default ctor
    EventSource() = default;

    // Copy ctor
    EventSource(const EventSource&) = default;

    // Move ctor
    EventSource(EventSource&& other) :
        EventSource::Events( std::move(other) )
    {}

    // Node ctor
    explicit EventSource(NodePtrT&& nodePtr) :
        EventSource::Events( std::move(nodePtr) )
    {}

    // Copy assignment
    EventSource& operator=(const EventSource&) = default;

    // Move assignment
    EventSource& operator=(EventSource&& other)
    {
        EventSource::Events::operator=( std::move(other) );
        return *this;
    }

    // Explicit emit
    void Emit(std::reference_wrapper<E> e) const        { EventSource::EventStreamBase::emit(e); }

    // Function object style
    void operator()(std::reference_wrapper<E> e) const  { EventSource::EventStreamBase::emit(e); }

    // Stream style
    const EventSource& operator<<(std::reference_wrapper<E> e) const
    {
        EventSource::EventStreamBase::emit(e);
        return *this;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TempEvents
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename TOp
>
class TempEvents : public Events<D,E>
{
protected:
    using NodeT     = REACT_IMPL::EventOpNode<D,E,TOp>;
    using NodePtrT  = std::shared_ptr<NodeT>;

public:
    // Default ctor
    TempEvents() = default;

    // Copy ctor
    TempEvents(const TempEvents&) = default;

    // Move ctor
    TempEvents(TempEvents&& other) :
        TempEvents::Events( std::move(other) )
    {}

    // Node ctor
    explicit TempEvents(NodePtrT&& nodePtr) :
        TempEvents::Events( std::move(nodePtr) )
    {}

    // Copy assignment
    TempEvents& operator=(const TempEvents&) = default;

    // Move assignment
    TempEvents& operator=(TempEvents&& other)
    {
        TempEvents::EventStreamBase::operator=( std::move(other) );
        return *this;
    }

    TOp StealOp()
    {
        return std::move(reinterpret_cast<NodeT*>(this->ptr_.get())->StealOp());
    }

    template <typename ... TArgs>
    auto Merge(TArgs&& ... args)
        -> decltype(REACT::Merge(std::declval<TempEvents>(), std::forward<TArgs>(args) ...))
    {
        return REACT::Merge(*this, std::forward<TArgs>(args) ...);
    }

    template <typename F>
    auto Filter(F&& f) const
        -> decltype(REACT::Filter(std::declval<TempEvents>(), std::forward<F>(f)))
    {
        return REACT::Filter(*this, std::forward<F>(f));
    }

    template <typename F>
    auto Transform(F&& f) const
        -> decltype(REACT::Transform(std::declval<TempEvents>(), std::forward<F>(f)))
    {
        return REACT::Transform(*this, std::forward<F>(f));
    }
};

/******************************************/ REACT_END /******************************************/

/***************************************/ REACT_IMPL_BEGIN /**************************************/

template <typename D, typename L, typename R>
bool Equals(const Events<D,L>& lhs, const Events<D,R>& rhs)
{
    return lhs.Equals(rhs);
}

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_EVENT_H_INCLUDED
// #include "react/Observer.h"

// #include "react/Algorithm.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_ALGORITHM_H_INCLUDED
#define REACT_ALGORITHM_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <type_traits>
#include <utility>

// #include "react/detail/graph/AlgorithmNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_ALGORITHMNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_ALGORITHMNODES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <memory>
#include <utility>

// #include "EventNodes.h"

// #include "SignalNodes.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// AddIterateRangeWrapper
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename E, typename S, typename F, typename ... TArgs>
struct AddIterateRangeWrapper
{
    AddIterateRangeWrapper(const AddIterateRangeWrapper& other) = default;

    AddIterateRangeWrapper(AddIterateRangeWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template
    <
        typename FIn,
        class = typename DisableIfSame<FIn,AddIterateRangeWrapper>::type
    >
    explicit AddIterateRangeWrapper(FIn&& func) :
        MyFunc( std::forward<FIn>(func) )
    {}

    S operator()(EventRange<E> range, S value, const TArgs& ... args)
    {
        for (const auto& e : range)
            value = MyFunc(e, value, args ...);

        return value;
    }

    F MyFunc;
};

template <typename E, typename S, typename F, typename ... TArgs>
struct AddIterateByRefRangeWrapper
{
    AddIterateByRefRangeWrapper(const AddIterateByRefRangeWrapper& other) = default;

    AddIterateByRefRangeWrapper(AddIterateByRefRangeWrapper&& other) :
        MyFunc( std::move(other.MyFunc) )
    {}

    template
    <
        typename FIn,
        class = typename DisableIfSame<FIn,AddIterateByRefRangeWrapper>::type
    >
    explicit AddIterateByRefRangeWrapper(FIn&& func) :
        MyFunc( std::forward<FIn>(func) )
    {}

    void operator()(EventRange<E> range, S& valueRef, const TArgs& ... args)
    {
        for (const auto& e : range)
            MyFunc(e, valueRef, args ...);
    }

    F MyFunc;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E,
    typename TFunc
>
class IterateNode :
    public SignalNode<D,S>
{
    using Engine = typename IterateNode::Engine;

public:
    template <typename T, typename F>
    IterateNode(T&& init, const std::shared_ptr<EventStreamNode<D,E>>& events, F&& func) :
        IterateNode::SignalNode( std::forward<T>(init) ),
        events_( events ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *events);
    }

    ~IterateNode()
    {
        Engine::OnNodeDetach(*this, *events_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        bool changed = false;

        {// timer
            using TimerT = typename IterateNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, events_->Events().size() );

            S newValue = func_(EventRange<E>( events_->Events() ), this->value_);

            if (! Equals(newValue, this->value_))
            {
                this->value_ = std::move(newValue);
                changed = true;
            }
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "IterateNode"; }
    virtual int         DependencyCount() const override    { return 1; }

private:
    std::shared_ptr<EventStreamNode<D,E>> events_;
    
    TFunc   func_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// IterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E,
    typename TFunc
>
class IterateByRefNode :
    public SignalNode<D,S>
{
    using Engine = typename IterateByRefNode::Engine;

public:
    template <typename T, typename F>
    IterateByRefNode(T&& init, const std::shared_ptr<EventStreamNode<D,E>>& events, F&& func) :
        IterateByRefNode::SignalNode( std::forward<T>(init) ),
        func_( std::forward<F>(func) ),
        events_( events )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *events);
    }

    ~IterateByRefNode()
    {
        Engine::OnNodeDetach(*this, *events_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        {// timer
            using TimerT = typename IterateByRefNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, events_->Events().size() );

            func_(EventRange<E>( events_->Events() ), this->value_);

        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        // Always assume change
        Engine::OnNodePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "IterateByRefNode"; }
    virtual int         DependencyCount() const override    { return 1; }

protected:
    TFunc   func_;

    std::shared_ptr<EventStreamNode<D,E>> events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E,
    typename TFunc,
    typename ... TDepValues
>
class SyncedIterateNode :
    public SignalNode<D,S>
{
    using Engine = typename SyncedIterateNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateNode(T&& init, const std::shared_ptr<EventStreamNode<D,E>>& events, F&& func,
                      const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedIterateNode::SignalNode( std::forward<T>(init) ),
        events_( events ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *events);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedIterateNode()
    {
        Engine::OnNodeDetach(*this, *events_);

        apply(
            DetachFunctor<D,SyncedIterateNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        events_->SetCurrentTurn(turn);

        bool changed = false;

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        if (! events_->Events().empty())
        {// timer
            using TimerT = typename SyncedIterateNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, events_->Events().size() );
            
            S newValue = apply(
                [this] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                {
                    return func_(EventRange<E>( events_->Events() ), this->value_, args->ValueRef() ...);
                },
                deps_);

            if (! Equals(newValue, this->value_))
            {
                changed = true;
                this->value_ = std::move(newValue);
            }
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SyncedIterateNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,E>> events_;
    
    TFunc       func_;
    DepHolderT  deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SyncedIterateByRefNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E,
    typename TFunc,
    typename ... TDepValues
>
class SyncedIterateByRefNode :
    public SignalNode<D,S>
{
    using Engine = typename SyncedIterateByRefNode::Engine;

public:
    template <typename T, typename F>
    SyncedIterateByRefNode(T&& init, const std::shared_ptr<EventStreamNode<D,E>>& events, F&& func,
                           const std::shared_ptr<SignalNode<D,TDepValues>>& ... deps) :
        SyncedIterateByRefNode::SignalNode( std::forward<T>(init) ),
        events_( events ),
        func_( std::forward<F>(func) ),
        deps_( deps ... )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *events);
        REACT_EXPAND_PACK(Engine::OnNodeAttach(*this, *deps));
    }

    ~SyncedIterateByRefNode()
    {
        Engine::OnNodeDetach(*this, *events_);

        apply(
            DetachFunctor<D,SyncedIterateByRefNode,
                std::shared_ptr<SignalNode<D,TDepValues>>...>( *this ),
            deps_);

        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        events_->SetCurrentTurn(turn);

        bool changed = false;

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        if (! events_->Events().empty())
        {// timer
            using TimerT = typename SyncedIterateByRefNode::ScopedUpdateTimer;
            TimerT scopedTimer( *this, events_->Events().size() );

            apply(
                [this] (const std::shared_ptr<SignalNode<D,TDepValues>>& ... args)
                {
                    func_(EventRange<E>( events_->Events() ), this->value_, args->ValueRef() ...);
                },
                deps_);

            changed = true;
        }// ~timer

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SyncedIterateByRefNode"; }
    virtual int         DependencyCount() const override    { return 1 + sizeof...(TDepValues); }

private:
    using DepHolderT = std::tuple<std::shared_ptr<SignalNode<D,TDepValues>>...>;

    std::shared_ptr<EventStreamNode<D,E>> events_;
    
    TFunc       func_;
    DepHolderT  deps_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// HoldNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
class HoldNode : public SignalNode<D,S>
{
    using Engine = typename HoldNode::Engine;

public:
    template <typename T>
    HoldNode(T&& init, const std::shared_ptr<EventStreamNode<D,S>>& events) :
        HoldNode::SignalNode( std::forward<T>(init) ),
        events_( events )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *events_);
    }

    ~HoldNode()
    {
        Engine::OnNodeDetach(*this, *events_);
        Engine::OnNodeDestroy(*this);
    }

    virtual const char* GetNodeType() const override    { return "HoldNode"; }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        bool changed = false;

        if (! events_->Events().empty())
        {
            const S& newValue = events_->Events().back();

            if (! Equals(newValue, this->value_))
            {
                changed = true;
                this->value_ = newValue;
            }
        }

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual int     DependencyCount() const override    { return 1; }

private:
    const std::shared_ptr<EventStreamNode<D,S>>    events_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SnapshotNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E
>
class SnapshotNode : public SignalNode<D,S>
{
    using Engine = typename SnapshotNode::Engine;

public:
    SnapshotNode(const std::shared_ptr<SignalNode<D,S>>& target,
                 const std::shared_ptr<EventStreamNode<D,E>>& trigger) :
        SnapshotNode::SignalNode( target->ValueRef() ),
        target_( target ),
        trigger_( trigger )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *target_);
        Engine::OnNodeAttach(*this, *trigger_);
    }

    ~SnapshotNode()
    {
        Engine::OnNodeDetach(*this, *target_);
        Engine::OnNodeDetach(*this, *trigger_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        trigger_->SetCurrentTurn(turn);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        bool changed = false;
        
        if (! trigger_->Events().empty())
        {
            const S& newValue = target_->ValueRef();

            if (! Equals(newValue, this->value_))
            {
                changed = true;
                this->value_ = newValue;
            }
        }

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (changed)
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "SnapshotNode"; }
    virtual int         DependencyCount() const override    { return 2; }

private:
    const std::shared_ptr<SignalNode<D,S>>      target_;
    const std::shared_ptr<EventStreamNode<D,E>> trigger_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MonitorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E
>
class MonitorNode : public EventStreamNode<D,E>
{
    using Engine = typename MonitorNode::Engine;

public:
    MonitorNode(const std::shared_ptr<SignalNode<D,E>>& target) :
        MonitorNode::EventStreamNode( ),
        target_( target )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *target_);
    }

    ~MonitorNode()
    {
        Engine::OnNodeDetach(*this, *target_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        using TurnT = typename D::Engine::TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        this->events_.push_back(target_->ValueRef());

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "MonitorNode"; }
    virtual int         DependencyCount() const override    { return 1; }

private:
    const std::shared_ptr<SignalNode<D,E>>    target_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulseNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E
>
class PulseNode : public EventStreamNode<D,S>
{
    using Engine = typename PulseNode::Engine;

public:
    PulseNode(const std::shared_ptr<SignalNode<D,S>>& target,
              const std::shared_ptr<EventStreamNode<D,E>>& trigger) :
        PulseNode::EventStreamNode( ),
        target_( target ),
        trigger_( trigger )
    {
        Engine::OnNodeCreate(*this);
        Engine::OnNodeAttach(*this, *target_);
        Engine::OnNodeAttach(*this, *trigger_);
    }

    ~PulseNode()
    {
        Engine::OnNodeDetach(*this, *target_);
        Engine::OnNodeDetach(*this, *trigger_);
        Engine::OnNodeDestroy(*this);
    }

    virtual void Tick(void* turnPtr) override
    {
        typedef typename D::Engine::TurnT TurnT;
        TurnT& turn = *reinterpret_cast<TurnT*>(turnPtr);

        this->SetCurrentTurn(turn, true);
        trigger_->SetCurrentTurn(turn);

        REACT_LOG(D::Log().template Append<NodeEvaluateBeginEvent>(
            GetObjectId(*this), turn.Id()));

        for (uint i=0; i<trigger_->Events().size(); i++)
            this->events_.push_back(target_->ValueRef());

        REACT_LOG(D::Log().template Append<NodeEvaluateEndEvent>(
            GetObjectId(*this), turn.Id()));

        if (! this->events_.empty())
            Engine::OnNodePulse(*this, turn);
        else
            Engine::OnNodeIdlePulse(*this, turn);
    }

    virtual const char* GetNodeType() const override        { return "PulseNode"; }
    virtual int         DependencyCount() const override    { return 2; }

private:
    const std::shared_ptr<SignalNode<D,S>>      target_;
    const std::shared_ptr<EventStreamNode<D,E>> trigger_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_ALGORITHMNODES_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

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

template <typename D, typename ... TValues>
class SignalPack;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Hold - Hold the most recent event in a signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename V,
    typename T = typename std::decay<V>::type
>
auto Hold(const Events<D,T>& events, V&& init)
    -> Signal<D,T>
{
    using REACT_IMPL::HoldNode;

    return Signal<D,T>(
        std::make_shared<HoldNode<D,T>>(
            std::forward<V>(init), GetNodePtr(events)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Monitor - Emits value changes of target signal
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
auto Monitor(const Signal<D,S>& target)
    -> Events<D,S>
{
    using REACT_IMPL::MonitorNode;

    return Events<D,S>(
        std::make_shared<MonitorNode<D, S>>(
            GetNodePtr(target)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Iteratively combines signal value with values from event stream (aka Fold)
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename V,
    typename FIn,
    typename S = typename std::decay<V>::type
>
auto Iterate(const Events<D,E>& events, V&& init, FIn&& func)
    -> Signal<D,S>
{
    using REACT_IMPL::IterateNode;
    using REACT_IMPL::IterateByRefNode;
    using REACT_IMPL::AddIterateRangeWrapper;
    using REACT_IMPL::AddIterateByRefRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using NodeT =
        typename std::conditional<
            IsCallableWith<F,S,EventRange<E>,S>::value,
            IterateNode<D,S,E,F>,
            typename std::conditional<
                IsCallableWith<F,S,E,S>::value,
                IterateNode<D,S,E,AddIterateRangeWrapper<E,S,F>>,
                typename std::conditional<
                    IsCallableWith<F, void, EventRange<E>, S&>::value,
                    IterateByRefNode<D,S,E,F>,
                    typename std::conditional<
                        IsCallableWith<F,void,E,S&>::value,
                        IterateByRefNode<D,S,E,AddIterateByRefRangeWrapper<E,S,F>>,
                        void
                    >::type
                >::type
            >::type
        >::type;

    static_assert(
        ! std::is_same<NodeT,void>::value,
        "Iterate: Passed function does not match any of the supported signatures.");

    return Signal<D,S>(
        std::make_shared<NodeT>(
            std::forward<V>(init), GetNodePtr(events), std::forward<FIn>(func)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Iterate - Synced
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename E,
    typename V,
    typename FIn,
    typename ... TDepValues,
    typename S = typename std::decay<V>::type
>
auto Iterate(const Events<D,E>& events, V&& init,
             const SignalPack<D,TDepValues...>& depPack, FIn&& func)
    -> Signal<D,S>
{
    using REACT_IMPL::SyncedIterateNode;
    using REACT_IMPL::SyncedIterateByRefNode;
    using REACT_IMPL::AddIterateRangeWrapper;
    using REACT_IMPL::AddIterateByRefRangeWrapper;
    using REACT_IMPL::IsCallableWith;
    using REACT_IMPL::EventRange;

    using F = typename std::decay<FIn>::type;

    using NodeT =
        typename std::conditional<
            IsCallableWith<F,S,EventRange<E>,S,TDepValues ...>::value,
            SyncedIterateNode<D,S,E,F,TDepValues ...>,
            typename std::conditional<
                IsCallableWith<F,S,E,S,TDepValues ...>::value,
                SyncedIterateNode<D,S,E,
                    AddIterateRangeWrapper<E,S,F,TDepValues ...>,
                    TDepValues ...>,
                typename std::conditional<
                    IsCallableWith<F,void,EventRange<E>,S&,TDepValues ...>::value,
                    SyncedIterateByRefNode<D,S,E,F,TDepValues ...>,
                    typename std::conditional<
                        IsCallableWith<F,void,E,S&,TDepValues ...>::value,
                        SyncedIterateByRefNode<D,S,E,
                            AddIterateByRefRangeWrapper<E,S,F,TDepValues ...>,
                            TDepValues ...>,
                        void
                    >::type
                >::type
            >::type
        >::type;

    static_assert(
        ! std::is_same<NodeT,void>::value,
        "Iterate: Passed function does not match any of the supported signatures.");

    //static_assert(NodeT::dummy_error, "DUMP MY TYPE" );

    struct NodeBuilder_
    {
        NodeBuilder_(const Events<D,E>& source, V&& init, FIn&& func) :
            MySource( source ),
            MyInit( std::forward<V>(init) ),
            MyFunc( std::forward<FIn>(func) )
        {}

        auto operator()(const Signal<D,TDepValues>& ... deps)
            -> Signal<D,S>
        {
            return Signal<D,S>(
                std::make_shared<NodeT>(
                    std::forward<V>(MyInit), GetNodePtr(MySource),
                    std::forward<FIn>(MyFunc), GetNodePtr(deps) ...));
        }

        const Events<D,E>&  MySource;
        V                   MyInit;
        FIn                 MyFunc;
    };

    return REACT_IMPL::apply(
        NodeBuilder_( events, std::forward<V>(init), std::forward<FIn>(func) ),
        depPack.Data);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Snapshot - Sets signal value to value of other signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E
>
auto Snapshot(const Events<D,E>& trigger, const Signal<D,S>& target)
    -> Signal<D,S>
{
    using REACT_IMPL::SnapshotNode;

    return Signal<D,S>(
        std::make_shared<SnapshotNode<D,S,E>>(
            GetNodePtr(target), GetNodePtr(trigger)));
}



///////////////////////////////////////////////////////////////////////////////////////////////////
/// Pulse - Emits value of target signal when event is received
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S,
    typename E
>
auto Pulse(const Events<D,E>& trigger, const Signal<D,S>& target)
    -> Events<D,S>
{
    using REACT_IMPL::PulseNode;

    return Events<D,S>(
        std::make_shared<PulseNode<D,S,E>>(
            GetNodePtr(target), GetNodePtr(trigger)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Changed - Emits token when target signal was changed
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename S
>
auto Changed(const Signal<D,S>& target)
    -> Events<D,Token>
{
    return Monitor(target).Tokenize();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ChangedTo - Emits token when target signal was changed to value
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename V,
    typename S = typename std::decay<V>::type
>
auto ChangedTo(const Signal<D,S>& target, V&& value)
    -> Events<D,Token>
{
    return Monitor(target)
        .Filter([=] (const S& v) { return v == value; })
        .Tokenize();
}

/******************************************/ REACT_END /******************************************/

#endif // REACT_ALGORITHM_H_INCLUDED
// #include "react/TypeTraits.h"

// #include "react/Reactor.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_REACTOR_H_INCLUDED
#define REACT_REACTOR_H_INCLUDED



// #include "react/detail/Defs.h"


#include <functional>
#include <memory>
#include <utility>

// #include "react/common/Util.h"


// #include "react/Event.h"

// #include "react/detail/ReactiveBase.h"

// #include "react/detail/graph/ReactorNodes.h"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef REACT_DETAIL_GRAPH_REACTORNODES_H_INCLUDED
#define REACT_DETAIL_GRAPH_REACTORNODES_H_INCLUDED



// #include "react/detail/Defs.h"


#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

#include <boost/coroutine/all.hpp>

// #include "GraphBase.h"

// #include "EventNodes.h"

// #include "SignalNodes.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ReactorNode
///////////////////////////////////////////////////////////////////////////////////////////////////
template
<
    typename D,
    typename TContext
>
class ReactorNode :
    public NodeBase<D>
{
    using Engine = typename ReactorNode::Engine;

public:
    using CoroutineT = boost::coroutines::coroutine<NodeBase<D>*>;
    using PullT = typename CoroutineT::pull_type;
    using PushT = typename CoroutineT::push_type;
    using TurnT = typename D::Engine::TurnT;

    template <typename F>
    ReactorNode(F&& func) :
        ReactorNode::NodeBase( ),
        func_( std::forward<F>(func) )
    {
        Engine::OnNodeCreate(*this);
    }

    ~ReactorNode()
    {
        Engine::OnNodeDestroy(*this);
    }

    void StartLoop()
    {
        mainLoop_ = PullT
        (
            [&] (PushT& out)
            {
                curOutPtr_ = &out;

                TContext ctx( *this );

                // Start the loop function.
                // It will reach it its first Await at some point.
                while (true)
                {
                    func_(ctx);
                }
            }
        );

        // First blocking event is not initiated by Tick() but after loop creation.
        NodeBase<D>* p = mainLoop_.get();
        doAttach(p);

        ++depCount_;
    }

    virtual const char* GetNodeType() const override { return "ReactorNode"; }

    virtual bool IsDynamicNode() const override { return true; }
    virtual bool IsOutputNode() const override  { return true; }

    virtual void Tick(void* turnPtr) override
    {
        turnPtr_ = reinterpret_cast<TurnT*>(turnPtr);

        mainLoop_();

        NodeBase<D>* p = mainLoop_.get();

        if (p != nullptr)
        {
            doDynAttach(p);
        }
        else
        {
            offsets_.clear();
        }
        
        turnPtr_ = nullptr;
    }

    virtual int DependencyCount() const override
    {
        return depCount_;
    }

    template <typename E>
    E& Await(const std::shared_ptr<EventStreamNode<D,E>>& events)
    {
        // First attach to target event node
        (*curOutPtr_)(events.get());

        while (! checkEvent(events))
            (*curOutPtr_)(nullptr);

        doDynDetach(events.get());

        auto index = offsets_[reinterpret_cast<uintptr_t>(events.get())]++;
        return events->Events()[index];
    }

    template <typename E, typename F>
    void RepeatUntil(const std::shared_ptr<EventStreamNode<D,E>>& eventsPtr, const F& func)
    {
        // First attach to target event node
        if (turnPtr_ != nullptr)
        {
            (*curOutPtr_)(eventsPtr.get());
        }
        else
        {
            // Non-dynamic attach in case first loop until is encountered before the loop was
            // suspended for the first time.
            doAttach(eventsPtr.get());
        }

        // Don't enter loop if event already present
        if (! checkEvent(eventsPtr))
        {
            auto* parentOutPtr = curOutPtr_;

            // Create and start loop
            PullT nestedLoop_
            {
                [&] (PushT& out)
                {
                    curOutPtr_ = &out;
                    while (true)
                        func();
                }
            };

            // First suspend from initial loop run
            (*parentOutPtr)(nestedLoop_.get());

            // Further iterations
            while (! checkEvent(eventsPtr))
            {
                // Advance loop, forward blocking event to parent, and suspend
                nestedLoop_();
                (*parentOutPtr)(nestedLoop_.get());
            }

            curOutPtr_ = parentOutPtr;
        }

        // Detach when this function is exited
        if (turnPtr_ != nullptr)
            Engine::OnDynamicNodeDetach(*this, *eventsPtr, *turnPtr_);
        else
            Engine::OnNodeDetach(*this, *eventsPtr);

        --depCount_;
    }

    template <typename S>
    const S& Get(const std::shared_ptr<SignalNode<D,S>>& sigPtr)
    {
        // Do dynamic attach followed by attach if Get() happens during a turn.
        if (turnPtr_ != nullptr)
        {
            // Request attach
            (*curOutPtr_)(sigPtr.get());

            doDynDetach(sigPtr.get());
        }

        return sigPtr->ValueRef();
    }

private:
    template <typename E>
    bool checkEvent(const std::shared_ptr<EventStreamNode<D,E>>& events)
    {
        if (turnPtr_ == nullptr)
            return false;

        events->SetCurrentTurn(*turnPtr_);

        auto index = reinterpret_cast<uintptr_t>(events.get());
        return offsets_[index] < events->Events().size();
    }

    void doAttach(NodeBase<D>* nodePtr)
    {
        assert(nodePtr != nullptr);
        assert(turnPtr_ == nullptr);
        Engine::OnNodeAttach(*this, *nodePtr);
        ++depCount_;
    }

    void doDetach(NodeBase<D>* nodePtr)
    {
        assert(nodePtr != nullptr);
        assert(turnPtr_ == nullptr);
        Engine::OnNodeDetach(*this, *nodePtr);
        --depCount_;
    }

    void doDynAttach(NodeBase<D>* nodePtr)
    {
        assert(nodePtr != nullptr);
        assert(turnPtr_ != nullptr);
        Engine::OnDynamicNodeAttach(*this, *nodePtr, *turnPtr_);
        ++depCount_;
    }

    void doDynDetach(NodeBase<D>* nodePtr)
    {
        assert(nodePtr != nullptr);
        assert(turnPtr_ != nullptr);
        Engine::OnDynamicNodeDetach(*this, *nodePtr, *turnPtr_);
        --depCount_;
    }

    std::function<void(TContext)>    func_;

    PullT   mainLoop_;
    TurnT*  turnPtr_;

    PushT*  curOutPtr_ = nullptr;

    uint    depCount_ = 0;

    std::unordered_map<std::uintptr_t, size_t>    offsets_;
};

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_DETAIL_GRAPH_REACTORNODES_H_INCLUDED

/*****************************************/ REACT_BEGIN /*****************************************/

template <typename D>
class Reactor
{
public:
    class Context;

    using NodeT = REACT_IMPL::ReactorNode<D,Context>;
    
    class Context
    {
    public:
        Context(NodeT& node) :
            node_( node )
        {}

        template <typename E>
        E& Await(const Events<D,E>& evn)
        {
            return node_.Await(GetNodePtr(evn));
        }

        template <typename E, typename F>
        void RepeatUntil(const Events<D,E>& evn, F&& func)
        {
            node_.RepeatUntil(GetNodePtr(evn), std::forward<F>(func));
        }

        template <typename S>
        const S& Get(const Signal<D,S>& sig)
        {
            return node_.Get(GetNodePtr(sig));
        }

    private:
        NodeT&    node_;
    };

    template <typename F>
    explicit Reactor(F&& func) :
        nodePtr_( new REACT_IMPL::ReactorNode<D, Context>(std::forward<F>(func)) )
    {
        nodePtr_->StartLoop();
    }

private:
    std::unique_ptr<NodeT>    nodePtr_;
};

/******************************************/ REACT_END /******************************************/

#endif // REACT_REACTOR_H_INCLUDED
// #include "../src/engine/PulsecountEngine.cpp"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// #include "react/engine/PulsecountEngine.h"


#include <cstdint>
#include <utility>

// #include "react/common/Types.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/
namespace pulsecount {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Constants
///////////////////////////////////////////////////////////////////////////////////////////////////
static const uint chunk_size    = 8;
static const uint dfs_threshold = 3;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// MarkerTask
///////////////////////////////////////////////////////////////////////////////////////////////////
class MarkerTask: public task
{
public:
    using BufferT = NodeBuffer<Node,chunk_size>;

    template <typename TInput>
    MarkerTask(TInput srcBegin, TInput srcEnd) :
        nodes_( srcBegin, srcEnd )
    {}

    MarkerTask(MarkerTask& other, SplitTag) :
        nodes_( other.nodes_, SplitTag( ) )
    {}

    task* execute()
    {
        uint splitCount = 0;

        while (! nodes_.IsEmpty())
        {
            Node& node = splitCount > dfs_threshold ? *nodes_.PopBack() : *nodes_.PopFront();

            // Increment counter of each successor and add it to smaller stack
            for (auto* succ : node.Successors)
            {
                succ->IncCounter();

                // Skip if already marked as reachable
                if (! succ->ExchangeMark(ENodeMark::visited))
                    continue;
                
                nodes_.PushBack(succ);

                if (nodes_.IsFull())
                {
                    splitCount++;

                    //Delegate half the work to new task
                    auto& t = *new(task::allocate_additional_child_of(*parent()))
                        MarkerTask(*this, SplitTag{});

                    spawn(t);
                }
            }
        }

        return nullptr;
    }

private:
    BufferT  nodes_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UpdaterTask
///////////////////////////////////////////////////////////////////////////////////////////////////
class UpdaterTask: public task
{
public:
    using BufferT = NodeBuffer<Node,chunk_size>;

    template <typename TInput>
    UpdaterTask(Turn& turn, TInput srcBegin, TInput srcEnd) :
        turn_( turn ),
        nodes_( srcBegin, srcEnd )
    {}

    UpdaterTask(Turn& turn, Node* node) :
        turn_( turn ),
        nodes_( node )
    {}

    UpdaterTask(UpdaterTask& other, SplitTag) :
        turn_( other.turn_ ),
        nodes_( other.nodes_, SplitTag( ) )
    {}

    task* execute()
    {
        uint splitCount = 0;

        while (!nodes_.IsEmpty())
        {
            Node& node = splitCount > dfs_threshold ? *nodes_.PopBack() : *nodes_.PopFront();

            if (node.Mark() == ENodeMark::should_update)
                node.Tick(&turn_);

            // Defer if node was dynamically attached to a predecessor that
            // has not pulsed yet
            if (node.State == ENodeState::dyn_defer)
                continue;

            // Repeat the update if a node was dynamically attached to a
            // predecessor that has already pulsed
            while (node.State == ENodeState::dyn_repeat)
                node.Tick(&turn_);

            // Mark successors for update?
            bool update = node.State == ENodeState::changed;
            node.State = ENodeState::unchanged;

            {// node.ShiftMutex
                Node::ShiftMutexT::scoped_lock lock(node.ShiftMutex, false);

                for (auto* succ : node.Successors)
                {
                    if (update)
                        succ->SetMark(ENodeMark::should_update);

                    // Delay tick?
                    if (succ->DecCounter())
                        continue;

                    // Heavyweight - spawn new task
                    if (succ->IsHeavyweight())
                    {
                        auto& t = *new(task::allocate_additional_child_of(*parent()))
                            UpdaterTask(turn_, succ);

                        spawn(t);
                    }
                    // Leightweight - add to buffer, split if full
                    else
                    {
                        nodes_.PushBack(succ);

                        if (nodes_.IsFull())
                        {
                            splitCount++;

                            //Delegate half the work to new task
                            auto& t = *new(task::allocate_additional_child_of(*parent()))
                                UpdaterTask(*this, SplitTag{});

                            spawn(t);
                        }
                    }
                }

                node.SetMark(ENodeMark::unmarked);
            }// ~node.ShiftMutex
        }

        return nullptr;
    }

private:
    Turn&   turn_;
    BufferT nodes_;
};


///////////////////////////////////////////////////////////////////////////////////////////////////
/// Turn
///////////////////////////////////////////////////////////////////////////////////////////////////
inline Turn::Turn(TurnIdT id, TransactionFlagsT flags) :
    TurnBase( id, flags )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulsecountEngine
///////////////////////////////////////////////////////////////////////////////////////////////////

inline void EngineBase::OnNodeAttach(Node& node, Node& parent)
{
    parent.Successors.Add(node);
}

inline void EngineBase::OnNodeDetach(Node& node, Node& parent)
{
    parent.Successors.Remove(node);
}

inline void EngineBase::OnInputChange(Node& node, Turn& turn)
{
    changedInputs_.push_back(&node);
    node.State = ENodeState::changed;
}

template <typename TTask, typename TIt, typename ... TArgs>
void spawnTasks
(
    task& rootTask, task_list& spawnList,
    const size_t count, TIt start, TIt end,
    TArgs& ... args
)
{
    assert(1 + count <=
        static_cast<size_t>((std::numeric_limits<int>::max)()));

    rootTask.set_ref_count(1 + static_cast<int>(count));

    for (size_t i=0; i < (count - 1); i++)
    {
        spawnList.push_back(*new(rootTask.allocate_child())
            TTask(args ..., start, start + chunk_size));
        start += chunk_size;
    }

    spawnList.push_back(*new(rootTask.allocate_child())
        TTask(args ..., start, end));

    rootTask.spawn_and_wait_for_all(spawnList);

    spawnList.clear();
}

inline void EngineBase::Propagate(Turn& turn)
{
    const size_t initialTaskCount = (changedInputs_.size() - 1) / chunk_size + 1;

    spawnTasks<MarkerTask>(rootTask_, spawnList_, initialTaskCount,
        changedInputs_.begin(), changedInputs_.end());

    spawnTasks<UpdaterTask>(rootTask_, spawnList_, initialTaskCount,
        changedInputs_.begin(), changedInputs_.end(), turn);

    changedInputs_.clear();
}

inline void EngineBase::OnNodePulse(Node& node, Turn& turn)
{
    node.State = ENodeState::changed;
}

inline void EngineBase::OnNodeIdlePulse(Node& node, Turn& turn)
{
    node.State = ENodeState::unchanged;
}

inline void EngineBase::OnDynamicNodeAttach(Node& node, Node& parent, Turn& turn)
{// parent.ShiftMutex (write)
    NodeShiftMutexT::scoped_lock lock(parent.ShiftMutex, true);
    
    parent.Successors.Add(node);

    // Has already nudged its neighbors?
    if (parent.Mark() == ENodeMark::unmarked)
    {
        node.State = ENodeState::dyn_repeat;
    }
    else
    {
        node.State = ENodeState::dyn_defer;
        node.IncCounter();
        node.SetMark(ENodeMark::should_update);
    }
}// ~parent.ShiftMutex (write)

inline void EngineBase::OnDynamicNodeDetach(Node& node, Node& parent, Turn& turn)
{// parent.ShiftMutex (write)
    NodeShiftMutexT::scoped_lock lock(parent.ShiftMutex, true);

    parent.Successors.Remove(node);
}// ~parent.ShiftMutex (write)

} // ~namespace pulsecount
/****************************************/ REACT_IMPL_END /***************************************/
// #include "../src/engine/SubtreeEngine.cpp"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// #include "react/engine/SubtreeEngine.h"


#include <cstdint>
#include <utility>

// #include "react/common/Types.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/
namespace subtree {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Parameters
///////////////////////////////////////////////////////////////////////////////////////////////////
static const uint chunk_size    = 8;
static const uint dfs_threshold = 3;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// Turn
///////////////////////////////////////////////////////////////////////////////////////////////////
inline Turn::Turn(TurnIdT id, TransactionFlagsT flags) :
    TurnBase( id, flags )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// PulsecountEngine
///////////////////////////////////////////////////////////////////////////////////////////////////

inline void EngineBase::OnNodeAttach(Node& node, Node& parent)
{
    parent.Successors.Add(node);

    if (node.Level <= parent.Level)
        node.Level = parent.Level + 1;
}

inline void EngineBase::OnNodeDetach(Node& node, Node& parent)
{
    parent.Successors.Remove(node);
}

inline void EngineBase::OnInputChange(Node& node, Turn& turn)
{
    processChildren(node, turn);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UpdaterTask
///////////////////////////////////////////////////////////////////////////////////////////////////
class UpdaterTask: public task
{
public:
    using BufferT = NodeBuffer<Node,chunk_size>;

    UpdaterTask(Turn& turn, Node* node) :
        turn_( turn ),
        nodes_( node )
    {}

    UpdaterTask(UpdaterTask& other, SplitTag) :
        turn_( other.turn_ ),
        nodes_( other.nodes_, SplitTag( ) )
    {}

    task* execute()
    {
        uint splitCount = 0;

        while (!nodes_.IsEmpty())
        {
            Node& node = splitCount > dfs_threshold ? *nodes_.PopBack() : *nodes_.PopFront();

            if (node.IsInitial() || node.ShouldUpdate())
                node.Tick(&turn_);

            node.ClearInitialFlag();
            node.SetShouldUpdate(false);

            // Defer if node was dynamically attached to a predecessor that
            // has not pulsed yet
            if (node.IsDeferred())
            {
                node.ClearDeferredFlag();
                continue;
            }

            // Repeat the update if a node was dynamically attached to a
            // predecessor that has already pulsed
            while (node.IsRepeated())
            {
                node.ClearRepeatedFlag();
                node.Tick(&turn_);
            }

            node.SetReadyCount(0);

            // Mark successors for update?
            bool update = node.IsChanged();

            {// node.ShiftMutex
                Node::ShiftMutexT::scoped_lock lock(node.ShiftMutex, false);

                for (auto* succ : node.Successors)
                {
                    if (update)
                        succ->SetShouldUpdate(true);

                    // Wait for more?
                    if (succ->IncReadyCount())
                        continue;

                    // Heavyweight - spawn new task
                    if (succ->IsHeavyweight())
                    {
                        auto& t = *new(task::allocate_additional_child_of(*parent()))
                            UpdaterTask(turn_, succ);

                        spawn(t);
                    }
                    // Leightweight - add to buffer, split if full
                    else
                    {
                        nodes_.PushBack(succ);

                        if (nodes_.IsFull())
                        {
                            ++splitCount;

                            //Delegate half the work to new task
                            auto& t = *new(task::allocate_additional_child_of(*parent()))
                                UpdaterTask(*this, SplitTag());

                            spawn(t);
                        }
                    }
                }

                node.ClearMarkedFlag();
            }// ~node.ShiftMutex
        }

        return nullptr;
    }

private:
    Turn&   turn_;
    BufferT nodes_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void EngineBase::Propagate(Turn& turn)
{
    // Phase 1
    while (scheduledNodes_.FetchNext())
    {
        for (auto* curNode : scheduledNodes_.NextValues())
        {
            if (curNode->Level < curNode->NewLevel)
            {
                curNode->Level = curNode->NewLevel;
                invalidateSuccessors(*curNode);
                scheduledNodes_.Push(curNode);
                continue;
            }

            curNode->ClearQueuedFlag();
            curNode->Tick(&turn);
        }
    }

    // Phase 2
    isInPhase2_ = true;

    assert((1 + subtreeRoots_.size()) <=
        static_cast<size_t>((std::numeric_limits<int>::max)()));

    rootTask_.set_ref_count(1 + static_cast<int>(subtreeRoots_.size()));

    for (auto* node : subtreeRoots_)
    {
        // Ignore if root flag has been cleared because node was part of another subtree
        if (! node->IsRoot())
        {
            rootTask_.decrement_ref_count();
            continue;
        }

        spawnList_.push_back(*new(rootTask_.allocate_child())
            UpdaterTask(turn, node));

        node->ClearRootFlag();
    }

    rootTask_.spawn_and_wait_for_all(spawnList_);

    subtreeRoots_.clear();
    spawnList_.clear();

    isInPhase2_ = false;
}

inline void EngineBase::OnNodePulse(Node& node, Turn& turn)
{
    if (isInPhase2_)
        node.SetChangedFlag();
    else
        processChildren(node, turn);
}

inline void EngineBase::OnNodeIdlePulse(Node& node, Turn& turn)
{
    if (isInPhase2_)
        node.ClearChangedFlag();
}

inline void EngineBase::OnDynamicNodeAttach(Node& node, Node& parent, Turn& turn)
{
    if (isInPhase2_)
    {
        applyAsyncDynamicAttach(node, parent, turn);
    }
    else
    {
        OnNodeAttach(node, parent);
    
        invalidateSuccessors(node);

        // Re-schedule this node
        node.SetQueuedFlag();
        scheduledNodes_.Push(&node);
    }
}

inline void EngineBase::OnDynamicNodeDetach(Node& node, Node& parent, Turn& turn)
{
    if (isInPhase2_)
        applyAsyncDynamicDetach(node, parent, turn);
    else
        OnNodeDetach(node, parent);
}

inline void EngineBase::applyAsyncDynamicAttach(Node& node, Node& parent, Turn& turn)
{// parent.ShiftMutex (write)
    NodeShiftMutexT::scoped_lock    lock(parent.ShiftMutex, true);
    
    parent.Successors.Add(node);

    // Level recalulation applied when added to topoqueue next time.
    // During the async phase 2 it's not needed.
    if (node.NewLevel <= parent.Level)
        node.NewLevel = parent.Level + 1;

    // Has already nudged its neighbors?
    if (! parent.IsMarked())
    {
        node.SetRepeatedFlag();
    }
    else
    {
        node.SetDeferredFlag();
        node.SetShouldUpdate(true);
        node.DecReadyCount();
    }
}// ~parent.ShiftMutex (write)

inline void EngineBase::applyAsyncDynamicDetach(Node& node, Node& parent, Turn& turn)
{// parent.ShiftMutex (write)
    NodeShiftMutexT::scoped_lock    lock(parent.ShiftMutex, true);

    parent.Successors.Remove(node);
}// ~parent.ShiftMutex (write)

inline void EngineBase::processChildren(Node& node, Turn& turn)
{
    // Add children to queue
    for (auto* succ : node.Successors)
    {
        // Ignore if node part of marked subtree
        if (succ->IsMarked())
            continue;

        // Light nodes use sequential toposort in phase 1
        if (! succ->IsHeavyweight())
        {
            if (!succ->IsQueued())
            {
                succ->SetQueuedFlag();
                scheduledNodes_.Push(succ);
            }
        }
        // Heavy nodes + subtrees are deferred for parallel updating in phase 2
        else
        {
            // Force an initial update for heavy non-input nodes.
            // (non-atomic flag, unlike ShouldUpdate)
            if (!succ->IsInputNode())
                succ->SetInitialFlag();

            succ->SetChangedFlag();
            succ->SetRootFlag();

            markSubtree(*succ);

            subtreeRoots_.push_back(succ);
        }
    }
}

inline void EngineBase::markSubtree(Node& root)
{
    root.SetMarkedFlag();
    root.WaitCount = 0;

    for (auto* succ : root.Successors)
    {
        if (!succ->IsMarked())
            markSubtree(*succ);

        // Successor of another marked node? -> not a root anymore
        else if (succ->IsRoot())
            succ->ClearRootFlag();

        ++succ->WaitCount;
    }
}

inline void EngineBase::invalidateSuccessors(Node& node)
{
    for (auto* succ : node.Successors)
    {
        if (succ->NewLevel <= node.Level)
            succ->NewLevel = node.Level + 1;
    }
}

} // ~namespace subtree
/****************************************/ REACT_IMPL_END /***************************************/
// #include "../src/engine/ToposortEngine.cpp"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// #include "react/engine/ToposortEngine.h"

#include "tbb/parallel_for.h"

/***************************************/ REACT_IMPL_BEGIN /**************************************/
namespace toposort {

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
inline SeqTurn::SeqTurn(TurnIdT id, TransactionFlagsT flags) :
    TurnBase( id, flags )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ParTurn
///////////////////////////////////////////////////////////////////////////////////////////////////
inline ParTurn::ParTurn(TurnIdT id, TransactionFlagsT flags) :
    TurnBase( id, flags )
{}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
template <typename TNode, typename TTurn>
void EngineBase<TNode,TTurn>::OnNodeAttach(TNode& node, TNode& parent)
{
    parent.Successors.Add(node);

    if (node.Level <= parent.Level)
        node.Level = parent.Level + 1;
}

template <typename TNode, typename TTurn>
void EngineBase<TNode,TTurn>::OnNodeDetach(TNode& node, TNode& parent)
{
    parent.Successors.Remove(node);
}

template <typename TNode, typename TTurn>
void EngineBase<TNode,TTurn>::OnInputChange(TNode& node, TTurn& turn)
{
    processChildren(node, turn);
}

template <typename TNode, typename TTurn>
void EngineBase<TNode,TTurn>::OnNodePulse(TNode& node, TTurn& turn)
{
    processChildren(node, turn);
}

// Explicit instantiation
template class EngineBase<SeqNode,SeqTurn>;
template class EngineBase<ParNode,ParTurn>;

///////////////////////////////////////////////////////////////////////////////////////////////////
/// SeqEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void SeqEngineBase::Propagate(SeqTurn& turn)
{
    while (scheduledNodes_.FetchNext())
    {
        for (auto* curNode : scheduledNodes_.NextValues())
        {
            if (curNode->Level < curNode->NewLevel)
            {
                curNode->Level = curNode->NewLevel;
                invalidateSuccessors(*curNode);
                scheduledNodes_.Push(curNode);
                continue;
            }

            curNode->Queued = false;
            curNode->Tick(&turn);
        }
    }
}

inline void SeqEngineBase::OnDynamicNodeAttach(SeqNode& node, SeqNode& parent, SeqTurn& turn)
{
    this->OnNodeAttach(node, parent);
    
    invalidateSuccessors(node);

    // Re-schedule this node
    node.Queued = true;
    scheduledNodes_.Push(&node);
}

inline void SeqEngineBase::OnDynamicNodeDetach(SeqNode& node, SeqNode& parent, SeqTurn& turn)
{
    this->OnNodeDetach(node, parent);
}

inline void SeqEngineBase::processChildren(SeqNode& node, SeqTurn& turn)
{
    // Add children to queue
    for (auto* succ : node.Successors)
    {
        if (!succ->Queued)
        {
            succ->Queued = true;
            scheduledNodes_.Push(succ);
        }
    }
}

inline void SeqEngineBase::invalidateSuccessors(SeqNode& node)
{
    for (auto* succ : node.Successors)
    {
        if (succ->NewLevel <= node.Level)
            succ->NewLevel = node.Level + 1;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// ParEngineBase
///////////////////////////////////////////////////////////////////////////////////////////////////
inline void ParEngineBase::Propagate(ParTurn& turn)
{
    while (topoQueue_.FetchNext())
    {
        //using RangeT = tbb::blocked_range<vector<ParNode*>::const_iterator>;
        using RangeT = ParEngineBase::TopoQueueT::NextRangeT;

        // Iterate all nodes of current level and start processing them in parallel
        tbb::parallel_for(
            topoQueue_.NextRange(),
            [&] (const RangeT& range)
            {
                for (const auto& e : range)
                {
                    auto* curNode = e.first;

                    if (curNode->Level < curNode->NewLevel)
                    {
                        curNode->Level = curNode->NewLevel;
                        invalidateSuccessors(*curNode);
                        topoQueue_.Push(curNode);
                        continue;
                    }

                    curNode->Collected = false;

                    // Tick -> if changed: OnNodePulse -> adds child nodes to the queue
                    curNode->Tick(&turn);
                }
            }
        );

        if (dynRequests_.size() > 0)
        {
            for (auto req : dynRequests_)
            {
                if (req.ShouldAttach)
                    applyDynamicAttach(*req.Node, *req.Parent, turn);
                else
                    applyDynamicDetach(*req.Node, *req.Parent, turn);
            }
            dynRequests_.clear();
        }
    }
}

inline void ParEngineBase::OnDynamicNodeAttach(ParNode& node, ParNode& parent, ParTurn& turn)
{
    DynRequestData data{ true, &node, &parent };
    dynRequests_.push_back(data);
}

inline void ParEngineBase::OnDynamicNodeDetach(ParNode& node, ParNode& parent, ParTurn& turn)
{
    DynRequestData data{ false, &node, &parent };
    dynRequests_.push_back(data);
}

inline void ParEngineBase::applyDynamicAttach(ParNode& node, ParNode& parent, ParTurn& turn)
{
    this->OnNodeAttach(node, parent);

    invalidateSuccessors(node);

    // Re-schedule this node
    node.Collected = true;
    topoQueue_.Push(&node);
}

inline void ParEngineBase::applyDynamicDetach(ParNode& node, ParNode& parent, ParTurn& turn)
{
    this->OnNodeDetach(node, parent);
}

inline void ParEngineBase::processChildren(ParNode& node, ParTurn& turn)
{
    // Add children to queue
    for (auto* succ : node.Successors)
        if (!succ->Collected.exchange(true, std::memory_order_relaxed))
            topoQueue_.Push(succ);
}

inline void ParEngineBase::invalidateSuccessors(ParNode& node)
{
    for (auto* succ : node.Successors)
    {// succ->InvalidateMutex
        ParNode::InvalidateMutexT::scoped_lock lock(succ->InvalidateMutex);

        if (succ->NewLevel <= node.Level)
            succ->NewLevel = node.Level + 1;
    }// ~succ->InvalidateMutex
}

} // ~namespace toposort
/****************************************/ REACT_IMPL_END /***************************************/
// #include "../src/logging/EventLog.cpp"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// #include "react/logging/EventLog.h"


#ifdef REACT_ENABLE_LOGGING

/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// EventLog
///////////////////////////////////////////////////////////////////////////////////////////////////
inline EventLog::Entry::Entry() :
    time_{ std::chrono::system_clock::now() },
    data_{ nullptr }
{
}

inline EventLog::Entry::Entry(const Entry& other) :
    time_{ other.time_ },
    data_{ other.data_}
{
}

inline EventLog::Entry::Entry(IEventRecord* ptr) :
    time_{ std::chrono::system_clock::now() },
    data_{ ptr }
{
}

inline void EventLog::Entry::Serialize(std::ostream& out, const TimestampT& startTime) const
{
    out << EventId() << " : " << std::chrono::duration_cast<std::chrono::microseconds>(Time() - startTime).count() << std::endl;
    data_->Serialize(out);
}

inline EventLog::Entry& EventLog::Entry::operator=(Entry& rhs)
{
    time_ = rhs.time_,
    data_ = std::move(rhs.data_);
    return *this;
}

inline bool EventLog::Entry::Equals(const Entry& other) const
{
    if (EventId() != other.EventId())
        return false;
    // Todo
    return false;
}

inline EventLog::EventLog() :
    startTime_(std::chrono::system_clock::now())
{
}

inline EventLog::~EventLog()
{
    Clear();
}

inline void EventLog::Print()
{
    Write(std::cout);
}

inline void EventLog::Write(std::ostream& out)
{
    for (auto& e : entries_)
        e.Serialize(out, startTime_);
}

inline void EventLog::Clear()
{
    for (auto& e : entries_)
        e.Release();
    entries_.clear();
}

/****************************************/ REACT_IMPL_END /***************************************/

#endif // REACT_ENABLE_LOGGING

// #include "../src/logging/EventRecords.cpp"

//          Copyright Sebastian Jeckel 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <thread>

// #include "react/logging/EventRecords.h"


/***************************************/ REACT_IMPL_BEGIN /**************************************/

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeCreateEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeCreateEvent::NodeCreateEvent(ObjectId nodeId, const char* type) :
    nodeId_( nodeId ),
    type_( type )
{}

inline void NodeCreateEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Type = " << type_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeDestroyEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeDestroyEvent::NodeDestroyEvent(ObjectId nodeId) :
    nodeId_( nodeId )
{}

inline void NodeDestroyEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeAttachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeAttachEvent::NodeAttachEvent(ObjectId nodeId, ObjectId parentId) :
    nodeId_( nodeId ),
    parentId_{ parentId }
{}

inline void NodeAttachEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Parent = " << parentId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeDetachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeDetachEvent::NodeDetachEvent(ObjectId nodeId, ObjectId parentId) :
    nodeId_( nodeId ),
    parentId_{ parentId }
{}

inline void NodeDetachEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Parent = " << parentId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// InputNodeAdmissionEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline InputNodeAdmissionEvent::InputNodeAdmissionEvent(ObjectId nodeId, int transactionId) :
    nodeId_( nodeId ),
    transactionId_( transactionId )
{}

inline void InputNodeAdmissionEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodePulseEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodePulseEvent::NodePulseEvent(ObjectId nodeId, int transactionId) :
    nodeId_( nodeId ),
    transactionId_( transactionId )
{}

inline void NodePulseEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeIdlePulseEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeIdlePulseEvent::NodeIdlePulseEvent(ObjectId nodeId, int transactionId) :
    nodeId_( nodeId ),
    transactionId_( transactionId )
{}

inline void NodeIdlePulseEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DynamicNodeAttachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline DynamicNodeAttachEvent::DynamicNodeAttachEvent(ObjectId nodeId, ObjectId parentId, int transactionId) :
    nodeId_( nodeId ),
    parentId_{ parentId },
    transactionId_( transactionId )
{}

inline void DynamicNodeAttachEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Parent = " << parentId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// DynamicNodeDetachEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline DynamicNodeDetachEvent::DynamicNodeDetachEvent(ObjectId nodeId, ObjectId parentId, int transactionId) :
    nodeId_( nodeId ),
    parentId_{ parentId },
    transactionId_( transactionId )
{}

inline void DynamicNodeDetachEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Parent = " << parentId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeEvaluateBeginEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeEvaluateBeginEvent::NodeEvaluateBeginEvent(ObjectId nodeId, int transactionId) :
    nodeId_( nodeId ),
    transactionId_( transactionId ),
    threadId_( std::this_thread::get_id() )
{}

inline void NodeEvaluateBeginEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
    out << "> Thread = " << threadId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// NodeEvaluateEndEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline NodeEvaluateEndEvent::NodeEvaluateEndEvent(ObjectId nodeId, int transactionId) :
    nodeId_( nodeId ),
    transactionId_( transactionId ),
    threadId_{ std::this_thread::get_id() }
{}

inline void NodeEvaluateEndEvent::Serialize(std::ostream& out) const
{
    out << "> Node = " << nodeId_ << std::endl;
    out << "> Transaction = " << transactionId_ << std::endl;
    out << "> Thread = " << threadId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionBeginEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline TransactionBeginEvent::TransactionBeginEvent(int transactionId) :
    transactionId_( transactionId )
{}

inline void TransactionBeginEvent::Serialize(std::ostream& out) const
{
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// TransactionEndEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline TransactionEndEvent::TransactionEndEvent(int transactionId) :
    transactionId_( transactionId )
{}

inline void TransactionEndEvent::Serialize(std::ostream& out) const
{
    out << "> Transaction = " << transactionId_ << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// UserBreakpointEvent
///////////////////////////////////////////////////////////////////////////////////////////////////
inline UserBreakpointEvent::UserBreakpointEvent(const char* name) :
    name_( name )
{}

inline void UserBreakpointEvent::Serialize(std::ostream& out) const
{
    out << "> Name = " << name_ << std::endl;
}

/****************************************/ REACT_IMPL_END /***************************************/


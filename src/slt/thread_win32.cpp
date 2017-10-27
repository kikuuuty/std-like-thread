#include "thread_win32.h"

#include <assert.h>
#include <process.h>

#include "thread.h"

#define Thr_min(a,b)            ((a) < (b)) ? (a) : (b)
#define Thr_max(a,b)            ((a) > (b)) ? (a) : (b)
#define Thr_clamp(n,min,max)    Thr_min(Thr_max((n), (min)), (max))

namespace {

#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD   dwType;     // Must be 0x1000.
        LPCSTR  szName;     // Pointer to name (in user addr space).
        DWORD   dwThreadID; // Thread ID (-1=caller thread).
        DWORD   dwFlags;    // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)

    void setThreadName(DWORD threadId, const char* name)
    {
        const DWORD kVCThreadNameException = 0x406D1388;
        THREADNAME_INFO info = {0x1000, name, threadId, 0};
        __try {
            ::RaiseException(kVCThreadNameException, 0,
                             sizeof(info)/sizeof(ULONG_PTR), reinterpret_cast<ULONG_PTR*>(&info));
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
        }
    }

}

namespace slt {

    thread::attributes::attributes() noexcept
        : stackSize(64*1024)
        , priority(thread::priority::normal)
        , affinity(thread::affinity::all)
        , name("slt::thread")
    {
    }

    thread::id this_thread::get_id() noexcept
    {
        detail::thread_t thr;
        thr.id = ::GetCurrentThreadId();
        thr.handle = ::GetCurrentThread();
        return thr;
    }

}

namespace slt {
namespace this_thread {

    void yield() noexcept
    {
        // https://msdn.microsoft.com/ja-jp/library/cc429474.aspx
        ::Sleep(0);
    }

    void sleep_for(const std::chrono::nanoseconds& ns) noexcept
    {
        using namespace std::chrono; 
        if (ns > nanoseconds::zero()) {
            milliseconds ms = duration_cast<milliseconds>(ns);
            DWORD msec;
            typedef decltype(msec) msec_t;
            if (ms.count() < std::numeric_limits<msec_t>::max()) {
                msec = static_cast<msec_t>(ms.count());
            }
            else {
                msec = std::numeric_limits<msec_t>::max();
            }
            ::Sleep(msec);
        }
    }

}
}

namespace slt {
namespace detail {

    thread_t create(placeholder* holder, size_t stackSize, int priority, int affinity, const std::string& name)
    {
        thread_t thr;
        thr.id = 0;

        struct thread_args_t
        {
            placeholder* holder;
            std::string name;
        } args = {holder, name};

        unsigned threadId;
        HANDLE handle = reinterpret_cast<HANDLE>(::_beginthreadex(
            NULL,
            static_cast<uint32_t>(stackSize),
            [] (void* ud) -> unsigned {
                auto& args = *static_cast<thread_args_t*>(ud);
#if defined(_DEBUG) || defined(DEBUG)
                setThreadName(::GetCurrentThreadId(), args.name.c_str());
#endif
                args.holder->go();
                return 0;
            },
            reinterpret_cast<void*>(&args),
            CREATE_SUSPENDED,
            &threadId));

        if (handle <= 0) {
            assert(0 && "_beginthreadex failed.");
            return thr;
        }

        DWORD ret = S_OK;
        const int32_t range = THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST;
        int32_t npriority = Thr_clamp(priority, static_cast<int32_t>(thread::priority::lowest), static_cast<int32_t>(thread::priority::highest));
        npriority = THREAD_PRIORITY_LOWEST + static_cast<int32_t>(std::roundf(range * static_cast<float>(npriority) * (1.0f/255)));
        ret = ::SetThreadPriority(handle, npriority);
        if (0 == ret) {
            assert(0 && "SetThreadPriority failed.");
            return thr;
        }
        DWORD_PTR previousMask = ::SetThreadAffinityMask(handle, static_cast<DWORD_PTR>(affinity));
        if (0 == previousMask) {
            assert(0 && "SetThreadAffinityMask failed.");
            return thr;
        }
        ret = ::SetThreadIdealProcessor(handle, MAXIMUM_PROCESSORS);
        if (0xFFFFFFFF == ret) {
            assert(0 && "SetThreadIdealProcessor failed.");
            return thr;
        }
        ret = ::ResumeThread(handle);
        if (0xFFFFFFFF == ret) {
            assert(0 && "ResumeThread failed.");
            return thr;
        }

        thr.id = threadId;
        thr.handle = handle;
        holder->waitForRunning();
        return thr;
    }

    bool join(thread_t& thr)
    {
        if (WAIT_OBJECT_0 == ::WaitForSingleObject(thr.handle, INFINITE)) {
            ::CloseHandle(thr.handle);
            return true;
        }
        return false;
    }

    void detach(thread_t& thr)
    {
        ::CloseHandle(thr.handle);
    }

    unsigned hardware_concurrency()
    {
        SYSTEM_INFO info;
        ::GetSystemInfo(&info);
        return info.dwNumberOfProcessors;
    }

}
}

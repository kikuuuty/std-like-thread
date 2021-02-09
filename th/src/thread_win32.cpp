#include <cassert>
#include <cmath>
#include <process.h>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <th/thread.h>
#include <th/thread_win32.h>

namespace {

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO {
    DWORD   dwType;     // Must be 0x1000.
    LPCSTR  szName;     // Pointer to name (in user addr space).
    DWORD   dwThreadID; // Thread ID (-1=caller thread).
    DWORD   dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

void setThreadName(DWORD threadId, const char* name) {
    const DWORD kVCThreadNameException = 0x406D1388;
    THREADNAME_INFO info = {0x1000, name, threadId, 0};
    __try {
        ::RaiseException(kVCThreadNameException, 0,
                         sizeof(info) / sizeof(ULONG_PTR),
                         reinterpret_cast<ULONG_PTR*>(&info));
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}

}

namespace th {

const size_t thread::DEFAULT_STACKSIZE = 64 * 1024;

namespace detail {

thread_t createThread(placeholder* holder, size_t stackSize, int32_t priority, uint64_t affinity, const char* name) {
    thread_t thr;
    thr.id = 0;

    struct thread_args_t {
        placeholder* holder;
        const char* name;
    };

    thread_args_t args = {holder, name};

    unsigned threadId;
    HANDLE handle = reinterpret_cast<HANDLE>(_beginthreadex(
        NULL,
        static_cast<uint32_t>(stackSize),
        [] (void* ud) -> unsigned {
            auto& args = *static_cast<thread_args_t*>(ud);
#if defined(_DEBUG) || defined(DEBUG)
            setThreadName(GetCurrentThreadId(), args.name);
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
    constexpr int32_t range = THREAD_PRIORITY_HIGHEST - THREAD_PRIORITY_LOWEST;
	int32_t npriority = std::min(std::max(priority, thread::PRIORITY_LOWEST), thread::PRIORITY_HIGHEST);
    npriority = THREAD_PRIORITY_LOWEST + static_cast<int32_t>(std::round(range * static_cast<float>(npriority) * (1.0f / 255)));
    ret = SetThreadPriority(handle, npriority);
    if (ret == FALSE) {
        assert(0 && "SetThreadPriority failed.");
        return thr;
    }
    DWORD_PTR previousMask = SetThreadAffinityMask(handle, static_cast<DWORD_PTR>(affinity));
    if (previousMask == NULL) {
        assert(0 && "SetThreadAffinityMask failed.");
        return thr;
    }
    ret = SetThreadIdealProcessor(handle, MAXIMUM_PROCESSORS);
    if (0xFFFFFFFF == ret) {
        assert(0 && "SetThreadIdealProcessor failed.");
        return thr;
    }
    ret = ResumeThread(handle);
    if (0xFFFFFFFF == ret) {
        assert(0 && "ResumeThread failed.");
        return thr;
    }

    thr.id = threadId;
    thr.handle = handle;
    holder->waitForRunning();
    return thr;
}

bool joinThread(thread_t& thr) {
    if (WAIT_OBJECT_0 == WaitForSingleObject(thr.handle, INFINITE)) {
        CloseHandle(thr.handle);
        return true;
    }
    return false;
}

void detachThread(thread_t& thr) {
    CloseHandle(thr.handle);
}

unsigned hardware_concurrency() {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors;
}

} // namespace detail

namespace this_thread {

thread::id get_id() noexcept {
    th::detail::thread_t thr;
    thr.handle = ::GetCurrentThread();
    thr.id = ::GetCurrentThreadId();
    return thr;
}

void yield() noexcept {
    // https://msdn.microsoft.com/ja-jp/library/cc429474.aspx
    Sleep(0);
}

void sleep_for(const std::chrono::nanoseconds& ns) noexcept {
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
        Sleep(msec);
    }
}

} // namespace this_thread

}

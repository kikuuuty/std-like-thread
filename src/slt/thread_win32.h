#ifndef THREAD_WIN32_H__
#define THREAD_WIN32_H__

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <string>

namespace slt {
namespace detail {

    class placeholder;

    struct thread_t
    {
        unsigned id;
        HANDLE handle;
    };

    inline uintptr_t thread_id(const thread_t& thr) noexcept { return static_cast<uintptr_t>(thr.id); }

    inline uintptr_t thread_handle(const thread_t& thr) noexcept { return reinterpret_cast<uintptr_t>(thr.handle); }

    inline bool thread_is_null(const thread_t& thr) noexcept { return thr.id == 0; }

    inline void thread_set_null(thread_t& thr) noexcept { thr.id = 0; }

    thread_t create(placeholder* holder, size_t stackSize, int priority, int affinity, const std::string& name);

    bool join(thread_t& thr);

    void detach(thread_t& thr);

    unsigned hardware_concurrency();

}
}

#endif

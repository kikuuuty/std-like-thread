#ifndef THREAD_H__
#define THREAD_H__
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <tuple>

#include <th/thread.inl>
#if defined(_WIN32) || defined(_WIN64)
#include <th/thread_win32.h>
#else
#error
#endif

namespace th {

class thread final {
public:
    static const size_t DEFAULT_STACKSIZE;

    static constexpr int32_t PRIORITY_HIGHEST = 255;
    static constexpr int32_t PRIORITY_ABOVE   = 191;
    static constexpr int32_t PRIORITY_NORMAL  = 127;
    static constexpr int32_t PRIORITY_BELOW   = 64;
    static constexpr int32_t PRIORITY_LOWEST  = 0;

    static constexpr uint64_t AFFINITY_MASK_ALL = UINT64_MAX;

    struct attributes {
        size_t stackSize = DEFAULT_STACKSIZE;
        int32_t priority = PRIORITY_NORMAL;
        uint64_t affinity = AFFINITY_MASK_ALL;
        const char* name = "th::thread";
    };

    class id;

    using native_handle_type = void*;

public:
    thread() noexcept {
        Thr_set_null(mThr);
    }

    template <typename F, typename... ARGS,
              std::enable_if_t<!std::is_same_v<std::decay_t<F>, thread>, int> = 0,
              std::enable_if_t<!std::is_same_v<std::decay_t<F>, attributes>, int> = 0>
    explicit thread(F&& f, ARGS&&... args) : thread(attributes(), std::forward<F>(f), std::forward<ARGS>(args)...) { }

    template <typename F, typename... ARGS,
              std::enable_if_t<!std::is_same_v<std::decay_t<F>, thread>, int> = 0,
              std::enable_if_t<std::is_same_v<std::decay_t<F>, attributes>, int> = 0>
    explicit thread(F&& attrs, ARGS&&... args) {
        using tuple = std::tuple<std::decay_t<ARGS>...>;
        detail::holder<tuple> holder(tuple(std::forward<ARGS>(args)...));
        mThr = detail::createThread(&holder, attrs.stackSize, attrs.priority, attrs.affinity, attrs.name);
    }

    ~thread() noexcept {
        if (joinable()) {
            std::terminate();
        }
    }

    thread(thread&& other) noexcept : mThr(std::exchange(other.mThr, {})) { }

    thread& operator=(thread&& other) noexcept {
        if (joinable()) {
            std::terminate();
        }

        mThr = std::exchange(other.mThr, {});
        return *this;
    }

    thread(const thread&) = delete;
    thread& operator =(const thread&) = delete;

    void swap(thread& other) noexcept {
        std::swap(mThr, other.mThr);
    }

    bool joinable() const noexcept {
        return Thr_is_null(mThr) ? false : true;
    }

    void join();

    void detach() {
        if (joinable()) {
            detail::detachThread(mThr);
            Thr_set_null(mThr);
        }
    }

    id get_id() const noexcept;

    static unsigned hardware_concurrency() noexcept {
        return detail::hardware_concurrency();
    }

    native_handle_type native_handle() const noexcept {
        return reinterpret_cast<native_handle_type>(Thr_handle(mThr));
    }

private:
    detail::thread_t mThr;
};

namespace this_thread {

thread::id get_id() noexcept;

void yield() noexcept;

void sleep_for(const std::chrono::nanoseconds& ns) noexcept;

template <class ClockT, class DurationT>
void sleep_until(const std::chrono::time_point<ClockT, DurationT>& atime) {
    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait_until(lock, atime, [&] { return ClockT::now() >= atime; });
}

template <class RepT, class PeriodT>
void sleep_for(const std::chrono::duration<RepT, PeriodT>& rtime) {
    if (rtime > std::chrono::duration<RepT, PeriodT>::zero()) {
        std::chrono::duration<long double> max = std::chrono::nanoseconds::max();
        std::chrono::nanoseconds ns;
        if (rtime < max) {
            ns = std::chrono::duration_cast<std::chrono::nanoseconds>(rtime);
            if (ns < rtime)
                ++ns;
        }
        else {
            ns = std::chrono::nanoseconds::max();
        }
        sleep_for(ns);
    }
}

} // namespace this_thread

class thread::id {
public:
    id() noexcept { Thr_set_null(mThr); }

    size_t hash() const { return std::hash<size_t>()(static_cast<size_t>(Thr_id(mThr))); }

private:
    id(const thread& thr) : mThr(thr.mThr) { }
    id(const detail::thread_t& thr) : mThr(thr) { }

    detail::thread_t mThr;

    friend thread::id thread::get_id() const noexcept;
    friend thread::id this_thread::get_id() noexcept;
    friend bool operator ==(const thread::id&, const thread::id&) noexcept;
    friend bool operator < (const thread::id&, const thread::id&) noexcept;
    template <class CharT, class TraitsT> inline
    friend std::basic_ostream<CharT, TraitsT>& operator <<(std::basic_ostream<CharT, TraitsT>& str, const thread::id& x);
};

inline thread::id thread::get_id() const noexcept { return id(*this); }

inline bool operator ==(const thread::id& lhs, const thread::id& rhs) noexcept { return Thr_id(lhs.mThr) == Thr_id(rhs.mThr); }
inline bool operator !=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(lhs == rhs); }
inline bool operator < (const thread::id& lhs, const thread::id& rhs) noexcept { return Thr_id(lhs.mThr) < Thr_id(rhs.mThr); }
inline bool operator <=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(rhs < lhs); }
inline bool operator > (const thread::id& lhs, const thread::id& rhs) noexcept { return rhs < lhs; }
inline bool operator >=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(lhs < rhs); }

template <class CharT, class TraitsT> inline
std::basic_ostream<CharT, TraitsT>& operator <<(std::basic_ostream<CharT, TraitsT>& str, const thread::id& x) {
    return str << std::hex << Thr_id(x);
}

inline void thread::join() {
    if (joinable()) {
        if (get_id() == this_thread::get_id()) {
            std::terminate();
        }
        if (detail::joinThread(mThr)) {
            Thr_set_null(mThr);
        }
    }
}

} // namespace th

#endif

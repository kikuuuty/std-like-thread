#ifndef THREAD_H__
#define THREAD_H__

#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>

#include "thread.inl"

#if defined(_WIN32) || defined(_WIN64)
#include "thread_win32.h"
#else
#error
#endif

namespace slt {

class thread final {
public:
    static constexpr int32_t kPriorityHighest = 255;
    static constexpr int32_t kPriorityAbove   = 191;
    static constexpr int32_t kPriorityNormal  = 127;
    static constexpr int32_t kPriorityBelow   = 64;
    static constexpr int32_t kPriorityLowest  = 0;

    static constexpr uint64_t kAffinityMaskAll = UINT64_MAX;

    class id;

    class attributes;

    using native_handle_type = void*;

public:
    thread() noexcept {
        detail::thread_set_null(mThr);
    }

    template <typename F, typename... Args,
              class = std::enable_if<!std::is_same<std::decay_t<F>, thread>::value>::type>
    explicit thread(F&& f, Args&&... args)
        : mThr(invoke<std::is_same<std::decay_t<F>, attributes>::type>()(
                    detail::decay_copy(std::forward<F>(f)),
                    detail::decay_copy(std::forward<Args>(args))...)) { }

    ~thread() noexcept {
        if (joinable()) {
            std::terminate();
        }
    }

    thread(thread&& other) noexcept
        : mThr(other.mThr) {
        detail::thread_set_null(other.mThr);
    }

    thread& operator =(thread&& other) noexcept {
        if (joinable()) {
            std::terminate();
        }
        mThr = other.mThr;
        detail::thread_set_null(other.mThr);
        return *this;
    }

    thread(const thread&) = delete;
    thread& operator =(const thread&) = delete;

    void swap(thread& other) noexcept {
        std::swap(mThr, other.mThr);
    }

    bool joinable() const noexcept {
        return detail::thread_is_null(mThr) ? false : true;
    }

    void join();

    void detach();

    id get_id() const noexcept;

    static unsigned hardware_concurrency() noexcept {
        return detail::hardware_concurrency();
    }

    native_handle_type native_handle() const noexcept {
        return reinterpret_cast<native_handle_type>(detail::thread_handle(mThr));
    }

private:
    template <typename T> struct invoke {};

    template <>
    struct invoke<std::true_type> {
        template <typename A, typename F, typename... Args>
        inline detail::thread_t operator()(A&& a, F&& f, Args&&... args) {
            auto b = std::make_unique<std::tuple<std::decay_t<F>, std::decay_t<Args>...> >(std::forward<F>(f), std::forward<Args>(args)...);
            detail::holder<decltype(b)> holder(std::move(b));
            attributes attrs = std::forward<A>(a);
            return detail::create(&holder, attrs.stackSize, attrs.priority, attrs.affinity, attrs.name);
        }
    };

    template <>
    struct invoke<std::false_type> {
        template <typename F, typename... Args>
        inline detail::thread_t operator()(F&& f, Args&&... args) {
            auto b = std::make_unique<std::tuple<std::decay_t<F>, std::decay_t<Args>...> >(std::forward<F>(f), std::forward<Args>(args)...);
            detail::holder<decltype(b)> holder(std::move(b));
            attributes attrs;
            return detail::create(&holder, attrs.stackSize, attrs.priority, attrs.affinity, attrs.name);
        }
    };

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
    id() noexcept {
        detail::thread_set_null(mThr);
    }

    template <class CharT, class TraitsT>
    std::basic_ostream<CharT, TraitsT>& to_text(std::basic_ostream<CharT, TraitsT>& str) const {
        return str << std::hex << Thr_id(mThr);
    }

    size_t hash() const { return std::hash<size_t>()(static_cast<size_t>(detail::thread_id(mThr))); }

private:
    id(const thread& thrd) : mThr(thrd.mThr) {}
    id(const detail::thread_t& thr) : mThr(thr) {}
    detail::thread_t mThr;

    friend thread::id thread::get_id() const noexcept;
    friend thread::id this_thread::get_id() noexcept;
    friend bool operator ==(const id&, const id&) noexcept;
    friend bool operator < (const id&, const id&) noexcept;

};

class thread::attributes {
public:
    attributes() noexcept;
    ~attributes() = default;

    size_t   stackSize;
	int32_t  priority;
    uint64_t affinity;
    std::string name;
};

inline void thread::join() {
    if (joinable()) {
        if (get_id() == this_thread::get_id()) {
            std::terminate();
        }
        if (detail::join(mThr)) {
            detail::thread_set_null(mThr);
        }
    }
}

inline void thread::detach() {
    if (joinable()) {
        detail::detach(mThr);
        detail::thread_set_null(mThr);
    }
}

inline thread::id thread::get_id() const noexcept { return id(*this); }

inline bool operator ==(const thread::id& lhs, const thread::id& rhs) noexcept { return detail::thread_id(lhs.mThr) == detail::thread_id(rhs.mThr); }
inline bool operator !=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(lhs == rhs); }
inline bool operator < (const thread::id& lhs, const thread::id& rhs) noexcept { return detail::thread_id(lhs.mThr) < detail::thread_id(rhs.mThr); }
inline bool operator <=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(rhs < lhs); }
inline bool operator > (const thread::id& lhs, const thread::id& rhs) noexcept { return rhs < lhs; } 
inline bool operator >=(const thread::id& lhs, const thread::id& rhs) noexcept { return !(lhs < rhs); }

template <class CharT, class TraitsT> inline
std::basic_ostream<CharT, TraitsT>& operator <<(std::basic_ostream<CharT, TraitsT>& str, const thread::id& x) {
    return x.to_text(str);
}

} // namespace slt

#endif

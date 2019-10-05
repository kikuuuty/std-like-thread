#ifndef THREAD_H__
#error "Must be included from thread.h"
#endif

#include <condition_variable>
#include <functional>
#include <mutex>
#include <tuple>

namespace slt {
namespace detail {

class placeholder {
public:
    inline placeholder() noexcept : started(false) {}
    virtual ~placeholder() = default;

    void waitForRunning() {
        std::unique_lock<std::mutex> lk(mtx);
        cv.wait(lk, [this] {return started;});
    }

    virtual void go() = 0;

protected:
    std::mutex mtx;
    std::condition_variable cv;
    bool started;
};

template <class F>
class holder : public placeholder {
public:
    template <class T>
    inline holder(T&& f) noexcept : fp(std::forward<T>(f)) {}

    void go() override {
        run(this);
    }

private:
    template <size_t... indices>
    static void execute(typename F::element_type& tuple, std::index_sequence<indices...>) {
        std::invoke(std::move(std::get<indices>(tuple))...);
    }

    static void run(holder* placeholder) {
        F local(std::forward<F>(placeholder->fp));
        {
            std::lock_guard<std::mutex> lk(placeholder->mtx);
            placeholder->started = true;
        }
        placeholder->cv.notify_one();
        execute(*local, std::make_index_sequence<std::tuple_size<typename F::element_type>::value>());
    }

    F fp;
};

template <class T> inline
typename std::decay<T>::type decay_copy(T&& v) noexcept(std::is_nothrow_constructible<T>::value) {
    return std::forward<T>(v);
}

}
}

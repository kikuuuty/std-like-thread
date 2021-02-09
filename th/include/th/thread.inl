#ifndef THREAD_H__
#error "Must be included from thread.h"
#endif

namespace th {

namespace detail {

class placeholder {
public:
    placeholder() noexcept : started(false) { }

    virtual ~placeholder() = default;

    virtual void go() = 0;

    void waitForRunning() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] {return started;});
    }

protected:
    std::mutex mtx;
    std::condition_variable cv;
    bool started;
};

template <class F>
class holder : public placeholder {
public:
    holder(F&& f) noexcept : fp(std::forward<F>(f)) { }

    void go() override {
        run(this);
    }

private:
    template <size_t... INDICES>
    static void execute(typename F&& tuple, std::index_sequence<INDICES...>) {
        std::invoke(std::move(std::get<INDICES>(tuple))...);
    }

    static void run(holder* placeholder) {
        F local(std::forward<F>(placeholder->fp));
        {
            std::lock_guard<std::mutex> lock(placeholder->mtx);
            placeholder->started = true;
        }
        placeholder->cv.notify_one();
        execute(std::move(local), std::make_index_sequence<std::tuple_size<typename F>::value>());
    }

    F fp;
};

}

}

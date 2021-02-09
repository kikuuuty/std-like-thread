#include <cstdint>
#include <cstdio>

#include <th/thread.h>

class Foo {
public:
    Foo() = default;

    void doSomething(int32_t n) {
        th::this_thread::sleep_for(std::chrono::seconds(n));
    }
};

int main(int, char**) {
    auto thr = th::thread([] (const char* str) { std::printf(str); }, "hello, world\n");

    thr.join();

    th::thread::attributes attrs;
    attrs.stackSize = 512 * 1024;
    attrs.priority = th::thread::PRIORITY_NORMAL;
    attrs.affinity = 0x03;
    attrs.name = "thread name for debugging";
    Foo foo;
    thr = th::thread(attrs, &Foo::doSomething, foo, 3);

    thr.join();

    return 0;
}

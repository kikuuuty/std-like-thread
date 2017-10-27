#include <cstdint>
#include <cstdio>

#include "slt/thread.h"

class Foo
{
public:
    Foo() {}
    ~Foo() {}

    void doSomething(int32_t n)
    {
        slt::this_thread::sleep_for(std::chrono::seconds(n));
    }
};

int main(int32_t, char**)
{
    using namespace slt;

    auto thr = thread([] (const char* str) { std::printf(str); }, "hello, world");

    thr.join();

    thread::attributes attrs;
    attrs.stackSize = 512 * 1024;
    attrs.priority = thread::priority::normal;
    attrs.affinity = thread::affinity::user0 | thread::affinity::user1;
    attrs.name = "thread name for debugging";
    Foo foo;
    thr = thread(attrs, &Foo::doSomething, foo, 3);

    thr.join();

    return 0;
}

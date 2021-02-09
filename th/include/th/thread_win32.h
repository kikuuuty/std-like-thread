#pragma once

namespace th {

namespace detail {

struct thread_t {
    void* handle;
    unsigned id;
};

class placeholder;

thread_t createThread(placeholder* holder, size_t stackSize, int32_t priority, uint64_t affinity, const char* name);

bool joinThread(thread_t& thr);

void detachThread(thread_t& thr);

unsigned hardware_concurrency();

}

#define Thr_id(thr)         (thr.id)
#define Thr_handle(thr)     (thr.handle)
#define Thr_is_null(thr)    (thr.id == 0)
#define Thr_set_null(thr)   (thr.id = 0)

}

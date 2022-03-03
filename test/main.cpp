#ifdef DEBUG_VALGRIND_
#include <valgrind/drd.h>
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(addr) ANNOTATE_HAPPENS_BEFORE(addr)
#define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(addr)  ANNOTATE_HAPPENS_AFTER(addr)
#define _GLIBCXX_EXTERN_TEMPLATE -1
#endif

#include <iostream>
#include <chrono>
#include <string>
#include <syncstream>

#include "thread-pool.h"

using namespace jks;

void a_ordinary_function_return_nothing()
{
    std::osyncstream(std::cout) << __func__ << std::endl;
}

std::string a_ordinary_function_return_string()
{
    return std::string(__func__);
}

future<void> a_coroutine_return_nothing()
{
    co_await thread_pool::awaitable<void>();
    std::osyncstream(std::cout) << __func__ << std::endl;
}

future<std::string> a_coroutine_return_string()
{
    auto h = co_await thread_pool::awaitable<std::string>();
    h.promise().set_value(__func__);
}

future<std::string> a_coroutine_return_string_with_input(int x)
{
    auto h = co_await thread_pool::awaitable<std::string>();
    h.promise().set_value(std::string(__func__) + ": " +  std::to_string(x));
}


std::string a_function_calling_a_coroutine()
{
    auto r = a_coroutine_return_string();
    return r.get() + " in " + __func__;
}

// You can submit your coroutine handle in your own awaitable
// This implementation is a simplified version of jks::thread_pool::awaitable
struct submit_awaitable: std::suspend_never
{
    void await_suspend(std::coroutine_handle<> h)
    {
        thread_pool::get(0).submit_coroutine(h);
    }
};

future<void> submit_raw_coroutine_handle()
{
    co_await submit_awaitable();
    std::osyncstream(std::cout) << __func__ << std::endl;
}

int main()
{
    using namespace std::chrono_literals;

    constexpr auto n_pool = 3;
    // get thread pool singleton
    auto& tpool = thread_pool::get(n_pool);

    // Ordinary function
    tpool.submit(a_ordinary_function_return_nothing);
    auto func_return_sth = tpool.submit(a_ordinary_function_return_string);

    // Coroutine
    tpool.submit(a_coroutine_return_nothing);
    auto coro_return_sth = tpool.submit(a_coroutine_return_string);
    auto coro_return_sth_with_input = tpool.submit([](){
        return a_coroutine_return_string_with_input(42);
    });

    // Function calling coroutine
    auto func_calling_coro = tpool.submit(a_function_calling_a_coroutine);

    // Raw coroutine handle
    submit_raw_coroutine_handle();
    
    std::this_thread::sleep_for(1s);

    // Also support lambda
    for (int i=0; i<=n_pool; ++i) {
        tpool.submit([i]() -> int{
            std::osyncstream(std::cout) << "* Task " << i << '+' << std::endl;
            std::this_thread::sleep_for(3s);
            std::osyncstream(std::cout) << "* Task " << i << '-' << std::endl;
            return i;
        });
    }
    std::this_thread::sleep_for(1s);


    std::osyncstream(std::cout) << func_return_sth.get() << std::endl;
    std::osyncstream(std::cout) << coro_return_sth.get().get() << std::endl;
    std::osyncstream(std::cout) << coro_return_sth_with_input.get().get() << std::endl;
    std::osyncstream(std::cout) << func_calling_coro.get() << std::endl;

    // Destructor of thread_pool blocks until tasks current executing completed
    // Tasks which are still in queue will not be executed
    // So above lambda example, Task 3 is not executed
}

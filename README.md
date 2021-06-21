# coroutine-thread-pool.h
Thread pool which supports c++20 coroutine. 一个支持c++20协程的线程池。

# API
```c++
// Thread pool class
jks::thread_pool;

// Singleton to get thread pool
jks::thread_pool::get();

// Submit a task to thread pool
jks::thread_pool.submit(std::invocable)

// Return type of submit, which can be used to get task result
jks::future<T>

// Awaitbale class which submit coroutine to thread pool
jks::thread_pool::awaitable
```

# Example
```c++
#include <iostream>
#include <chrono>
#include <string>

#include "thread-pool.h"

using namespace jks;

void a_ordinary_function_return_nothing()
{
    std::cout << __func__ << std::endl;
}

std::string a_ordinary_function_return_string()
{
    return std::string(__func__);
}

future<void> a_coroutine_return_nothing()
{
    co_await thread_pool::awaitable<void>();
    std::cout << __func__ << std::endl;
}

future<std::string> a_coroutine_return_string()
{
    auto h = co_await thread_pool::awaitable<std::string>();
    h.promise().set_value(__func__);
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
    std::cout << __func__ << std::endl;
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

    // Function calling coroutine
    auto func_calling_coro = tpool.submit(a_function_calling_a_coroutine);

    // Raw coroutine handle
    submit_raw_coroutine_handle();
    
    std::this_thread::sleep_for(1s);

    // Also support lambda
    for (int i=0; i<=n_pool; ++i) {
        tpool.submit([i]() -> int{
            std::cout << "* Task " << i << '+' << std::endl;
            std::this_thread::sleep_for(3s);
            std::cout << "* Task " << i << '-' << std::endl;
            return i;
        });
    }
    std::this_thread::sleep_for(1s);


    std::cout << func_return_sth.get() << std::endl;
    std::cout << coro_return_sth.get().get() << std::endl;
    std::cout << func_calling_coro.get() << std::endl;

    // Destructor of thread_pool blocks until tasks current executing completed
    // Tasks which are still in queue will not be executed
    // So above lambda example, Task 3 is not executed
}
```

Result (your result may be a little different due to multi-threading):

```sh
$ g++ --version
g++ (Ubuntu 11.1.0-1ubuntu1~20.04) 11.1.0
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
$ g++ -std=c++20 -pthread -Wall -Wextra -Isrc test/main.cpp -o main
$ ./main
a_ordinary_function_return_nothing
submit_raw_coroutine_handle
a_coroutine_return_nothing
* Task 0+
* Task 2+
* Task 1+
a_ordinary_function_return_string
a_coroutine_return_string
a_coroutine_return_string in a_function_calling_a_coroutine
* Task * Task 02--
* Task 1-
```

# Check memory leak

```sh
$ g++ -DDEBUG_VALGRIND_ -g -std=c++20 -pthread -Wall -Wextra -Isrc test/main.cpp -o main
$ valgrind --tool=drd ./main
```

Or you can bus gcc builtin santitizer.

```sh
$ g++ -fsanitize=address -std=c++20 -pthread -Wall -Wextra -Isrc test/main.cpp -o main
$ ./main
```

#pragma once

#include <atomic>
#include <concepts>
#include <coroutine>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "thread-safe-queue.h"

namespace jks {
    
template <typename T>
struct future: std::future<T>
{
    struct promise_type: std::promise<T>
    {
        std::future<T> get_return_object() { return this->get_future(); }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };

    // Construct future object may after destruction of promise_type
    // So get furure before this time point
    future(std::future<T>&& f): std::future<T>(std::move(f)) {}
};



struct thread_pool
{    
    // Singleton
    static thread_pool& get(int thread_number)
    {
        static thread_pool tp{thread_number};
        return tp;
    }

    template <typename T>
    struct awaitable
    {
        using PT = future<T>::promise_type;
        bool await_ready()
        { 
            return false; 
        }
        void await_suspend(std::coroutine_handle<PT> h)
        {
            m_h = h;
            thread_pool::get(0).submit_coroutine(h);
        }
        std::coroutine_handle<PT> await_resume() {return m_h;}
        
        std::coroutine_handle<PT> m_h = nullptr;
    };

    template <std::invocable F>
    future<std::invoke_result_t<F>> submit(F task)
    {
        using RT = std::invoke_result_t<F>;
        using PT = future<RT>::promise_type;
        std::coroutine_handle<PT> h = co_await awaitable<RT>();

        if constexpr (std::is_void_v<RT>) {
            task();
        } else {
            h.promise().set_value(task());
        }
    }

    // coroutine_handle is also callable
    // So cannot overload submit simply
    void submit_coroutine(std::coroutine_handle<> h)
    {
        m_queue.put(h);
    }
private:
    thread_pool(int thread_number)
    {
        for (int i=0; i<thread_number; ++i) {
            m_threads.emplace(std::jthread([this] {
                this->worker();
            }));
        }
    }

    ~thread_pool()
    {
        m_queue.destroy();
        while (!m_threads.empty()) {
            m_threads.pop();
        }
    }

    thread_pool(const thread_pool&) = delete;
    thread_pool operator=(const thread_pool&) = delete;

    void worker()
    {
        while (auto task = m_queue.take()) {
            task.value().resume();
        }
    }

    thread_safe_queue<std::coroutine_handle<>> m_queue;
    std::queue<std::jthread> m_threads;
};

} // namespace jks

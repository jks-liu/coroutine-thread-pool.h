#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>

namespace jks {

template <typename T>
struct thread_safe_queue
{
    thread_safe_queue() {}

    void put(T e)
    {
        std::unique_lock<std::mutex> lk(m_m);
        m_queue.emplace(e);
        m_cv.notify_one();
    }

    std::optional<T> take()
    {
        std::unique_lock<std::mutex> lk(m_m);
        m_cv.wait(lk, [q = this] { return q->m_must_return_nullptr.test() || !q->m_queue.empty(); });

        if (m_must_return_nullptr.test())
            return {};

        T ret = m_queue.front();
        m_queue.pop();

        return ret;
    }

    std::queue<T>& destroy()
    {
        // Even flag should under lock
        // https://stackoverflow.com/a/38148447/4144109
        std::unique_lock<std::mutex> _(m_m);
        m_must_return_nullptr.test_and_set();
        m_cv.notify_all();

        // No element will be taken from the queue
        // So we return the queue in case the caller need them
        // WARNING: put may happen after this operation, caller should think about this
        return m_queue;
    }

private:
    std::queue<T> m_queue;
    std::mutex m_m;
    std::condition_variable m_cv;

    std::atomic_flag m_must_return_nullptr;
};

} // namespace jks

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

    void put(T task)
    {
        std::unique_lock<std::mutex> lk(m_m);
        m_queue.emplace(task);
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

    void destroy()
    {
        m_must_return_nullptr.test_and_set();
        m_cv.notify_all();
    }

private:
    std::queue<T> m_queue;
    std::mutex m_m;
    std::condition_variable m_cv;

    std::atomic_flag m_must_return_nullptr=false;
};

} // namespace jks

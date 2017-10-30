#pragma once

#include <functional>
#include <vector>
#include <thread>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <atomic>

class Parallel
{
public:
    Parallel(uint32_t num_workers);
    ~Parallel();
    void run(std::function<void(uint32_t)>&& task);

private:
    std::function<void(uint32_t)> m_task;
    std::vector<std::thread> m_workers;
    std::mutex m_mutex;
    std::condition_variable m_signal_work;
    std::condition_variable m_signal_done;
    std::atomic_size_t m_workers_active;
    std::atomic_bool m_accept_work{true};

    void do_work(int32_t worker_id);
    void wait();

    void operator=(const Parallel&) = delete;
    Parallel(const Parallel&) = delete;
};

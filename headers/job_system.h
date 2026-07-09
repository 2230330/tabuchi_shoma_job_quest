#pragma once

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstdint>
#include <algorithm>
#include <cassert>

//マルチスレッドを用いて、処理を並行して実行するクラス
class JobSystem
{
public:
    using Job = std::function<void()>;
    //struct Job
    //{
    //    void(*Excute)(void*);
    //    void* Data;
    //};

public:
    static JobSystem& Instance();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void Initialize(uint32_t thread_count = 0);
    void Shutdown();

    void Enqueue(Job job);

    template<typename JobContainer>
    void EnqueueBatch(JobContainer&& jobs)
    {
        size_t pusued_count = 0;
        {
            std::lock_guard<std::mutex>lock(mutex_);

            if (!initialized_ || stopping_)
                return;

            for (auto& job : jobs)
            {
                jobs_.push_back(std::move(job));
                ++pusued_count;
            }
        }

        if (pusued_count > 0)
        {
            job_cv_.notify_all();
        }
    }

    void Wait();

    
    template<typename Func>
    void ParallelFor(size_t count, size_t batch_size, Func&& func)
    {
        if (count == 0)
        {
            return;
        }

        if (batch_size == 0)
        {
            batch_size = 1;
        }

        const uint32_t worker_count = GetWorkerCount();

        if (!IsInitialized() || worker_count == 0)
        {
            for (size_t i = 0; i < count; ++i)
            {
                func(i);
            }

            return;
        }

        // 軽すぎる処理は並列化しない
        if (count < batch_size * 2)
        {
            for (size_t i = 0; i < count; ++i)
            {
                func(i);
            }

            return;
        }

        const size_t batch_count = (count + batch_size - 1) / batch_size;

        std::atomic<size_t> next_index = 0;

        auto worker_body =[&] {
            while (true)
            {
                const size_t begin = next_index.fetch_add(
                    batch_size,
                    std::memory_order_relaxed
                );

                if (begin >= count)
                {
                    break;
                }

                const size_t end = (std::min)(begin + batch_size, count);

                for (size_t i = begin; i < end; ++i)
                {
                    func(i);
                }
            }
        };

        // メインスレッドも処理に参加するので、ワーカーに投げる数は最大 worker_count
        const uint32_t queued_job_count = static_cast<uint32_t>(
            (std::min<size_t>)(worker_count, batch_count - 1)
            );

        std::vector<Job> parallel_jobs;
        parallel_jobs.reserve(queued_job_count);

        for (uint32_t job_index = 0; job_index < queued_job_count; ++job_index)
        {
            parallel_jobs.emplace_back(worker_body);
        }

        EnqueueBatch(parallel_jobs);

        // メインスレッドも働く
        worker_body();

        // ワーカー側の完了待ち
        Wait();
    }


    uint32_t GetWorkerCount()const;
    bool IsInitialized()const;

private:
    JobSystem() = default;
    ~JobSystem();

    void WorkerLoop();

private:
    std::vector<std::thread> workers_;
    std::deque<Job> jobs_;

    mutable std::mutex mutex_;
    std::condition_variable job_cv_;
    std::condition_variable wait_cv_;

    bool initialized_ = false;
    bool stopping_ = false;

    std::atomic<uint32_t> active_job_count_{ 0 };

};
#include"../headers/job_system.h"

#include<array>

JobSystem& JobSystem::Instance()
{
    static JobSystem instance;
    return instance;
}

void JobSystem::Initialize(uint32_t thread_count)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_)
        return;

    if (thread_count == 0)
    {
        thread_count = std::thread::hardware_concurrency();

        if (thread_count == 0)
            thread_count = 4;

        //メインスレッド分を残す
        if (thread_count > 1)
            thread_count -= 1;
    }

    stopping_ = false;
    initialized_ = true;
    active_job_count_ = 0;

    workers_.reserve(thread_count);

    for (uint32_t i = 0; i < thread_count; ++i)
    {
        workers_.emplace_back(
            [this]
            {
                WorkerLoop();
            }
        );
    }
}

void JobSystem::Shutdown()
{
    {
        std::lock_guard<std::mutex>lock(mutex_);

        if (!initialized_)
            return;

        stopping_ = true;
    }

    job_cv_.notify_all();

    for (std::thread& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    workers_.clear();

    {
        std::lock_guard<std::mutex>lock(mutex_);

        while (!jobs_.empty())
        {
            jobs_.pop_front();
        }

        initialized_ = false;
        stopping_ = false;
        active_job_count_ = 0;
    }
}

void JobSystem::Enqueue(Job job)
{

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_ || stopping_)
        {
            return;
        }

        jobs_.push_back(std::move(job));
    }

    job_cv_.notify_one();

}

void JobSystem::Wait()
{
    std::unique_lock<std::mutex>lock(mutex_);

    wait_cv_.wait(
        lock,
        [this]
        {
            return jobs_.empty() && active_job_count_ == 0;
        }
    );
}

uint32_t JobSystem::GetWorkerCount() const
{
    std::lock_guard<std::mutex>lock(mutex_);
    return static_cast<uint32_t>(workers_.size());
}

bool JobSystem::IsInitialized() const
{
    std::lock_guard<std::mutex>lock(mutex_);
    return initialized_;
}

JobSystem::~JobSystem()
{
    Shutdown();
}
void JobSystem::WorkerLoop()
{
    constexpr size_t BatchSize = 8;

    std::array<Job, BatchSize>localJobs;

    while (true)
    {
        size_t jobCount = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            job_cv_.wait(lock,
                [this]
                {
                    return stopping_ || !jobs_.empty();
                });

            if (stopping_ && jobs_.empty())
            {
                return;
            }

            while (jobCount < BatchSize && !jobs_.empty())
            {
                localJobs[jobCount++] = std::move(jobs_.front());
                jobs_.pop_front();
            }

            active_job_count_.fetch_add(
                static_cast<uint32_t>(jobCount),
                std::memory_order_relaxed);
        }

        for (size_t i = 0; i < jobCount; ++i)
        {
            localJobs[i]();

            uint32_t remain =
                active_job_count_.fetch_sub(
                    1,
                    std::memory_order_acq_rel) - 1;

            if (remain == 0)
            {
                std::lock_guard<std::mutex> lock(mutex_);

                if (jobs_.empty())
                {
                    wait_cv_.notify_all();
                }
            }
        }
    }
}
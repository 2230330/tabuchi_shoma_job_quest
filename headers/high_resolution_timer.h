#ifndef PART2_HIGH_RESOLUTION_TIMER_H
#define PART2_HIGH_RESOLUTION_TIMER_H

#include<windows.h>

class HighResolutionTimer
{
public:
    HighResolutionTimer()
    {
        LONGLONG counts_per_sec;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&counts_per_sec));
        seconds_per_count_ = 1.0 / static_cast<double>(counts_per_sec);

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time_));
        base_time_ = this_time_;
        last_time_ = this_time_;
    }
    ~HighResolutionTimer() = default;
    HighResolutionTimer(const HighResolutionTimer&) = delete;
    HighResolutionTimer& operator=(const HighResolutionTimer&) = delete;
    HighResolutionTimer(HighResolutionTimer&&)noexcept = delete;
    HighResolutionTimer& operator=(HighResolutionTimer&&)noexcept = delete;

    float TimeStamp()const
    {
        if (stopped_)
        {
            return static_cast<float>(((stop_time_ - paused_time_) - base_time_) * seconds_per_count_);
        }
        else
        {
            return static_cast<float>(((this_time_ - paused_time_) - base_time_) * seconds_per_count_);
        }
    }

    float TimeInterval()const
    {
        return static_cast<float>(delta_time_);
    }

    float Reset()
    {
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time_));
        base_time_ = this_time_;
        last_time_ = this_time_;

        stop_time_ = 0;
        stopped_ = false;
    }

    void Start()
    {
        LONGLONG start_time;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_time));

        if (stopped_)
        {
            paused_time_ += (start_time - stop_time_);
            last_time_ = start_time;
            stop_time_ = 0;
            stopped_ = false;
        }
    }

    void Stop()
    {
        if (!stopped_)
        {
            QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&stop_time_));
            stopped_ = true;
        }
    }
    
    void Tick()
    {
        if (stopped_)
        {
            delta_time_ = 0.0;
            return;
        }

        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&this_time_));
        delta_time_ = (this_time_ - last_time_) * seconds_per_count_;

        last_time_ = this_time_;

        if (delta_time_ < 0.0)
        {
            delta_time_ = 0.0;
        }

    }

private:
    double seconds_per_count_{ 0.0 };
    double delta_time_{ 0.0 };

    LONGLONG base_time_{ 0LL };
    LONGLONG paused_time_{ 0LL };
    LONGLONG stop_time_{ 0LL };
    LONGLONG last_time_{ 0LL };
    LONGLONG this_time_{ 0LL };

    bool stopped_{ false };
};
#endif // PART2_HIGH_RESOLUTION_TIMER_H
#ifndef PART2_MISC_H
#define PART2_MISC_H

#include<windows.h>
#include<crtdbg.h>



#if defined(DEBUG)||defined(_DEBUG)
#define _ASSERT_EXPR_A(expr,msg) \
    (void)((!!(expr))||\
    (1!=_CrtDbgReport(_CRT_ASSERT,__FILE__,__LINE__,NULL,"%s",msg))||\
    (_CrtDbgBreak(),0))
#else
#define _ASSERT_EXPR_A(expr,expr_str)((void)0)
#endif

inline LPWSTR HRTrace(HRESULT hr)
{
    LPWSTR msg{ 0 };
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL
        , hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&msg), 0, NULL);
    return msg;
}

class BenchMark
{
    LARGE_INTEGER ticks_per_second_;
    LARGE_INTEGER start_ticks_;
    LARGE_INTEGER current_ticks_;

public:
    BenchMark()
    {
        QueryPerformanceFrequency(&ticks_per_second_);
        QueryPerformanceCounter(&start_ticks_);
        QueryPerformanceCounter(&current_ticks_);
    }
    ~BenchMark() = default;
    BenchMark(const BenchMark&) = delete;
    BenchMark& operator=(const BenchMark&) = delete;
    BenchMark(BenchMark&&)noexcept = delete;
    BenchMark& operator=(BenchMark&&)noexcept = delete;

    void Begin()
    {
        QueryPerformanceCounter(&start_ticks_);
    }
    float End()
    {
        QueryPerformanceCounter(&current_ticks_);
        return static_cast<float>(current_ticks_.QuadPart - start_ticks_.QuadPart) / static_cast<float>(ticks_per_second_.QuadPart);
    }
};
#endif // !PART2_MISC_H
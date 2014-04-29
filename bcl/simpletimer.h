/* This small code file adds a simple, platform independent, timer class
 * Code available at: http://stackoverflow.com/questions/483164/looking-for-benchmarking-code-snippet-c
 */

#ifndef _SIMPLETIMER_H

#define _SIMPLETIMER_H

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)

namespace win32 {
    #include <windows.h>
}

class timer
{
    win32::LARGE_INTEGER start_time_;
public:
    timer() { QueryPerformanceCounter( &start_time_ ); }
    void   restart() { QueryPerformanceCounter( &start_time_ ); }
    double elapsed() const
    {
        win32::LARGE_INTEGER end_time, frequency;
        QueryPerformanceCounter( &end_time );
        QueryPerformanceFrequency( &frequency );
        return double( end_time.QuadPart - start_time_.QuadPart )
            / frequency.QuadPart;
    }
};

#else

#include <ctime>

class timer
{
    clock_t _start_time;
public:
    timer() { _start_time = clock(); }
    void   restart() { _start_time = clock(); }
    double elapsed() const
    {
        return double(clock() - _start_time) / CLOCKS_PER_SEC;
    }
};

#endif

#endif

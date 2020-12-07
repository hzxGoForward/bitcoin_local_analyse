#include "time.h"


std::chrono::milliseconds GetTimeMillis(){
        system_clock::time_point now = system_clock::now();
    std::chrono::nanoseconds d = now.time_since_epoch();
    std::chrono::milliseconds millsec = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    return millsec;
}
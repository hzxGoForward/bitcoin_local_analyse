
#include "shutdown.h"

#include <atomic>

static std::atomic<bool> fRequestShutdown(false);

void StartShutdown()
{
    fRequestShutdown = true;
}
void AbortShutdown()
{
    fRequestShutdown = false;
}
bool ShutdownRequested()
{
    return fRequestShutdown;
}
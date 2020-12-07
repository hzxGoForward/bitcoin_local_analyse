#ifndef BLOCKCHAIN_SHUTDOWN_H
#define BLOCKCHAIN_SHUTDOWN_H

void StartShutdown();
void AbortShutdown();
bool ShutdownRequested();

#endif
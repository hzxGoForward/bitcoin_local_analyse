#ifndef BLOCKCHAIN_SYSTEM_HZX_H
#define BLOCKCHAIN_SYSTEM_HZX_H
// #include "boost/filesystem.hpp"
#include <string>

//void TryCreateDirectory(const std::string&);

template<typename... Args>
bool error(const char* fmt, const Args&... args);
#endif
#include "system_hzx.h"

#include <cstdio>
#include "tinyformat.h"


// void TryCreateDirectory(const std::string& path){
//     boost::filesystem::path boost_path(path);
//     if( !boost::filesystem::exists(boost_path) )
//         boost::filesystem::create_directory(boost_path);
// }

template<typename... Args>
bool error(const char* fmt, const Args&... args)
{
    printf("ERROR: %s\n", tfm::format(fmt, args...));
    return false;
}
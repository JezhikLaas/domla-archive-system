#ifndef UTILS_HXX
#define UTILS_HXX

#include <string>
#include <vector>

namespace Utils
{
    
std::vector<std::string> Split(const std::string& path, std::string::value_type delimiter = '/');

} // namespace Utils

#endif

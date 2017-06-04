#ifndef UTILS_HXX
#define UTILS_HXX

#include <string>
#include <vector>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace Utils
{
    
std::vector<std::string> Split(const std::string& path, std::string::value_type delimiter = '/');
std::int64_t Ticks(const boost::posix_time::ptime& when);
std::string NewId();

} // namespace Utils

#endif

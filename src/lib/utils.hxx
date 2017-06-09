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

class PeriodicTimer
{
private:
    struct Implementation;
    Implementation* Inner;
    
public:
    PeriodicTimer(const boost::posix_time::time_duration& interval, const std::function<void()>& callback);
    ~PeriodicTimer();
    PeriodicTimer(const PeriodicTimer&) = delete;
    void operator= (const PeriodicTimer&) = delete;
    
    void Start();
    void Cancel();
};

} // namespace Utils

#endif

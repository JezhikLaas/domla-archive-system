#include "utils.hxx"
#include <algorithm>
#include <mutex>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace std;
using namespace boost;
using namespace boost::posix_time;
using namespace boost::gregorian;

using StrictGuard = lock_guard<mutex>;

namespace
{

static mutex& AnonymousSync()
{
    static mutex Result;
    return Result;
}

static uuids::uuid Guid()
{
    static uuids::random_generator Result;
    StrictGuard Lock(AnonymousSync());
    return Result();
}

} // anonymous namespace

vector<string> Utils::Split(const string& path, string::value_type delimiter)
{
    vector<string> Source;
    split(Source, path, [&delimiter](string::value_type item) { return item == delimiter; }, boost::token_compress_on);
    auto& Victims = remove_if(Source.begin(), Source.end(), [](const string& item) { return item.empty(); });
    while (Victims != Source.end()) {
        Source.erase(Victims);
        Victims = remove_if(Source.begin(), Source.end(), [](const string& item) { return item.empty(); });
    }

    return Source;
}

int64_t Utils::Ticks(const ptime& when) 
{
    int64_t netEpochOffset = 441481536000000000LL;
    ptime ptimeEpoch(date(1400, 1, 1), time_duration(0, 0, 0));
    auto difference = when - ptimeEpoch;
    int64_t nano = difference.total_microseconds() * 10LL;

    return nano + netEpochOffset;
}

std::string Utils::NewId()
{
    return uuids::to_string(Guid());
}

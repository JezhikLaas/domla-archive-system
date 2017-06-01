#include "utils.hxx"
#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;
using namespace boost;

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

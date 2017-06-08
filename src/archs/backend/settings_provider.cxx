#include "settings_provider.hxx"
#include <boost/filesystem.hpp>

using namespace std;
using namespace Archive::Backend;

namespace FS = boost::filesystem;

const string SettingsProvider::FulltextFile() const
{
    return (FS::path(DataLocation()) / "fulltext" / "db").string();
}
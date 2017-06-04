#include "data_bucket.hxx"
#include "document_schema.hxx"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

using namespace std;
using namespace boost;
using namespace boost::filesystem;
using namespace Archive::Backend;

DataBucket::DataBucket(int id, const SettingsProvider& settings)
{
    auto Location = settings.DataLocation() != ":memory:"
                    ?
                    path(path(settings.DataLocation()) / (format("%03ddomla.archive") % id).str()).string()
                    :
                    settings.DataLocation()
                    ;
                    
    SQLite::Configuration Setup;
    Setup.BusyTimeout = 100;
    Setup.CacheSize = -20000;
    Setup.Path = Location;
    Setup.ForeignKeys = true;
    Setup.Journal = SQLite::Configuration::JournalMode::Wal;
    Setup.PageSize = 65536;
    Setup.MaxPageCount = 2147483646;
    
    Write = unique_ptr<SQLite::Connection>(new SQLite::Connection(Setup));
    Write->OpenAlways();
    DocumentSchema::Ensure(Writer());
    
    Setup.ReadOnly = true;

    Read = unique_ptr<SQLite::Connection>(new SQLite::Connection(Setup));
    Read->Open();
}

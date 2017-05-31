#define BOOST_TEST_MODULE "DocumentStorageModule"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "archs/backend/document_schema.hxx"
#include "archs/backend/settings_provider.hxx"
#include "archs/backend/document_storage.hxx"

using namespace std;
using namespace boost::filesystem;
using namespace Archive::Backend;

class Provider : public SettingsProvider
{
private:
    string DataLocation_;
    
public:
    Provider()
    : DataLocation_(path("./sdata").string())
    {
        remove_all(DataLocation_);
    }
    
    const string& DataLocation() const override
    {
        return DataLocation_;
    }
    
    int Backends() const override { return 10; }
};

BOOST_AUTO_TEST_CASE(StorageCreatesRequestedBucketsTest)
{
    Provider Settings;
    auto& Storage = DocumentStorage(Settings);
    
    BOOST_CHECK(is_regular_file("./sdata/001domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/002domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/003domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/004domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/005domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/006domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/007domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/008domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/009domla.archive"));
    BOOST_CHECK(is_regular_file("./sdata/010domla.archive"));
}

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

class SlimProvider : public SettingsProvider
{
private:
    string DataLocation_;
    
public:
    SlimProvider()
    : DataLocation_(":memory:")
    { }
    
    const string& DataLocation() const override
    {
        return DataLocation_;
    }

	int Backends() const override { return 1; }
};

class OneBucketProvider : public SettingsProvider
{
private:
    string DataLocation_;
    
public:
    OneBucketProvider()
    : DataLocation_(path("./odata").string())
    {
        remove_all(DataLocation_);
    }
    
    const string& DataLocation() const override
    {
        return DataLocation_;
    }
    
    int Backends() const override { return 1; }
};

BOOST_AUTO_TEST_CASE(Storage_Creates_Requested_Buckets_Test)
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

BOOST_AUTO_TEST_CASE(Save_New_Document)
{
    SlimProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    BOOST_CHECK(Header->Id.empty() == false);
}

BOOST_AUTO_TEST_CASE(Lock_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    Storage.Lock(Header->Id, "willi");
    
    Header = Storage.Load(Header->Id, "willi");

    BOOST_CHECK(Header->Locker.empty() == false);
}

BOOST_AUTO_TEST_CASE(Update_Saved_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content {
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
    };
    
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Storage.Save(Header, Content, "willi");

    const Access::BinaryData NewContent {
        '0','1','2','3','4','5','6','7','8','9',
        '9','8','7','6','5','4','3','2','1','0',
        '0','1','2','3','4','5','6','7','8','9',
    };

    Storage.Save(Header, NewContent, "willi");
    
    auto Loaded = Storage.FindById(Header->Id);
    BOOST_CHECK(Loaded->Revision == 2);
}

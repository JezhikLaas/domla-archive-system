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

BOOST_AUTO_TEST_CASE(Unlock_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    Storage.Lock(Header->Id, "willi");
    Storage.Unlock(Header->Id, "willi");
    
    Header = Storage.Load(Header->Id, "willi");

    BOOST_CHECK(Header->Locker.empty());
}

BOOST_AUTO_TEST_CASE(Find_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    
    Storage.Save(Header, Content, "willi");
    
    auto Check = Storage.Find("/one", "test.xxx");

    BOOST_CHECK(Check->Id.empty() == false);
}

BOOST_AUTO_TEST_CASE(Find_Document_By_Title)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    
    auto Check = Storage.FindTitle("/one", "Testing");

    BOOST_CHECK(Check.empty() == false);
    BOOST_CHECK(Check[0]->Id.empty() == false);
}

BOOST_AUTO_TEST_CASE(Move_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    Storage.Move(Header->Id, "/one", "/two", "willi");
    
    auto Loaded = Storage.FindById(Header->Id);

    BOOST_CHECK(Loaded->FolderPath == "/two");
}

BOOST_AUTO_TEST_CASE(Folder_Infos_Follow_Move)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    auto Infos = Storage.FoldersForPath("/");
    BOOST_CHECK(Infos.size() == 1);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    
    Infos = Storage.FoldersForPath("/");
    BOOST_CHECK(Infos.size() == 2);
    BOOST_CHECK(Infos[1].Name == "/one");
    
    Storage.Move(Header->Id, "/one", "/two", "willi");
    
    Infos = Storage.FoldersForPath("/");
    BOOST_CHECK(Infos.size() == 2);
    BOOST_CHECK(Infos[1].Name == "/two");
}

BOOST_AUTO_TEST_CASE(Copy_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    Storage.Copy(Header->Id, "/one", "/two", "willi");
    
    auto Check = Storage.FindTitle("/two", "Testing");

    BOOST_CHECK(Check.empty() == false);
    BOOST_CHECK(Check[0]->Id.empty() == false);
    BOOST_CHECK(Check[0]->Id != Header->Id);
}

BOOST_AUTO_TEST_CASE(Link_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    Storage.Link(Header->Id, "/one", "/two", "willi");
    
    auto Check = Storage.FindTitle("/two", "Testing");

    BOOST_CHECK(Check.empty() == false);
    BOOST_CHECK(Check[0]->Id.empty() == false);
    BOOST_CHECK(Check[0]->Id == Header->Id);
}

BOOST_AUTO_TEST_CASE(Associate_Info_To_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    Storage.Associate(Header->Id, "/one", "AnId", "AType", "willi");
    
    auto Loaded = Storage.FindById(Header->Id);
    BOOST_CHECK(Loaded->AssociatedItem == "AnId");
    BOOST_CHECK(Loaded->AssociatedClass == "AType");
}

BOOST_AUTO_TEST_CASE(Folder_Names_For_Linked_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    Header->FolderPath = "/one";
    Header->Name = "test.xxx";
    Header->Display = "Testing";
    
    Storage.Save(Header, Content, "willi");
    Storage.Link(Header->Id, "/one", "/two", "willi");
    
    auto Check = Storage.FoldersOf(Header->Id);

    BOOST_CHECK(Check.size() == 2);
}

BOOST_AUTO_TEST_CASE(Delete_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    Storage.Delete(Header->Id, "willi");
    
    Header = Storage.Load(Header->Id, "willi");

    BOOST_CHECK(Header->Deleted);
}

BOOST_AUTO_TEST_CASE(Undelete_Document)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    Storage.Delete(Header->Id, "willi");
    Storage.Undelete(Header->Id, "willi");
    
    Header = Storage.Load(Header->Id, "willi");

    BOOST_CHECK(Header->Deleted == false);
}

BOOST_AUTO_TEST_CASE(Undelete_Documents)
{
    OneBucketProvider Settings;
    DocumentStorage Storage(Settings);
    
    const Access::BinaryData Content { 3, 2, 1, 0, 1, 2, 3 };
    Access::DocumentDataPtr Header = new Access::DocumentData();
    
    Storage.Save(Header, Content, "willi");
    Storage.Delete(Header->Id, "willi");
    Storage.Undelete({ Header->Id }, "willi");
    
    Header = Storage.Load(Header->Id, "willi");

    BOOST_CHECK(Header->Deleted == false);
}

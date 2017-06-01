#define BOOST_TEST_MODULE "DocumentSchemaModule"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include "archs/backend/document_schema.hxx"
#include "archs/backend/settings_provider.hxx"
#include "archs/backend/data_bucket.hxx"

using namespace std;
using namespace boost::filesystem;
using namespace Archive::Backend;

BOOST_AUTO_TEST_CASE(SchemaCanBeCreatedTest)
{
  SQLite::Configuration Setup;
  Setup.Path = ":memory:";
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  DocumentSchema::Ensure(Con);
}

class Provider : public SettingsProvider
{
private:
    string DataLocation_;
    
public:
    Provider()
    : DataLocation_(path("./tdata").string())
    {
        remove_all(DataLocation_);
        create_directory(DataLocation_);
    }
    
    const string& DataLocation() const override
    {
        return DataLocation_;
    }
};

BOOST_AUTO_TEST_CASE(Data_Bucket_Test)
{
    Provider Test;
    DataBucket Bucket(1, Test);
    
    BOOST_CHECK(is_regular_file("./tdata/001domla.archive"));
}

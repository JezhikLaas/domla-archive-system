#define BOOST_TEST_MODULE "DocumentSchemaModule"

#include <boost/test/unit_test.hpp>
#include "archs/backend/document_schema.hxx"
#include "archs/backend/document_schema.hxx"

using namespace std;
using namespace Archive::Backend;

BOOST_AUTO_TEST_CASE(SchemaCanBeCreatedTest)
{
  SQLite::Configuration Setup;
  Setup.Path = ":memory:";
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  DocumentSchema::Ensure(Con);
}

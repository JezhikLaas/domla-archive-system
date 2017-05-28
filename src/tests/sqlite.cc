#define BOOST_TEST_MODULE "SQLiteCxxModule"

#include <iostream>
#include <boost/test/unit_test.hpp>
#include "archs/backend/sqlite.hxx"

using namespace Archive::Backend::SQLite;

BOOST_AUTO_TEST_CASE(OpenConnectionInMemory)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Target(Setup);
  Target.OpenNew();

  BOOST_CHECK(Target.IsOpen());
}

BOOST_AUTO_TEST_CASE(TablesCanBeCreatedInMemory)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
}

BOOST_AUTO_TEST_CASE(ErrorsAreProperlyReported)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Reported = false;
  try {
    auto Target = Con.Create("CREATE TABLE a(one INT, two INT");
    Target->Execute();
  }
  catch (sqlite_exception& error) {
      std::cerr << "Error: " << error.what() << std::endl
                << "at " << error.line() << " in " << error.where() << std::endl;
      Reported = true;
  }

  BOOST_CHECK(Reported);
}

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
      std::cerr << "Harmless, expeced message follows" << std::endl;
      std::cerr << "Error: " << error.what() << std::endl
                << "at " << error.line() << " in " << error.where() << std::endl;
      Reported = true;
  }

  BOOST_CHECK(Reported);
}

BOOST_AUTO_TEST_CASE(ExecuteScalarWorks)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("SELECT 1 + 1");
  auto Result = Target->ExecuteScalar<int>();

  BOOST_CHECK(Result == 2);
}

BOOST_AUTO_TEST_CASE(ParametersDetected)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  int Count = 0;
  for (auto p : Target->Parameters()) ++Count;

  BOOST_CHECK(Count == 2);
}

BOOST_AUTO_TEST_CASE(ParametersAccessibleByName)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");

  BOOST_CHECK(Target->Parameters()["one"].IsEmpty());
  BOOST_CHECK(Target->Parameters()["two"].IsEmpty());
}

BOOST_AUTO_TEST_CASE(ParametersNameAccessCaseInsensitive)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");

  BOOST_CHECK(Target->Parameters()["OnE"].IsEmpty());
}

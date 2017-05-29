#define BOOST_TEST_MODULE "SQLiteCxxModule"

#include <array>
#include <iostream>
#include <vector>
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

BOOST_AUTO_TEST_CASE(InsertingValuesIntoMemory)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target->Parameters()["one"].SetValue(1);
  Target->Parameters()["two"].SetValue(1);
  Target->Execute();
  
  Target = Con.Create("SELECT COUNT(*) FROM a");
  auto Result = Target->ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
}

BOOST_AUTO_TEST_CASE(OpeningResultSet)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target->Parameters()["one"].SetValue(1);
  Target->Parameters()["two"].SetValue(1);
  Target->Execute();
  
  Target = Con.Create("SELECT * FROM a");
  auto Result = Target->Open();

  BOOST_CHECK(Result.Fields() == 2);
}

BOOST_AUTO_TEST_CASE(CountingRowsInResultSet)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target->Parameters()["one"].SetValue(1);
  Target->Parameters()["two"].SetValue(1);
  Target->Execute();
  
  auto Rows = 0;
  Target = Con.Create("SELECT * FROM a");
  auto Result = Target->Open();
  
  for (const ResultRow& Row : Result) ++Rows;

  BOOST_CHECK(Rows == 1);
}

BOOST_AUTO_TEST_CASE(ReadValuesFromResultRow)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target->Parameters()["one"].SetValue(1);
  Target->Parameters()["two"].SetValue(2);
  Target->Execute();
  
  auto One = 0;
  auto Two = 0;
  
  Target = Con.Create("SELECT * FROM a");
  auto Result = Target->Open();
  
  for (const ResultRow& Row : Result) {
      One = Row.Get<int>("One");
      Two = Row.Get<int>("Two");
  }

  BOOST_CHECK(One == 1);
  BOOST_CHECK(Two == 2);
}

BOOST_AUTO_TEST_CASE(WriteAndReadBinaryData)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
  
  auto Target = Con.Create("CREATE TABLE a(one BLOB NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target->Parameters()["one"].SetValue(Buffer);
  Target->Execute();
  
  Target = Con.Create("SELECT one FROM a");
  auto Result = Target->Open();
  std::vector<unsigned char> Check;
  
  for (const ResultRow& Row : Result) {
      Check = Row.GetBlob("One");
  }

  BOOST_CHECK(Buffer.size() == Check.size());
  BOOST_CHECK(std::equal(Buffer.begin(), Buffer.end(), Check.begin(), Check.end()));
}

BOOST_AUTO_TEST_CASE(WriteAndReadText)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
  
  auto Target = Con.Create("CREATE TABLE a(one TEXT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target->Parameters()["one"].SetValue(std::string("MyTestValue"));
  Target->Execute();
  
  Target = Con.Create("SELECT one FROM a");
  auto Result = Target->Open();
  std::string Check;
  
  for (const ResultRow& Row : Result) {
      Check = Row.Get<std::string>("One");
  }

  BOOST_CHECK(Check == "MyTestValue");
}

BOOST_AUTO_TEST_CASE(WriteAndReadTextFromCharacterPointer)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
  
  auto Target = Con.Create("CREATE TABLE a(one TEXT NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target->Parameters()["one"].SetValue("MyTestValue");
  Target->Execute();
  
  Target = Con.Create("SELECT one FROM a");
  auto Result = Target->Open();
  std::string Check;
  
  for (const ResultRow& Row : Result) {
      Check = Row.Get<std::string>("One");
  }

  BOOST_CHECK(Check == "MyTestValue");
}

BOOST_AUTO_TEST_CASE(WriteAndReadBinaryDataFromPointer)
{
  Configuration Setup {
      ":memory:"
  };
  
  Connection Con(Setup);
  Con.OpenNew();
  
  unsigned char Buffer[] = { 1, 2, 3, 0, 3, 2, 1 };
  
  auto Target = Con.Create("CREATE TABLE a(one BLOB NOT NULL)");
  Target->Execute();
  
  Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target->Parameters()["one"].SetRawValue(Buffer, sizeof(Buffer));
  Target->Execute();
  
  Target = Con.Create("SELECT one FROM a");
  auto Result = Target->Open();
  std::vector<unsigned char> Check;
  
  for (const ResultRow& Row : Result) {
      Check = Row.GetBlob("One");
  }

  BOOST_CHECK(Check.size() == sizeof(Buffer));
  BOOST_CHECK(Check[0] == 1);
  BOOST_CHECK(Check[1] == 2);
  BOOST_CHECK(Check[2] == 3);
  BOOST_CHECK(Check[3] == 0);
  BOOST_CHECK(Check[4] == 3);
  BOOST_CHECK(Check[5] == 2);
  BOOST_CHECK(Check[6] == 1);
}

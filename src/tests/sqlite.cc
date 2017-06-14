#define BOOST_TEST_MODULE "SQLiteCxxModule"

#include <array>
#include <iostream>
#include <vector>
#include <boost/test/unit_test.hpp>
#include "archs/backend/sqlite.hxx"

using namespace Archive::Backend::SQLite;

BOOST_AUTO_TEST_CASE(Open_Connection_In_Memory)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Target(Setup);
  Target.OpenNew();

  BOOST_CHECK(Target.IsOpen());
}

BOOST_AUTO_TEST_CASE(Tables_Can_Be_Created_In_Memory)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target.Execute();
}

BOOST_AUTO_TEST_CASE(Errors_Are_Properly_Reported)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Reported = false;
  try {
    auto Target = Con.Create("CREATE TABLE a(one INT, two INT");
    Target.Execute();
  }
  catch (sqlite_exception& error) {
      std::cerr << "Harmless, expeced message follows" << std::endl;
      std::cerr << "Error: " << error.what() << std::endl
                << "at " << error.line() << " in " << error.where() << std::endl;
      Reported = true;
  }

  BOOST_CHECK(Reported);
}

BOOST_AUTO_TEST_CASE(Execute_Scalar_Works)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("SELECT 1 + 1");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 2);
}

BOOST_AUTO_TEST_CASE(Parameters_Detected)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target.Execute();
  }
  
  int Count = 0;
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  for (auto p : Target.Parameters()) ++Count;
  }
  
  BOOST_CHECK(Count == 2);
}

BOOST_AUTO_TEST_CASE(Parameters_Accessible_By_Name)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");

  BOOST_CHECK(Target.Parameters()["one"].IsEmpty());
  BOOST_CHECK(Target.Parameters()["two"].IsEmpty());
  }
}

BOOST_AUTO_TEST_CASE(Parameters_Name_Access_Case_Insensitive)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT, two INT)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  BOOST_CHECK(Target.Parameters()["OnE"].IsEmpty());
  }
}

BOOST_AUTO_TEST_CASE(Inserting_Values_Into_Memory)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("SELECT COUNT(*) FROM a");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }
}

BOOST_AUTO_TEST_CASE(Opening_Result_Set)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("SELECT * FROM a");
  auto Result = Target.Open();
  
  BOOST_CHECK(Result.Fields() == 2);
  }
}

BOOST_AUTO_TEST_CASE(Counting_Rows_In_Result_Set)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }
  
  auto Rows = 0;
  {
  auto Target = Con.Create("SELECT * FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) ++Rows;
  }
  
  BOOST_CHECK(Rows == 1);
}

BOOST_AUTO_TEST_CASE(Read_Values_From_Result_Row)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(2);
  Target.Execute();
  }
  
  auto One = 0;
  auto Two = 0;
  
  {
  auto Target = Con.Create("SELECT * FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) {
      One = Row.Get<int>("One");
      Two = Row.Get<int>("Two");
  }
  }
  BOOST_CHECK(One == 1);
  BOOST_CHECK(Two == 2);
}

BOOST_AUTO_TEST_CASE(Write_And_Read_Binary_Data)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
  
  { 
  auto Target = Con.Create("CREATE TABLE a(one BLOB NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target.Parameters()["one"].SetValue(Buffer);
  Target.Execute();
  }
  
  std::vector<unsigned char> Check;
  {
  auto Target = Con.Create("SELECT one FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) {
      Check = Row.GetBlob("One");
  }
  }
  BOOST_CHECK(Buffer.size() == Check.size());
  BOOST_CHECK(std::equal(Buffer.begin(), Buffer.end(), Check.begin(), Check.end()));
}

BOOST_AUTO_TEST_CASE(Write_And_Read_Text)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
 
  { 
  auto Target = Con.Create("CREATE TABLE a(one TEXT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target.Parameters()["one"].SetValue(std::string("MyTestValue"));
  Target.Execute();
  }
  
  std::string Check;
  {
  auto Target = Con.Create("SELECT one FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) {
      Check = Row.Get<std::string>("One");
  }
  }
  
  BOOST_CHECK(Check == "MyTestValue");
}

BOOST_AUTO_TEST_CASE(Write_And_Read_Text_From_Character_Pointer)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  std::vector<unsigned char> Buffer { 1, 2, 3, 0, 3, 2, 1 };
  
  {
  auto Target = Con.Create("CREATE TABLE a(one TEXT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target.Parameters()["one"].SetValue("MyTestValue");
  Target.Execute();
  }
  
  std::string Check;
  {
  auto Target = Con.Create("SELECT one FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) {
      Check = Row.Get<std::string>("One");
  }
  }
  BOOST_CHECK(Check == "MyTestValue");
}

BOOST_AUTO_TEST_CASE(Write_And_Read_Binary_Data_From_Pointer)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  unsigned char Buffer[] = { 1, 2, 3, 0, 3, 2, 1 };
  
  {
  auto Target = Con.Create("CREATE TABLE a(one BLOB NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one) VALUES (:one)");
  Target.Parameters()["one"].SetRawValue(Buffer, sizeof(Buffer));
  Target.Execute();
  }
  
  std::vector<unsigned char> Check;
  {
  auto Target = Con.Create("SELECT one FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) {
      Check = Row.GetBlob("One");
  }
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

BOOST_AUTO_TEST_CASE(Rollback_Reverts_Changes)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }

  {
  auto Transaction = Con.Begin();
  auto Target = Con.Create("DELETE FROM a");
  Target.Execute();
  Transaction.Rollback();
  }
  
  {
  auto Target = Con.Create("SELECT COUNT(*) FROM a");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }
}

BOOST_AUTO_TEST_CASE(Commit_Writes_Changes)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }

  {
  auto Transaction = Con.Begin();
  auto Target = Con.Create("DELETE FROM a");
  Target.Execute();
  Transaction.Commit();
  }

  {
  auto Target = Con.Create("SELECT COUNT(*) FROM a");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 0);
  }
}

BOOST_AUTO_TEST_CASE(Scoped_Rollback_Reverts_Changes)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  }

  {
  auto Transaction = Con.Begin();
  auto Target = Con.Create("DELETE FROM a");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("SELECT COUNT(*) FROM a");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }
}

BOOST_AUTO_TEST_CASE(Reusing_Prepared_Statement)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE a(one INT NOT NULL, two INT NOT NULL)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO a (one, two) VALUES (:one, :two)");
  Target.Parameters()["one"].SetValue(1);
  Target.Parameters()["two"].SetValue(1);
  Target.Execute();
  
  Target.Parameters()["one"].SetValue(2);
  Target.Parameters()["two"].SetValue(2);
  Target.Execute();
  }
  
  auto Rows = 0;
  {
  auto Target = Con.Create("SELECT * FROM a");
  auto Result = Target.Open();
  
  for (const ResultRow& Row : Result) ++Rows;
  }
  
  BOOST_CHECK(Rows == 2);
}

BOOST_AUTO_TEST_CASE(Create_Free_Command)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.CreateFree("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
}

BOOST_AUTO_TEST_CASE(Non_Default_Configuration)
{
  Configuration Setup;
  Setup.Path = "";
  Setup.ForeignKeys = true;
  Setup.PageSize = 65536;
  Setup.Journal = Configuration::JournalMode::Wal;
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.CreateFree("CREATE TABLE a(one INT, two INT)");
  Target->Execute();
}

BOOST_AUTO_TEST_CASE(PartsCount_Detects_One_Part)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("SELECT PARTSCOUNT('/one/', '/')");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
}

BOOST_AUTO_TEST_CASE(PartsCount_Detects_Two_Parts)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("SELECT PARTSCOUNT('/one/two', '/')");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 2);
}

BOOST_AUTO_TEST_CASE(PartsCount_Tolerates_No_Parts)
{
  Configuration Setup;
  Setup.Path = ":memory:";
  
  Connection Con(Setup);
  Con.OpenNew();
  
  auto Target = Con.Create("SELECT PARTSCOUNT('//', '/')");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 0);
}

#define BOOST_TEST_MODULE "TransformerBackendModule"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include "archs/backend/transformer.hxx"

using namespace std;
using namespace Archive::Backend;

class Test : public Persistable
{
friend class TestTransformer;

private:
    int Amount_;
    string Value_;
    
public:
    static string TypeId_;
    const string& TypeId() const override { return Test::TypeId_; }
    int Amount() const { return Amount_; }
    string Value() const { return Value_; }
    void SetAmount(int value) { Amount_ = value; }
    void SetValue(const string& value) { Value_ = value; }
};

string Test::TypeId_ = "Test";

class TestTransformer : Transformer<Test>
{
protected:
    string TableName() const override
    {
        return "test";
    }
    
    vector<string> Fields() const override
    {
        return AppendFields({ "amount", "value" });
    }
    
    void Materialize(const SQLite::ResultRow& data, Test& item) const override
    {
        item.SetAmount(data.Get<int>("amount"));
        item.SetValue(data.Get<string>("value"));
    }
    
    void Serialize(const SQLite::ParameterSet& target, const Test& item) const override
    {
        target["id"].SetValue(item.Id());
        target["amount"].SetValue(item.Amount());
        target["value"].SetValue(item.Value());
    }
};

BOOST_AUTO_TEST_CASE(InsertQueueTest)
{
  SQLite::Configuration Setup {
      ":memory:"
  };
  
  TransformerRegistry::Register(Test::TypeId_, []() { return (TransformerBase*)new TestTransformer(); });
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE test(id TEXT NOT NULL, amount INT, value TEXT)");
  Target.Execute();
  }
  
  Test Item;
  Item.SetId("1");
  Item.SetAmount(10);
  Item.SetValue("Box");
  
  TransformerQueue Queue(&Con);
  Queue.Insert(Item);
  Queue.Flush();

  {
  auto Target = Con.Create("SELECT COUNT(*) FROM test");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }
}

BOOST_AUTO_TEST_CASE(UpdateQueueTest)
{
  SQLite::Configuration Setup {
      ":memory:"
  };
  
  TransformerRegistry::Register(Test::TypeId_, []() { return (TransformerBase*)new TestTransformer(); });
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE test(id TEXT NOT NULL, amount INT, value TEXT)");
  Target.Execute();
  }
  
  {
  auto Target = Con.Create("INSERT INTO test (id, amount, value) VALUES (:id, :amount, :value)");
  Target.Parameters()["id"].SetValue("1");
  Target.Parameters()["amount"].SetValue(5);
  Target.Parameters()["value"].SetValue("Box");
  Target.Execute();
  }
  
  Test Item;
  Item.SetId("1");
  Item.SetAmount(10);
  Item.SetValue("Box");
  
  TransformerQueue Queue(&Con);
  Queue.Update(Item);
  Queue.Flush();

  {
  auto Target = Con.Create("SELECT COUNT(*) FROM test");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }

  {
  auto Target = Con.Create("SELECT amount FROM test");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 10);
  }
}

BOOST_AUTO_TEST_CASE(DeleteQueueTest)
{
  SQLite::Configuration Setup {
      ":memory:"
  };
  
  TransformerRegistry::Register(Test::TypeId_, []() { return (TransformerBase*)new TestTransformer(); });
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE test(id TEXT NOT NULL, amount INT, value TEXT)");
  Target.Execute();
  }

  {  
  Test Item;
  Item.SetId("1");
  Item.SetAmount(10);
  Item.SetValue("Box");
  
  TransformerQueue Queue(&Con);
  Queue.Insert(Item);
  Queue.Flush();
  }
  {
  auto Target = Con.Create("SELECT COUNT(*) FROM test");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 1);
  }

  {  
  Test Item;
  Item.SetId("1");
  
  TransformerQueue Queue(&Con);
  Queue.Delete(Item);
  Queue.Flush();
  }
  {
  auto Target = Con.Create("SELECT COUNT(*) FROM test");
  auto Result = Target.ExecuteScalar<int>();

  BOOST_CHECK(Result == 0);
  }
}

BOOST_AUTO_TEST_CASE(LoadFromTransformerTest)
{
  SQLite::Configuration Setup {
      ":memory:"
  };
  
  TransformerRegistry::Register(Test::TypeId_, []() { return (TransformerBase*)new TestTransformer(); });
  
  SQLite::Connection Con(Setup);
  Con.OpenNew();
  
  {
  auto Target = Con.Create("CREATE TABLE test(id TEXT NOT NULL, amount INT, value TEXT)");
  Target.Execute();
  }

  {  
  Test Item;
  Item.SetId("1");
  Item.SetAmount(10);
  Item.SetValue("Box");
  
  TransformerQueue Queue(&Con);
  Queue.Insert(Item);
  Queue.Flush();
  }

  {  
  unique_ptr<TransformerBase> Transformer(TransformerRegistry::Fetch(Test::TypeId_));
  Test Item;
  Item.SetId("1");
  Transformer->Connect(&Con);
  BOOST_CHECK(Transformer->Load(Item));
  BOOST_CHECK(Item.Amount() == 10);
  BOOST_CHECK(Item.Value() == "Box");
  }
}

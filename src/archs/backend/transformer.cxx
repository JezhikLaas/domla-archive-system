#include <sstream>
#include <boost/format.hpp>
#include "transformer.hxx"
#include "Common.h"

using namespace std;
using namespace boost;
using namespace Archive::Backend;

unordered_map<string, TransformerFactory> TransformerRegistry::Transformers_;

string TransformerBase::StandardInsert() const
{
    stringstream Buffer;
    Buffer << "INSERT INTO " << TableName() << " (";
    auto First = true;
    for (auto& Field : Fields()) {
        if (First == false) { Buffer << ", "; }
        First = false;
        Buffer << Field;
    }
    
    Buffer << ") VALUES (";
    
    First = true;
    for (auto& Field : Fields()) {
        if (First == false) { Buffer << ", "; }
        First = false;
        Buffer << ':' << Field;
    }
    
    Buffer << ')';
    return Buffer.str();
}

string TransformerBase::StandardUpdate() const
{
    stringstream Buffer;
    Buffer << "UPDATE " << TableName() << " SET ";
    auto First = true;
    for (auto& Field : Fields()) {
        if (Field == "id") continue;
        if (First == false) { Buffer << ", "; }
        First = false;
        Buffer << Field << " = " << ':' << Field;
    }
    
    Buffer << " WHERE id = :id";
    
    return Buffer.str();
}

string TransformerBase::StandardDelete() const
{
    stringstream Buffer;
    Buffer << "DELETE FROM " << TableName() << " WHERE id = :id ";
    
    return Buffer.str();
}

string TransformerBase::StandardSelect() const
{
    stringstream Buffer;
    Buffer << "SELECT ";
    auto First = true;
    for (auto& Field : Fields()) {
        if (First == false) { Buffer << ", "; }
        First = false;
        Buffer << Field;
    }
    
    Buffer << " FROM " << TableName() << " WHERE id = :id";
    
    return Buffer.str();
}

void TransformerBase::Delete(const Common::Persistable& item)
{
    if (DeleteCommand_ == false) DeleteCommand_ = Connection_->CreateFree(Deleter());
    ToDelete(item);
    DeleteCommand_->Execute();
}

void TransformerBase::Insert(const Common::Persistable& item)
{
    if (InsertCommand_ == false) InsertCommand_ = Connection_->CreateFree(Inserter());
    ToInsert(item);
    InsertCommand_->Execute();
}

void TransformerBase::Update(const Common::Persistable& item)
{
    if (UpdateCommand_ == false) UpdateCommand_ = Connection_->CreateFree(Updater());
    ToUpdate(item);
    UpdateCommand_->Execute();
}

bool TransformerBase::Load(Common::Persistable& item)
{
    if (SelectCommand_ == false) SelectCommand_ = Connection_->CreateFree(Selector());
    SelectCommand_->Parameters()["id"].SetValue(item.Id);
    auto& Result = SelectCommand_->Open();
    if (Result.HasData()) FromData(*Result.begin(), item);
    return Result.HasData();
}

bool TransformerBase::Load(const SQLite::ResultSet& data, Common::Persistable& item) const
{
    if (data.HasData()) FromData(*(data.begin()), item);
    return data.HasData();
}

void TransformerBase::Load(const SQLite::ResultRow& data, Common::Persistable& item) const
{
    FromData(data, item);
}

vector<string> TransformerBase::AppendFields(const vector<string>& fields) const
{
    auto& Result = TransformerBase::Fields();
    back_insert_iterator<vector<string>> Appender(Result);
    copy(fields.begin(), fields.end(), Appender);
    
    return Result;
}

void TransformerBase::Connect(const SQLite::Connection* connection)
{
    Connection_ = connection;
}

void TransformerBase::Reset()
{
    Connection_ = nullptr;
    InsertCommand_.reset();
    DeleteCommand_.reset();
    UpdateCommand_.reset();
    SelectCommand_.reset();
}

void TransformerBase::ToDelete(const Common::Persistable& item) const
{
    DeleteCommand_->Parameters()["id"].SetValue(item.Id);
};

TransformerQueue::TransformerQueue(SQLite::Connection* connection)
: Connection_(connection)
{ }

void TransformerQueue::Flush()
{
    auto Scope = Connection_->Begin();
    unordered_map<string, unique_ptr<TransformerBase>> Cache;
    
    for (auto& Delete : Deletes_) {
        auto Key = Delete->ice_id();
        if (Cache.find(Key) == Cache.end()) {
            Cache.insert({ Key, unique_ptr<TransformerBase>(TransformerRegistry::Fetch(Key)) });
            Cache[Key]->Connect(Connection_);
        }
        Cache[Key]->Delete(*Delete);
    }
    
    for (auto& Insert : Inserts_) {
        auto Key = Insert->ice_id();
        if (Cache.find(Key) == Cache.end()) {
            Cache.insert({ Key, unique_ptr<TransformerBase>(TransformerRegistry::Fetch(Key)) });
            Cache[Key]->Connect(Connection_);
        }
        Cache[Key]->Insert(*Insert);
    }
    
    for (auto& Update : Updates_) {
        auto Key = Update->ice_id();
        if (Cache.find(Key) == Cache.end()) {
            Cache.insert({ Key, unique_ptr<TransformerBase>(TransformerRegistry::Fetch(Key)) });
            Cache[Key]->Connect(Connection_);
        }
        Cache[Key]->Update(*Update);
    }
    
    Deletes_.clear();
    Updates_.clear();
    Inserts_.clear();
    
    Scope.Commit();
}

void TransformerRegistry::Register(const string& id, TransformerFactory transformer)
{
    Transformers_.insert({ id, transformer });
}

TransformerBase* TransformerRegistry::Fetch(const string& id)
{
    auto Factory = Transformers_[id];
    return Factory();
}

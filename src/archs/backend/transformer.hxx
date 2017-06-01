#ifndef TRANSFORMER_HXX
#define TRANSFORMER_HXX

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "sqlite.hxx"
//#include "Common.h"

namespace Common
{
    class Persistable;
}

namespace Archive
{
namespace Backend
{

class TransformerBase
{
protected:
    const SQLite::Connection* Connection_;
    
    std::shared_ptr<SQLite::Command> InsertCommand_;
    std::shared_ptr<SQLite::Command> UpdateCommand_;
    std::shared_ptr<SQLite::Command> DeleteCommand_;
    std::shared_ptr<SQLite::Command> SelectCommand_;
    
    std::string StandardInsert() const;
    std::string StandardUpdate() const;
    std::string StandardDelete() const;
    std::string StandardSelect() const;
    std::vector<std::string> AppendFields(const std::vector<std::string>& fields) const;
    
    virtual std::string Inserter() const { return StandardInsert(); }
    virtual std::string Updater() const { return StandardUpdate(); }
    virtual std::string Deleter() const { return StandardDelete(); }
    virtual std::string Selector() const { return StandardSelect(); }
    virtual void ToInsert(const Common::Persistable& item) const = 0;
    virtual void ToUpdate(const Common::Persistable& item) const = 0;
    virtual void ToDelete(const Common::Persistable& item) const;
    virtual void FromData(const SQLite::ResultRow& data, Common::Persistable& item) const = 0;
    virtual std::string TableName() const = 0;
    virtual std::vector<std::string> Fields() const { return { "id" }; }

public:
    void Delete(const Common::Persistable& item);
    void Insert(const Common::Persistable& item);
    void Update(const Common::Persistable& item);
    bool Load(Common::Persistable& item);
    void Connect(const SQLite::Connection* connection);

public:
    void Reset();
};

template <typename T>
class Transformer : public TransformerBase
{
protected:
    virtual void Serialize(const SQLite::ParameterSet& target, const T& item) const = 0;
    virtual void Materialize(const SQLite::ResultRow& data, T& item) const = 0;
    
    void ToInsert(const Common::Persistable& item) const override { Serialize(InsertCommand_->Parameters(), (const T&) item); }
    void ToUpdate(const Common::Persistable& item) const override { Serialize(UpdateCommand_->Parameters(), (const T&) item); }
    void FromData(const SQLite::ResultRow& data, Common::Persistable& item) const override { Materialize(data, (T&) item); }
};

class TransformerQueue
{
private:
    SQLite::Connection* Connection_;
    std::vector<const Common::Persistable*> Deletes_;
    std::vector<const Common::Persistable*> Updates_;
    std::vector<const Common::Persistable*> Inserts_;

public:
    TransformerQueue(SQLite::Connection* Connection_);
    
    void Insert(const Common::Persistable& item)
    {
        Inserts_.push_back(&item);
    }

    void Update(const Common::Persistable& item)
    {
        Updates_.push_back(&item);
    }

    void Delete(const Common::Persistable& item)
    {
        Deletes_.push_back(&item);
    }
    
    void Flush();
};

typedef std::function<TransformerBase*()> TransformerFactory;

class TransformerRegistry
{
private:
    static std::unordered_map<std::string, TransformerFactory> Transformers_;

public:
    static void Register(const std::string& id, TransformerFactory transformer);
    static TransformerBase* Fetch(const std::string& id);
};

} // namespace Backend
} // namespace Archive

#endif TRANSFORMER_HXX

#ifndef TRANSFORMER_HXX
#define TRANSFORMER_HXX

#include <algorithm>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "sqlite.hxx"

namespace Archive
{
namespace Backend
{

class Persistable
{
protected:
    std::string Id_;
    
public:
    const std::string& Id() const { return Id_; }
};

class TransformerBase
{
private:
    SQLite::Command* InsertCommand;
    SQLite::Command* UpdateCommand;
    SQLite::Command* DeleteCommand;
    SQLite::Command* SelectCommand;
    
protected:
    virtual const std::string& Inserter() const = 0;
    virtual const std::string& Updater() const = 0;
    virtual const std::string& Deleter() const = 0;
    virtual const std::string& Selector() const = 0;

protected:
    void Delete(const Persistable& item);
    void Insert(const Persistable& item);
    void Update(const Persistable& item);
    void Connect(const SQLite::Connection& connection);

public:
    virtual const std::type_info& PersistingType() const = 0;
};

template <typename T>
class Transformer : public TransformerBase
{
public:
    const std::type_info& PersistingType() const
    {
        return typeid(T);
    }
};

class TransformerQueue
{
private:
    std::vector<Persistable> Deletes_;
    std::vector<Persistable> Updates_;
    std::vector<Persistable> Inserts_;

public:
    void Insert(const Persistable& item)
    {
        Inserts_.push_back(item);
    }

    void Update(const Persistable& item)
    {
        Updates_.push_back(item);
    }

    void Delete(const Persistable& item)
    {
        Deletes_.push_back(item);
    }
    
    void Flush();
};

class TransformerRegistry
{
private:
    static std::unordered_map<std::type_index, TransformerBase*> Transformers_;

public:
    static void Register(const std::type_info& id, TransformerBase* transformer);
    static TransformerBase& Fetch(const std::type_info& id);
};

} // namespace Backend
} // namespace Archive

#endif TRANSFORMER_HXX

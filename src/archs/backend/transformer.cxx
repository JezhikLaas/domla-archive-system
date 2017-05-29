#include "transformer.hxx"

using namespace std;
using namespace Archive::Backend;

std::unordered_map<std::type_index, TransformerBase*> TransformerRegistry::Transformers_;

void TransformerBase::Delete(const Persistable& item)
{
}

void TransformerBase::Insert(const Persistable& item)
{
}

void TransformerBase::Update(const Persistable& item)
{
}

void TransformerBase::Connect(const SQLite::Connection& connection)
{
}

void TransformerQueue::Flush()
{
}

void TransformerRegistry::Register(const std::type_info& id, TransformerBase* transformer)
{
    Transformers_.insert({ type_index(id), transformer });
}

TransformerBase& TransformerRegistry::Fetch(const std::type_info& id)
{
    return *Transformers_[type_index(id)];
}

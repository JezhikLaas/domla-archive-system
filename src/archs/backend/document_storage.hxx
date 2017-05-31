#ifndef DOCUMENT_STORAGE_HXX
#define DOCUMENT_STORAGE_HXX

#include <functional>
#include <memory>
#include <vector>
#include "data_bucket.hxx"
#include "settings_provider.hxx"

namespace Archive
{
namespace Backend
{

class DocumentStorage
{
private:
    std::shared_ptr<DataBucket> Buckets_[256];
    const SettingsProvider& Settings_;

private:
    void InitializeBuckets();
    void RegisterTransformers();
    void Bucketing(int count, std::function<std::shared_ptr<DataBucket>(int)> generator, std::shared_ptr<DataBucket>buckets[]);
    
public:
    DocumentStorage(const SettingsProvider& settings);
    ~DocumentStorage();
    DocumentStorage(const DocumentStorage&) = delete;
    void operator= (const DocumentStorage&) = delete;
};

} // Backend
} // Archive

#endif
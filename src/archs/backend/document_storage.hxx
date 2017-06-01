#ifndef DOCUMENT_STORAGE_HXX
#define DOCUMENT_STORAGE_HXX

#include <functional>
#include <memory>
#include <vector>
#include "data_bucket.hxx"
#include "settings_provider.hxx"
#include "virtual_tree.hxx"
#include "Archive.h"

namespace Archive
{
namespace Backend
{

using BucketHandle = std::shared_ptr<DataBucket>;
using CreateHandle = std::function<BucketHandle(int)>;

class DocumentStorage
{
private:
    BucketHandle Buckets_[256];
    const SettingsProvider& Settings_;
    std::vector<BucketHandle> DistinctHandles_;
    VirtualTree Folders_;

private:
    void InitializeBuckets();
    void RegisterTransformers();
    void Bucketing(int count, CreateHandle generator, BucketHandle buckets[]);
    void BuildFolderTree();
    std::vector<Access::FolderInfo> ReadBranches(const std::string& startWith);
    
public:
    DocumentStorage(const SettingsProvider& settings);
    ~DocumentStorage();
    DocumentStorage(const DocumentStorage&) = delete;
    void operator= (const DocumentStorage&) = delete;
};

} // Backend
} // Archive

#endif
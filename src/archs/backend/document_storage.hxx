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
    BucketHandle FetchBucket(const std::string& value) const;
    void ReadOnlyDenied(const std::string& user) const;
    void InsertIntoDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& comment);
    void UpdateInDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& user, const std::string& comment);
    Access::DocumentDataPtr Fetch(BucketHandle handle, const std::string& id) const;
    int LatestRevision(SQLite::Connection* connection, const std::string& id);
    Access::DocumentContentPtr LatestContent(SQLite::Connection* connection, const std::string& id);
    
public:
    DocumentStorage(const SettingsProvider& settings);
    ~DocumentStorage();
    DocumentStorage(const DocumentStorage&) = delete;
    void operator= (const DocumentStorage&) = delete;

public:
    Access::DocumentDataPtr Load(const std::string& id, const std::string& user) const;
    void Save(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& user, const std::string& comment = "");
    void Lock(const std::string& id, const std::string& user) const;
    void Unlock(const std::string& id, const std::string& user) const;
    Access::DocumentDataPtr FindById(const std::string& id, int number = 0) const;
    Access::DocumentDataPtr Find(const std::string& folderPath, const std::string& fileName) const;
    std::vector<Access::DocumentDataPtr> FindTitle(const std::string& folderPath, const std::string& displayName) const;
};

} // Backend
} // Archive

#endif
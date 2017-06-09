#ifndef DOCUMENT_STORAGE_HXX
#define DOCUMENT_STORAGE_HXX

#include <functional>
#include <memory>
#include <vector>
#include "data_bucket.hxx"
#include "settings_provider.hxx"
#include "virtual_tree.hxx"
#include "Archive.h"
#include "utils.hxx"

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
    mutable VirtualTree Folders_;
    Utils::PeriodicTimer Timer_;

private:
    void InitializeBuckets();
    void RegisterTransformers();
    void Bucketing(int count, CreateHandle generator, BucketHandle buckets[]);
    void BuildFolderTree();
    std::vector<Access::FolderInfo> ReadBranches(const std::string& startWith);
    BucketHandle FetchBucket(const std::string& value) const;
    void ReadOnlyDenied(const std::string& user) const;
    void InsertIntoDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& comment) const;
    void UpdateInDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& user, const std::string& comment) const;
    Access::DocumentDataPtr Fetch(BucketHandle handle, const std::string& id) const;
    Access::DocumentDataPtr FetchChecked(BucketHandle handle, const std::string& id, const std::string& user) const;
    int LatestRevision(SQLite::Connection* connection, const std::string& id) const;
    Access::DocumentContentPtr LatestContent(SQLite::Connection* connection, const std::string& id) const;
    Access::DocumentAssignmentPtr Fetch(SQLite::Connection* connection, const std::string& id, const std::string& path) const;
    void Optimizer();
    
public:
    DocumentStorage(const SettingsProvider& settings);
    ~DocumentStorage();
    DocumentStorage(const DocumentStorage&) = delete;
    void operator= (const DocumentStorage&) = delete;

public:
    std::vector<Access::FolderInfo> FoldersForPath(const std::string& root) const;
    std::vector<std::string> FoldersOf(const std::string& id) const;
    Access::DocumentDataPtr Load(const std::string& id, const std::string& user) const;
    void Save(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const std::string& user, const std::string& comment = "");
    void Lock(const std::string& id, const std::string& user) const;
    void Unlock(const std::string& id, const std::string& user) const;
    Access::DocumentDataPtr FindById(const std::string& id, int number = 0) const;
    Access::DocumentDataPtr Find(const std::string& folderPath, const std::string& fileName) const;
    std::vector<Access::DocumentDataPtr> FindKeywords(const std::string& keywords) const;
    std::vector<Access::DocumentDataPtr> FindTitle(const std::string& folderPath, const std::string& displayName) const;
    std::vector<Access::DocumentDataPtr> FindMetaData(const std::string& tags) const;
    void Move(const std::string& id, const std::string& oldPath, const std::string& newPath, const std::string& user) const;
    void Copy(const std::string& id, const std::string& sourcePath, const std::string& targetPath, const std::string& user) const;
    void Link(const std::string& id, const std::string& sourcePath, const std::string& targetPath, const std::string& user) const;
    void Associate(const std::string& id, const std::string& path, const std::string& item, const std::string& type, const std::string& user) const;
    void Destroy(const std::string& id, const std::string& user) const;
    void Delete(const std::string& id, const std::string& user) const;
    void Undelete(const std::string& id, const std::string& user) const;
    void Undelete(const std::vector<std::string>& ids, const std::string& user) const;
    void AssignKeywords(const std::string& id, const std::string& keywords, const std::string& user) const;
    void AssignMetaData(const std::string& id, const std::string& data, const std::string& user) const;
    void ReplaceMetaData(const std::string& id, const std::string& data, const std::string& user) const;
    std::vector<std::string> ListMetaTags() const;
    std::vector<std::string> ListMetaTags(const std::string& id) const;
};

} // Backend
} // Archive

#endif
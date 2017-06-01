#include "document_storage.hxx"
#include "document_schema.hxx"
#include "transformer.hxx"
#include "Archive.h"
#include <math.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <future>
#include <mutex>

using namespace std;
using namespace Archive::Backend;
using namespace boost;
using namespace boost::filesystem;

DocumentStorage::DocumentStorage(const SettingsProvider& settings)
: Settings_(settings)
{
    InitializeBuckets();
    RegisterTransformers();
    auto& Builder = async(launch::async, [this]() { BuildFolderTree(); });
    Builder.wait();
}

DocumentStorage::~DocumentStorage()
{
    for (auto& Bucket : Buckets_) Bucket.reset();
}

void DocumentStorage::InitializeBuckets()
{
    create_directory(Settings_.DataLocation());
    Bucketing(Settings_.Backends(), [this](int number) { return BucketHandle(new DataBucket(number, Settings_)); }, Buckets_);
}

void DocumentStorage::RegisterTransformers()
{
    TransformerRegistry::Register(Access::DocumentData::ice_staticId(), []() { return (TransformerBase*)new DocumentTransformer(); });    
    TransformerRegistry::Register(Access::DocumentHistoryEntry::ice_staticId(), []() { return (TransformerBase*)new HistoryTransformer(); });    
    TransformerRegistry::Register(Access::DocumentContent::ice_staticId(), []() { return (TransformerBase*)new ContentTransformer(); });    
    TransformerRegistry::Register(Access::DocumentAssignment::ice_staticId(), []() { return (TransformerBase*)new AssignmentTransformer(); });    
}

void DocumentStorage::Bucketing(int count, CreateHandle generator, BucketHandle buckets[])
{
    auto Limit = (int)round((double)256 / (double)count);
    int Created = 0;
    BucketHandle Access;
    auto Generator = [&Created, &generator, this]() {
        ++Created;
        auto& Handle = generator(Created);
        DistinctHandles_.push_back(Handle);
        return Handle;
    };

    for (int Index = 0; Index < 256; ++Index) {
        Access = Access ? Access : Generator();
        buckets[Index] = Access;
        if (Index > 0 && (Index % Limit) == 0 && Created < count) Access.reset();
    }
}

void DocumentStorage::BuildFolderTree()
{
    Folders_.Load(ReadBranches(""));
}

vector<Access::FolderInfo> DocumentStorage::ReadBranches(const string& startWith)
{
        const char*  AllFoldersQuery = R"(
SELECT
    Path as Path, Documents.FileName as FileName
FROM
    DocumentAssignments
INNER JOIN
    DocumentHistories ON DocumentHistories.Id = DocumentAssignments.Owner
INNER JOIN
    Documents ON Documents.Id = DocumentHistories.Owner AND Documents.State = 0
)";

        const char* SubFoldersQuery = R"(
SELECT
    DocumentAssignments.Path as Path, Documents.FileName as FileName
FROM
    DocumentAssignments
INNER JOIN
    DocumentHistories ON DocumentHistories.Id = DocumentAssignments.Owner
INNER JOIN
    Documents ON Documents.Id = DocumentHistories.Owner AND Documents.State = 0
WHERE
    DocumentAssignments.Path LIKE '%1%'
)";
    vector<Access::FolderInfo> Result;
    
    auto Query = startWith.empty()
                 ?
                 static_cast<string>(AllFoldersQuery)
                 :
                 (format(SubFoldersQuery) % startWith).str()
                 ;
    
    vector<future<vector<pair<string,string>>>> Intermediates;
    
    for (auto& Handle : DistinctHandles_) {
        Intermediates.push_back(
            async(
                [&Handle, &Query]() {
                    lock_guard<recursive_mutex> Lock(Handle->ReadGuard);
                    vector<pair<string, string>> Intermediate;
                    auto& Command = Handle->Reader().Create(Query);
                    
                    for (auto& Row : Command.Open()) {
                        Intermediate.push_back(make_pair<string, string>(Row.Get<string>(0), Row.Get<string>(1)));
                    }
                    
                    return Intermediate;
                }
            )
        );
    }
    
    unordered_map<string, int> Groups;
    
    for (auto& Intermediate : Intermediates) {
        auto& Folders = Intermediate.get();
        for (auto& Folder : Folders) {
            Groups[Folder.first] += 1;
        }
    }
    
    for (auto& Group : Groups) {
        Access::FolderInfo Info { Group.first, Group.second };
        Result.push_back(Info);
        
        vector<string> Paths;
        split(Paths, Group.first, [](string::value_type item) { return item == '/'; }, boost::token_compress_on);
        
        if (Paths.empty() == false) {
            Paths.pop_back();
            while (Paths.empty() == false) {
                auto Path = string("/") + join(Paths, "/");
                if (Groups.count(Path) == 0 && Path.size() > startWith.size()) {
                    Groups[Path] = 0;
                    Access::FolderInfo Info { Path, 0 };
                    Result.push_back(Info);
                }
                Paths.pop_back();
            }
        }
    }
    
    return Result;
}

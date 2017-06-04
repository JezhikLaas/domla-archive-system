#include "document_storage.hxx"
#include "document_schema.hxx"
#include "transformer.hxx"
#include "utils.hxx"
#include "Archive.h"
#include "Authentication.h"
#include <math.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <future>
#include <mutex>

using namespace std;
using namespace Archive::Backend;
using namespace boost;
using namespace boost::filesystem;
using namespace boost::posix_time;
using namespace boost::gregorian;

using Guard = lock_guard<recursive_mutex>;

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

Access::DocumentDataPtr DocumentStorage::Load(const string& id, const string& user) const
{
    auto Handle = FetchBucket(id);
    return Fetch(Handle, id);
}

void DocumentStorage::Save(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const string& user, const string& comment)
{
    if (document->Id.empty()) {
        document->Creator = user;
        document->User = user;
        InsertIntoDatabase(document, data, comment);
    }
    else {
        UpdateInDatabase(document, data, user, comment);
    }
}

void DocumentStorage::Lock(const std::string& id, const std::string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = Fetch(Handle, id);
    if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is already locked by %2%") % Document->Display % Document->Locker).str());
    
    TransformerQueue Actions(Handle->Writing());
    Document->Locker = user;
    Actions.Update(*Document);
    Actions.Flush();
}

void DocumentStorage::InitializeBuckets()
{
    if (Settings_.DataLocation() != ":memory:") create_directory(Settings_.DataLocation());
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
        const char* const AllFoldersQuery = R"(
SELECT
    Path as Path, Documents.FileName as FileName
FROM
    DocumentAssignments
INNER JOIN
    DocumentHistories ON DocumentHistories.Id = DocumentAssignments.Owner
INNER JOIN
    Documents ON Documents.Id = DocumentHistories.Owner AND Documents.State = 0
)";

        const char* const SubFoldersQuery = R"(
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

Access::DocumentDataPtr DocumentStorage::Fetch(BucketHandle handle, const std::string& id) const
{
    DocumentTransformer Transformer(handle->Reading());
    Access::DocumentDataPtr Result = new Access::DocumentData();
    Result->Id = id;
    
    Guard Lock(handle->ReadGuard);
    if (Transformer.Load(*Result) == false) throw Access::NotFoundError((format("a document with id %1% is not known") % id).str());
    
    return Result;
}

BucketHandle DocumentStorage::FetchBucket(const string& value) const
{
    if (value.size() < 2) throw runtime_error("DocumentStorage::FetchBucket - value too short");
    char* Dummy;
    char Buffer[] = "00";
    Buffer[0] = value[0];
    Buffer[1] = value[1];
    
    return Buckets_[strtoul(Buffer, &Dummy, 16)];
}

void DocumentStorage::ReadOnlyDenied(const string& user) const
{
    if (user == Access::ViewOnlyUser) throw Authentication::AuthenticationError("read only user is not permitted for this operation");
}

void DocumentStorage::InsertIntoDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const string& comment)
{
    document->Id = Utils::NewId();
    auto Handle = FetchBucket(document->Id);
    auto& FolderInfo = async(launch::async, [this, &document]() {
        if (document->Name != Access::DocumentDirectoryName) {
            Folders_.Add(document->FolderPath);
        }
        else {
            Folders_.AddUncounted(document->FolderPath);
        }
    });
    
    TransformerQueue Actions(Handle->Writing());

    document->Created = Utils::Ticks(microsec_clock::local_time());
    document->Creator = document->Creator.empty() ? document->User : document->Creator;
    document->Deleted = false;
    document->Size = data.size();
    Actions.Insert(*document);

    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Action = Access::Created;
    History->Actor = document->Creator;
    History->Created = document->Created;
    History->Document = document->Id;
    History->Id = Utils::NewId();
    History->Comment = comment;
    History->Revision = 1;
    Actions.Insert(*History);

    Access::DocumentContentPtr Data = new Access::DocumentContent();
    Data->Content = data;
    Data->History = History->Id;
    Data->Id = Utils::NewId();
    Data->Revision = 1;
    Actions.Insert(*Data);

    Access::DocumentAssignmentPtr Assignment = new Access::DocumentAssignment();
    Assignment->AssignmentId = document->AssociatedItem;
    Assignment->AssignmentType = document->AssociatedClass;
    Assignment->History = History->Id;
    Assignment->Id = Utils::NewId();
    Assignment->Path = document->FolderPath;
    Assignment->Revision = 1;
    Actions.Insert(*Assignment);

    Actions.Flush();

    /*
    UpdateFullTextSearch(Data.Content, document.Id, document.Name);
    */
    FolderInfo.wait();
}

void DocumentStorage::UpdateInDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const string& user, const string& comment)
{
}

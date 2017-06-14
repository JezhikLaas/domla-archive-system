#include "binary_data.hxx"
#include "document_storage.hxx"
#include "document_schema.hxx"
#include "transformer.hxx"
#include "Archive.h"
#include "Authentication.h"
#include <math.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
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

namespace
{

string AliasFields(const vector<string>& fields, const string alias)
{
    vector<string> Result;
    transform(fields.cbegin(), fields.cend(), back_inserter(Result), [&alias](const string& field) { return alias + "." + field; });
    return join(Result, ", ");
}

void FillHeaderFromRow(Access::DocumentDataPtr& target, const SQLite::ResultRow& source)
{
    target->Id = source.Get<string>(0);
    target->User = "";
    target->Name = source.Get<string>(3);
    target->Display = source.Get<string>(4);
    target->Keywords = source.Get<string>(7);
    target->Locker = source.Get<string>(6);
    target->Creator = source.Get<string>(1);
    target->Created = source.Get<int64_t>(2);
    target->Modified = source.Get<int64_t>(13);
    target->Size = source.Get<int>(8);
    target->AssociatedItem = source.Get<string>(10);
    target->AssociatedClass = source.Get<string>(9);
    target->Checksum = "";
    target->FolderPath = source.Get<string>(11);
    target->Deleted = false;
    target->Revision = source.Get<int>(12);
}

} // anonymous namespace

using Guard = lock_guard<recursive_mutex>;

DocumentStorage::DocumentStorage(const SettingsProvider& settings)
: Settings_(settings), Timer_(hours(3), boost::bind(&DocumentStorage::Optimizer, this))
{
    InitializeBuckets();
    auto& Builder = async(launch::async, [this]() { BuildFolderTree(); });
    RegisterTransformers();
    Timer_.Start();
    Builder.wait();
}

DocumentStorage::~DocumentStorage()
{
    for (auto& Bucket : Buckets_) Bucket.reset();
}

vector<Access::FolderInfo> DocumentStorage::FoldersForPath(const string& root) const
{
    vector<Access::FolderInfo> Result;
    auto& Infos = Folders_.Content(root);
    transform(
        Infos.begin(),
        Infos.end(),
        back_inserter(Result), [](const FolderInfo& info) {
            return Access::FolderInfo { get<2>(info), get<1>(info) };
        }
    );
    
    return Result;
}

std::vector<std::string> DocumentStorage::FoldersOf(const std::string& id) const
{
    const string QueryTemplate =
R"(SELECT
    asg.Path
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst
ON
    asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
WHERE
    hst.Owner = '%1%'
)";
    auto Query = (format(QueryTemplate) % id).str();
    auto& Handle = FetchBucket(id);
    
    Guard Lock(Handle->ReadGuard);
    
    auto& Command = Handle->Reader().Create(Query);
    auto& Data = Command.Open();
    
    vector<string> Result;
    transform(Data.begin(), Data.end(), back_inserter(Result), [](auto& row) { return row.Get<string>(0); });
    
    return Result;
}

Access::DocumentDataPtr DocumentStorage::Load(const string& id, const string& user) const
{
    auto Handle = FetchBucket(id);
    return Fetch(Handle, id);
}

void DocumentStorage::AssignKeywords(const string& id, const string& keywords, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = FetchChecked(Handle, id, user);
    
    TransformerQueue Actions(Handle->Writing());
    
    Document->Keywords = keywords;
    Actions.Update(*Document);
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Keywords;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    Actions.Insert(*History);
    
    Actions.Flush();
}

void DocumentStorage::AssignMetaData(const string& id, const string& data, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    auto Tokens = Utils::Split(data, 30);
    if (Tokens.empty()) throw invalid_argument("unable to parse meta tags");
    
    vector<string> Metas;
    transform(
        Tokens.begin(),
        Tokens.end(),
        back_inserter(Metas),
        [](const string& token) {
            auto Parts = Utils::Split(token, '=');
            return Parts.size() == 2 ? Parts[0] : string();
        }
    );
    
    auto Transaction = Handle->Writing()->Begin();
    {
        auto Command = Handle->Writing()->Create("INSERT INTO DocumentMetas(Owner, Tags)VALUES(:Owner, :Tags)");
        Command.Parameters()["Owner"].SetValue(id);
        Command.Parameters()["Tags"].SetValue(data);
        Command.Execute();
    }
    {
        auto Command = Handle->Writing()->Create("REPLACE INTO DocumentTags(Tag)VALUES(:Tag)");
        
        for (auto& Meta : Metas) {
            if (Meta.empty() == false) {
                Command.Parameters()["Tag"].SetValue(Meta);
                Command.Execute();
            }
        }
    }
    Transaction.Commit();
}

void DocumentStorage::ReplaceMetaData(const string& id, const string& data, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    auto Tokens = Utils::Split(data, 30);
    
    vector<string> Metas;
    transform(
        Tokens.begin(),
        Tokens.end(),
        back_inserter(Metas),
        [](const string& token) {
            auto Parts = Utils::Split(token, '=');
            return Parts.size() == 2 ? Parts[0] : string();
        }
    );
    
    auto Transaction = Handle->Writing()->Begin();
    {
        auto Command = Handle->Writing()->Create("DELETE FROM DocumentMetas WHERE Owner = :Owner");
        Command.Parameters()["Owner"].SetValue(id);
        Command.Execute();
    }
    if (data.empty() == false) {
        auto Command = Handle->Writing()->Create("INSERT INTO DocumentMetas(Owner, Tags)VALUES(:Owner, :Tags)");
        Command.Parameters()["Owner"].SetValue(id);
        Command.Parameters()["Tags"].SetValue(data);
        Command.Execute();
    }
    if (Metas.empty() == false) {
        auto Command = Handle->Writing()->Create("REPLACE INTO DocumentTags(Tag)VALUES(:Tag)");
        
        for (auto& Meta : Metas) {
            if (Meta.empty() == false) {
                Command.Parameters()["Tag"].SetValue(Meta);
                Command.Execute();
            }
        }
    }
    Transaction.Commit();
}

vector<string> DocumentStorage::ListMetaTags() const
{
    const string Query = "SELECT Tag FROM DocumentTags";

    vector<future<vector<string>>> Intermediates;
    
    for (auto& Handle : DistinctHandles_) {
        Intermediates.push_back(
            async(
                [&Handle, &Query]() {
                    vector<string> Result;
                    lock_guard<recursive_mutex> Lock(Handle->ReadGuard);
                    auto& Command = Handle->Reader().Create(Query);
                    
                    for (auto& Row : Command.Open()) {
                        Result.push_back(Row.Get<string>(0));
                    }
                    
                    return Result;
                }
            )
        );
    }
    
    vector<string> Result;
    
    for (auto& Found : Intermediates) {
        auto& Item = Found.get();
        copy(Item.cbegin(), Item.cend(), back_inserter(Result));
    }
    
    return Result;
}

vector<string> DocumentStorage::ListMetaTags(const string& id) const
{
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->ReadGuard);
    
    const string Query = "SELECT Tag FROM DocumentTags WHERE Owner = '" + id + "'";
    
    vector<string> Result;
    auto& Command = Handle->Reader().Create(Query);
    
    for (auto& Row : Command.Open()) {
        Result.push_back(Row.Get<string>(0));
    }
    
    return Result;
}

Access::DocumentDataPtr DocumentStorage::FindById(const string& id, int number) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    doc.Id = '%1%' AND doc.State = 0
)";

    const string QueryRevisionTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
WHERE
    doc.Id = '%1%' AND doc.State = 0
AND
    hst.SeqId = %2%
)";

    auto Query = number == 0 ? (format(QueryTemplate) % id).str() : (format(QueryRevisionTemplate) % id % number).str();
    auto& Handle = FetchBucket(id);
    
    Guard Lock(Handle->ReadGuard);
    
    Access::DocumentDataPtr Result = new Access::DocumentData();
    auto& Command = Handle->Reader().Create(Query);
    auto& Data = Command.Open();
    
    if (Data.HasData()) {
        auto& Row = Data.begin();
        FillHeaderFromRow(Result, *Row);
    }

    return Result;
}

Access::DocumentDataPtr DocumentStorage::Find(const string& folderPath, const string& fileName) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    asg.Path = '%1%' AND doc.FileName = '%2%' AND doc.State = 0
)";
    
    auto Query = (format(QueryTemplate) % boost::algorithm::to_lower_copy(folderPath) % boost::algorithm::to_lower_copy(fileName)).str();
    vector<future<Access::DocumentDataPtr>> Intermediate;
    vector<Access::DocumentDataPtr> Result;
    
    for (auto& Handle : DistinctHandles_) {
        Intermediate.push_back(
            async(
                [&Handle, &Query]() {
                    lock_guard<recursive_mutex> Lock(Handle->ReadGuard);
                    auto& Command = Handle->Reader().Create(Query);
                    Access::DocumentDataPtr Item;
                    for (auto& Row : Command.Open()) {
                        Item = new Access::DocumentData();
                        FillHeaderFromRow(Item, Row);
                        break;
                    }
                    
                    return Item;
                }
            )
        );
    }
    
    for (auto& Found : Intermediate) {
        auto Item = Found.get();
        if (Item->Id.empty()) continue;
        
        Result.push_back(Item);
    }
    
    return Result.empty() ? Access::DocumentDataPtr() : Result[0].get();
}

vector<Access::DocumentDataPtr> DocumentStorage::FindTitle(const string& folderPath, const string& displayName) const
{
            const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    asg.Path = '%1%' AND lower(doc.DisplayName) = '%2%' AND doc.State = 0
)";
    auto Query = (format(QueryTemplate) % algorithm::to_lower_copy(folderPath) % boost::algorithm::to_lower_copy(displayName)).str();
    
    return FetchFromAll(Query);
}

vector<Access::DocumentDataPtr> DocumentStorage::FindKeywords(const string& keywords) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    (%1%) AND doc.State = 0
)";
    auto Values = Utils::Split(keywords, ' ');
    vector<string> Parts;
    transform(Values.begin(), Values.end(), back_inserter(Parts), [](const string& word) { return "doc.Keywords LIKE '%" + word + "%'"; });
    auto Restrict = join(Parts, " OR ");
    auto Query = (format(QueryTemplate) % Restrict).str();
    
    return FetchFromAll(Query);
}

vector<Access::DocumentDataPtr> DocumentStorage::FindMetaData(const string& tags) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
INNER JOIN 
    DocumentMetas ON DocumentMetas.Owner = doc.Id
WHERE
    DocumentMetas MATCH '%1%' AND doc.State = 0
)";
    auto Values = Utils::Split(tags, 30);
    vector<string> Parts;
    transform(Values.begin(), Values.end(), back_inserter(Parts), [](const string& word) { return "\"" + word + "\"*"; });
    auto Restrict = join(Parts, " AND ");
    auto Query = (format(QueryTemplate) % Restrict).str();
    
    return FetchFromAll(Query);
}

vector<Access::DocumentDataPtr> DocumentStorage::FindFilenames(const string& names) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    (%1%) AND doc.State = 0
)";
    auto Values = Utils::Split(names, ' ');
    vector<string> Parts;
    transform(Values.begin(), Values.end(), back_inserter(Parts), [](const string& word) { return "doc.FileName LIKE '%" + word + "%'"; });
    auto Restrict = join(Parts, " OR ");
    auto Query = (format(QueryTemplate) % Restrict).str();
    
    return FetchFromAll(Query);
}

vector<Access::DocumentDataPtr> DocumentStorage::FindFilenameMatch(const string& expression) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    (FileName REGEXP '%1%') AND doc.State = 0
)";
    auto Query = (format(QueryTemplate) % expression).str();
    
    return FetchFromAll(Query);
}

vector<Access::DocumentDataPtr> DocumentStorage::FindDeleted(const string& root, int depth) const
{
    const string QueryTemplate =
R"(SELECT
    doc.Id, doc.Creator, doc.Created, doc.FileName, doc.DisplayName, doc.State, doc.Locker, doc.Keywords, doc.Size, asg.AssignmentType, asg.AssignmentId, asg.Path, hsv.SeqId, hsv.Created
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst ON asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
INNER JOIN
    Documents doc ON hst.Owner = doc.Id
INNER JOIN
    DocumentHistories hsv ON hsv.SeqId = (SELECT MAX(hsi.SeqId) FROM DocumentHistories hsi WHERE hsi.Owner = doc.Id) AND hsv.Owner = doc.Id
WHERE
    asg.Path LIKE lower('%1%%%')
AND
    doc.State = 1
AND
    PARTSCOUNT(asg.Path, '/') - %2% <= %3%
)";
    auto Difference = depth == LONG_MAX ? 0 : Utils::Split(root, '/').size();
    auto Query = (format(QueryTemplate) % root % Difference % depth).str();
    
    return FetchFromAll(Query);
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

void DocumentStorage::Lock(const string& id, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = FetchChecked(Handle, id, user);
    if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is already locked by %2%") % Document->Display % Document->Locker).str());
    
    TransformerQueue Actions(Handle->Writing());
    Document->Locker = user;
    Actions.Update(*Document);
    Actions.Flush();
}

void DocumentStorage::Unlock(const string& id, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = FetchChecked(Handle, id, user);
    
    TransformerQueue Actions(Handle->Writing());
    Document->Locker = "";
    Actions.Update(*Document);
    Actions.Flush();
}

void DocumentStorage::Move(const string& id, const string& oldPath, const string& newPath, const string& user) const
{
    ReadOnlyDenied(user);
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);

    Access::DocumentAssignmentPtr Assignment = Fetch(Handle->Writing(), id, oldPath);

    auto& FolderInfo = async(launch::async, [this, &newPath, &oldPath]() {
         Folders_.Add(newPath);
         Folders_.Remove(oldPath);
    });
    
    TransformerQueue Actions(Handle->Writing());
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Moved;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    History->Source = oldPath;
    History->Target = newPath;
    Actions.Insert(*History);
    
    Assignment->Path = newPath;
    Actions.Update(*Assignment);
    
    Actions.Flush();
    
    FolderInfo.wait();
}

void DocumentStorage::Link(const string& id, const string& sourcePath, const string& targetPath, const string& user) const
{
    ReadOnlyDenied(user);
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentAssignmentPtr Assignment = Fetch(Handle->Writing(), id, sourcePath);
    auto Item = Fetch(Handle, id);

    auto& FolderInfo = async(launch::async, [this, &targetPath]() {
         Folders_.Add(targetPath);
    });
    
    TransformerQueue Actions(Handle->Writing());
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Linked;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    History->Source = sourcePath;
    History->Target = targetPath;
    Actions.Insert(*History);
    
    Access::DocumentAssignmentPtr NewAssignment = new Access::DocumentAssignment();
    NewAssignment->History = History->Id;
    NewAssignment->Id = Utils::NewId();
    NewAssignment->Path = targetPath;
    NewAssignment->AssignmentId = Assignment->AssignmentId;
    NewAssignment->AssignmentType = Assignment->AssignmentType;
    NewAssignment->Revision = History->Revision;
    Actions.Insert(*NewAssignment);
    
    Actions.Flush();
    
    FolderInfo.wait();
}

void DocumentStorage::Copy(const string& id, const string& sourcePath, const string& targetPath, const string& user) const
{
    ReadOnlyDenied(user);
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentAssignmentPtr Assignment = Fetch(Handle->Writing(), id, sourcePath);
    auto Item = Fetch(Handle, id);

    auto& FolderInfo = async(launch::async, [this, &targetPath]() {
         Folders_.Add(targetPath);
    });
    
    Access::DocumentDataPtr Clone = new Access::DocumentData(*Item);
    Clone->FolderPath = targetPath;
    InsertIntoDatabase(Clone, LatestContent(Handle->Writing(), Item->Id)->Content, "");
    
    FolderInfo.wait();
}

void DocumentStorage::Associate(const string& id, const string& path, const string& item, const string& type, const string& user) const
{
    ReadOnlyDenied(user);
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentAssignmentPtr Assignment = Fetch(Handle->Writing(), id, path);
    Assignment->AssignmentId = item;
    Assignment->AssignmentType = type;
    
    TransformerQueue Actions(Handle->Writing());
    Actions.Update(*Assignment);
    Actions.Flush();
}

void DocumentStorage::Delete(const string& id, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = Fetch(Handle, id);
    if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is locked by %2%") % Document->Display % Document->Locker).str());
    if (Document->Deleted) throw Access::LockError((format("document %1% is already in the deleted state") % Document->Display).str());
    
    auto Folders = FoldersOf(id);
    auto& FolderInfo = async(launch::async, [this, &Folders, &Document]() {
         for (auto& Folder : Folders) {
             if (Document->Name == Access::DocumentDirectoryName)
                Folders_.RemoveUncounted(Folder);
             else {
                Folders_.Remove(Folder);
             }
         }
    });
    
    TransformerQueue Actions(Handle->Writing());
    
    Document->Deleted = true;
    Actions.Update(*Document);
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Deleted;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    Actions.Insert(*History);
    
    Actions.Flush();
    FolderInfo.wait();
}

void DocumentStorage::Destroy(const string& id, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = Fetch(Handle, id);
    if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is locked by %2%") % Document->Display % Document->Locker).str());
    if (Document->Deleted) throw Access::LockError((format("document %1% is already in the deleted state") % Document->Display).str());
    
    auto Folders = FoldersOf(id);
    auto& FolderInfo = async(launch::async, [this, &Folders, &Document]() {
         for (auto& Folder : Folders) {
             if (Document->Name == Access::DocumentDirectoryName)
                Folders_.RemoveUncounted(Folder);
             else {
                Folders_.Remove(Folder);
             }
         }
    });
    
    TransformerQueue Actions(Handle->Writing());
    Actions.Delete(*Document);
    Actions.Flush();
    
    FolderInfo.wait();
}

void DocumentStorage::Undelete(const vector<string>& ids, const string& user) const
{
    ReadOnlyDenied(user);
    
    for (auto& Id : ids) {
        auto Handle = FetchBucket(Id);
        Guard Lock(Handle->WriteGuard);
        
        Access::DocumentDataPtr Document = Fetch(Handle, Id);
        if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is locked by %2%") % Document->Display % Document->Locker).str());
        if (Document->Deleted == false) throw Access::LockError((format("document %1% is not in the deleted state") % Document->Display).str());
        
        auto Folders = FoldersOf(Id);
        auto& FolderInfo = async(launch::async, [this, &Folders, &Document]() {
             for (auto& Folder : Folders) {
                 if (Document->Name == Access::DocumentDirectoryName)
                    Folders_.AddUncounted(Folder);
                 else {
                    Folders_.Add(Folder);
                 }
             }
        });
        
        TransformerQueue Actions(Handle->Writing());
        
        Document->Deleted = false;
        Actions.Update(*Document);
        
        Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
        History->Id = Utils::NewId();
        History->Action = Access::Recovered;
        History->Actor = user;
        History->Created = Utils::Ticks(microsec_clock::local_time());
        History->Document = Id;
        History->Revision = LatestRevision(Handle->Writing(), Id) + 1;
        Actions.Insert(*History);
        
        Actions.Flush();
        FolderInfo.wait();
    }
}

void DocumentStorage::Undelete(const string& id, const string& user) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = Fetch(Handle, id);
    if (Document->Locker.empty() == false && Document->Locker != user) throw Access::LockError((format("document %1% is locked by %2%") % Document->Display % Document->Locker).str());
    if (Document->Deleted == false) throw Access::LockError((format("document %1% is not in the deleted state") % Document->Display).str());
    
    auto Folders = FoldersOf(id);
    auto& FolderInfo = async(launch::async, [this, &Folders, &Document]() {
         for (auto& Folder : Folders) {
             if (Document->Name == Access::DocumentDirectoryName)
                Folders_.AddUncounted(Folder);
             else {
                Folders_.Add(Folder);
             }
         }
    });
    
    TransformerQueue Actions(Handle->Writing());
    
    Document->Deleted = false;
    Actions.Update(*Document);
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Recovered;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    Actions.Insert(*History);
    
    Actions.Flush();
    FolderInfo.wait();
}

void DocumentStorage::Rename(const string& id, const string& user, const string& display) const
{
    ReadOnlyDenied(user);
    
    auto Handle = FetchBucket(id);
    Guard Lock(Handle->WriteGuard);
    
    Access::DocumentDataPtr Document = FetchChecked(Handle, id, user);
    TransformerQueue Actions(Handle->Writing());
    
    Document->Display = display;
    Actions.Update(*Document);
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Id = Utils::NewId();
    History->Action = Access::Retitled;
    History->Actor = user;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = id;
    History->Revision = LatestRevision(Handle->Writing(), id) + 1;
    Actions.Insert(*History);
    
    Actions.Flush();
}

Access::DocumentContentPtr DocumentStorage::Read(const string& id, const string& user) const
{
    auto& Handle = FetchBucket(id);
    auto Content = LatestContent(Handle->Reading(), id);

    return Content;
}

Access::DocumentContentPtr DocumentStorage::Read(const string& id, const string& user, int revision) const
{
    const string QueryTemplate =
R"(SELECT
    %1%
FROM
    DocumentContents cnt
INNER JOIN
    DocumentHistories hst
ON
    cnt.Owner = hst.Id
WHERE
    hst.Owner = '%2%'
ORDER BY
    cnt.SeqId DESC)";
    
    auto& Handle = FetchBucket(id);
    Guard Lock(Handle->ReadGuard);
    auto Header = FetchChecked(Handle, id, user);

    auto Fields = AliasFields(ContentTransformer::FieldNames(), "cnt");
    auto Query = (format(QueryTemplate) % Fields % id).str();
    auto Command = Handle->Reading()->Create(Query);
    
    Access::DocumentContentPtr Last = new Access::DocumentContent();
    vector<unsigned char> Content;

    ContentTransformer Transformer;
    for (auto& Row : Command.Open()) {
        Transformer.Load(Row, *Last);
        if (Content.empty()) {
            Content = Last->Content;
        }
        else {
            if (Last->Revision < revision) break;
            Content = BinaryData::ApplyPatch(Content, Last->Content);
        }
    }

    Access::DocumentContentPtr Result = new Access::DocumentContent();
    Result->Checksum = Last->Checksum;
    Result->Content = Content;
    Result->History = Last->History;
    Result->Id = Last->Id;
    Result->Revision = Last->Revision;
    
    return Result;
}

vector<Access::DocumentHistoryEntryPtr> DocumentStorage::Revisions(const string& id) const
{
    const string QueryTemplate = "SELECT %1% FROM DocumentHistories WHERE Owner = '%2%'";
    auto& Handle = FetchBucket(id);
    Guard Lock(Handle->ReadGuard);

    auto Fields = join(HistoryTransformer::FieldNames(), ", ");
    auto Query = (format(QueryTemplate) % Fields % id).str();
    auto Command = Handle->Reading()->Create(Query);
    
    vector<Access::DocumentHistoryEntryPtr> Result;
    HistoryTransformer Transformer;
    for (auto& Row : Command.Open()) {
        Access::DocumentHistoryEntryPtr Entry = new Access::DocumentHistoryEntry();
        Transformer.Load(Row, *Entry);
        Result.push_back(Entry);
    }
    
    return Result;
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

Access::DocumentDataPtr DocumentStorage::Fetch(BucketHandle handle, const string& id) const
{
    DocumentTransformer Transformer(handle->Reading());
    Access::DocumentDataPtr Result = new Access::DocumentData();
    Result->Id = id;
    
    Guard Lock(handle->ReadGuard);
    if (Transformer.Load(*Result) == false) throw Access::NotFoundError((format("a document with id %1% is not known") % id).str());
    
    return Result;
}

Access::DocumentDataPtr DocumentStorage::FetchChecked(BucketHandle handle, const string& id, const string& user) const
{
    DocumentTransformer Transformer(handle->Reading());
    Access::DocumentDataPtr Result = new Access::DocumentData();
    Result->Id = id;
    
    Guard Lock(handle->ReadGuard);
    if (Transformer.Load(*Result) == false) throw Access::NotFoundError((format("a document with id %1% is not known") % id).str());
    if (Result->Locker.empty() == false && Result->Locker != user) throw Access::LockError((format("document %1% is locked by %2%") % Result->Display % Result->Locker).str());
    if (Result->Deleted) throw Access::LockError((format("document %1% is in the deleted state") % Result->Display).str());
    
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

void DocumentStorage::InsertIntoDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const string& comment) const
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

void DocumentStorage::UpdateInDatabase(const Access::DocumentDataPtr& document, const Access::BinaryData& data, const string& user, const string& comment) const
{
    auto Handle = FetchBucket(document->Id);
    Guard Lock(Handle->WriteGuard);
    
    TransformerQueue Queue(Handle->Writing());
    Access::DocumentDataPtr Item = Fetch(Handle, document->Id);
    
    if (Item->Locker.empty() == false && Item->Locker != user) throw Access::LockError((format("document %1% is already locked by %2%") % Item->Display % Item->Locker).str());

    vector<string> Actions;

    if (document->Name != Item->Name) {
        Actions.push_back(Access::Renamed);
    }
    if (document->Display != Item->Display) {
        Actions.push_back(Access::Retitled);
    }
    if (document->Keywords != Item->Keywords) {
        Actions.push_back(Access::Keywords);
    }
    if (data.empty() == false) {
        Actions.push_back(Access::Revision);
    }
    
    Item->Name = document->Name;
    Item->Display = document->Display;
    Item->Keywords = document->Keywords;
    Item->Size = data.empty() ? Item->Size : data.size();
    
    Queue.Update(*Item);
    
    Access::DocumentHistoryEntryPtr History = new Access::DocumentHistoryEntry();
    History->Action = join(Actions, ";");
    History->Actor = document->Creator;
    History->Created = Utils::Ticks(microsec_clock::local_time());
    History->Document = document->Id;
    History->Id = Utils::NewId();
    History->Comment = comment;
    History->Revision = LatestRevision(Handle->Writing(), document->Id) + 1;
    
    Queue.Insert(*History);
    Access::DocumentContentPtr Content;
    Access::DocumentContentPtr OldData;
    
    if (data.empty() == false) {
        Content = new Access::DocumentContent();

        OldData = LatestContent(Handle->Writing(), document->Id);
        OldData->Content = BinaryData::CreatePatch(data, OldData->Content);

        Content->Id = Utils::NewId();
        Content->Content = data;
        Content->History = History->Id;
        Content->Revision = OldData->Revision + 1;
        Queue.Update(*OldData);
        Queue.Insert(*Content);
    }

    Queue.Flush();
}

int DocumentStorage::LatestRevision(SQLite::Connection* connection, const string& id) const
{
    const string QueryTemplate = "SELECT MAX(SeqId) FROM DocumentHistories WHERE Owner = '%1%'";

    auto Command = connection->Create((format(QueryTemplate) % id).str());
    return Command.ExecuteScalar<int>();
}

Access::DocumentContentPtr DocumentStorage::LatestContent(SQLite::Connection* connection, const string& id) const
{
    const string QueryTemplate =
R"(SELECT
    %1%
FROM
    DocumentContents cnt
INNER JOIN
    DocumentHistories hst
ON
    cnt.Owner = hst.Id
AND
    hst.SeqId = (SELECT MAX(SeqId) FROM DocumentHistories hin WHERE hin.Owner = hst.Owner AND EXISTS(SELECT 1 FROM DocumentContents nst WHERE nst.Owner = hin.Id))
WHERE
    hst.Owner = '%2%'
AND
    cnt.Owner = hst.Id)";

    ContentTransformer Transformer;
    Access::DocumentContentPtr Result = new Access::DocumentContent();
    auto Fields = AliasFields(ContentTransformer::FieldNames(), "cnt");
    auto& Command = connection->Create((format(QueryTemplate) % Fields % id).str());
    auto& Data = Command.Open();
    if (Transformer.Load(Data, *Result) == false) throw Access::NotFoundError((format("no content for document id %1%") % id).str());

    return Result;
}

Access::DocumentAssignmentPtr DocumentStorage::Fetch(SQLite::Connection* connection, const string& id, const string& path) const
{
    const string QueryTemplate =
R"(SELECT
    %1%
FROM
    DocumentAssignments asg
INNER JOIN
    DocumentHistories hst
ON
    asg.Owner = hst.Id AND asg.SeqId = hst.SeqId
WHERE
    asg.Path = '%2%'
AND
    hst.Owner = '%3%'
)";

    AssignmentTransformer Transformer;
    Access::DocumentAssignmentPtr Assignment = new Access::DocumentAssignment();
    auto Fields = AliasFields(AssignmentTransformer::FieldNames(), "asg");
    auto& Command = connection->Create((format(QueryTemplate) % Fields % algorithm::to_lower_copy(path) % id).str());
    auto& Data = Command.Open();
    if (Transformer.Load(Data, *Assignment) == false) throw Access::NotFoundError((format("no assignment for document id %1%") % id).str());

    return Assignment;
}

void DocumentStorage::Optimizer()
{
    vector<future<void>> Actions;
    for (auto& Handle : DistinctHandles_) {
        Actions.push_back(
            async(
                [&Handle]() {
                    Guard Lock(Handle->WriteGuard);
                    auto& Command = Handle->Reader().Create("PRAGMA optimize");
                    Command.Execute();
                }
            )
        );
    }
    
    for (auto& Action : Actions) Action.wait();
}

vector<Access::DocumentDataPtr> DocumentStorage::FetchFromAll(const string& query) const
{
    vector<future<vector<Access::DocumentDataPtr>>> Intermediates;
    DocumentTransformer Transformer;
    
    for (auto& Handle : DistinctHandles_) {
        Intermediates.push_back(
            async(
				launch::async,
                [handle = Handle, query = query, transformer = Transformer]() {
                    Guard Lock(handle->ReadGuard);
                    SQLite::Command Command(handle->Reader().Create(query));
                    vector<Access::DocumentDataPtr> Items;
                    auto ResultSet = Command.Open();
                    for (auto& Row : ResultSet) {
                        Access::DocumentDataPtr Item = new Access::DocumentData();
                        transformer.Load(Row, *Item);
                        Items.push_back(Item);
                    }
                    
                    return Items;
                }
            )
        );
    }
    
    vector<Access::DocumentDataPtr> Result;
    
    for (auto& Found : Intermediates) {
        auto Items = Found.get();
        for(auto& Item : Items) Result.push_back(Item);
    }
    
    Intermediates.clear();
    
    return Result;
}

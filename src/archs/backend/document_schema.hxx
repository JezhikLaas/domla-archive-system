#ifndef DOCUMENT_SCHEMA_HXX
#define DOCUMENT_SCHEMA_HXX

#include "sqlite.hxx"
#include "transformer.hxx"
#include "Archive.h"

namespace Archive
{
namespace Backend
{

class DocumentSchema
{
public:
    static void Ensure(const SQLite::Connection& connection);
};

class DocumentTransformer : public Transformer<Access::DocumentData>
{
protected:
    std::string TableName() const override
    {
        return "Documents";
    }
    
    std::vector<std::string> Fields() const override
    {
        return AppendFields({
            "Creator",
            "Created",
            "FileName",
            "DisplayName",
            "State",
            "Locker",
            "Keywords",
            "Size",
        });
    }
    
    void Materialize(const SQLite::ResultRow& data, Access::DocumentData& item) const override
    {
        item.Id = data.Get<std::string>("Id");
        item.Creator = data.Get<std::string>("Creator");
        item.Created = data.Get<std::int64_t>("Created");
        item.Name = data.Get<std::string>("FileName");
        item.Display = data.Get<std::string>("DisplayName");
        item.Deleted = data.Get<int>("State") == 1;
        item.Locker = data.Get<std::string>("Locker");
        item.Keywords = data.Get<std::string>("Keywords");
        item.Size = data.Get<int>("Size");
    }
    
    void Serialize(const SQLite::ParameterSet& target, const Access::DocumentData& item) const override
    {
        target["Id"].SetValue(item.Id);
        target["Creator"].SetValue(item.Creator);
        target["Created"].SetValue(item.Created);
        target["FileName"].SetValue(item.Name);
        target["DisplayName"].SetValue(item.Display);
        target["State"].SetValue(item.Deleted ? 1 : 0);
        target["Locker"].SetValue(item.Locker);
        target["Keywords"].SetValue(item.Keywords);
        target["Size"].SetValue(item.Size);
    }

public:
    DocumentTransformer() { }
    
    DocumentTransformer(const SQLite::Connection* connection)
    : Transformer(connection)
    { }

    static std::vector<std::string> FieldNames()
    {
        return {
            "id",
            "Creator",
            "Created",
            "FileName",
            "DisplayName",
            "State",
            "Locker",
            "Keywords",
            "Size",
        };
    }
};

class HistoryTransformer : public Transformer<Access::DocumentHistoryEntry>
{
protected:
    std::string TableName() const override
    {
        return "DocumentHistories";
    }
    
    std::vector<std::string> Fields() const override
    {
        return AppendFields({
            "Owner",
            "SeqId",
            "Created",
            "Action",
            "Actor",
            "Comment",
            "Source",
            "Target",
        });
    }
    
    void Materialize(const SQLite::ResultRow& data, Access::DocumentHistoryEntry& item) const override
    {
        item.Id = data.Get<std::string>("Id");
        item.Document = data.Get<std::string>("Owner");
        item.Revision = data.Get<int>("SeqId");
        item.Created = data.Get<std::int64_t>("Created");
        item.Action = data.Get<std::string>("Action");
        item.Actor = data.Get<std::string>("Actor");
        item.Comment = data.Get<std::string>("Comment");
        item.Source = data.Get<std::string>("Source");
        item.Target = data.Get<std::string>("Target");
    }
    
    void Serialize(const SQLite::ParameterSet& target, const Access::DocumentHistoryEntry& item) const override
    {
        target["Id"].SetValue(item.Id);
        target["Owner"].SetValue(item.Document);
        target["SeqId"].SetValue(item.Revision);
        target["Created"].SetValue(item.Created);
        target["Action"].SetValue(item.Action);
        target["Actor"].SetValue(item.Actor);
        target["Comment"].SetValue(item.Comment);
        target["Source"].SetValue(item.Source);
        target["Target"].SetValue(item.Target);
    }

public:
    static std::vector<std::string> FieldNames()
    {
        return {
            "id",
            "Owner",
            "SeqId",
            "Created",
            "Action",
            "Actor",
            "Comment",
            "Source",
            "Target",
        };
    }
};

class AssignmentTransformer : public Transformer<Access::DocumentAssignment>
{
protected:
    std::string TableName() const override
    {
        return "DocumentAssignments";
    }
    
    std::vector<std::string> Fields() const override
    {
        return AppendFields({
            "Owner",
            "SeqId",
            "AssignmentType",
            "AssignmentId",
            "Path",
        });
    }
    
    void Materialize(const SQLite::ResultRow& data, Access::DocumentAssignment& item) const override
    {
        item.Id = data.Get<std::string>("Id");
        item.History = data.Get<std::string>("Owner");
        item.Revision = data.Get<int>("SeqId");
        item.AssignmentType = data.Get<std::string>("AssignmentType");
        item.AssignmentId = data.Get<std::string>("AssignmentId");
        item.Path = data.Get<std::string>("Path");
    }
    
    void Serialize(const SQLite::ParameterSet& target, const Access::DocumentAssignment& item) const override
    {
        target["Id"].SetValue(item.Id);
        target["Owner"].SetValue(item.History);
        target["SeqId"].SetValue(item.Revision);
        target["AssignmentType"].SetValue(item.AssignmentType);
        target["AssignmentType"].SetValue(item.AssignmentId);
        target["Path"].SetValue(item.Path);
    }

public:
    static std::vector<std::string> FieldNames()
    {
        return {
            "id",
            "Owner",
            "SeqId",
            "AssignmentType",
            "AssignmentId",
            "Path",
        };
    }
};

class ContentTransformer : public Transformer<Access::DocumentContent>
{
protected:
    std::string TableName() const override
    {
        return "DocumentContents";
    }
    
    std::vector<std::string> Fields() const override
    {
        return AppendFields({
            "Owner",
            "SeqId",
            "Checksum",
            "Data",
        });
    }
    
    void Materialize(const SQLite::ResultRow& data, Access::DocumentContent& item) const override
    {
        item.Id = data.Get<std::string>("Id");
        item.History = data.Get<std::string>("Owner");
        item.Revision = data.Get<int>("SeqId");
        item.Checksum = data.Get<std::string>("Checksum");
        item.Content = data.GetBlob("data");
    }
    
    void Serialize(const SQLite::ParameterSet& target, const Access::DocumentContent& item) const override
    {
        target["Id"].SetValue(item.Id);
        target["Owner"].SetValue(item.History);
        target["SeqId"].SetValue(item.Revision);
        target["Checksum"].SetValue(item.Checksum);
        target["Data"].SetValue(item.Content);
    }

public:
    static std::vector<std::string> FieldNames()
    {
        return {
            "id",
            "Owner",
            "SeqId",
            "Checksum",
            "Data",
        };
    }
};

} // namespace Backend
} // namespace Archive

#endif
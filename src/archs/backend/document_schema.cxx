#include "document_schema.hxx"

using namespace std;
using namespace Archive::Backend;

namespace {

const char* SchemaCommand[] = {R"(
CREATE TABLE IF NOT EXISTS Documents(
    Id TEXT NOT NULL PRIMARY KEY,
    Creator TEXT NOT NULL,
    Created LONG NOT NULL,
    FileName TEXT NOT NULL,
    DisplayName TEXT,
    State INT NOT NULL,
    Locker TEXT,
    Keywords TEXT,
    Size INT
);
)",
R"(
CREATE INDEX IF NOT EXISTS Documents_IDX1 ON Documents(
    Keywords
);
)",
R"(
CREATE TABLE IF NOT EXISTS DocumentTags(
    Tag TEXT NOT NULL PRIMARY KEY
);
)",
R"(
CREATE TABLE IF NOT EXISTS DocumentHistories(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    Created LONG NOT NULL,
    Action TEXT NOT NULL,
    Actor TEXT NOT NULL,
    Comment TEXT,
    Source TEXT,
    Target TEXT,
    FOREIGN KEY(Owner) REFERENCES Documents(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
)",
R"(
CREATE UNIQUE INDEX IF NOT EXISTS DocumentHistories_IDX1 ON DocumentHistories(
    Owner, SeqId
);
)",
R"(
CREATE TABLE IF NOT EXISTS DocumentAssignments(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    AssignmentType TEXT,
    AssignmentId TEXT,
    Path TEXT NOT NULL,
    FOREIGN KEY(Owner) REFERENCES DocumentHistories(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
)",
R"(
CREATE INDEX IF NOT EXISTS DocumentAssignments_IDX1 ON DocumentAssignments(
    Path, Owner
);
)",
R"(
CREATE UNIQUE INDEX IF NOT EXISTS DocumentAssignments_IDX2 ON DocumentAssignments(
    Owner, SeqId
);
)",
R"(
CREATE TABLE IF NOT EXISTS DocumentContents(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    Checksum TEXT NOT NULL,
    Data BLOB NOT NULL,
    FOREIGN KEY(Owner) REFERENCES DocumentHistories(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
)",
R"(
CREATE UNIQUE INDEX IF NOT EXISTS DocumentContents_IDX1 ON DocumentContents(
    Owner, SeqId
);
)",
R"(
CREATE VIRTUAL TABLE IF NOT EXISTS DocumentMetas USING fts5(
    Owner UNINDEXED,
    Tags
);
)",
R"(
CREATE TRIGGER IF NOT EXISTS Documents_Del AFTER DELETE ON Documents BEGIN
  DELETE FROM DocumentMetas WHERE Owner = old.Id;
END;
)"
};

} // anonymous namespace

void DocumentSchema::Ensure(const SQLite::Connection& connection)
{
    for (auto& Sql : SchemaCommand) {
        auto& Command = connection.Create(Sql);
        Command.Execute();
    }
}

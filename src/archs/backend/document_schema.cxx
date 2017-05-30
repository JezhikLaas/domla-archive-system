#include "document_schema.hxx"

using namespace std;
using namespace Archive::Backend;

namespace {

const char* SchemaCommand = R"(
CREATE TABLE IF NOT EXISTS Documents(
    Id TEXT NOT NULL PRIMARY KEY,
    Creator TEXT NOT NULL,
    Created INT NOT NULL,
    FileName TEXT NOT NULL,
    DisplayName TEXT,
    State INT NOT NULL,
    Locker TEXT,
    Keywords TEXT,
    Size INT
);
CREATE INDEX IF NOT EXISTS Documents_IDX1 ON Documents(
    Keywords
);
CREATE TABLE IF NOT EXISTS DocumentHistories(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    Created INT NOT NULL,
    Action TEXT NOT NULL,
    Actor TEXT NOT NULL,
    FOREIGN KEY(Owner) REFERENCES Documents(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX IF NOT EXISTS DocumentHistories_IDX1 ON DocumentHistories(
    Owner, SeqId
);
CREATE TABLE IF NOT EXISTS DocumentAssignments(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    AssignmentType TEXT,
    AssignmentId TEXT,
    Path TEXT NOT NULL,
    FOREIGN KEY(Owner) REFERENCES DocumentHistories(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE INDEX IF NOT EXISTS DocumentAssignments_IDX1 ON DocumentAssignments(
    Path, Owner
);
CREATE UNIQUE INDEX IF NOT EXISTS DocumentAssignments_IDX2 ON DocumentAssignments(
    Owner, SeqId
);
CREATE TABLE IF NOT EXISTS DocumentContents(
    Id TEXT NOT NULL PRIMARY KEY,
    Owner TEXT NOT NULL,
    SeqId INT NOT NULL,
    Checksum TEXT NOT NULL,
    Data BLOB NOT NULL,
    FOREIGN KEY(Owner) REFERENCES DocumentHistories(Id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
CREATE UNIQUE INDEX IF NOT EXISTS DocumentContents_IDX1 ON DocumentContents(
    Owner, SeqId
);
)";

} // anonymous namespace

void DocumentSchema::Ensure(const SQLite::Connection& connection)
{
    auto Command = connection.Create(SchemaCommand);
    Command.Execute();
}

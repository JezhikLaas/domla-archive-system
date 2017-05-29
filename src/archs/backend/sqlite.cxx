#include <algorithm>
#include <array>
#include <initializer_list>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "sqlite.hxx"
#include "sqlite/sqlite3.h"

using namespace std;
using namespace Archive::Backend::SQLite;

namespace
{

int CheckAndThrow(int code, sqlite3* handle, int line, const char* file)
{
    if (code != SQLITE_OK) throw sqlite_exception(sqlite3_errmsg(handle), code, line, file);
    return code;
}

int CheckAndThrow(int code, const initializer_list<int>& accepted, sqlite3* handle, int line, const char* file)
{
    if (count(accepted.begin(), accepted.end(), code) == 0) throw sqlite_exception(sqlite3_errmsg(handle), code, line, file);
    return code;
}

#define CHECK_AND_THROW(op, handle) CheckAndThrow(op, handle, __LINE__, __FILE__)
#define CHECKS_AND_THROW(op, list, handle) CheckAndThrow(op, list, handle, __LINE__, __FILE__)
#define THROW(msg) throw sqlite_exception(msg, SQLITE_ERROR, __LINE__, __FILE__);
    
} // anonymous namespace

struct Connection::Implementation
{
    Implementation(Configuration configuration)
    : Handle(nullptr), Setup(configuration)
    { }
    
    ~Implementation()
    {
        if (Handle != nullptr) sqlite3_close(Handle);
        Handle = nullptr;
    }
    
    sqlite3* Handle;
    Configuration Setup;
    
    void Open()
    {
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle, SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE, nullptr), Handle);
    }
    
    void Create()
    {
        if (Setup.Path.size() && Setup.Path[0] != ':') {
            boost::filesystem::path FilePath(Setup.Path);
            if (boost::filesystem::is_regular_file(FilePath)) THROW((boost::format("file %1% exists") % Setup.Path).str());
            if (boost::filesystem::is_directory(FilePath)) THROW((boost::format("directory %1% exists") % Setup.Path).str());
        }
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
    }
    
    void CreateOrOpen()
    {
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
    }
    
    void AlwaysCreate()
    {
        if (Setup.Path.size() && Setup.Path[0] != ':') {
            boost::filesystem::path FilePath(Setup.Path);
            if (boost::filesystem::is_directory(FilePath)) THROW((boost::format("directory %1% exists") % Setup.Path).str());
            if (boost::filesystem::is_regular_file(FilePath)) boost::filesystem::remove(FilePath);
        }
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
    }
};

struct ResultSet::Implementation
{
    Implementation(sqlite3_stmt* statement, bool data)
    : Handle(statement), Data(data)
    { }
    
    sqlite3_stmt* Handle;
    bool Data;
};

struct Command::Implementation
{
    Implementation(Command& owner)
    : Owner(owner), Handle(nullptr), Parameters(ParameterList)
    { }
    
    ~Implementation()
    {
        if (Handle != nullptr) sqlite3_finalize(Handle);
        Handle = nullptr;
    }
    
    Command& Owner;
    sqlite3_stmt* Handle;
    vector<Parameter> ParameterList;
    ParameterSet Parameters;
    
    void Prepare(sqlite3* handle, const string& sql)
    {
        CHECK_AND_THROW(sqlite3_prepare_v2(
            handle,
            sql.c_str(),
            -1,
            &Handle,
            nullptr
        ),
        handle);
        
        for (int Index = 1; Index <= sqlite3_bind_parameter_count(Handle); ++Index) {
            ParameterList.push_back(Parameter(Owner, sqlite3_bind_parameter_name(Handle, Index), sqlite3_bind_parameter_name(Handle, Index) + 1));
        }
    }
    
    void BindParameters()
    {
        for (auto& Item : ParameterList) {
            if (Item.IsEmpty()) {
                sqlite3_bind_null(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()));
            }
            else if (Item.Value().type() == typeid(int)) {
                sqlite3_bind_int(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<int>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(double)) {
                sqlite3_bind_double(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<double>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(float)) {
                sqlite3_bind_double(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), (double)boost::any_cast<float>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(string)) {
                const string& Buffer = boost::any_cast<const string&>(Item.Value_);
                sqlite3_bind_text(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), Buffer.c_str(), -1, nullptr);
            }
            else if (Item.Value().type() == typeid(vector<unsigned char>)) {
				const vector<unsigned char>& Buffer = boost::any_cast<const vector<unsigned char>&>(Item.Value_);
                const void* Pointer = Buffer.empty() ? nullptr : &Buffer[0];
                sqlite3_bind_blob(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), Pointer, Buffer.size(), nullptr);
            }
            else if (Item.Value().type() == typeid(const char*)) {
                sqlite3_bind_text(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<const char*>(Item.Value_), -1, nullptr);
            }
            else if (Item.Value().type() == typeid(const void*)) {
                sqlite3_bind_blob(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<const void*>(Item.Value_), Item.RawSize(), nullptr);
            }
        }
    }
    
    int ExecuteScalarInt()
    {
        BindParameters();
        
        switch (CHECKS_AND_THROW(sqlite3_step(Handle), { SQLITE_ROW }, sqlite3_db_handle(Handle))) {
            case SQLITE_ROW:
                return sqlite3_column_int(Handle, 0);
        }
        
        return 0;
    }
    
    void Execute()
    {
        BindParameters();
        
        auto Accepted = { SQLITE_OK, SQLITE_DONE, SQLITE_ROW };
        CHECKS_AND_THROW(sqlite3_step(Handle), Accepted, sqlite3_db_handle(Handle));
    }
    
    ResultSet Open()
    {
        BindParameters();
        
        auto Accepted = { SQLITE_OK, SQLITE_DONE, SQLITE_ROW };
        auto HasRow = CHECKS_AND_THROW(sqlite3_step(Handle), Accepted, sqlite3_db_handle(Handle)) == SQLITE_ROW;
        return ResultSet(Owner, HasRow);
    }
};

struct Transaction::Implementation
{
    sqlite3* Handle;
};

Parameter::Parameter(const Command& owner)
: Owner_(owner)
{ }

Parameter::Parameter(const Command& owner, const string& realName, const string& name)
: Owner_(owner), RealName_(realName), Name_(name)
{ }

void Parameter::SetRawValue(const void* data, int size)
{
    Value_ = data;
    RawSize_ = size;
}

Parameter& ParameterSet::operator[](const std::string key) const
{
    auto Result = find_if(
        Parameters_.begin(),
        Parameters_.end(),
        [&key](Parameter item) { return boost::algorithm::to_lower_copy(item.Name()) == boost::algorithm::to_lower_copy(key); }
    );
    if (Result == Parameters_.end()) throw runtime_error("key not found");
    
    return *Result;
}

int ResultRow::ColumnIndex(const std::string& name) const
{
    int Index = 0;
    while (_stricmp(name.c_str(), sqlite3_column_name(Owner_.Inner->Handle, Index)) && Index < sqlite3_column_count(Owner_.Inner->Handle)) ++Index;
    if (Index == sqlite3_column_count(Owner_.Inner->Handle)) throw runtime_error("key not found");
    
    return Index;
}

template <>
int ResultRow::Get(const std::string& name) const
{
    return sqlite3_column_int(Owner_.Inner->Handle, ColumnIndex(name));
}

template <>
int ResultRow::Get(int index) const
{
    return sqlite3_column_int(Owner_.Inner->Handle, index);
}

template <>
string ResultRow::Get(const std::string& name) const
{
    return reinterpret_cast<const char*>(sqlite3_column_text(Owner_.Inner->Handle, ColumnIndex(name)));
}

template <>
string ResultRow::Get(int index) const
{
    return reinterpret_cast<const char*>(sqlite3_column_text(Owner_.Inner->Handle, index));
}

vector<unsigned char> ResultRow::GetBlob(int index) const
{
    auto Size = sqlite3_column_bytes(Owner_.Inner->Handle, index);
    vector<unsigned char> Result((vector<unsigned char>::size_type)Size);
    if (Size) memcpy(&Result[0], sqlite3_column_blob(Owner_.Inner->Handle, index), Size);
    
    return Result;
}

vector<unsigned char> ResultRow::GetBlob(const std::string& name) const
{
    return GetBlob(ColumnIndex(name));
}

ResultSet::ResultSet(const Command& command, bool hasRow)
: Inner(new Implementation(command.Inner->Handle, hasRow)), Data_(*this)
{ }

ResultSet::ResultSet(ResultSet&& other)
: Inner(nullptr), Data_(*this)
{
    Inner = other.Inner;
    other.Inner = nullptr;
}

ResultSet::~ResultSet()
{
    delete Inner;
    Inner = nullptr;
}

int ResultSet::Fields() const
{
    return sqlite3_column_count(Inner->Handle);
}

ResultSet::iterator ResultSet::begin()
{
    return iterator(this, Inner->Data == false);
}

ResultSet::iterator ResultSet::end()
{
    return iterator();
}
            
void ResultSet::iterator::increment()
{
    if (sqlite3_step(Owner_->Inner->Handle) == SQLITE_DONE) Done_ = true;
}

bool ResultSet::iterator::equal(iterator const& other) const
{
    return Done_ == other.Done_;
}

const ResultRow& ResultSet::iterator::dereference() const
{
    return Owner_->Data_;
}

Command::Command(const string& sql)
: Inner(new Command::Implementation(*this))
{
    
}

Command::Command(Command&& other)
: Inner(nullptr)
{
    Inner = other.Inner;
    other.Inner = nullptr;
}

Command::~Command()
{
    delete Inner;
    Inner = nullptr;
}

const ParameterSet& Command::Parameters() const
{
    return Inner->Parameters;
}

void Command::Execute()
{
    Inner->Execute();
}

template <>
int Command::ExecuteScalar()
{
    return Inner->ExecuteScalarInt();
}

ResultSet Command::Open()
{
    return Inner->Open();
}

Transaction::Transaction()
: Inner(new Transaction::Implementation())
{ }

Connection::Connection(const Configuration& configuration)
: Inner(new Connection::Implementation(configuration))
{
}

Connection::~Connection()
{
    delete Inner;
    Inner = nullptr;
}

void Connection::Open()
{
    Inner->Open();
}

void Connection::OpenNew()
{
    Inner->AlwaysCreate();
}

void Connection::OpenAlways()
{
    Inner->CreateOrOpen();
}

Command Connection::Create(const string& command)
{
    auto Result = Command(command);
    Result.Inner->Prepare(Inner->Handle, command);
    return Result;
}

shared_ptr<Command> Connection::CreateFree(const string& command)
{
    auto Result = shared_ptr<Command>(new Command(command));
    Result->Inner->Prepare(Inner->Handle, command);
    return Result;
}

bool Connection::IsOpen() const
{
    return Inner->Handle != nullptr;
}

Transaction::Transaction(Transaction&& other)
: Inner(nullptr)
{
    Inner = other.Inner;
    other.Inner = nullptr;
}

Transaction::~Transaction()
{
    try {
        Rollback();
    }
    catch(...) { }
    
    delete Inner;
    Inner = nullptr;
}

void Transaction::Commit()
{
    if (Inner == nullptr || Inner->Handle == nullptr) return;
    CHECK_AND_THROW(sqlite3_exec(Inner->Handle, "COMMIT", nullptr, nullptr, nullptr), Inner->Handle);
    Inner->Handle = nullptr;
}

void Transaction::Rollback()
{
    if (Inner == nullptr || Inner->Handle == nullptr) return;
    CHECK_AND_THROW(sqlite3_exec(Inner->Handle, "ROLLBACK", nullptr, nullptr, nullptr), Inner->Handle);
    Inner->Handle = nullptr;
}

Transaction Connection::Begin()
{
    Transaction Result;
    CHECK_AND_THROW(sqlite3_exec(Inner->Handle, "BEGIN", nullptr, nullptr, nullptr), Inner->Handle);
    Result.Inner->Handle = Inner->Handle;
    
    return Result;
}

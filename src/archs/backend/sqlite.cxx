#include <algorithm>
#include <array>
#include <initializer_list>
#include <unordered_map>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "sqlite.hxx"
#include "sqlite/sqlite3.h"

using namespace std;
using namespace Archive::Backend::SQLite;

namespace
{
    
extern "C"
void PartsCount(sqlite3_context* context, int countOfArguments, sqlite3_value** arguments)
{
    if (countOfArguments != 2) {
        sqlite3_result_error(context, "PARTSCOUNT expects two strings", -1);
        return;
    }
    
    auto Value = sqlite3_value_text(arguments[0]);
    auto Separator = sqlite3_value_text(arguments[1]);
    auto Length = strlen(reinterpret_cast<const char*>(Value));
    auto SeparatorLength = strlen(reinterpret_cast<const char*>(Separator));
    
    if (SeparatorLength == 0) {
        sqlite3_result_error(context, "PARTSCOUNT, separator must not be empty", -1);
        return;
    }
    
    int Found = 0;
    unsigned SeparatorIndex = 0;
    bool InPart = false;

    for (unsigned Index = 0; Index < Length; ++Index) {
        unsigned SeparatorIndex = 0;
        while (Index < Length && SeparatorIndex < SeparatorLength && Value[Index] == Separator[SeparatorIndex]) {
            ++Index;
            ++SeparatorIndex;
        }
        
        // Input finished?
        if (Index == Length) break;
        
        // Separator completed
        if (SeparatorIndex == SeparatorLength) {
            //Next part starts
            InPart = false;
            continue;
        }
        
        // Already in part, don't count every character as match
        if (InPart) continue;
        
        ++Found;
        InPart = true;
    }
    
    sqlite3_result_int(context, Found);
}

int CheckAndThrow(int code, sqlite3* handle, int line, const char* file)
{
	if (code != SQLITE_OK) {
		throw sqlite_exception(sqlite3_errmsg(handle), code, line, file);
	}
    return code;
}

int CheckAndThrow(int code, const initializer_list<int>& accepted, sqlite3* handle, int line, const char* file)
{
	if (count(accepted.begin(), accepted.end(), code) == 0) {
		throw sqlite_exception(sqlite3_errmsg(handle), code, line, file);
	}
    return code;
}

#define CHECK_AND_THROW(op, handle) CheckAndThrow(op, handle, __LINE__, __FILE__)
#define CHECKS_AND_THROW(op, list, handle) CheckAndThrow(op, list, handle, __LINE__, __FILE__)
#define THROW(msg) throw sqlite_exception(msg, SQLITE_ERROR, __LINE__, __FILE__);
    
} // anonymous namespace

struct Connection::Implementation
{
#ifdef _DEBUG
	static int ActiveConnections;
#endif
	
	Implementation(Configuration configuration)
    : Handle(nullptr), Setup(configuration)
    {
#ifdef _DEBUG
		++ActiveConnections;
#endif
	}
    
    ~Implementation()
    {
        if (Handle != nullptr) sqlite3_close(Handle);
        Handle = nullptr;
#ifdef _DEBUG
		--ActiveConnections;
#endif
	}
    
    sqlite3* Handle;
    Configuration Setup;
    
    int ReadSingleInteger(const string& sql)
    {
        sqlite3_stmt* Statement = nullptr;
        try {
            CHECK_AND_THROW(sqlite3_prepare_v2(
                Handle,
                sql.c_str(),
                -1,
                &Statement,
                nullptr
            ),
            Handle);
            
            CHECKS_AND_THROW(sqlite3_step(Statement), { SQLITE_ROW }, Handle);
            
            auto Result = sqlite3_column_int(Statement, 0);
            sqlite3_finalize(Statement);
            
            return Result;
        }
        catch(...) {
            if (Statement != nullptr) sqlite3_finalize(Statement);
            throw;
        }
    }
    
    string ReadSingleString(const string& sql)
    {
        sqlite3_stmt* Statement = nullptr;
        try {
            CHECK_AND_THROW(sqlite3_prepare_v2(
                Handle,
                sql.c_str(),
                -1,
                &Statement,
                nullptr
            ),
            Handle);
            
            CHECKS_AND_THROW(sqlite3_step(Statement), { SQLITE_ROW }, Handle);
            
            auto Result = reinterpret_cast<const char*>(sqlite3_column_text(Statement, 0));
            sqlite3_finalize(Statement);
            
            return Result;
        }
        catch(...) {
            if (Statement != nullptr) sqlite3_finalize(Statement);
            throw;
        }
    }
    
    Configuration ReadCurrentSetup()
    {
        Configuration Result;
        
        Result.BusyTimeout = ReadSingleInteger("PRAGMA busy_timeout");
        Result.CacheSize = ReadSingleInteger("PRAGMA cache_size");
        Result.ForeignKeys = ReadSingleInteger("PRAGMA foreign_keys") == 1;
        Result.MaxPageCount = ReadSingleInteger("PRAGMA max_page_count");
        Result.PageSize = ReadSingleInteger("PRAGMA page_size");
        Result.TransactionIsolation = ReadSingleInteger("PRAGMA read_uncommitted") == 0 ? Configuration::IsolationLevel::Serializable : Configuration::IsolationLevel::ReadUncommitted;
        
        auto Journal = ReadSingleString("PRAGMA journal_mode");
        
        if (Journal == "wal") {
            Result.Journal = Configuration::JournalMode::Wal;
        }
        else if (Journal == "delete") {
            Result.Journal = Configuration::JournalMode::Delete;
        }
        else if (Journal == "truncate") {
            Result.Journal = Configuration::JournalMode::Truncate;
        }
        else if (Journal == "persist") {
            Result.Journal = Configuration::JournalMode::Persist;
        }
        else if (Journal == "memory") {
            Result.Journal = Configuration::JournalMode::Memory;
        }
        else {
            Result.Journal = Configuration::JournalMode::Off;
        }
        
        return Result;
    }
    
    void ApplySetup()
    {
        auto Modified = false;
        auto Current = ReadCurrentSetup();
        Current.Path = Setup.Path;
        
        if (Setup.PageSize != Current.PageSize) {
            auto ResetMode = false;
            if (Current.Journal == Configuration::JournalMode::Wal) {
                ResetMode = true;
                CHECK_AND_THROW(sqlite3_exec(Handle, "PRAGMA journal_mode=DELETE", nullptr, nullptr, nullptr), Handle);
            }
            CHECK_AND_THROW(sqlite3_exec(Handle, (string("PRAGMA page_size=") + to_string(Setup.PageSize)).c_str(), nullptr, nullptr, nullptr), Handle);
            CHECK_AND_THROW(sqlite3_exec(Handle, "VACUUM", nullptr, nullptr, nullptr), Handle);
            
            if (ResetMode) CHECK_AND_THROW(sqlite3_exec(Handle, "PRAGMA journal_mode=wal", nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.BusyTimeout != Current.BusyTimeout) {
            CHECK_AND_THROW(sqlite3_exec(Handle, (string("PRAGMA busy_timeout=") + to_string(Setup.BusyTimeout)).c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.CacheSize != Current.CacheSize) {
            CHECK_AND_THROW(sqlite3_exec(Handle, (string("PRAGMA cache_size=") + to_string(Setup.CacheSize)).c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.ForeignKeys != Current.ForeignKeys) {
            CHECK_AND_THROW(sqlite3_exec(Handle, (string("PRAGMA foreign_keys=") + (Setup.ForeignKeys ? "1" : "0")).c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.MaxPageCount != Current.MaxPageCount) {
            CHECK_AND_THROW(sqlite3_exec(Handle, (string("PRAGMA max_page_count=") + to_string(Setup.MaxPageCount)).c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.Journal != Current.Journal) {
            string Mode = "delete";
            switch (Setup.Journal) {
                case Configuration::JournalMode::Delete:
                    Mode = "delete";
                    break;
                case Configuration::JournalMode::Truncate:
                    Mode = "truncate";
                    break;
                case Configuration::JournalMode::Persist:
                    Mode = "persist";
                    break;
                case Configuration::JournalMode::Memory:
                    Mode = "memory";
                    break;
                case Configuration::JournalMode::Wal:
                    Mode = "wal";
                    break;
                case Configuration::JournalMode::Off:
                    Mode = "off";
                    break;
            }
			string Command = "PRAGMA journal_mode=";
			Command += Mode;
            CHECK_AND_THROW(sqlite3_exec(Handle, Command.c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
        if (Setup.TransactionIsolation != Current.TransactionIsolation) {
			string Command = "PRAGMA read_uncommitted=";
			Command += Setup.TransactionIsolation == Configuration::IsolationLevel::ReadUncommitted ? "1" : "0";
            CHECK_AND_THROW(sqlite3_exec(Handle, Command.c_str(), nullptr, nullptr, nullptr), Handle);
            Modified = true;
        }
    }
    
    void RegisterFunctions()
    {
        sqlite3_create_function_v2(
            Handle,
            "PARTSCOUNT",
            2,
            SQLITE_UTF8 | SQLITE_DETERMINISTIC,
            nullptr,
            PartsCount,
            nullptr,
            nullptr,
            nullptr
        );
    }
    
    void Open()
    {
        auto Flags = Setup.ReadOnly
                     ?
                     SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READONLY
                     :
                     SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_READWRITE;
                     
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle, Flags, nullptr), Handle);
        if (Setup.ReadOnly == false) ApplySetup();
        RegisterFunctions();
    }
    
    void Create()
    {
        if (Setup.Path.size() && Setup.Path[0] != ':') {
            boost::filesystem::path FilePath(Setup.Path);
            if (boost::filesystem::is_regular_file(FilePath)) THROW((boost::format("file %1% exists") % Setup.Path).str());
            if (boost::filesystem::is_directory(FilePath)) THROW((boost::format("directory %1% exists") % Setup.Path).str());
        }
        
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
        ApplySetup();
        RegisterFunctions();
    }
    
    void CreateOrOpen()
    {
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
        ApplySetup();
        RegisterFunctions();
    }
    
    void AlwaysCreate()
    {
        if (Setup.Path.size() && Setup.Path[0] != ':') {
            boost::filesystem::path FilePath(Setup.Path);
            if (boost::filesystem::is_directory(FilePath)) THROW((boost::format("directory %1% exists") % Setup.Path).str());
            if (boost::filesystem::is_regular_file(FilePath)) boost::filesystem::remove(FilePath);
        }
        
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
        ApplySetup();
        RegisterFunctions();
    }
};

#ifdef _DEBUG
int Connection::Implementation::ActiveConnections = 0;
#endif

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
#ifdef _DEBUG
	static int ActiveCommands;
#endif

    Implementation(Command& owner)
    : Owner(owner), Handle(nullptr), Parameters(ParameterList)
    {
#ifdef _DEBUG
		++ActiveCommands;
#endif
	}
    
    ~Implementation()
    {
        if (Handle != nullptr) sqlite3_finalize(Handle);
        Handle = nullptr;
#ifdef _DEBUG
		--ActiveCommands;
#endif
	}

	Implementation(const Implementation& other) = delete;
	void operator=(const Implementation& other) = delete;

    Command& Owner;
    sqlite3_stmt* Handle;
    vector<Parameter> ParameterList;
    ParameterSet Parameters;
    
    void Prepare(sqlite3* handle, const string& sql)
    {
		if (Handle != nullptr) sqlite3_finalize(Handle);
		Handle = nullptr;
		
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
            else if (Item.Value().type() == typeid(int64_t)) {
                sqlite3_bind_int64(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<int64_t>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(double)) {
                sqlite3_bind_double(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), boost::any_cast<double>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(float)) {
                sqlite3_bind_double(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), (double)boost::any_cast<float>(Item.Value_));
            }
            else if (Item.Value().type() == typeid(string)) {
                auto& Buffer = boost::any_cast<const string&>(Item.Value_);
                sqlite3_bind_text(Handle, sqlite3_bind_parameter_index(Handle, Item.RealName_.c_str()), Buffer.c_str(), -1, nullptr);
            }
            else if (Item.Value().type() == typeid(vector<unsigned char>)) {
				auto& Buffer = boost::any_cast<const vector<unsigned char>&>(Item.Value_);
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

#ifdef _DEBUG
int Command::Implementation::ActiveCommands = 0;
#endif

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

Parameter& ParameterSet::operator[](const std::string& key) const
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
int64_t ResultRow::Get(const std::string& name) const
{
    return sqlite3_column_int64(Owner_.Inner->Handle, ColumnIndex(name));
}

template <>
int ResultRow::Get(int index) const
{
    return sqlite3_column_int(Owner_.Inner->Handle, index);
}

template <>
int64_t ResultRow::Get(int index) const
{
    return sqlite3_column_int64(Owner_.Inner->Handle, index);
}

template <>
string ResultRow::Get(const std::string& name) const
{
	auto data = sqlite3_column_text(Owner_.Inner->Handle, ColumnIndex(name));
    return data != nullptr ? reinterpret_cast<const char*>(data) : "";
}

template <>
string ResultRow::Get(int index) const
{
	auto data = sqlite3_column_text(Owner_.Inner->Handle, index);
	return data != nullptr ? reinterpret_cast<const char*>(data) : "";
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

bool ResultSet::HasData() const
{
    return Inner->Data;
}

ResultSet::iterator ResultSet::begin() const
{
    return iterator(this, Inner->Data == false);
}

ResultSet::iterator ResultSet::end() const
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
{ }

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

Command Connection::Create(const string& command) const
{
    auto Result = Command(command);
    Result.Inner->Prepare(Inner->Handle, command);
    return Result;
}

shared_ptr<Command> Connection::CreateFree(const string& command) const
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

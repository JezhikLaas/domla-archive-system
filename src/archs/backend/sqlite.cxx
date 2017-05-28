#include <algorithm>
#include <initializer_list>
#include <boost/algorithm/string.hpp>
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
        CHECK_AND_THROW(sqlite3_open_v2(Setup.Path.c_str(), &Handle,  SQLITE_OPEN_SHAREDCACHE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | SQLITE_OPEN_CREATE, nullptr), Handle);
    }
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
    ParameterRange Parameters;
    
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
            ParameterList.push_back(Parameter(Owner, sqlite3_bind_parameter_name(Handle, Index) + 1));
        }
    }
    
    int ExecuteScalarInt()
    {
        switch (CHECKS_AND_THROW(sqlite3_step(Handle), { SQLITE_ROW }, sqlite3_db_handle(Handle))) {
            case SQLITE_ROW:
                return sqlite3_column_int(Handle, 0);
        }
        
        return 0;
    }
    
    void Execute()
    {
        auto Accepted = { SQLITE_OK, SQLITE_DONE, SQLITE_ROW };
        CHECKS_AND_THROW(sqlite3_step(Handle), Accepted, sqlite3_db_handle(Handle));
    }
};

Parameter::Parameter(const Command& owner)
: Owner_(owner)
{ }

Parameter::Parameter(const Command& owner, const string& name)
: Owner_(owner), Name_(name)
{ }

Parameter& ParameterRange::operator[](const std::string key) const
{
    auto Result = find_if(
        Parameters_.begin(),
        Parameters_.end(),
        [&key](Parameter item) { return boost::algorithm::to_lower_copy(item.Name()) == boost::algorithm::to_lower_copy(key); }
    );
    if (Result == Parameters_.end()) throw runtime_error("key not found");
    
    return *Result;
}

Command::Command(const string& sql)
: Inner(new Command::Implementation(*this))
{
    
}

Command::~Command()
{
    delete Inner;
    Inner = nullptr;
}

const ParameterRange& Command::Parameters() const
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
    Inner->Create();
}

unique_ptr<Command> Connection::Create(const string& command)
{
    auto Result = unique_ptr<Command>(new Command(command));
    Result->Inner->Prepare(Inner->Handle, command);
    return Result;
}

unique_ptr<Command> Connection::Create(const string& command, const Transaction& transaction)
{
    return unique_ptr<Command>(new Command(command));
}

bool Connection::IsOpen() const
{
    return Inner->Handle != nullptr;
}
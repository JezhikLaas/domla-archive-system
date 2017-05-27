#include "sqlite.hxx"
#include "sqlite/sqlite3.h"

using namespace std;
using namespace Archive::Backend;

struct Connection::Implementation
{
};

Connection::Connection(const string& connectionString)
: Inner(new Connection::Implementation())
{
    
}

Connection::~Connection()
{
    delete Inner;
    Inner = nullptr;
}

unique_ptr<Command> Connection::Create(const string& command)
{
    return unique_ptr<Command>(new Command());
}

unique_ptr<Command> Connection::Create(const string& command, const Transaction& transaction)
{
    return unique_ptr<Command>(new Command());
}

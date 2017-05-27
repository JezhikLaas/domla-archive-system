#include <string>
#include <memory>

namespace Archive
{

namespace Backend
{

class Parameter
{
};

class Command
{
};

class Transaction
{
};

class Connection
{
private:
    struct Implementation;
    Implementation* Inner;
    
public:
    explicit Connection(const std::string& connectionString);
    ~Connection();
    Connection(const Connection&) = delete;
    void operator=(const Connection&) = delete;
    
    std::unique_ptr<Command> Create(const std::string& command);
    std::unique_ptr<Command> Create(const std::string& command, const Transaction& transaction);
};

}

}
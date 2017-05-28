#ifndef SQLITE_HXX
#define SQLITE_HXX

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <boost/any.hpp>

namespace Archive
{
namespace Backend
{
namespace SQLite
{
    
class Command;

class Parameter
{
friend class Command;

private:
    const Command& Owner_;
    std::string Name_;
    boost::any Value_;
    explicit Parameter(const Command& owner);
    explicit Parameter(const Command& owner, const std::string& name);

public:
    void SetName(const std::string& name) { Name_ = name; }
    const std::string& Name() const { return Name_; }
    template <typename T>
    void SetValue(T&& value) { Value_ = value; }
    const boost::any& Value() const { return Value_; }
    void Clear() { Value_ = boost::any(); }
    bool IsEmpty() const { return Value_.empty(); }
};

class Command
{
friend class Connection;

private:
    typedef std::tuple<std::vector<Parameter>::const_iterator, std::vector<Parameter>::const_iterator> ParameterRange;
    
    struct Implementation;
    Implementation* Inner;

    explicit Command(const std::string& sql);
    
public:
    ~Command();
    const ParameterRange Parameters() const;
    void Execute();
    template <typename T> T ExecuteScalar();
};

class Transaction
{
};

struct Configuration
{
    std::string Path;
};

class Connection
{
private:
    struct Implementation;
    Implementation* Inner;
    
public:
    explicit Connection(const Configuration& configuration);
    ~Connection();
    Connection(const Connection&) = delete;
    void operator=(const Connection&) = delete;
    
    void Open();
    void OpenNew();
    std::unique_ptr<Command> Create(const std::string& command);
    std::unique_ptr<Command> Create(const std::string& command, const Transaction& transaction);
    bool IsOpen() const;
};

class sqlite_exception : public std::runtime_error
{
private:
    int Code_;
    int Line_;
    std::string File_;
    
public:
    sqlite_exception(const std::string& msg, int code, int line, const char* file)
    : runtime_error(msg), Code_(code), Line_(line), File_(file)
    { }
    
    int code() const { return Code_; }
    int line() const { return Line_; }
    const char* where() const { return File_.c_str(); }
};

} // namespace SQLite
} // namespace Backend
} // namespace Archive

#endif

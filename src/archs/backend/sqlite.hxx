#ifndef SQLITE_HXX
#define SQLITE_HXX

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <boost/any.hpp>
#include <boost/iterator/iterator_facade.hpp>

namespace Archive
{
namespace Backend
{
namespace SQLite
{
    
class Command;
class ResultSet;

class Parameter
{
friend class Command;

private:
    const Command& Owner_;
    std::string RealName_;
    std::string Name_;
    boost::any Value_;
    int RawSize_;
    explicit Parameter(const Command& owner);
    explicit Parameter(const Command& owner, const std::string& realName, const std::string& name);

public:
    void SetName(const std::string& name) { Name_ = name; }
    const std::string& Name() const { return Name_; }
    template <typename T>
    void SetValue(T&& value) { Value_ = value; }
    void SetRawValue(const void* data, int size);
    const boost::any& Value() const { return Value_; }
    void Clear() { Value_ = boost::any(); }
    bool IsEmpty() const { return Value_.empty(); }
    int RawSize() const { return RawSize_; }
};

class ParameterSet
{
private:
    std::vector<Parameter>& Parameters_;
    
public:
    ParameterSet(std::vector<Parameter>& parameters)
    : Parameters_(parameters)
    { }
    
    class iterator : public boost::iterator_facade<iterator, Parameter, boost::random_access_traversal_tag>
    {
        public:
            iterator()
            { }
        
            iterator(std::vector<Parameter>::iterator position)
            : Position_(position)
            { }
        
        private:
            friend class boost::iterator_core_access;
            
            std::vector<Parameter>::iterator Position_;
            void increment() { ++Position_; }
            void decrement() { --Position_; }
            bool equal(iterator const& other) const
            {
                return Position_ == other.Position_;
            }
            Parameter& dereference() const { return *Position_; }
    };

    iterator begin() const { return iterator(Parameters_.begin()); }
    iterator end() const { return iterator(Parameters_.end()); }
    Parameter& operator[](const std::string key) const;
};

class ResultRow
{
private:
    const ResultSet& Owner_;
    
public:
    ResultRow(const ResultSet& owner)
    : Owner_(owner)
    { }
    
    ResultRow(const ResultRow&) = delete;
    void operator= (const ResultRow&) = delete;
    template <typename T> T Get(const std::string& name) const;
    template <typename T> T Get(int index) const;
    std::vector<unsigned char> GetBlob(int index) const;
    std::vector<unsigned char> GetBlob(const std::string& name) const;
    int ColumnIndex(const std::string& name) const;
};

class ResultSet
{
friend class ResultRow;

private:
    struct Implementation;
    Implementation* Inner;
    ResultRow Data_;
    
public:
    ResultSet(const Command& command, bool data);
    ResultSet(ResultSet&& other);
    ~ResultSet();
    ResultSet(const ResultSet&) = delete;
    void operator= (const ResultSet&) = delete;
    
    class iterator : public boost::iterator_facade<iterator, const ResultRow, boost::forward_traversal_tag>
    {
        public:
            iterator()
            : Owner_(nullptr), Done_(true)
            { }
        
            iterator(const ResultSet* result, bool done)
            : Owner_(result), Done_(done)
            { }
        
        private:
            friend class boost::iterator_core_access;
            const ResultSet* Owner_;
            bool Done_;
            
            void increment();
            bool equal(iterator const& other) const;
            const ResultRow& dereference() const;
    };

    iterator begin();
    iterator end();
    int Fields() const;
    bool HasData() const;
};

class Command
{
friend class Connection;
friend class ResultSet;

private:
    struct Implementation;
    Implementation* Inner;

    explicit Command(const std::string& sql);
    
public:
    Command(Command& other) = delete;
    void operator= (Command& other) = delete;
    Command(Command&& other);
    ~Command();
    
    const ParameterSet& Parameters() const;
    void Execute();
    template <typename T> T ExecuteScalar();
    ResultSet Open();
};

class Transaction
{
friend class Connection;

private:
    struct Implementation;
    Implementation* Inner;

    Transaction();

public:
    Transaction(Transaction& other) = delete;
    void operator= (Transaction& other) = delete;
    Transaction(Transaction&& other);
    ~Transaction();
    
    void Commit();
    void Rollback();
};

struct Configuration
{
    enum class JournalMode
    {
        Delete,
        Truncate,
        Persist,
        Memory,
        Wal,
        Off
    };
    
    enum class IsolationLevel
    {
        Serializable,
        ReadUncommitted
    };
    
    std::string Path;
    int BusyTimeout { 0 };
    int CacheSize { -2000 };
    bool ForeignKeys { false };
    JournalMode Journal { JournalMode::Delete };
    IsolationLevel TransactionIsolation { IsolationLevel::Serializable };
    int MaxPageCount { 0 };
    int PageSize { 4096 };
    bool ReadOnly { false };
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
    void OpenAlways();
    Command Create(const std::string& command) const;
    std::shared_ptr<Command> CreateFree(const std::string& command) const;
    Transaction Begin();
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

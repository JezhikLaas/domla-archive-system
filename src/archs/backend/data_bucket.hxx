#ifndef DATA_BUCKET_HXX
#define DATA_BUCKET_HXX

#include <memory>
#include <mutex>
#include "sqlite.hxx"
#include "settings_provider.hxx"

namespace Archive
{
namespace Backend
{

class DataBucket
{
private:
    std::unique_ptr<SQLite::Connection> Read;
    std::unique_ptr<SQLite::Connection> Write;
    
public:
    DataBucket(int id, const SettingsProvider& settings);
    DataBucket(const DataBucket&) = delete;
    void operator= (const DataBucket&) = delete;

    const SQLite::Connection& Reader() const { return *Read; }
    const SQLite::Connection& Writer() const { return *Write; }
    std::recursive_mutex ReadGuard;
    std::recursive_mutex WriteGuard;
};

} // namespace Backend
} // namespace Archive

#endif
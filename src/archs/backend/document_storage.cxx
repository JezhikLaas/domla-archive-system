#include "document_storage.hxx"
#include <math.h>
#include <boost/filesystem.hpp>

using namespace std;
using namespace Archive::Backend;
using namespace boost::filesystem;

DocumentStorage::DocumentStorage(const SettingsProvider& settings)
: Settings_(settings)
{
    InitializeBuckets();
}

DocumentStorage::~DocumentStorage()
{
    for (auto& Bucket : Buckets_) Bucket.reset();
}

void DocumentStorage::InitializeBuckets()
{
    create_directory(Settings_.DataLocation());
    Bucketing(Settings_.Backends(), [this](int number) { return shared_ptr<DataBucket>(new DataBucket(number, Settings_)); }, Buckets_);
}

void DocumentStorage::RegisterTransformers()
{
}

void DocumentStorage::Bucketing(int count, std::function<shared_ptr<DataBucket>(int)> generator, shared_ptr<DataBucket> buckets[])
{
    auto Limit = (int)round((double)256 / (double)count);
    int Created = 0;
    shared_ptr<DataBucket> Access;
    auto Generator = [&Created, &generator]() { ++Created; return generator(Created); };

    for (int Index = 0; Index < 256; ++Index) {
        Access = Access ? Access : Generator();
        buckets[Index] = Access;
        if (Index > 0 && (Index % Limit) == 0 && Created < count) Access.reset();
    }
}
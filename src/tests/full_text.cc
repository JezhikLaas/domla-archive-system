#define BOOST_TEST_MODULE "FulltextIndexModule"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include "archs/backend/full_text.hxx"
#include "lib/utils.hxx"

using namespace std;
using namespace Archive::Backend;

class SlimProvider : public SettingsProvider
{
private:
    string DataLocation_;
    
public:
    SlimProvider()
    : DataLocation_(":memory:")
    { }

	SlimProvider(SlimProvider&& other)
	: DataLocation_(std::move(other.DataLocation_))
	{ }

	SlimProvider(const SlimProvider& other)
		: DataLocation_(other.DataLocation_)
	{ }

    const string& DataLocation() const override
    {
        return DataLocation_;
    }

	int Backends() const override { return 1; }
};

BOOST_AUTO_TEST_CASE(Create_Database)
{
	SlimProvider Settings;
    auto Indexer = make_unique<FulltextIndex>(Settings);
    Indexer->Open();
}

BOOST_AUTO_TEST_CASE(Index_Some_Words)
{
	SlimProvider Settings;
    auto Indexer = make_unique<FulltextIndex>(Settings);
    Indexer->Open();
    
    vector<string> Words {
        "one",
        "two",
        "three"
    };
    
    auto Id = Utils::NewId();
    Indexer->Index(Id, Words);
}

BOOST_AUTO_TEST_CASE(Search_In_Index)
{
	SlimProvider Settings;
    auto Indexer = make_unique<FulltextIndex>(Settings);
    Indexer->Open();
    
    vector<string> Words {
        "one",
        "two",
        "three"
    };
    
    auto Id = Utils::NewId();
    Indexer->Index(Id, Words);
    
    auto Result = Indexer->Search({ "one" });
    
    BOOST_CHECK(Result.size() == 1);
    BOOST_CHECK(Result[0] == Id);
}

BOOST_AUTO_TEST_CASE(Failing_Search_In_Index)
{
	SlimProvider Settings;
    auto Indexer = make_unique<FulltextIndex>(Settings);
    Indexer->Open();
    
    vector<string> Words {
        "one",
        "two",
        "three"
    };
    
    auto Id = Utils::NewId();
    Indexer->Index(Id, Words);
    
    auto Result = Indexer->Search({ "four" });
    
    BOOST_CHECK(Result.empty());
}

BOOST_AUTO_TEST_CASE(Search_Result_Is_Ordered)
{
	SlimProvider Settings;
    auto Indexer = make_unique<FulltextIndex>(Settings);
    Indexer->Open();
    
    string Id1;
    string Id2;
    
    {
    vector<string> Words {
        "one",
        "two",
        "three"
    };
    
    Id1 = Utils::NewId();
    Indexer->Index(Id1, Words);
    }

	{
    vector<string> Words {
        "three",
        "four",
        "five",
    };
    
    Id2 = Utils::NewId();
    Indexer->Index(Id2, Words);
    }
    
	auto Result = Indexer->Search({ "three", "four" });
    
    BOOST_CHECK(Result.size() == 2);
    BOOST_CHECK(Result[0] == Id2);
    BOOST_CHECK(Result[1] == Id1);
}

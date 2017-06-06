#define BOOST_TEST_MODULE "DiffAndPatchModule"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include "archs/backend/binary_data.hxx"

using namespace std;
using namespace Archive::Backend;

BOOST_AUTO_TEST_CASE(Create_Patch)
{
    vector<unsigned char> OldData {
        0,1,2,3,4,5,6,7,8,9,
        0,1,2,3,4,5,6,7,8,9,
        0,1,2,3,4,5,6,7,8,9,
    };

    vector<unsigned char> NewData {
        0,1,2,3,4,5,6,7,8,9,
        9,8,7,6,5,4,3,2,1,0,
        0,1,2,3,4,5,6,7,8,9,
    };
    
    auto Check = BinaryData::CreatePatch(OldData, NewData);
    BOOST_CHECK(memcmp(&Check[0], "BSDIFF40", 8) == 0);
}

BOOST_AUTO_TEST_CASE(Apply_Patch_Middle)
{
    vector<unsigned char> OldData {
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
    };

    vector<unsigned char> NewData {
        '0','1','2','3','4','5','6','7','8','9',
        '9','8','7','6','5','4','3','2','1','0',
        '0','1','2','3','4','5','6','7','8','9',
    };
    
    auto Patch = BinaryData::CreatePatch(OldData, NewData);
    auto Check = BinaryData::ApplyPatch(OldData, Patch);
    
	BOOST_CHECK(NewData.size() == Check.size());
	BOOST_CHECK(memcmp(&Check[0], &NewData[0], Check.size()) == 0);
}

BOOST_AUTO_TEST_CASE(Apply_Patch_Front)
{
    vector<unsigned char> OldData {
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
    };

    vector<unsigned char> NewData {
        '9','8','7','6','5','4','3','2','1','0',
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
    };
    
    auto Patch = BinaryData::CreatePatch(OldData, NewData);
    auto Check = BinaryData::ApplyPatch(OldData, Patch);
    
	BOOST_CHECK(NewData.size() == Check.size());
	BOOST_CHECK(memcmp(&Check[0], &NewData[0], Check.size()) == 0);
}

BOOST_AUTO_TEST_CASE(Apply_Patch_Tail)
{
    vector<unsigned char> OldData {
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
    };

    vector<unsigned char> NewData {
        '0','1','2','3','4','5','6','7','8','9',
        '0','1','2','3','4','5','6','7','8','9',
        '9','8','7','6','5','4','3','2','1','0',
    };
    
    auto Patch = BinaryData::CreatePatch(OldData, NewData);
    auto Check = BinaryData::ApplyPatch(OldData, Patch);
    
	BOOST_CHECK(NewData.size() == Check.size());
	BOOST_CHECK(memcmp(&Check[0], &NewData[0], Check.size()) == 0);
}

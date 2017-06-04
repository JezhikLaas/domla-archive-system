#define BOOST_TEST_MODULE "UtilsModule"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "lib/utils.hxx"

using namespace std;
using namespace boost::posix_time;
using namespace boost::gregorian;

BOOST_AUTO_TEST_CASE(Ticks_Yields_Expected_Result)
{
    // DateTime(2001, 2, 5, 12, 0, 0)
    auto Check = 631169712000000000;
    auto Calculate = ptime(date(2001, Feb, 5), time_duration(12, 0 , 0));
    auto Calculated = Utils::Ticks(Calculate);
    BOOST_CHECK(Calculated == Check);
}

BOOST_AUTO_TEST_CASE(Guid_Formatted_As_Expected)
{
    auto Id = Utils::NewId();

    BOOST_CHECK(Id.empty() == false);
    BOOST_CHECK(Id[0] != '{');
    BOOST_CHECK(Id[8] == '-');
}

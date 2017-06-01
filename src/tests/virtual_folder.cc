#define BOOST_TEST_MODULE "TransformerBackendModule"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <boost/test/unit_test.hpp>
#include "archs/backend/virtual_tree.hxx"
#include "lib/Archive.h"

using namespace std;
using namespace Archive::Backend;

BOOST_AUTO_TEST_CASE(AddDocumentsToRoot)
{
    VirtualTree Tree;
    Tree.Add("/");

    auto Result = Tree.Root();
    BOOST_CHECK(get<0>(Result) == "/");
    BOOST_CHECK(get<1>(Result) == 1);
}

BOOST_AUTO_TEST_CASE(AddThreeDocumentLevels)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/", 1 },
        Access::FolderInfo { "/one", 2 },
        Access::FolderInfo { "/one/two", 3 },
    };
    
    Tree.Load(Levels);

    auto Result = Tree.Content("/one");
    BOOST_CHECK(Result.size() == 2);
    BOOST_CHECK(get<0>(Result[1]) == "two");
    BOOST_CHECK(get<1>(Result[1]) == 3);
    auto Check = get<2>(Result[1]);
    BOOST_CHECK(Check == "/one/two");
}

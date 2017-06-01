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

BOOST_AUTO_TEST_CASE(Add_Documents_To_Root)
{
    VirtualTree Tree;
    Tree.Add("/");

    auto Result = Tree.Root();
    BOOST_CHECK(get<0>(Result) == "/");
    BOOST_CHECK(get<1>(Result) == 1);
    BOOST_CHECK(get<2>(Result) == "/");
}

BOOST_AUTO_TEST_CASE(Add_Three_Document_Levels)
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
    BOOST_CHECK(get<2>(Result[1]) == "/one/two");
}

BOOST_AUTO_TEST_CASE(Add_Three_Document_Levels_Reversed)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/one/two", 3 },
        Access::FolderInfo { "/one", 2 },
        Access::FolderInfo { "/", 1 },
    };
    
    Tree.Load(Levels);

    auto Result = Tree.Content("/one");
    BOOST_CHECK(Result.size() == 2);
    BOOST_CHECK(get<0>(Result[1]) == "two");
    BOOST_CHECK(get<1>(Result[1]) == 3);
    BOOST_CHECK(get<2>(Result[1]) == "/one/two");
}

BOOST_AUTO_TEST_CASE(Adding_Documents_To_Intermediate)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/one/two", 3 },
        Access::FolderInfo { "/", 1 },
    };
    
    Tree.Load(Levels);
    Tree.Add("/one");

    auto Result = Tree.Content("/");
    BOOST_CHECK(Result.size() == 2);
    BOOST_CHECK(get<0>(Result[1]) == "one");
    BOOST_CHECK(get<1>(Result[1]) == 1);
    BOOST_CHECK(get<2>(Result[1]) == "/one");
}

BOOST_AUTO_TEST_CASE(Remove_Document_From_Tree)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/one/two", 3 },
        Access::FolderInfo { "/one", 2 },
        Access::FolderInfo { "/", 1 },
    };
    
    Tree.Load(Levels);
    Tree.Remove("/one");

    auto Result = Tree.Content("/");
    BOOST_CHECK(Result.size() == 2);
    BOOST_CHECK(get<0>(Result[1]) == "one");
    BOOST_CHECK(get<1>(Result[1]) == 1);
    BOOST_CHECK(get<2>(Result[1]) == "/one");
}

BOOST_AUTO_TEST_CASE(Remove_Document_Deletes_Empty_Leafs)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/", 3 },
        Access::FolderInfo { "/one", 2 },
        Access::FolderInfo { "/one/two", 1 },
    };
    
    Tree.Load(Levels);
    Tree.Remove("/one/two");

    auto Result = Tree.Content("/one");
    BOOST_CHECK(Result.size() == 1);
}

BOOST_AUTO_TEST_CASE(Remove_Documents_Clears_Tree_Except_Root)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/", 1 },
        Access::FolderInfo { "/one", 1 },
        Access::FolderInfo { "/one/two", 1 },
    };
    
    Tree.Load(Levels);
    Tree.Remove("/one/two");
    Tree.Remove("/one");
    Tree.Remove("/");

    auto Result = Tree.Content("/");
    BOOST_CHECK(Result.size() == 1);
    
    auto Root = Tree.Root();
    BOOST_CHECK(get<0>(Root) == "/");
    BOOST_CHECK(get<1>(Root) == 0);
}

BOOST_AUTO_TEST_CASE(Remove_Documents_From_Branch_Clears_Tree)
{
    VirtualTree Tree;
    auto Levels = {
        Access::FolderInfo { "/", 1 },
        Access::FolderInfo { "/one", 1 },
        Access::FolderInfo { "/one/two", 1 },
    };
    
    Tree.Load(Levels);
    Tree.Remove("/one");
    Tree.Remove("/one/two");
    Tree.Remove("/");

    auto Result = Tree.Content("/");
    BOOST_CHECK(Result.size() == 1);
    
    auto Root = Tree.Root();
    BOOST_CHECK(get<0>(Root) == "/");
    BOOST_CHECK(get<1>(Root) == 0);
}

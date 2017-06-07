#include "utils.hxx"
#include "virtual_tree.hxx"
#include <algorithm>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/Split.hpp>

using namespace std;
using namespace Archive::Backend;
using namespace boost;

using LockType = lock_guard<recursive_mutex>;

namespace
{

recursive_mutex& DisplaySync()
{
    static recursive_mutex DisplaySync_;
    return DisplaySync_;
}

} // anonymous namespace

const string& VirtualFolder::Display() const
{
    LockType Lock(DisplaySync());
    
    if (Display_.empty() == false) return Display_;
    
    Display_ = Name_;
    auto Cursor = Parent_;
    while (Cursor != nullptr) {
        Display_ = Cursor->Name_ != "/" ? Cursor->Name_ + "/" + Display_ : "/" + Display_;
        Cursor = Cursor->Parent_;
    }

    return Display_;
}

void VirtualFolder::RemoveNode(const string& name)
{
    auto Removal = Children_.find(name);
    delete Removal->second;
    Children_.erase(Removal);
    
    if (Documents_ == 0 && References_ == 0 && Parent_ != nullptr && Children_.empty()) {
        auto Name = Name_;
        Parent_->RemoveNode(Name);
    }
}

VirtualFolder::~VirtualFolder()
{
    for (auto& Child : Children_) {
        delete Child.second;
    }
}

FolderInfo VirtualTree::Root() const
{
    LockType Lock(SyncRoot_);
    return tuple<string, int, string>(Root_.Name_, Root_.Documents_, Root_.Display());
}


void VirtualTree::Load(const vector<Access::FolderInfo>& entries)
{
    LockType Lock(SyncRoot_);
    for (auto& Entry : entries) {
        auto Parts = Utils::Split(Entry.Name);
        auto Cursor = &Root_;
        
        for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
            auto Where = Cursor->Children_.lower_bound(Parts[Index]);
            VirtualFolder* Node = Where != Cursor->Children_.end() ? Where->second : new VirtualFolder(Parts[Index], Cursor);
            
            Cursor = Node;
        }
        
        Cursor->Documents_ = Entry.Count;
    }
}

void VirtualTree::Add(const string& path)
{
    auto Parts = Utils::Split(path);
    LockType Lock(SyncRoot_);
    auto Cursor = &Root_;

    for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
        auto Where = Cursor->Children_.lower_bound(Parts[Index]);
        VirtualFolder* Node = Where != Cursor->Children_.end() ? Where->second : new VirtualFolder(Parts[Index], Cursor);
        
        Cursor = Node;
    }
    
    ++Cursor->Documents_;
}

void VirtualTree::AddUncounted(const string& path)
{
    auto Parts = Utils::Split(path);
    LockType Lock(SyncRoot_);
    auto Cursor = &Root_;

    for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
        auto Where = Cursor->Children_.lower_bound(Parts[Index]);
        VirtualFolder* Node = Where != Cursor->Children_.end() ? Where->second : new VirtualFolder(Parts[Index], Cursor);
        
        Cursor = Node;
    }
    
    ++Cursor->References_;
}


vector<FolderInfo> VirtualTree::Content(const string& path) const
{
    auto Parts = Utils::Split(path);
    LockType Lock(SyncRoot_);
    auto Cursor = &Root_;
    
    for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
        Cursor = Cursor->Children_.at(Parts[Index]);
    }
    
    vector<FolderInfo> Result;
    Result.push_back(tuple<string, int, string>(Cursor->Name_, Cursor->Documents_, Cursor->Display()));
    
    for_each(
        Cursor->Children_.cbegin(),
        Cursor->Children_.cend(),
        [&Result](const pair<string, VirtualFolder*> node) {
            Result.push_back(tuple<string, int, string>(node.second->Name_, node.second->Documents_, node.second->Display()));
        }
    );
    
    return Result;
}

void VirtualTree::Remove(const string& path)
{
    auto Parts = Utils::Split(path);
    LockType Lock(SyncRoot_);
    auto Cursor = &Root_;
    
    for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
        Cursor = Cursor->Children_.at(Parts[Index]);
    }

    --Cursor->Documents_;
    if (Cursor->Parent_ != nullptr && Cursor->Documents_ == 0 && Cursor->References_ == 0 && Cursor->Children_.empty()) {
        auto Name = Cursor->Name_;
        Cursor->Parent_->RemoveNode(Name);
    }
}

void VirtualTree::RemoveUncounted(const string& path)
{
    auto Parts = Utils::Split(path);
    LockType Lock(SyncRoot_);
    auto Cursor = &Root_;
    
    for (vector<string>::size_type Index = 0; Index < Parts.size(); ++Index) {
        Cursor = Cursor->Children_.at(Parts[Index]);
    }

    --Cursor->References_;
    if (Cursor->Parent_ != nullptr && Cursor->Documents_ == 0 && Cursor->References_ == 0 && Cursor->Children_.empty()) {
        auto Name = Cursor->Name_;
        Cursor->Parent_->RemoveNode(Name);
    }
}

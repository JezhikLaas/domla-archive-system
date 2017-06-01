#ifndef VIRTUAL_TREE_HXX
#define VIRTUAL_TREE_HXX

#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include "Archive.h"

namespace Archive
{
namespace Backend
{

class VirtualFolder;

using Folders = std::map<std::string, VirtualFolder*>;
using FolderInfo = std::tuple<std::string, int, std::string>;
    
class VirtualFolder
{
friend class VirtualTree;

private:
    std::string Name_;
    VirtualFolder* Parent_ = nullptr;
    int Documents_ = 0;
    int References_ = 0;
    mutable std::string Display_;
    Folders Children_;
    
    void RemoveNode(const std::string& name);

public:
    VirtualFolder(const std::string& name)
    : Name_(name)
    { }
    
    VirtualFolder(const std::string& name, VirtualFolder* parent)
    : Name_(name), Parent_(parent)
    {
        Parent_->Children_[name] = this;
    }
    
    VirtualFolder(VirtualFolder&& other)
    : Name_(std::move(other.Name_)),
      Parent_(std::move(other.Parent_)),
      Documents_(std::move(other.Documents_)),
      References_(std::move(other.References_)),
      Display_(std::move(other.Display_)),
      Children_(std::move(other.Children_))
    { }
    
    ~VirtualFolder();
    
    VirtualFolder(const VirtualFolder& other) = delete;
    void operator= (const VirtualFolder& other) = delete;
    
    const std::string& Name() const { return Name_; }
    const VirtualFolder* Parent() const { return Parent_; }
    const std::string& Display() const;
};

class VirtualTree
{
private:
    VirtualFolder Root_;
    mutable std::recursive_mutex SyncRoot_;
    std::vector<std::string> Split(const std::string& path) const;

public:
    VirtualTree()
    : Root_("/")
    { }
    
    VirtualTree(const VirtualTree& other) = delete;
    void operator= (const VirtualTree& other) = delete;
    
    FolderInfo Root() const;
    void Load(const std::vector<Access::FolderInfo>& entries);
    void Add(const std::string& path);
    void AddUncounted(const std::string& path);
    std::vector<FolderInfo> Content(const std::string& path) const;
    void Remove(const std::string& path);
    void RemoveUncounted(const std::string& path);
};

} // namespace Backend
} // namespace Archive

#endif

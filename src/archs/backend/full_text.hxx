#ifndef FULL_TEXT_HXX
#define FULL_TEXT_HXX

#include "settings_provider.hxx"
#include <string>
#include <vector>

namespace Archive
{
namespace Backend
{

class FulltextIndex
{
private:
    struct Implementation;

private:
    const SettingsProvider& Settings_;
    Implementation* Inner;
    
public:
    explicit FulltextIndex(const SettingsProvider& settings);
    ~FulltextIndex();
    FulltextIndex(const FulltextIndex&) = delete;
    void operator =(const FulltextIndex&) = delete;
    
    void Open();
    void Index(const std::string& id, const std::vector<std::string>& words);
    std::vector<std::string> Search(const std::vector<std::string>& words) const;
};

} // namespace Backend
} // namespace Archive

#endif
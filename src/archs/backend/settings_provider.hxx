#ifndef SETTINGS_PROVIDER_HXX
#define  SETTINGS_PROVIDER_HXX

#include <string>

namespace Archive
{
namespace Backend
{

class SettingsProvider
{
public:
    virtual const std::string& DataLocation() const = 0;
    virtual int Backends() const { return 1; }
    virtual const std::string FulltextFile() const;
};

} // namespace Backend
} // namespace Archive

#endif

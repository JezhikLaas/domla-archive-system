#ifndef BINARY_DATA
#define BINARY_DATA

#include <vector>

namespace Archive
{
namespace Backend
{

class BinaryData
{
public:
    static std::vector<unsigned char> CreatePatch(const std::vector<unsigned char>& oldData, const std::vector<unsigned char>& newData);
    static std::vector<unsigned char> ApplyPatch(const std::vector<unsigned char>& data, const std::vector<unsigned char>& patch);
};

} // namespace Backend
} // namespace Archive

#endif

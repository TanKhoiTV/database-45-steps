#include <database.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <system_error>
#include <cstdlib>

class KV {
    private:

    std::unordered_map<std::string, std::vector<std::byte>> mem;

    public:


};
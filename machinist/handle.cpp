#include "handle.hpp"

HERE

#include <cstddef>
#include <cstdlib>
#include <string>


        std::string

EnvironmentValue(const std::string &name) {
#if _MSC_VER

    std::string retval;
    char *temp;
    std::size_t sz;
    if (_dupenv_s(&temp, &sz, name.c_str()) != 0 || !temp) {
        THROW("Can't find '" + name + "' in environment");
    }
    retval = temp;
    free(temp);
    return retval;

#else

    char* retval = std::getenv(name.c_str());
    REQUIRE(retval, "Can't find '" + name + "' in environment");
    return std::string(retval);

#endif
}

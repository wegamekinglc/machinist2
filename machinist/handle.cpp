
#include "handle.hpp"


std::string EnvironmentValue(const std::string& name) {
#if defined(_MSC_VER)
    std::string retval;
    char* temp;
    size_t sz;
    if (_dupenv_s(&temp, &sz, name.c_str()) != 0 || !temp)
        THROW("Can't find '" + name + "' in environment");
    retval = temp;
    free(temp);
    return retval;
#else
    std::string retval;
    char* ret = getenv(name.c_str());
    REQUIRE(ret, "Can't find '" + name + "' in environment");
    return std::string(ret);
#endif
}

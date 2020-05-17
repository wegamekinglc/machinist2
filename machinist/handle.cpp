 
#include "handle.hpp"


#if defined(_MSC_VER)
    string EnvironmentValue(const string& name)
    {
        string retval;
        char* temp;
        size_t sz;
        if (_dupenv_s(&temp, &sz, name.c_str()) != 0 || !temp)
            THROW("Can't find '" + name + "' in environment");
        retval = temp;
        free(temp);
        return retval;
    }
#else
    #include <cstdlib>
    string EnvironmentValue(const string& name)
    {
        string retval;
        char* ret = getenv(name.c_str());
        if(ret == nullptr) {
            THROW("Can't find '" + name + "' in environment");
        } else {
            retval = string(ret);
        }
        return retval;
    }
#endif

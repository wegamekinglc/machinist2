 
#include "Handle.h"

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

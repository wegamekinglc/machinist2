 
#ifndef MACHINIST_CONFIG__
#define MACHINIST_CONFIG__

#ifndef MACHINIST_HANDLE__
#include "Handle.h"
#endif

#ifndef MACHINIST_EMITTER__
#include "Emitter.h"
#endif

#include <map>

using std::map;

/* The purpose of a config is to state, for each type of mark-up input:
	-- what files to write
	-- what emitter(s) to use for each

  Filenames are defined by a prefix and postfix which will be wrapped around the info name.
  Here is an example of the process:
	-- DayBasis.Enum.if is discovered
	-- the config for "Enum" is read
	-- it contains file descriptors of the form {"MG_", ".cpp"} which will cause MG_DayBasis.cpp to be created
	-- the emitters associated with that file descriptor are invoked and their output collated into that file

  Note that the reader for Enum blocks is not configurable; it is coded into this app.
*/

struct Config_
{
	struct Source_
	{
		string filePattern_;
		vector<string> rejectPatterns_;
		string startToken_;	// if empty, start of file
		string stopToken_;	// if empty, end of file
		Source_(const string& pattern) : filePattern_(pattern) {}
	};

	struct Namer_	// information to generate a filename
	{
		Emitter::Funcs_ funcs_;
		string operator()(const Info_& info) const;
	};

	struct Output_
	{
		Namer_ dst_;
		vector<string> emitters_;	// referenced by name
	};

	vector<Source_> sources_;
	map<string, vector<Output_> > vals_;	// string key is the type of input ("Enum" above)
	string ownPath_;
	string templatePath_;	// relative to config file, not to working directory!
};

namespace Config
{
	Config_ Read(const string& filename);
}

#endif


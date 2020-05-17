
#ifndef MACHINIST_FILE__
#define MACHINIST_FILE__

#ifndef MACHINIST_HANDLE__
#include "Handle.h"
#endif

namespace File
{
	string Path(const string& dir_in);
	string CombinedPath(const string& path1, const string& path2);	// cd path1; cd path2; -- has to see if path2 is relative
	string PathOnly(const string& filename);

	void Read	// appends -- does not clear dst
		(const string& filename,
		vector<string>* dst);

	vector<string> List
		(const string& dir,
		const string& pattern,	// default pattern is *.if
		const vector<string>& reject_patterns);	// lets us exclude MG_* from scan

	string InfoType(const string& filename);
	string InfoName(const string& filename);
	string DirInfoName(const string& dir);
}

#endif


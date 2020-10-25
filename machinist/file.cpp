 
#include "file.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <istream>

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

void File::Read
	(const string& filename,
	 vector<string>* dst)
{
	std::ifstream src(filename);
	char buf[2048];
	while (src.getline(buf, 2048))
	{
		dst->push_back(string(buf));
	}
}

namespace
{
	bool IsSlash(char c)
	{
		return c == '/' || c == '\\';
	}
}

string File::Path(const string& dir_in)
{
	return (dir_in.empty() || dir_in == ".")
			? string()
			: (IsSlash(dir_in.back()) ? dir_in : dir_in + "\\");
}
string File::CombinedPath(const string& path1, const string& path2)
{
	return (path2.empty() || path2[0] == '.')
			? path1 + path2 // path2 is relative
			: path2;	
}

string File::PathOnly(const string& filename)
{
	string retval(filename);
	while (!retval.empty() && !IsSlash(retval.back()))
		retval.pop_back();
	return retval;
}

namespace fs = boost::filesystem;
vector<string> File::List
        (const string& dir,
         const string& pattern,
         const vector<string>& reject_patterns)
{
    fs::recursive_directory_iterator it(dir);
    fs::recursive_directory_iterator endit;
    vector<string> retval;
    const boost::regex filter(pattern);
    while(it != endit)
    {
        bool reject = false;
        boost::smatch what;
        string file_name = it->path().filename().string();
        if(boost::regex_match(file_name, what, filter)) {
            for (auto pr = reject_patterns.begin(); pr != reject_patterns.end() && !reject; ++pr) {
                const boost::regex reject_pattern(*pr);
                reject = boost::regex_match(file_name, what, reject_pattern);
            }
            if (!reject)
                retval.push_back(it->path().string());
        }
        ++it;
    }
    return retval;
}

string File::InfoType(const string& filename)
{
	// filenames should have the form "<name>.<type>.if"
	auto dot = filename.find('.');
	REQUIRE(dot != string::npos, "Input filename should have two separator periods");
	string rest = filename.substr(dot + 1);
	dot = rest.find('.');
	REQUIRE(dot != string::npos, "Input filename should have two separator periods");
	return rest.substr(0, dot);
}
string File::InfoName(const string& filename)
{
	// filenames should have the form "<name>.<type>.if"
	auto dot = filename.find('.');
	REQUIRE(dot != string::npos, "Input filename should have two separator periods");
	return filename.substr(0, dot);
}

string File::DirInfoName(const string& dir)
{
    // only take the leaf directory name
    auto last = dir.find_last_of("/\\");
    return last == string::npos 
                    ? dir
                    : dir.substr(last + 1);
}

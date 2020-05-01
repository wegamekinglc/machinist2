 // Machinist.cpp : Defines the entry point for the console application.
//
#include "Handle.h"
#include <direct.h>
#include <iostream>
#include <fstream>
#include <assert.h>
#include <numeric>
#include "Config.h"
#include "Emitter.h"
#include "Info.h"
#include "File.h"

using std::pair;
using std::cout;

namespace
{
	void ReadCommandLine
		(int argc,
		 char* argv[],
		 string* config,
		 vector<string>* libs,
		 vector<string>* dirs)
	{
		for (int ii = 1; ii < argc; ++ii)
		{
			string arg(argv[ii]);
			assert(!arg.empty());
			if (arg[0] == '-')	// command-line switch
			{
				REQUIRE(arg.size() == 2, "Unrecognized command line switch");
				switch (arg[1])
				{
				case 'c':
					REQUIRE(ii + 1 < argc, "-c switch must be followed by a config filename");
					REQUIRE(config->empty(), "Can't supply multiple configs");
					*config = argv[++ii];	// jump forward to this input
					break;

				case 'd':
					REQUIRE(ii + 1 < argc, "-d switch must be followed by a working directory");
					dirs->push_back(string(argv[++ii]));	// jump forward to this input
					break;

				case 'l':
					REQUIRE(ii + 1 < argc, "-l must be followed by a template library filename");
					libs->push_back(string(argv[++ii]));	// jump forward to this input
					break;

				default:
					throw std::exception("Unrecognized command line switch");
				}
			}
			else
			{
				// can't understand this
				REQUIRE(0, "Unexpected command line input");
			}
		}

		if (config->empty())
			*config = "config.ifc";	// the default
		if (dirs->empty())
			dirs->push_back(".");	// the default
	}

	bool SameExceptLF(const string& s1, const string& s2)
	{
		char LF(10);
		auto p1 = s1.begin(), p2 = s2.begin();
		while (p1 != s1.end() && p2 != s2.end())
		{
			if (*p1 == LF)
				++p1;
			else if (*p2 == LF)
				++p2;
			else if (*p1++ != *p2++)
				return false;
		}
		// let p2 catch up, if needed
		while (p2 != s2.end() && *p2 == LF)
			++p2;
		return p1 == s1.end() && p2 == s2.end();
	}

	bool StartsWith(const string& line, const string& token)
	{
		return line.find(token) == 0;
	}

	pair<string, string> TypeAndName(const string& line)
	{
		// just handle space-separated pair
		auto space = line.find(' ');
		REQUIRE(space != 0 && space != string::npos, "Can't find type/name separator");
		return make_pair(line.substr(0, space), line.substr(space + 1));
	}

	vector<string> AsDir(const vector<pair<string, string>> types_and_names)	// reverses the operation of File::InfoType/Name, just to get things back in a format the dir emitter understands
	{
		vector<string> retval;
		for (auto pf = types_and_names.begin(); pf != types_and_names.end(); ++pf)
		{
			retval.push_back(pf->second + "." + pf->first + ".if");
		}
		return retval;
	}


	void WriteIfChanged
		(const string& dst_name, 
		 const vector<string>& output,
		 int* n_written)
	{
		// I guess we have to read it
		vector<string> prior;
		File::Read(dst_name, &prior);
		const string allOld = accumulate(prior.begin(), prior.end(), string(), std::plus<string>());
		const string allNew = accumulate(output.begin(), output.end(), string(), std::plus<string>());

		if (!SameExceptLF(allOld, allNew))
		{
			cout << "\tWriting " << dst_name << "\n";
			++*n_written;
			std::ofstream dst(dst_name);
			for (auto po = output.begin(); po != output.end(); ++po)
				dst << *po;
		}
	}

	void WriteFromContents
		(const Config_& config,
		 const vector<string>& lib,	
		 const string& path,
		 const pair<string, string>& type_and_name,
		 const vector<string>& content,
		 int* n_written)
	{
		const Emitter::Funcs_& allFuncs = Emitter::GetAll(type_and_name.first, File::CombinedPath(config.ownPath_, config.templatePath_), lib);	// might parse a template file, lazily; will hold a static registry
		const Handle_<Info_> info = Info::Parse(type_and_name.first, type_and_name.second, content);	// will access a static registry of parsers
		const vector<Config_::Output_>& targets = config.vals_.find(type_and_name.first)->second;
		for (auto pt = targets.begin(); pt != targets.end(); ++pt)
		{
			string dstName = path + pt->dst_(*info);
			vector<string> output;
			for (auto pToEmit = pt->emitters_.begin(); pToEmit != pt->emitters_.end(); ++pToEmit)
			{
				auto out = Emitter::Call(*info, allFuncs, *pToEmit);
				output.insert(output.end(), out.begin(), out.end());
			}
			WriteIfChanged(dstName, output, n_written);
		}
	}

	// there are two ways to specify an input:
	//		-- in a file of its own, where the name and type will be read from the filename (e.g. MyFunc.public.if)
	//		-- embedded in another file between start and end tokens, where the first line after the start token will supply the name and type, e.g.:
	//					/*IF
	//					public MyFunc
	// the second method is slower because it requires scanning of every file which might have embedded markup
	// but may be more amenable to developers

	void WriteInfoFile
		(const Config_& config,
		 const vector<string>& lib,	
		 const string& path,
		 const string& filename,
		 vector<pair<string, string>>* things_read,
		 int* n_written,
		 int* n_lines)
	{
		cout << "Reading " << filename.c_str() << "\n";
		const string infoType = File::InfoType(filename);
		if (!config.vals_.count(infoType))
		{
			cout << "Skipping -- nothing to write\n";
			return;	
		}
		vector<string> content;
		File::Read(path + filename, &content);
		*n_lines += content.size();
		things_read->push_back(make_pair(infoType, File::InfoName(filename)));
		WriteFromContents(config, lib, path, things_read->back(), content, n_written);
	}

	void WriteOneFile
		(const Config_& config,
		 const vector<string>& lib,	
		 const string& path,
		 const string& filename,
		 const string& start_token,
		 const string& stop_token,
		 vector<pair<string, string>>* things_read,
		 int* n_written,
		 int* n_lines)
	{
		if (start_token.empty())
		{
			WriteInfoFile(config, lib, path, filename, things_read, n_written, n_lines);
			return;
		}
		cout << "Scanning " << filename.c_str() << "\n";
		cout.flush();
		vector<string> content;
		File::Read(path + filename, &content);
		*n_lines += content.size();
		for (auto pl = content.begin(); pl != content.end(); ++pl)
		{
			if (StartsWith(*pl, start_token))
			{
            cout << "Found start token at " << (pl - content.begin()) << "\n";
				// scan forward to find the stop token
				auto pStop = pl;
				for ( ; ; )
				{
					if (++pStop == content.end())
						REQUIRE(stop_token.empty(), "Reached end of file without stop token");
					if (pStop == content.end() || StartsWith(*pStop, stop_token))
						break;
				}
				auto pFirst = Next(pl);
				REQUIRE(pStop > pFirst, "No content between start and end tokens");
				auto pContent = Next(pFirst);
				cout << "Found content under " << pFirst->c_str() << "\n";
				auto typeAndName = TypeAndName(*pFirst);
				if (!config.vals_.count(typeAndName.first))
					cout << "Skipping -- nothing to write\n";
				else
				{
					things_read->push_back(typeAndName);
					WriteFromContents(config, lib, path, typeAndName, vector<string>(pContent, pStop), n_written);
				}
			}
		}
	}
}	// leave local

void XMain(int argc, char* argv[])
{
	char buf[2048];
	_getcwd(buf, 2048);
	cout << "In directory " << buf << "\n";

	string configFile;
	vector<string> libs, dirs;
	ReadCommandLine(argc, argv, &configFile, &libs, &dirs);
	Config_ config = Config::Read(configFile);
	// read library contents
	vector<string> libContents;
	for (auto pl = libs.begin(); pl != libs.end(); ++pl)
		File::Read(*pl, &libContents);

	// loop over directories
	for (auto pd = dirs.begin(); pd != dirs.end(); ++pd)
	{
		// keep track of actions in this directory only
		vector<pair<string, string>> thingsRead;
		int nWritten = 0, nRead = 0, nLines = 0;	
		// set directory
		cout << "Scanning " << *pd << "\n";
		const string path = File::Path(*pd);

		// loop over input file types
		for (auto ps = config.sources_.begin(); ps != config.sources_.end(); ++ps)
		{
			vector<string> infoFiles = File::List(*pd, ps->filePattern_, ps->rejectPatterns_);
			for (auto pi = infoFiles.begin(); pi != infoFiles.end(); ++pi, ++nRead)
				WriteOneFile(config, libContents, path, *pi, ps->startToken_, ps->stopToken_, &thingsRead, &nWritten, &nLines);
		}
		cout << "Scanned " << nRead << " files (" << nLines << " lines), found " << thingsRead.size() << " blocks\n";
		// finally write the whole-directory info
      WriteFromContents(config, libContents, path, make_pair(string("dir"), File::DirInfoName(*pd)), AsDir(thingsRead), &nWritten);

		cout << "Wrote " << nWritten << " files\n";
		cout << "Machinist finished directory " << *pd << "\n";
	}
}

int main(int argc, char* argv[])
{
	try
	{
		XMain(argc, argv);
		return 0;
	}
	catch (std::exception& e)
	{
		std::cerr << "Error:  " << e.what() << "\n";
		return -1;
	}
}



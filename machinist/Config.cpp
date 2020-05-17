 
#include "Config.h"
#include <sstream>
#include <numeric>
#include <functional>
#include <iostream>
#include <regex>
#include "Template.h"
#include "File.h"

/* the format for config files is minimal: 
a line beginning with a tab names an emitter, which will be associated with the current markup type and file namer
a line beginning with "->" and containing '%' or '#' changes the current file namer:  the info name will be substituted for the '#' to create a file name
any other nonblank line changes the current markup type

e.g.
Enum
->MG_#.cpp
	EnumCppSource
->MG_#.h
	EnumCppClass
	EnumCppMoreHeader
*/

namespace
{
	bool NamesFile(const string& line)
	{
		return line.size() > 3 
			&& line[0] == '-'
			&& line[1] == '>'
			&& line.find_first_of("#%") != line.npos;
	}

	bool NamesSources(const string& line)
	{
		return line.size() > 3
			&& line[0] == '<'
			&& line[1] == '-';
	}

	vector<string> SourcePatterns(const string& line, vector<string>* reject)
	{
		string rest = line.substr(2);
		vector<string> retval;
		bool rejecting = false;
		while (!rest.empty())
		{
			auto stop = rest.find_first_of(";!");
			(rejecting ? reject : &retval)->push_back(rest.substr(0, stop));
			if (stop == string::npos)
				rest.clear();
			else
			{
				if (rest[stop] == '!')
					rejecting = true;
				rest = rest.substr(stop + 1);
			}
		}
		return retval;
	}

	vector<string> split(const string& input, char sep, vector<string> sofar = vector<string>()) 
	{
		auto bar = input.find(sep);
		if (bar == string::npos)
		{
			sofar.push_back(input);
			return sofar;
		}
		sofar.push_back(input.substr(0, bar));
		return split(input.substr(bar + 1), sep, sofar);
	}

	void CommitAndReset
		(Config_* dst,
		 const string& type,
		 Config_::Output_* contrib)
	{
		if (!contrib->emitters_.empty())
		{
			for (auto t : split(type, '|'))
				dst->vals_[t].push_back(*contrib);
			contrib->emitters_.clear();
		}
	}

	bool StartsWithBackquote(const string& line)
	{
		int nonblank = line.find_first_not_of(" \t");
		return nonblank == string::npos	// all blank
			|| line[nonblank] == '`';
	}

	static const string FILENAME_FUNC("__OutputFile");

	string AddEnvironment(const string& src, int offset = 0, const string& sofar = string())
	{
		auto start = src.find("$(", offset);
		if (start == string::npos)
			return sofar + src.substr(offset);
		start += 2;
		auto stop = src.find(")", start);
		REQUIRE(stop != string::npos, "Nonterminated environment variable");
		return AddEnvironment(src, stop + 1, sofar + EnvironmentValue(src.substr(start, stop - start)));
	}
}	// leave local

Config_ Config::Read(const string& filename)
{
	vector<string> src;
	std::cout << "Reading configuration from " << filename << "\n";
	File::Read(filename, &src);
	Config_ retval;
	retval.ownPath_ = File::PathOnly(filename);
	string theType;
	vector<int> theSources;
	Config_::Output_ theFile;

	for (auto pl = src.begin(); pl != src.end(); ++pl)
	{
		const string& line = *pl;
		if (line.empty() || StartsWithBackquote(line))
			continue;
		else if (line[0] == '@')
		{
			retval.templatePath_ = AddEnvironment(line.substr(1));
		}
		else if (line[0] == '\t')
		{
			if (!theType.empty())
			{
				REQUIRE(!theFile.dst_.funcs_.empty(), "Nonempty file namer must be supplied before emitters can be assigned to it");
				// add emitters for this type
				theFile.emitters_.push_back(line.substr(1));
				REQUIRE(!theFile.emitters_.back().empty(), "Emitter name cannot be empty");
			}
			else if (!theSources.empty())
			{
				const string token = line.substr(1);
				// add start/stop tokens for these sources
				for (auto ps = theSources.begin(); ps != theSources.end(); ++ps)
				{
					auto& dst = retval.sources_[*ps];
					if (dst.startToken_.empty())
						dst.startToken_ = token;
					else
					{
						REQUIRE(dst.stopToken_.empty(), "Too many tokens; can only specify start/stop");
						dst.stopToken_ = token;
					}
				}
			}
			else
			{
				REQUIRE(0, "Mark-up type must be supplied before emitters can be assigned to it");
			}
		}
		else if (NamesFile(line))
		{
			REQUIRE(!theType.empty(), "Mark-up type must be supplied before file namers can be assigned to it");
			// store the previous file output
			CommitAndReset(&retval, theType, &theFile);
			auto func = line;
			auto hash = line.find('#');
			if (hash != string::npos)
				func = line.substr(0, hash) + "%_()" + line.substr(hash + 1);
			// turn it into a template declaration and parse it
			auto bang = func.find('>');
			func = "%:" + FILENAME_FUNC + ":" + func.substr(bang + 1) + '`';
			theFile.dst_.funcs_ = Template::Parse(vector<string>(1, func));	// has access to built-ins
			theSources.clear();
		}
		else if (NamesSources(line))
		{
			theSources.clear();
			vector<string> reject;
			auto patterns = SourcePatterns(line, &reject);
			for (auto pp = patterns.begin(); pp != patterns.end(); ++pp)
			{
				theSources.push_back(retval.sources_.size());
				retval.sources_.push_back(*pp);
				retval.sources_.back().rejectPatterns_ = reject;
			}
			// theSources keeps locations to which start/stop tokens can be written
		}
		else
		{
			// store the previous file output
			CommitAndReset(&retval, theType, &theFile);
			theFile.dst_.funcs_.clear();
			theType = line;
			theSources.clear();
		}
	}
	// store the tail
	CommitAndReset(&retval, theType, &theFile);
	return retval;
}

string Config_::Namer_::operator()(const Info_& info) const
{
	auto vs = Emitter::Call(info, funcs_, FILENAME_FUNC);
	return std::accumulate(vs.begin(), vs.end(), string());
}


 
// parse enumeration mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include <algorithm>
#include <iostream>
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"
using namespace ParseUtils;
using Info::MakeLeaf;

/* Mark-up format (filename is structure name, except for the trailing underscore)

	<help>
(alternative|default) <name> <alias>* [=numeric] [legacy shortname[|longname]]							(only one default -- can omit name for default, in which case the default constructor will create an uninitialized state; can specify numeric value; can specify short/long names for legacy CORE_ENUMERATION_VALUE, but must |-separate because the names themselves can contain spaces)
	<help>
method <code>
[switchable]
[extensible]
[legacy] prefix
*/

namespace
{
	static const string ENUMERATION("enumeration");
	static const string HELP("help");
	static const string ALTERNATIVE("alternative");
	static const string DEFAULT("default");
	static const string METHOD("method");
	static const string SWITCHABLE("switchable");
	static const string EXTENSIBLE("extensible");
	static const string LEGACY("legacy");
	static const string LONGNAME("longname");
	static const string ALIAS("alias");
	static const string NUMERIC("numeric");

	Info_* NewMember(const Info_* parent, bool is_default, string all_names, auto_ptr<Info_>* help)
	{
		if (is_default)
			std::cout << "Enumeration has default\n";

		string lShort, lLong;
		// find any legacy names
		auto lLoc = all_names.find(LEGACY);
		if (lLoc != string::npos)
		{
			lShort = AfterInitialWhitespace(all_names.substr(lLoc + LEGACY.size()));	// store it here, then split if needed
			all_names = all_names.substr(0, lLoc);
			auto sep = lShort.find('|');
			if (sep != string::npos)
			{
				lLong = AfterInitialWhitespace(lShort.substr(sep + 1));
				lShort = lShort.substr(0, sep);
			}
			else
				lLong = lShort;	// easier output if these always exist together
		}

		string numeric;
		// find any numeric value
		auto eqLoc = all_names.find('=');
		if (eqLoc != string::npos)
		{
			numeric = AfterInitialWhitespace(all_names.substr(eqLoc+1));
			all_names = all_names.substr(0, eqLoc);
		}

		// split names on whitespace
		vector<string> names;
		for (string rest = AfterInitialWhitespace(all_names); !rest.empty(); )
		{
			auto stop = find_if(rest.begin(), rest.end(), IsWhite);
			names.push_back(string(rest.begin(), stop));
			rest = AfterInitialWhitespace(string(stop, rest.end()));
		}
		REQUIRE(!names.empty() || is_default, "Every alternative needs a name");

		auto_ptr<Info_> retval(new Info_(parent, parent, names.empty() ? "_NOT_SET" : EmbeddableForm(names[0])));
		for (auto pn = names.begin(); pn != names.end(); ++pn)
			retval->children_.insert(make_pair(ALIAS, MakeLeaf(retval.get(), retval->root_, *pn)));
		if (is_default)
			retval->children_.insert(make_pair(DEFAULT, MakeLeaf(retval.get(), retval->root_, "_")));

		if (help->get())
		{
			(*help)->parent_ = retval.get();
			retval->children_.insert(make_pair(HELP, Handle_<Info_>(help->release())));
		}
		if (!numeric.empty())
			retval->children_.insert(make_pair(NUMERIC, MakeLeaf(retval.get(), retval->root_, numeric)));
		if (!lShort.empty())
			retval->children_.insert(make_pair(LEGACY, MakeLeaf(retval.get(), retval->root_, lShort)));
		if (!lLong.empty())
			retval->children_.insert(make_pair(LONGNAME, MakeLeaf(retval.get(), retval->root_, lLong)));

		return retval.release();
	}

	vector<string>::const_iterator ReadAlternative
		(const Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 Handle_<Info_>* dst)
	{
		REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare argument");
		const string start = UntilWhite(*line);
		const string rest = line->substr(start.size());

		auto_ptr<Info_> help;
		line = ReadHelp(0, parent, ++line, end, &help);	// fix help's parent later
		dst->reset(NewMember(parent, start == DEFAULT, rest, &help));
		return line;
	}

	struct ParseEnumeration_ : Info::Parser_
	{
		ParseEnumeration_() 
		{
			Info::RegisterParser(ENUMERATION, *this);
		}

		Info_* operator()
			(const string& info_name,
			 const vector<string>& content)
		const
		{
			auto_ptr<Info_> retval(new Info_(0, 0, info_name));
			auto line = content.begin();
			// read the help
			auto_ptr<Info_> help;
			line = ReadHelp(retval.get(), retval.get(), line, content.end(), &help);
			if (help.get())
				retval->children_.insert(make_pair(HELP, Handle_<Info_>(help.release())));

			while (line != content.end())
			{
				// !a.find(b) is true iff a starts with b
				if (!line->find(ALTERNATIVE) || !line->find(DEFAULT))
				{
					Handle_<Info_> info;
					line = ReadAlternative(retval.get(), line, content.end(), &info);
					retval->children_.insert(make_pair(ALTERNATIVE, info));	// note that DEFAULT is stored as an ALTERNATIVE
				}
				else if (!line->find(METHOD))
				{
					retval->children_.insert(make_pair(METHOD, MakeLeaf(retval.get(), retval->root_, AfterInitialWhitespace(line->substr(METHOD.size())))));
					++line;
				}
				else if (!line->find(SWITCHABLE))
				{
					retval->children_.insert(make_pair(SWITCHABLE, MakeLeaf(retval.get(), retval->root_, "_")));
					++line;
				}
				else if (!line->find(EXTENSIBLE))
				{
					retval->children_.insert(make_pair(EXTENSIBLE, MakeLeaf(retval.get(), retval->root_, "_")));
					++line;
				}
				else if (!line->find(LEGACY))
				{
					retval->children_.insert(make_pair(LEGACY, MakeLeaf(retval.get(), retval->root_, line->substr(LEGACY.size() + 1))));
					++line;
				}
				else
				{
					REQUIRE(0, "Unrecognized line: " + *line);
				}
			}
			return retval.release();
		}
	};
	static const ParseEnumeration_ TheParser;

	string MaxNumericSentinel(const Info_& src)
	{
		int max = -1;
		auto alts = src.children_.equal_range(ALTERNATIVE);
		for (auto ia = alts.first; ia != alts.second; ++ia)
		{
			string num = GetOptional(*ia->second, NUMERIC);
			if (num.empty())
				break;
			int n = std::stoi(num);
			if (n > max)
				max = n;
		}
		const int startCount = src.children_.count(LEGACY);
		Template::SetGlobalCount(max < 0 ? startCount : max);	// has side effect!
		return max > 0
				? "__MG_SENTINEL_VAL = " + std::to_string(max) + "\n\t\t"
				: string();
	}

	string Uppercase(const Info_& src)
	{
		string retval(src.content_);
		transform(retval.begin(), retval.end(), retval.begin(), toupper);
		return retval;
	}

	string SizeBase(const Info_& src)
	{
		if (src.children_.size() < 252)
			return " : char";
		return string();
	}

	struct MakeEnumerationEmitter_ : Emitter::Source_
	{
		MakeEnumerationEmitter_() 
		{
			Emitter::RegisterSource(ENUMERATION, *this);
		}
		Emitter::Funcs_ Parse
			(const vector<string>& lib,
			 const string& path) 
		const
		{
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Enumeration.mgt", &tLines);
			auto retval = Template::Parse(tLines);
			// now add C++ functions
			retval.ofInfo_["MaxNumericSentinel"].reset(EmitUnassisted(MaxNumericSentinel));
			retval.ofInfo_["Uppercase"].reset(EmitUnassisted(Uppercase));
			retval.ofInfo_["SizeBase"].reset(EmitUnassisted(SizeBase));
			// and library functions

			return retval;
		}
	};
	static MakeEnumerationEmitter_ TheEmitter_;
}	// leave local


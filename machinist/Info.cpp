 
#include "Info.h"
#include <assert.h>
#include <algorithm>
#include <iostream>

using std::map;
using std::auto_ptr;

bool Info::IsRoot(const Info_& i)
{
	// two ways to check:  root is this, and parent is empty
	assert(!i.parent_ == (i.root_ == &i));
	return !i.parent_;
}

Info::Parser_::~Parser_()
{	}

namespace
{
	map<string, const Info::Parser_*>& TheParsers()
	{
		static map<string, const Info::Parser_*> RETVAL;
		return RETVAL;
	}

//----------------------------------------------------------------------------

// create an Info_ directly from a model of its contents, with no type-specific parsing
// format of nested pairs, looks like the block below
/*
child1:child1_ownval
	gchild1:gchild1_ownval
	gchild2:_
child2:child2_ownval
...
*/
// top-level ownval (=info_name) comes from file name or first line of mark-up block, not seen here

	int Indent(const string& s)
	{
		int retval = 0;
		for (auto pc = s.begin(); pc != s.end(); ++pc, ++retval)
			if (*pc != '\t')
				break;
		return retval;
	}

	Info_* XParseRaw
		(const Info_* parent,
		 const Info_* root,
		 const string& info_name,
		 vector<string>::const_iterator& line,
		 vector<string>::const_iterator end,
		 int tab_offset)
	{
		auto_ptr<Info_> retval(new Info_(parent, root, info_name));
		for ( ; ; )
		{
			int indent = line == end ? -1 : Indent(*line);
			if (indent < tab_offset)	// pop out to parent
				return retval.release();
			assert(indent == tab_offset);
			// new child starts here
			auto content = line->substr(indent);
			auto colon = content.find(':');
			auto childName = content.substr(0, colon);
			auto childVal = content.substr(colon + 1);
			retval->children_.insert(make_pair(childName, Handle_<Info_>(XParseRaw(retval.get(), root, childVal, ++line, end, indent + 1))));
		}
	}

	Info_* ParseRaw
		(const string& info_name,
		 const vector<string>& content)
	{
		for (auto pc = content.begin(); pc != content.end(); ++pc)
			if (pc->find(':') == string::npos)
				return 0;
	// if any line does not contain a colon, then this function returns NULL and we can try some other parser
	// if every line contains a colon, we will try to parse, and throw an exception on failure
		return XParseRaw(0, 0, info_name, content.begin(), content.end(), 0);
	}

	void CheckBackPointers
		(const Info_& info,
		 const Info_* parent = nullptr,
		 const Info_* root = nullptr)
	{
		if (parent && info.parent_ != parent)
			std::cout << "Here";
		REQUIRE(!parent || info.parent_ == parent, "Bad parent of '" + info.content_ + "'");
		REQUIRE(!root || info.root_ == root, "Bad root");
		for (auto c : info.children_)
			CheckBackPointers(*c.second, &info, root ? root : info.root_);
	}
}	// leave local

Info_* Info::Parse
	(const string& type,
	 const string& name,
	 const vector<string>& content)
{
	if (Info_* raw = ParseRaw(name, content))
		return raw;
	// not in generic format; find the specialized parser
	REQUIRE(TheParsers().count(type), "No parsers for '" + type + "' info");
	std::unique_ptr<Info_> retval((*TheParsers()[type])(name, content));
	CheckBackPointers(*retval);
	return retval.release();
}

void Info::RegisterParser
	(const string& type,
	 const Info::Parser_& parser)
{
	std::cout << "Registering parser for " << type << "\n";
	assert(!TheParsers().count(type));
	TheParsers()[type] = &parser;
}

Info::Path_::Path_(const string& src) 
	: 
absolute_(false)
{
	for (auto start = src.begin(); start != src.end(); ++start)
	{
		if (*start == '/')
		{
			REQUIRE(start == src.begin(), "'//' not allowed in a path");
			absolute_ = true;
		}
		else
		{
			auto stop = find(start, src.end(), '/');
			childNames_.push_back(string(start, stop));
			start = stop;	// don't need to skip the '/', the increment will do that
			if (start == src.end())
				break;
		}
	}
}

const Info_& Info::Path_::operator()(const Info_& here, bool quiet) const
{
	static const Handle_<Info_> FALSE = MakeLeaf(nullptr, nullptr, string());
	const Info_* retval = absolute_ ? here.root_ : &here;
	for (auto pc = childNames_.begin(); pc != childNames_.end(); ++pc)
	{
		const char step = '1' + (pc - childNames_.begin());
		if (!retval)
		{
			REQUIRE(quiet, "Path navigation failed before step " + string(1, step));
			return *FALSE;
		}
		if (*pc == "..")
		{
			REQUIRE(retval->parent_, "No parent to navigate to");
			retval = retval->parent_;
		}
		else if (*pc != ".")
		{
			auto cr = retval->children_.equal_range(*pc);
			if (cr.first == cr.second)
			{
				REQUIRE(quiet, "Path navigation failed at step " + string(1, step) + ":  child '" + *pc + "' not found");
				return *FALSE;
			}
			REQUIRE(cr.first == --cr.second, "Path navigation failed at step " + string(1, step) + ":  child '" + *pc + "' not unique");	// Next(cr.first) doesn't work due to compiler bug
			retval = cr.first->second.get();
		}
	}
	if (!retval)
	{
		REQUIRE(quiet, "Path navigation failed at final step");
		return *FALSE;
	}
	return *retval;
}

Handle_<Info_> Info::MakeLeaf(const Info_* parent, const Info_* root, const string& content)
{
	return new Info_(parent, root, content);
}


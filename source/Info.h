 
#ifndef MACHINIST_INFO__
#define MACHINIST_INFO__

#ifndef MACHINIST_HANDLE__
#include "Handle.h"
#endif

#include <string>
#include <map>

using std::multimap;

// Info_ structure contains the high-level description of an object
struct Info_
{
	Info_(const Info_* parent, const Info_* root, const string& content) : parent_(parent), root_(root), content_(content) {if (!root) root_ = this;}
	string content_;	// content at this level
	multimap<string, Handle_<Info_> > children_;
	const Info_* parent_;
	const Info_* root_;
};

namespace Info
{
	bool IsRoot(const Info_& i);
	// relies on an internal parser registry for each type
	Info_* Parse
		(const string& type,
		 const string& name,
		 const vector<string>& content);

	struct Parser_
	{
		virtual ~Parser_();
		virtual Info_* operator()
			(const string& info_name,
			 const vector<string>& content)
		const = 0;
	};

	void RegisterParser
		(const string& info_type,
		 const Parser_& parser);	// assumed to be a file static -- we hold a pointer to the parser

	struct Path_
	{
		bool absolute_;
		vector<string> childNames_;
		Path_(const string& text);	// parses a path -- '/' is the separator
		const Info_& operator()(const Info_& here, bool quiet) const;
	};

	// a generally useful function
	Handle_<Info_> MakeLeaf(const Info_* parent, const Info_* root, const string& content);
}

#endif


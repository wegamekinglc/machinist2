 
#ifndef MACHINIST_PARSEUTILS__
#define MACHINIST_PARSEUTILS__

#ifndef MACHINIST_HANDLE__
#include "Handle.h"
#endif

using std::auto_ptr;
struct Info_;
class Emitter_;
class StringTransform_;

namespace ParseUtils
{
	bool IsWhite(char c);
	bool StartsWithWhitespace(const string& line);
	bool IsAllWhite(const string& line);
	string AfterInitialWhitespace(const string& line);
	string UntilWhite(const string& src);
	string TrimWhitespace(const string& src);

	vector<string>::const_iterator ReadHelp
		(const Info_* parent,
		 const Info_* root,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 auto_ptr<Info_>* dst,
		 vector<Handle_<Info_>>* local_conditions = 0);

	vector<string>::const_iterator ReadInsert
		(const Info_* parent,
		 const Info_* root,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 auto_ptr<Info_>* dst);

	bool AddNonempty
		(Info_* info,
		 const string& tag,
		 const string& val);

	vector<string>::const_iterator ReadCondition
		(const Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 Handle_<Info_>* dst);

	vector<string>::const_iterator ReadLink
		(const Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 Handle_<Info_>* dst);

	// support output too
	string GetMandatory(const Info_& info, const string& child);
	string GetOptional(const Info_& info, const string& child);
	string EmbeddableForm(const string& src);	// strips out things that cannot be in a C++ identifier
	string HtmlSafe(const string& src);	// makes embeddable within HTML
	string TexSafe(const string& src);	// makes embeddable within TeX
   string Condensed(const string& src);
	
	string WithParentName(const Info_& src);	// substitutes parent name for '$'
	string WithGrandparentName(const Info_& src);	// substitutes grandparent name for '$'

	// one kind of emitter just wraps a function(Info_)
	typedef string (*emit_from_info_t)(const Info_&);
	Emitter_* EmitUnassisted(emit_from_info_t func);
	typedef string (*emit_from_string_t)(const string&);
	StringTransform_* EmitTransform(emit_from_string_t func);
}

#endif
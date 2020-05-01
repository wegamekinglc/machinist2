 
// parse public mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include <iostream>
#include <algorithm>
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"

using namespace ParseUtils;
using std::pair;

/* Mark-up format 
	filename is function_name.public.if (or other extension specified by config); or first line is "public " then function name
	dimension declarator, if present, is "[]" or "[][]"
	greedy declarator, if present, is "+"
*/

/*
	<help>
[category <cat>]
[-<target>]*													`` suppress 
[volatile]
&inputs
[&optional]													`` separates mandatory from optional inputs
<name>[ aka <tempname>] is <type>[<dimension>][<greedy>] [<subtype>] [(<default>)] [or <intype>-><translator>]
	[&condition using $[\help]]
	<help>
[+<insert_code>]
&conditions
<code>
	<help>
&outputs
<name> is <type>[<dimension>] [<subtype>]
	<help>
[&notes
	<help text>]
[&links]
<name> [<type if not public>]
*/

// if tempname is used, then the input will be given that name, and insert_code is responsible for creating a quantity with the master name to send to the internal function

namespace
{
	static const string HELP("help");
	static const string INSERT("insert");
	static const string INPUT("input");
	static const string OUTPUT("output");
	static const string OPTIONAL("optional");
	static const string CONDITION("condition");
	static const string START_INPUTS("&inputs");
	static const string START_OUTPUTS("&outputs");
	static const string START_OPTIONAL("&optional");
	static const string START_CONDITIONS("&conditions");
	static const string IS(" is ");	// includes space separators, meaning tabs won't be recognized
	static const string AKA(" aka ");	// includes space separators
	static const string MYNAME("myname");	// internal name
	static const string TYPE("type");
	static const string SUBTYPE("subtype");
	static const string DEFAULT("default");
	static const string DIMENSION("dimension");
	static const string GREEDY("greedy");
	static const string PUBLIC("public");
	static const string SUPPRESS("suppress");
	static const string CATEGORY("category");
	static const string VOLATILE("volatile");
	static const string IN_TYPE("intype");
	static const string TRANSLATOR("translator");
	static const string NOTES("notes");
	static const string START_NOTES("&notes");
	static const string LINK("link");
	static const string START_LINKS("&links");

	pair<string, string> NamePair(const string& src)
	{
		auto aka = src.find(AKA);
		if (aka == string::npos)
			return make_pair(src, src);	// internal and external names are the same
		return make_pair(src.substr(0, aka), src.substr(aka + AKA.size()));
	}

	void PopulateArgument(Info_* arg, const string& name, const string& type, int dim, bool greedy, const string& subtype, bool optional, const string& def_val, const map<string, string>& in_type_translators, auto_ptr<Info_>* help, const vector<Handle_<Info_>>& conditions, auto_ptr<Info_>* insert)
	{
		auto names = NamePair(name);
		arg->content_ = names.first;
		arg->children_.insert(make_pair(MYNAME, Info::MakeLeaf(arg, arg->root_, names.second)));
		arg->children_.insert(make_pair(TYPE, Info::MakeLeaf(arg, arg->root_, type)));
		if (dim > 0)
			arg->children_.insert(make_pair(DIMENSION, Info::MakeLeaf(arg, arg->root_, string(1, '0' + dim))));
		if (greedy)
			arg->children_.insert(make_pair(GREEDY, Info::MakeLeaf(arg, arg->root_, "1")));
		(void) AddNonempty(arg, SUBTYPE, subtype);
		if (help->get())
		{
			(*help)->parent_ = arg;
			arg->children_.insert(make_pair(HELP, Handle_<Info_>(help->release())));
		}
		for (auto pc = conditions.begin(); pc != conditions.end(); ++pc)
		{
			arg->children_.insert(make_pair(CONDITION, *pc));
		}
		if (insert->get())
		{
			(*insert)->parent_ = arg;
			arg->children_.insert(make_pair(INSERT, Handle_<Info_>(insert->release())));
		}
		if (optional)
		{
			arg->children_.insert(make_pair(OPTIONAL, Info::MakeLeaf(arg,arg->root_,"1")));
			AddNonempty(arg, DEFAULT, def_val);
		}
		for (auto pi = in_type_translators.begin(); pi != in_type_translators.end(); ++pi)
		{
			auto_ptr<Info_> child(new Info_(arg, arg->root_, pi->first));
			if (!pi->second.empty())
				child->children_.insert(make_pair(TRANSLATOR, Info::MakeLeaf(child.get(), arg->root_, pi->second)));
			arg->children_.insert(make_pair(IN_TYPE, child.release()));
		}
	}

	pair<string, string> SubtypeAndDefault(const string& mash)
	{
		auto bra = mash.find('(');
		if (bra == string::npos)
			return make_pair(mash, string());	// no default
		pair<string, string> retval(TrimWhitespace(mash.substr(0, bra)), TrimWhitespace(mash.substr(bra+1)));
		if (!retval.second.empty())
		{
			REQUIRE(retval.second.back() == ')', "Found '(' for default input value without matching ')'");
			retval.second.pop_back();
		}
		return retval;
	}

	void ExtractTranslators	// peel translators (e.g. "or string->MyTypeFromString") off the right side of the markup line
		(string* rest,
		 map<string, string>* translators)
	{
		for ( ; ; )
		{
			// find last occurrence of separator string " or "
			auto orLoc = rest->rfind(" or ");
			if (orLoc == string::npos)
				return;
			auto tt = rest->substr(orLoc + 4);
			*rest = rest->substr(0, orLoc);
			// split the type and translator on "->"
			auto sepLoc = tt.find("->");
			if (sepLoc == string::npos)
				translators->insert(make_pair(tt, string()));	// no translator supplied, we'll autogenerate something
			else
				translators->insert(make_pair(tt.substr(0, sepLoc), tt.substr(sepLoc + 2)));
		}
	}

	vector<string>::const_iterator ReadArgument
		(const Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 bool optional,
		 Handle_<Info_>* dst)
	{
		auto_ptr<Info_> arg(new Info_(parent, parent, string()));	// populate content later
		REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare argument");
		auto is = line->find(IS);
		REQUIRE(is != line->npos, "Input needs type declaration using 'is' (" + *line + ")");
		const string name = line->substr(0, is);
		string rest = line->substr(is + IS.size());
		auto endType = rest.find_first_of(" \t[");	// anything that terminates the type
		const string type = rest.substr(0, endType);
		rest = rest.substr(type.size());
		int dim = 0;
		while (rest.substr(0, 2) == "[]")
		{
			++dim;
			rest = rest.substr(2);
		}
		bool greedy = !rest.empty() && rest[0] == '+';
		if (greedy)
			rest = rest.substr(1);
		REQUIRE(StartsWithWhitespace(rest), "No space before subtype");
		map<string, string> inTranslators;
		ExtractTranslators(&rest, &inTranslators);
		auto sub_def = SubtypeAndDefault(AfterInitialWhitespace(rest));
		const string subtype = rest.empty() ? string() : AfterInitialWhitespace(rest);
		auto_ptr<Info_> help, insert;
		++line;
		vector<Handle_<Info_>> conditions;
		line = ReadHelp(arg.get(), parent->root_, line, end, &help, &conditions);
		line = ReadInsert(arg.get(), parent->root_, line, end, &insert);
		PopulateArgument(arg.get(), name, type, dim, greedy, sub_def.first, optional, sub_def.second, inTranslators, &help, conditions, &insert);
		dst->reset(arg.release());
		return line;
	}

	vector<string>::const_iterator ReadCategory
		(Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end)
	{
		if (line != end && !line->empty() && line->substr(0, CATEGORY.size()) == CATEGORY)
		{
			Handle_<Info_> category = Info::MakeLeaf(parent, parent->root_, AfterInitialWhitespace(line->substr(CATEGORY.size())));
			parent->children_.insert(make_pair(CATEGORY, category));
			++line;
		}
		return line;
	}

	vector<string>::const_iterator ReadVolatile
		(Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end)
	{
		if (line != end && *line == VOLATILE)
		{
			Handle_<Info_> child = Info::MakeLeaf(parent, parent->root_, "1");
			parent->children_.insert(make_pair(VOLATILE, child));
			++line;
		}
		return line;
	}

	vector<string>::const_iterator ReadSuppress
		(const Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 Handle_<Info_>* suppress)
	{
		auto_ptr<Info_> temp;
		while (line != end && !line->empty() && line->front() == '-')
		{
			if (!temp.get())
				temp.reset(new Info_(parent, parent, string()));
			temp->children_.insert(make_pair(line->substr(1), Info::MakeLeaf(temp.get(), parent->root_, "_")));
			++line;
		}
		if (temp.get())
			suppress->reset(temp.release());
		return line;
	}

	struct ParsePublic_ : Info::Parser_
	{
		ParsePublic_() 
		{
			Info::RegisterParser(PUBLIC, *this);
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
			line = ReadCategory(retval.get(), line, content.end());				// finds any category specification, inserts it and advances line
			line = ReadVolatile(retval.get(), line, content.end());				// handles volatile specification if present
			// any targets to suppress?
			Handle_<Info_> suppress;
			line = ReadSuppress(retval.get(), line, content.end(), &suppress);
			if (suppress.get())
				retval->children_.insert(make_pair(SUPPRESS, suppress));
			REQUIRE(line != content.end() && !line->empty() && line->front() == '&', "After function help, need '&inputs' or '&outputs'");
			if (*line == START_INPUTS)	// OK, there might be some inputs
			{
				++line;
				bool optional = false;
				while (line != content.end() && *line != START_OUTPUTS && *line != START_CONDITIONS)
				{
					if (*line == START_OPTIONAL)
					{
						optional = true;
						++line;
						continue;
					}
					Handle_<Info_> thisArg;
					line = ReadArgument(retval.get(), line, content.end(), optional, &thisArg);
					retval->children_.insert(make_pair(INPUT, thisArg));
				}
			}
			// maybe there are conditions
			if (line != content.end() && *line == START_CONDITIONS)
			{
				++line;
				while (line != content.end() && *line != START_OUTPUTS)
				{
					Handle_<Info_> thisCond;
					line = ReadCondition(retval.get(), line, content.end(), &thisCond);
					retval->children_.insert(make_pair(CONDITION, thisCond));
				}
			}
			// now should be at the outputs
			REQUIRE(line != content.end() && *line == START_OUTPUTS, "Expected '&outputs'");
			++line;
			while (line != content.end() && *line != START_LINKS && *line != START_NOTES)
			{
				Handle_<Info_> thisArg;
				line = ReadArgument(retval.get(), line, content.end(), false, &thisArg);
				retval->children_.insert(make_pair(OUTPUT, thisArg));
			}
			// maybe there are notes, or links
			if (line != content.end() && *line == START_NOTES)
			{
				auto_ptr<Info_> notes;
				line = ReadHelp(retval.get(), retval.get(), ++line, content.end(), &notes);
				if (notes.get())
					retval->children_.insert(make_pair(NOTES, Handle_<Info_>(notes.release())));
			}
			if (line != content.end() && *line == START_LINKS)
			{
				++line;
				while (line != content.end() && !line->empty() && line->front() != '&')
				{
					Handle_<Info_> link;
					line = ReadLink(retval.get(), line, content.end(), &link);
					retval->children_.insert(make_pair(LINK, link));
				}
			}

			REQUIRE(line == content.end(), "Unexpected extra content beginning with '" + *line + "'");
			return retval.release();
		}
	};
	static ParsePublic_ TheParser;

	// Now for the outputs
	// here are all the basic types
	enum
	{
		NUMBER = 0,
		INTEGER,
		BOOLEAN,
		STRING,
		TIME,
		CELL,
		DICTIONARY,
		ENUM,
		HANDLE,
		SETTINGS,
		DATE,
		RECORD,
		NUM_TYPES
	};

	int TypeToIndex(const string& type)
	{
		static map<string, int> VALS;
		static bool first = true;
		if (first)
		{
			first = false;
			VALS["number"] = NUMBER;
			VALS["integer"] = INTEGER;
			VALS["boolean"] = BOOLEAN;
			VALS["string"] = STRING;
			VALS["time"] = TIME;
			VALS["cell"] = CELL;
			VALS["dictionary"] = DICTIONARY;
			VALS["enum"] = ENUM;
			VALS["handle"] = HANDLE;
			VALS["settings"] = SETTINGS;
			VALS["date"] = DATE;
			VALS["record"] = RECORD;
		}
		auto pt = VALS.find(type);
		REQUIRE(pt != VALS.end(), "Invalid type '" + type + "'");
		return pt->second;
	}

	string ScalarCType(const string& type, const string& subtype, bool suppress_subtype = false)
	{
		const char* RETVAL[NUM_TYPES] = {"double", "int", "bool", "String_", "DateTime_", "Cell_", "Dictionary_", ":::", ":::", ":::","Date_", ":::"};
		const int which = TypeToIndex(type);
		switch (which)
		{
		case ENUM:
			return suppress_subtype ? "String_" : subtype + "_";
		case HANDLE:
			return "Handle_<" + (subtype.empty() || suppress_subtype ? string("Storable") : subtype) + "_>";
		case SETTINGS:
		case RECORD:
			return subtype + "_";
		default:
			return string(RETVAL[which]);
		}
	}

	string XDNType(const string& type, int dimension, int is_top = true)
	{
		const char* RETVAL[NUM_TYPES] = {"System::Double", "System::Int32", "System::Boolean", "System::String^", "System::DateTime^", "TALibNet::Atom_^", "System::Collections::Hashtable^", "System::String^", "TALibNet::Object_^", "System::Collections::Hashtable^", "System::DateTime^", "System::Collections::Hashtable^"};
		return dimension == 0
				? RETVAL[TypeToIndex(type)]
				: "array<" + XDNType(type, dimension - 1, false) + " >^";
	}

	int Dimension(const Info_& arg)
	{
		const string dim = GetOptional(arg, DIMENSION);
		if (dim == "0" || dim.empty())
			return 0;
		if (dim == "1")
			return 1;
		REQUIRE(dim == "2", "Unexpected dimension");
		return 2;
	}

	bool HasCanonicalDefault(const string& scalar_type)
	{
		switch (TypeToIndex(scalar_type))
		{
		case NUMBER:
		case INTEGER:
		case BOOLEAN:
		case TIME:
		case DATE:
			return false;
		}
		return true;
	}

	string IsCookie(const Info_& src)
	{
		if (Dimension(src) == 0)
		{
			switch (TypeToIndex(GetMandatory(src, TYPE)))
			{
			case NUMBER:
			case INTEGER:
			case BOOLEAN:
			case TIME:
			case DATE:
			case STRING:
				return string();	// these are the types we can pass directly
			}
		}
		return "1";
	}

	string XCType(const Info_& src, bool suppress_subtype)
	{
		const string type = GetMandatory(src, TYPE);
		const string scalar = ScalarCType(type, GetOptional(src, SUBTYPE), suppress_subtype);
		switch (Dimension(src))
		{
		default:
		case 0:
			return (!GetOptional(src, OPTIONAL).empty() && !HasCanonicalDefault(type) && GetOptional(src, DEFAULT).empty())	// optionals with a default always end up with a value
					? "boost::optional<" + scalar + ">"
					: scalar;
		case 1:
			return "Vector_<" + scalar + ">";
		case 2:
			return "Matrix_<" + scalar + ">";
		}
	}
	string CType(const Info_& src) {return XCType(src, false);}
	string CBaseType(const Info_& src) {return XCType(src, true);}

	string XCppInType(const Info_& src, bool suppress_subtype)
	{
		const string type = GetMandatory(src, TYPE);
		string scalar = ScalarCType(type, GetOptional(src, SUBTYPE), suppress_subtype);

		string retval;
		switch (Dimension(src))
		{
		default:
		case 0:
			retval = scalar;
			break;
		case 1:
			retval = "Vector_<" + scalar + ">";
			break;
		case 2:
			retval = "Matrix_<" + scalar + ">";
			break;
		}
		const bool ptrArg = !GetOptional(src, OPTIONAL).empty() && GetOptional(src, DEFAULT).empty();
		return "const " + retval + (ptrArg ? '*' : '&');
	}
	string CppInType(const Info_& src) {return XCppInType(src, true);}	// used at external interface level
	string XCppOutType(const Info_& src, bool suppress_subtype)
	{
		string temp = XCppInType(src, suppress_subtype);	
		temp.back() = '*';	// never a reference
		// assert(temp.substr(0, 6) == "const ");
		return temp.substr(6);	// never const
	}
	string CppOutType(const Info_& src) {return XCppOutType(src, false);}

	string CppPassArg_ConvertPointers(const Info_& src)
	{
		if (GetOptional(src, OPTIONAL).empty())
			return src.content_;	// pass as-is
		if (Dimension(src) == 0)
		{
			auto type = GetMandatory(src, TYPE);
			auto defval = GetOptional(src, DEFAULT);
			if (!HasCanonicalDefault(type))
			{
				return defval.empty()
						? "AsOptional(" + src.content_ + ')'	// have to wrap in an optional
						: src.content_;	// default already supplied or unavailable
			}
			if (TypeToIndex(type) == STRING && !defval.empty())
				return src.content_;	// default already supplied
		}
		string ctype = XCppOutType(src, true);
		ctype.pop_back();	// the '*'
		return '(' + src.content_ + " ? *" + src.content_ + " : " + ctype + "())";
	}
	string CppPassArg(const Info_& src)
	{
		const string retval = CppPassArg_ConvertPointers(src);	// might be all we need

		const string subtype = GetOptional(src, SUBTYPE);
		const string outtype = CppOutType(src);
		const string enumsubtype = outtype.substr(0,outtype.size() - 1);

		switch (TypeToIndex(GetMandatory(src, TYPE)))
		{
		case HANDLE:
			if (!subtype.empty())	// have to downcast the handles
				return "CheckedCast_<" + subtype + "_>()(" + retval + ", \"subtype\")";
			break;

		case ENUM:
			if (!Dimension(src))
				return subtype + "_(" + retval + ")"; 
			else 
				return "Apply(ConstructCast_<String_, " + subtype + "_>(), " + retval + ")";
		}
		// ok, no special cases triggered
		return retval;
	}

	string TypeForCat(const string& type, const string& subtype)
	{
		string retval = subtype.empty() ? type : subtype;
		retval[0] = toupper(retval[0]);
		return retval;
	}

	string InTypeTranslator(const Info_& src)
	{
		auto user = GetOptional(src, TRANSLATOR);
		if (!user.empty())	// user has supplied a translator
			return user;
		return TypeForCat(GetMandatory(*src.parent_, TYPE), GetOptional(*src.parent_, SUBTYPE))
				+ "::From"
				+ TypeForCat(src.content_, string());
	}

	string DotNetType(const Info_& src)
	{
		const string type = GetMandatory(src, TYPE);
		string retval = XDNType(type, Dimension(src));
		return (!GetOptional(src, OPTIONAL).empty() && !HasCanonicalDefault(type) && Dimension(src) == 0)
				? "TALibNet::Optional_<" + retval + ">^"
				: retval;
	}

	string MMPassAsString(const Info_& src)
	{
		const int dim = Dimension(src);
		switch (TypeToIndex(GetMandatory(src, TYPE)))
		{
		case STRING:
			if (dim < 1) break;
		case CELL:
		case DICTIONARY:
		case SETTINGS:
		case RECORD:
			return string("1");

		default:
			break;
		}
		return string();
	}

	string DimensionString(int dim)
	{
		string retval("0");
		retval[0] += dim;
		return retval;
	}
	string XMMType(const string& type, int dimension, int is_top = true)
	{
		const char* RETVAL[NUM_TYPES] = {"Real", "Integer", "\"Boolean\"", "\"UTF8String\"", "\"TIME\"", "\"CELL\"", "\"DICTIONARY\"", "\"UTF8String\"", "\"UTF8String\"", "\"SETTINGS\"", "\"UTF8String\"", "\"SETTINGS\""};
		return dimension == 0
				? RETVAL[TypeToIndex(type)]
				: "{" + XMMType(type, 0, false) + "," + DimensionString(dimension) + ",Constant}";
	}
	string XMMTypeShort(const string& type, int dimension, int is_top = true)
	{
		const char* RETVAL[NUM_TYPES] = {"Num", "Int", "Bool", "Str", "Time", "Cell", "Dict", "Str", "Str", "Dict", "Date", "Dict"};
		return RETVAL[TypeToIndex(type)] + (dimension == 0 ? string() : DimensionString(dimension));
	}

	string MathematicaArgType(const Info_& src)
	{
		if (!MMPassAsString(src).empty())
			return string("\"UTF8String\"");

		const string type = GetMandatory(src, TYPE);
		string retval = XMMType(type, Dimension(src));
		return retval;	// postponed -- handle optional args?  maybe with overloading?
	}
	string MathematicaArgTypeQ(const Info_& src)
	{
		string retval = MathematicaArgType(src);
		if (!retval.empty() && retval[0] == '"')	// retval is quoted, need to escape the quotes
		{
			retval.back() = '\\';
			return '\\' + retval + '"';
		}
		return retval;	// postponed -- handle optional args?  maybe with overloading?
	}
	string MathematicaShortType(const Info_& src)
	{
		const string type = GetMandatory(src, TYPE);
		string retval = XMMTypeShort(type, Dimension(src));
		return retval;	// postponed -- handle optional args?  maybe with overloading?
	}
	string HashedArgNumbers(const Info_& src)
	{
		const int nInputs = src.children_.count("input");
		string retval;
		for (int ii = 0; ii < nInputs; ++ii)
			retval += '#' + DimensionString(ii + 1) + ", ";
		while (!retval.empty() && (retval.back() == ' ' || retval.back() == ','))
			retval.pop_back();
		return retval;
	}

	string XJNISignatureType(const string& type, int dimension)
	{
		const char* RETVAL[NUM_TYPES] = { "D", "I", "B", "Ljava/lang/String;", "Ljava/util/Date;", "LDA/util/Cell;", "LDA/Dictionary;", "Ljava/lang/String;", "LDA/Handle;", "LDA/Dictionary;", "Ljava/util/Date;", "LDA/Dictionary;" };
		string retval(RETVAL[TypeToIndex(type)]);
		for (int id = 0; id < dimension; ++id)
			retval = '[' + retval + ')';
		return retval;
	}
	string JNI_SignatureType(const Info_& src)
	{
		return XJNISignatureType(GetMandatory(src, TYPE), Dimension(src));
	}

	string XJNINativeType(const string& type, int dimension)
	{
		const char* SCALAR_TYPE[NUM_TYPES] = { "jdouble", "jint", "jboolean", "jstring", "jobject", "jobject", "jobject", "jstring", "jobject", "jobject", "jobject", "jobject" };
		const char* ELEMENT_TYPE[NUM_TYPES] = { "double", "int", "boolean", "", "", "", "", "", "", "", "", "" };
		if (dimension > 0)
		{
			string e(ELEMENT_TYPE[TypeToIndex(type)]);
			if (dimension > 1 || e.empty())
				return "jobjectArray";
			return "j" + e + "Array";
		}
		return SCALAR_TYPE[TypeToIndex(type)];
	}
	string JNI_NativeType(const Info_& src)
	{
		return XJNINativeType(GetMandatory(src, TYPE), Dimension(src));
	}

	string XJuliaType(const string& type, int dimension)
	{
		const char* SCALAR[NUM_TYPES] = {"Float64", "Int32", "Bool", "Ptr{Uint8}", "TIME", "CELL", "DICTIONARY", "ENUM", "Handle", "SETTINGS", "DATE", "RECORD"};
		const string retval = SCALAR[TypeToIndex(type)];
		return dimension == 0
				? retval
				: "Array{" + retval + ", " + DimensionString(dimension) + "}";
	}
	string JuliaType(const Info_& src)
	{
		return XJuliaType(GetMandatory(src, TYPE), Dimension(src));
	}

	string CookiePostfix(const Info_& src)
	{
		const char* SCALAR[NUM_TYPES] = { "Double", "Int", "Bool", "String", "DateTime", "Cell", "Dict", "Enum", "Handle", "Settings", "Date", "Record" };
		string retval;
		switch (Dimension(src))
		{
		case 1:
			retval += "Vector";
			break;
		case 2:
			retval += "Matrix";
			break;
		}
		retval += SCALAR[TypeToIndex(GetMandatory(src, TYPE))];
		return retval;
	}

	string CSubTypeAngleBracket(const Info_& src)
	{
		string scalar, postfix;
		const string type = GetMandatory(src, TYPE);
		if (TypeToIndex(type) == HANDLE)
		{
			const string subtype = GetOptional(src, SUBTYPE);
			if (subtype.empty())
				return string("");
			else
				return string("<") + subtype + "_>";
		}

		return string("");			
	}

	string CTypeTranslator(const Info_& src)
	{
		string scalar, postfix;
		const string type = GetMandatory(src, TYPE);
		switch (TypeToIndex(type))
		{
		case ENUM:
			scalar = "Enum";
			postfix = "<" + GetMandatory(src, SUBTYPE) + "_>";
			break;

		case HANDLE:
			{
				const string subtype = GetOptional(src, SUBTYPE);
				if (subtype.empty())
					scalar = "HandleBase";
				else
					scalar = "Handle";
			}
			break;

		case SETTINGS:
			scalar = "Settings";
			postfix = "<" + GetMandatory(src, SUBTYPE) + "_>";
			break;
		case RECORD:
			scalar = "Record";
			postfix = "<" + GetMandatory(src, SUBTYPE) + "_>";
			break;

		default:
			scalar = EmbeddableForm(ScalarCType(GetMandatory(src, TYPE), GetOptional(src, SUBTYPE)));
		}
		switch (Dimension(src))
		{
		default:
		case 0:
			return "To" + scalar + postfix;
		case 1:
			return "To" + scalar + "Vector" + postfix;
		case 2:
			return "To" + scalar + "Matrix" + postfix;
		}
	}

	string ExcelName(const Info_& src)
	{
		if (src.children_.count(CATEGORY))	// rule is that functions with a category have verbatim names
			return src.content_;
		string retval;
		for (auto pc = src.content_.begin(); pc != src.content_.end(); ++pc)
		{
			const char C = toupper(*pc);
			retval.push_back(C == '_' ? '.' : C);
		}
		return (retval.find('.') == retval.npos)
				? "DA." + retval	
				: retval;
	}
	string PythonName(const Info_& src)
	{
		string retval = src.content_;
		std::transform(retval.begin(), retval.end(), retval.begin(), tolower);
		if (retval.substr(0, 3) == "axl")
			retval = retval.substr(3);
		return retval;
	}
	string CppName(const Info_& src)
	{
		string retval;
		bool upper = true;
		// coerce to CamelCase
		for (auto pc = src.content_.begin(); pc != src.content_.end(); ++pc)
		{
			if (upper)
			{
				retval.push_back(toupper(*pc));
				upper = false;
			}
			else if (*pc == '_')
				upper = true;
			else
				retval.push_back(*pc);
		}
		if (retval.substr(0, 3) == "AXL")
			retval = retval.substr(3);
		return retval;
	}

	string MMName(const Info_& src)
	{
		string retval;
		for (auto pc = src.content_.begin(); pc != src.content_.end(); ++pc)
		{
			if (toupper(*pc) != tolower(*pc))
				retval.push_back(*pc);
		}
		return retval;
	}

	string MultipleOutputs(const Info_& src)
	{
		return src.children_.count("output") > 1
				? string("1")
				: string();
	}

//--------------------------------------------------------------------------

	string TypeForHelp(const Info_& src)
	{
		const string& t = GetMandatory(src, TYPE);
		string s = t;
		switch (Dimension(src))
		{
		case 1:
			s = "vector of " + s + "s";
			break;
		case 2:
			s = "matrix of " + s + "s";
			break;
		}
		string sub = GetOptional(src, SUBTYPE);
		if (!sub.empty())
		{
			switch (TypeToIndex(t))
			{
			case ENUM:
				sub = "<a href=\"MG_" + sub + "_enum.htm\">" + sub + "</a>";
				break;
			case SETTINGS:
			case RECORD:
				sub = "<a href=\"MG_" + sub + "_settings.htm\">" + sub + "</a>";
				break;
			}
			s += " of type " + sub;
		}

		return s;
	}

	struct MakePublicEmitter_ : Emitter::Source_
	{
		MakePublicEmitter_()
		{
			Emitter::RegisterSource(PUBLIC, *this);
		}
		Emitter::Funcs_ Parse
			(const vector<string>& lib,
			const string& path)
			const
		{
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Public.mgt", &tLines);
			auto retval = Template::Parse(tLines);
			// now add C++ functions
			retval.ofInfo_["CType"].reset(EmitUnassisted(CType));
			retval.ofInfo_["CBaseType"].reset(EmitUnassisted(CBaseType));
			retval.ofInfo_["CTypeTranslator"].reset(EmitUnassisted(CTypeTranslator));
			retval.ofInfo_["CSubTypeAngleBracket"].reset(EmitUnassisted(CSubTypeAngleBracket));
			retval.ofInfo_["ExcelName"].reset(EmitUnassisted(ExcelName));
			retval.ofInfo_["PythonName"].reset(EmitUnassisted(PythonName));
			retval.ofInfo_["CppName"].reset(EmitUnassisted(CppName));
			retval.ofInfo_["MMName"].reset(EmitUnassisted(MMName));
			retval.ofInfo_["TypeForHelp"].reset(EmitUnassisted(TypeForHelp));
			retval.ofInfo_["MultipleOutputs"].reset(EmitUnassisted(MultipleOutputs));
			retval.ofInfo_["DotNetType"].reset(EmitUnassisted(DotNetType));
			retval.ofInfo_["MathematicaArgType"].reset(EmitUnassisted(MathematicaArgType));
			retval.ofInfo_["MathematicaArgTypeQ"].reset(EmitUnassisted(MathematicaArgTypeQ));
			retval.ofInfo_["MathematicaShortType"].reset(EmitUnassisted(MathematicaShortType));
			retval.ofInfo_["HashedArgNumbers"].reset(EmitUnassisted(HashedArgNumbers));
			retval.ofInfo_["MMPassAsString"].reset(EmitUnassisted(MMPassAsString));
			retval.ofInfo_["JNI_SignatureType"].reset(EmitUnassisted(JNI_SignatureType));
			retval.ofInfo_["JNI_NativeType"].reset(EmitUnassisted(JNI_NativeType));
			retval.ofInfo_["IsCookie"].reset(EmitUnassisted(IsCookie));
			retval.ofInfo_["JuliaType"].reset(EmitUnassisted(JuliaType));
			retval.ofInfo_["CookiePostfix"].reset(EmitUnassisted(CookiePostfix));
			retval.ofInfo_["InTypeTranslator"].reset(EmitUnassisted(InTypeTranslator));
			retval.ofInfo_["CppInType"].reset(EmitUnassisted(CppInType));
			retval.ofInfo_["CppOutType"].reset(EmitUnassisted(CppOutType));
			retval.ofInfo_["CppPassArg"].reset(EmitUnassisted(CppPassArg));

			return retval;
		}
	};
	static MakePublicEmitter_ TheEmitter_;
}	// leave local


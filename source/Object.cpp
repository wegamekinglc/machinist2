 
// parse settings mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"

using namespace ParseUtils;
using std::pair;

/* Mark-up format (filename is structure name, except for the trailing underscore)(one cheesy feature:  using a dot ('.') for the default will give a truly optional parameter)

	<help>
<is_record (settings only)>
<version v (storables only)>
<manual (storables only)>
&members
<quantifier?><name> is <type> <subtype?> <from func?> <default defval?>
	<help>
&conditions
<code>
	<help>
*/

namespace
{
	static const string HELP("help");
	static const string VERSION("version");
	static const string CONDITION("condition");
	static const string MEMBER("member");
	static const string START_MEMBERS("&members");
	static const string START_CONDITIONS("&conditions");
	static const string IS(" is ");	// includes space separators, meaning tabs won't be recognizes
	static const string TYPE("type");
	static const string SUBTYPE("subtype");
	static const string DIMENSION("dimension");
	static const string MULTIPLE("multiple");
	static const string OPTIONAL("optional");
	static const string REPEAT("repeat");
	static const string FROM("from");
	static const string DEFAULT("default");
	static const string OBJECT("object");
	static const string IS_RECORD("is_record");
	static const string IS_STORABLE("is_storable");
	static const string IS_SETTINGS("is_settings");
	static const string MANUAL("manual");
	static const string SETTINGS("settings");
	static const string STORABLE("storable");
	static const string RECORD("record");

	Info_* NewMember(const Info_* parent, const string& name, const string& type, int dim, bool multiple, bool optional, const string& subtype, const string& from_func, const string& default_val, auto_ptr<Info_>* help)
	{
		auto_ptr<Info_> retval(new Info_(parent, parent, name));
		retval->children_.insert(make_pair(TYPE, Info::MakeLeaf(retval.get(),retval->root_,type)));
		if (dim > 0)
			retval->children_.insert(make_pair(DIMENSION, Info::MakeLeaf(retval.get(),retval->root_,string(1, '0' + dim))));
		if (multiple)
			retval->children_.insert(make_pair(MULTIPLE, Info::MakeLeaf(retval.get(),retval->root_,"1")));
		if (optional)
			retval->children_.insert(make_pair(OPTIONAL, Info::MakeLeaf(retval.get(),retval->root_,"1")));
		(void) AddNonempty(retval.get(), SUBTYPE, subtype);
		(void) AddNonempty(retval.get(), FROM, from_func);
		(void) AddNonempty(retval.get(), DEFAULT, default_val);
		if (help->get())
		{
			(*help)->parent_ = retval.get();
			retval->children_.insert(make_pair(HELP, Handle_<Info_>(help->release())));
		}
		return retval.release();
	}

	pair<string, bool> NamedArg(const string& src, const string& name)	// appends a TRUE flag if it starts at the front
	{
		auto keyLoc = src.find(name);
		if (keyLoc == string::npos)
			return make_pair(string(), false);
		return make_pair(UntilWhite(AfterInitialWhitespace(src.substr(keyLoc + name.size()))), keyLoc == 0);
	}

	void SetFlag(Info_* parent, const string& flag)
	{
		parent->children_.insert(make_pair(flag, Info::MakeLeaf(parent, parent->root_, "1")));
	}

	vector<string>::const_iterator ReadFrontMatter
		(Info_* parent,
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end)
	{
		for ( ; line != end; ++line)
		{
			if (line->find(VERSION) == 0)
				parent->children_.insert(make_pair(VERSION, Info::MakeLeaf(parent, parent->root_, ParseUtils::AfterInitialWhitespace(line->substr(VERSION.size())))));
			else if (*line == MANUAL)
				SetFlag(parent, MANUAL);
			else if (line->find(IS_RECORD) == 0)
				SetFlag(parent, IS_RECORD);
			else if (line->find(IS_STORABLE) == 0)
				SetFlag(parent, IS_STORABLE);
			else if (line->find(IS_SETTINGS) == 0)
				SetFlag(parent, IS_SETTINGS);
			else
				break;
		}
		return line;
	}

	vector<string>::const_iterator ReadMember
		(const Info_* parent, 
		 vector<string>::const_iterator line,
		 vector<string>::const_iterator end,
		 Handle_<Info_>* dst)
	{
		REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare argument");
		auto is = line->find(IS);
		REQUIRE(is != line->npos, "Member needs type declaration using 'is'");
		const string name = line->substr(0, is);

		string rest = ParseUtils::AfterInitialWhitespace(line->substr(is + IS.size()));
		const bool multiple = !rest.empty() && (rest[0] == '+' || rest[0] == '*');
		const bool optional = !rest.empty() && (rest[0] == '?' || rest[0] == '*');
		if (multiple || optional)
			rest = rest.substr(1);	// eat the quantifier
		auto endType = rest.find_first_of(" \t[+");	// anything that terminates the type
		const string type = rest.substr(0, endType);
		rest = rest.substr(type.size());
		int dim = 0;
		while (rest.substr(0, 2) == "[]")
		{
			++dim;
			rest = rest.substr(2);
		}
		REQUIRE(StartsWithWhitespace(rest), "No space before subtype/from/default");
		rest = ParseUtils::AfterInitialWhitespace(rest);
		const pair<string, bool> from = NamedArg(rest, FROM);
		const pair<string, bool> defval = NamedArg(rest, DEFAULT);
		const string subtype = from.second || defval.second
				? string()	// one of the optional things started at the start of rest
				: UntilWhite(rest);

		auto_ptr<Info_> help;
		line = ReadHelp(0, parent, ++line, end, &help);	// set help's parent later
		dst->reset(NewMember(parent, name, type, dim, multiple, optional, subtype, from.first, defval.first, &help));
		return line;
	}

	struct ParseObject_ : Info::Parser_
	{
		vector<string> flags_;	// will be set in object
		ParseObject_(const string& type, const vector<string>& flags) : flags_(flags)
		{
			Info::RegisterParser(type, *this);
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
			line = ReadFrontMatter(retval.get(), line, content.end());
			// set the flags at this point
			for (auto f : flags_)
				SetFlag(retval.get(), f);
			REQUIRE(line != content.end() && !line->empty() && line->front() == '&', "After object description, need '&members'");
			if (*line == START_MEMBERS)	// there must be members
			{
				++line;
				while (line != content.end() && *line != START_CONDITIONS)
				{
					Handle_<Info_> thisMem;
					line = ReadMember(retval.get(), line, content.end(), &thisMem);
					retval->children_.insert(make_pair(MEMBER, thisMem));
				}
			}
			// maybe there are conditions
			if (line != content.end() && *line == START_CONDITIONS)
			{
				++line;
				while (line != content.end())
				{
					Handle_<Info_> thisCond;
					line = ReadCondition(retval.get(), line, content.end(), &thisCond);
					retval->children_.insert(make_pair(CONDITION, thisCond));
				}
			}
			// now should be at the end
			REQUIRE(line == content.end(), "Expected end of file after members and conditions");
			return retval.release();
		}
	};
	static const ParseObject_ TheParser(OBJECT, vector<string>());
	static const ParseObject_ TheSettingsParser(SETTINGS, { IS_SETTINGS });	// "settings" is a synonym for "object" with is_settings set
	static const ParseObject_ TheStorableParser(STORABLE, { IS_STORABLE });		// "storable" is a synonym for "object" with is_storable set
	static const ParseObject_ TheRecordParser(RECORD, { IS_RECORD });			// "record" is a synonym for "object" with is_record set

	// Now for the outputs
	// here are all the basic types
	enum
	{
		NUMBER = 0,
		INTEGER,
		BOOLEAN,
		STRING,
		CELL,
		HANDLE,
		SETTINGS_,
		DATE,
		ENUM,
		DATETIME,
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
			VALS["cell"] = CELL;
			VALS["handle"] = HANDLE;
			VALS["settings"] = SETTINGS_;
			VALS["date"] = DATE;
			VALS["datetime"] = DATETIME;
			VALS["enum"] = ENUM;
		}
		auto pt = VALS.find(type);
		REQUIRE(pt != VALS.end(), "Invalid type '" + type + "'");
		return pt->second;
	}

	string ScalarCType(const string& type, const string& subtype)
	{
		const char* RETVAL[NUM_TYPES] = { "double", "int", "bool", "String_", "Cell_", ":::", "Settings_", "Date_", ":::", "DateTime_" };
		const int which = TypeToIndex(type);
		switch (which)
		{
		case HANDLE:
			return "Handle_<" + (subtype.empty() ? string("Storable") : subtype) + "_>";
		case SETTINGS_:
		case ENUM:
			return subtype + '_';
		default:
			return string(RETVAL[which]);
		}
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

	bool HasSpecificDefault(const Info_& arg)
	{
		return arg.children_.count(DEFAULT) > 0;
	}

	string SpecificDefault(const Info_& src)
	{
		return HasSpecificDefault(src)
				? src.children_.find(DEFAULT)->second->content_
				: string();
	}

	bool HasCanonicalDefault(const string& scalar_type)
	{
		switch (TypeToIndex(scalar_type))
		{
		case NUMBER:
		case INTEGER:
		case BOOLEAN:
		case DATE:
		case DATETIME:
			return false;
		}
		return true;
	}

	string CType(const Info_& src);
	/*	here is the settings version -- we are using the storable version, below
	{
		const string type = GetMandatory(src, TYPE);
		const string scalar = ScalarCType(type, GetOptional(src, SUBTYPE));
		switch (Dimension(src))
		{
		default:
		case 0:
			return (!HasCanonicalDefault(type) && !GetOptional(src, OPTIONAL).empty() && SpecificDefault(src).empty())
					? "boost::optional<" + scalar + ">"
					: scalar;
		case 1:
			return "Vector_<" + scalar + ">";
		case 2:
			return "Matrix_<" + scalar + ">";
		}
	}*/

	string ExistenceTest(const Info_& src)
	{
		return src.content_ + "_ != " + CType(src) + "()";
	}

	string ExtractorImp(const Info_& src, const string& prefix)
	{
		string retval = prefix;
		string postfix;
		switch (TypeToIndex(GetMandatory(src, TYPE)))
		{
		case NUMBER:
			retval += "Double";
			break;
		case INTEGER:
			retval += "Int";
			break;
		case BOOLEAN:
			retval += "Bool";
			break;
		case STRING:
			retval += "String";
			break;
		case CELL:	// construct cell from cell
			if (prefix.find("Cell") == 0)
				return "Cell::Identity";	// ignore dimension; can't construct multiple cells from a single cell
			retval += "Cell";
		case HANDLE:
			if (GetOptional(src, SUBTYPE).empty())
				retval += "HandleBase";
			else
			{
				retval += "Handle";
				postfix = "<" + GetOptional(src, SUBTYPE) + "_>";
			}
			break;
		case ENUM:
			retval += "Enum";
			postfix = "<" + GetMandatory(src, SUBTYPE) + "_>";
			break;
		case SETTINGS_:
			if (prefix.find("Cell") == 0)
				retval += ":::";	// can't embed a settings in another settings
			retval += "Dictionary";
			break;
		case DATE:
			retval += "Date";
			break;
		}

		// now the dimension
		switch (Dimension(src))
		{
		case 1:
			retval += "Vector";
			break;
		case 2:
			retval += "Matrix";
			break;
		}
		return retval + postfix;
	}
	string Extractor(const Info_& src)
	{
		return ExtractorImp(src, "Cell::To");
	}
	string Inserter(const Info_& src)
	{
		return ExtractorImp(src, "Cell::From");
	}
	string RecordExtractor(const Info_& src)
	{
		return ExtractorImp(src, "Extract");
	}

	string MustExist(const Info_& src)
	{
		return GetOptional(src, DEFAULT) == "."
				? string()
				: string("1");
	}

	string DotValue(const Info_& src)
	{
		const string type = GetMandatory(src, TYPE);
		return !HasCanonicalDefault(type) && SpecificDefault(src) == "." && Dimension(src) == 0
				? ".get()"
				: string();
	}

//--------------------------------------------------------------------------

	string ScalarJType(const string& type, const string& subtype)
	{
		const char* RETVAL[NUM_TYPES] = { "double", "int", "boolean", "String", "Cell", ":::", ":::", "Date", ":::" };
		const int which = TypeToIndex(type);
		switch (which)
		{
		case HANDLE:
			return subtype.empty() ? string("Storable") : subtype;
		case ENUM:
		case SETTINGS_:
			return subtype;
		default:
			return string(RETVAL[which]);
		}
	}
	string JType(const Info_& src)
	{
		string retval = ScalarJType(GetMandatory(src, "type"), GetOptional(src, "subtype"));
		for (int dd = Dimension(src); dd > 0; --dd)
			retval = "Array<" + retval + ">";
		return retval;
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
//			case ENUM:
//				sub = "<a href=\"MG_" + sub + "_enum.htm\">" + sub + "</a>";
//				break;
			case SETTINGS_:
//			case RECORD:
				sub = "<a href=\"MG_" + sub + "_settings.htm\">" + sub + "</a>";
				break;
			}
			s += " of type " + sub;
		}
		return s;
	}

// Output code for serialization/deserialization

	string IsSettings(const Info_& src)
	{
		return TypeToIndex(GetMandatory(src, TYPE)) == SETTINGS_
			? "1"
			: string();
	}
	string IsEnum(const Info_& src)
	{
		return TypeToIndex(GetMandatory(src, TYPE)) == ENUM
			? "1"
			: string();
	}

	string NonrepeatingCType(const Info_& src)
	{
		const string type = GetMandatory(src, TYPE);
		const string scalar = ScalarCType(type, GetOptional(src, SUBTYPE));
		switch (Dimension(src))
		{
		default:
		case 0:
			return (!HasCanonicalDefault(type) && SpecificDefault(src) == ".")
				? "boost::optional<" + scalar + ">"
				: scalar;
		case 1:
			return "Vector_<" + scalar + ">";
		case 2:
			return "Matrix_<" + scalar + ">";
		}
	}

	string CType(const Info_& src)
	{
		string retval = NonrepeatingCType(src);
		if (src.children_.count(REPEAT))
			return "Vector_<" + (retval.back() == '>' ? retval + ' ' : retval) + ">";
		if (src.children_.count(OPTIONAL) && !src.children_.count(MULTIPLE) && !HasSpecificDefault(src) && Dimension(src) == 0 && !HasCanonicalDefault(GetMandatory(src, TYPE)))
			return "boost::optional<" + retval + ">";
		return retval;
	}

	string SupplyDefault(const Info_& src)	// this routine is also responsible for supplying the leading comma, if needed
	{
		if (HasSpecificDefault(src))
		{
			const string dv = SpecificDefault(src);
			return ", " + (dv == "." ? CType(src) + "()" : SpecificDefault(src));
		}
		else if (HasCanonicalDefault(GetMandatory(src, TYPE)))
			return ", " + CType(src) + "()";	// i.e., default constructor of the type
		else
			return string();
	}

	string CoerceFromView(const Info_& src)	// query view for the desired type
	{
		string type = GetMandatory(src, TYPE);
		string subtype = GetOptional(src, SUBTYPE);
		if (TypeToIndex(type) == HANDLE)
		{
			return subtype.empty()
				? "Archive::Builder_<>(share, \"" + src.content_ + "\", \"\")"
				: "Archive::Builder_<" + subtype + "_>(share, \"" + src.content_ + "\", \"" + subtype + "\")";
		}
		if (TypeToIndex(type) == ENUM)
		{
			return "[](const Archive::View_& src) { return " + subtype + "_(src.AsString()); }";
		}
		string retval = "std::mem_fun_ref(&Archive::View_::As";
		string scalar = ScalarCType(type, subtype);	// except Settings
		if (TypeToIndex(type) == SETTINGS_)
			scalar = "Dictionary";
		if (scalar.substr(0, 5) == "std::")
			scalar = scalar.substr(5);
		scalar[0] = toupper(scalar[0]);
		if (scalar.back() == '_')
			scalar.pop_back();
		retval += scalar;
		switch (Dimension(src))
		{
		case 1:
			retval += "Vector";
			break;
		case 2:
			retval += "Matrix";
			break;
		}
		return retval + ")";
	}

//----------------------------------------------------------------------------

	struct MakeObjectEmitter_ : Emitter::Source_
	{
		MakeObjectEmitter_() 
		{
			Emitter::RegisterSource(OBJECT, *this);
			Emitter::RegisterSource(SETTINGS, *this);
			Emitter::RegisterSource(STORABLE, *this);
			Emitter::RegisterSource(RECORD, *this);
		}
		Emitter::Funcs_ Parse
			(const vector<string>& lib,
			 const string& path) 
		const
		{
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Object.mgt", &tLines);
			auto retval = Template::Parse(tLines);
			// now add C++ functions
			retval.ofInfo_["CType"].reset(EmitUnassisted(CType));
			retval.ofInfo_["SupplyDefault"].reset(EmitUnassisted(SupplyDefault));

			retval.ofInfo_["Extractor"].reset(EmitUnassisted(Extractor));
			retval.ofInfo_["Inserter"].reset(EmitUnassisted(Inserter));
			retval.ofInfo_["RecordExtractor"].reset(EmitUnassisted(RecordExtractor));
			retval.ofInfo_["MustExist"].reset(EmitUnassisted(MustExist));
			retval.ofInfo_["ExistenceTest"].reset(EmitUnassisted(ExistenceTest));
			retval.ofInfo_["JType"].reset(EmitUnassisted(JType));
			retval.ofInfo_["TypeForHelp"].reset(EmitUnassisted(TypeForHelp));
			retval.ofInfo_["DotValue"].reset(EmitUnassisted(DotValue));

			retval.ofInfo_["IsSettings"].reset(EmitUnassisted(IsSettings));
			retval.ofInfo_["IsEnum"].reset(EmitUnassisted(IsEnum));
			retval.ofInfo_["CoerceFromView"].reset(EmitUnassisted(CoerceFromView));

			return retval;
		}
	};
	static MakeObjectEmitter_ TheEmitter_;
}	// leave local


// parse public mark-up, emit results
// this source file has no associated header

#include "info.hpp"

HERE

#include "emitter.hpp"
#include "file.hpp"
#include "parseutils.hpp"
#include "template.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
// Mark-up format
//    filename is function_name.public.if (or other extension specified by config); or first line is "public " then function name
//    dimension declarator, if present, is "[]" or "[][]"
//    greedy declarator, if present, is "+"
//
//      <help>
//  [category <cat>]
//  [-<target>]*                                                    `` suppress
//  [volatile]
//  &inputs
//  [&optional]                                                     `` separates mandatory from optional inputs
//  <name>[ aka <tempname>] is <type>[<dimension>][<greedy>] [<subtype>] [(<default>)] [or <intype>-><translator>]
//      [&condition using $[\help]]
//      <help>
//  [+<insert_code>]
//  &conditions
//  <code>
//      <help>
//  &outputs
//  <name> is <type>[<dimension>] [<subtype>]
//      <help>
//  [&notes
//      <help text>]
//  [&links]
//  <name> [<type if not public>]
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //


// if tempname is used, then the input will be given that name, and insert_code is responsible for creating a quantity with the master name to send to the internal function



using namespace ParseUtils;


namespace {
    static const std::string HELP("help");
    static const std::string INSERT("insert");
    static const std::string INPUT("input");
    static const std::string OUTPUT("output");
    static const std::string OPTIONAL("optional");
    static const std::string CONDITION("condition");
    static const std::string START_INPUTS("&inputs");
    static const std::string START_OUTPUTS("&outputs");
    static const std::string START_OPTIONAL("&optional");
    static const std::string START_CONDITIONS("&conditions");
    static const std::string IS_TOKEN(
            " is ");                                              // includes space separators, meaning tabs won't be recognized
    static const std::string TAKES("takes");
    static const std::string TAKES_TOKEN(' ' + TAKES +
                                         ' ');                                // includes space separators, meaning tabs won't be recognized
    static const std::string IS_COL("is_column");
    static const std::string IS_COL_TOKEN(' ' + IS_COL +
                                          ' ');                              // includes space separators, meaning tabs won't be recognized
    static const std::string AKA(" aka ");                                                  // includes space separators
    static const std::string MYNAME("myname");                                              // internal name
    static const std::string TYPE("type");
    static const std::string SUBTYPE("subtype");
    static const std::string DEFAULT("default");
    static const std::string DIMENSION("dimension");
    static const std::string GREEDY("greedy");
    static const std::string PUBLIC("public");
    static const std::string SUPPRESS("suppress");
    static const std::string CATEGORY("category");
    static const std::string VOLATILE("volatile");
    static const std::string IN_TYPE("intype");
    static const std::string TRANSLATOR("translator");
    static const std::string NOTES("notes");
    static const std::string START_NOTES("&notes");
    static const std::string LINK("link");
    static const std::string START_LINKS("&links");

    std::pair<std::string, std::string>
    NamePair(const std::string &src) {
        auto aka = src.find(AKA);
        if (aka == std::string::npos) {
            return std::make_pair(src,
                                  src);                                                // internal and external names are the same
        }
        return std::make_pair(src.substr(0, aka), src.substr(aka + AKA.size()));
    }


    void
    PopulateArgument(Info_ *arg,
                     const std::string &name,
                     const std::string &type,
                     int dim,
                     bool takes,
                     bool is_col,
                     bool greedy,
                     const std::string &subtype,
                     bool optional,
                     const std::string &def_val,
                     const std::map<std::string,
                             std::string> &in_type_translators,
                     std::unique_ptr<Info_> *help,
                     const std::vector<Handle_ < Info_>

    >& conditions,
    std::unique_ptr<Info_> *insert
    ) {
    auto names = NamePair(name);
    arg->
    content_ = names.first;
    arg->children_.
    insert(std::make_pair(MYNAME, Info::MakeLeaf(arg, arg->root_, names.second))
    );
    arg->children_.
    insert(std::make_pair(TYPE, Info::MakeLeaf(arg, arg->root_, type))
    );
    if (dim > 0) {
    arg->children_.
    insert(std::make_pair(DIMENSION, Info::MakeLeaf(arg, arg->root_, std::string(1, '0' + dim)))
    );
}
if (takes)
{
arg->children_.
insert(std::make_pair(TAKES, Info::MakeLeaf(arg, arg->root_, "1"))
);
}
if (is_col)
{
arg->children_.
insert(std::make_pair(IS_COL, Info::MakeLeaf(arg, arg->root_, "1"))
);
}
if (greedy)
{
arg->children_.
insert(std::make_pair(GREEDY, Info::MakeLeaf(arg, arg->root_, "1"))
);
}
(void)
AddNonempty(arg, SUBTYPE, subtype
);
if (help->

get()

)
{
(*help)->
parent_ = arg;
arg->children_.
insert(std::make_pair(HELP, Handle_<Info_>(help->release()))
);
}
for (
auto pc = conditions.begin();
pc != conditions.

end();

++pc)
{
arg->children_.
insert(std::make_pair(CONDITION, *pc)
);
}
if (insert->

get()

)
{
(*insert)->
parent_ = arg;
arg->children_.
insert(std::make_pair(INSERT, Handle_<Info_>(insert->release()))
);
}
if (optional)
{
arg->children_.
insert(std::make_pair(OPTIONAL, Info::MakeLeaf(arg, arg->root_, "1"))
);
AddNonempty(arg, DEFAULT, def_val
);
}
for (
auto pi = in_type_translators.begin();
pi != in_type_translators.

end();

++pi)
{
std::unique_ptr<Info_> child(new Info_(arg, arg->root_, pi->first));
if (!pi->second.

empty()

)
{
child->children_.
insert(std::make_pair(TRANSLATOR, Info::MakeLeaf(child.get(), arg->root_, pi->second))
);
}
arg->children_.
insert(std::make_pair(IN_TYPE, child.release())
);
}
}


std::pair<std::string, std::string>
SubtypeAndDefault(const std::string &mash) {
    auto bra = mash.find('(');
    if (bra == std::string::npos) {
        return std::make_pair(mash, std::string());                                     // no default
    }
    std::pair<std::string, std::string> retval(TrimWhitespace(mash.substr(0, bra)),
                                               TrimWhitespace(mash.substr(bra + 1)));
    if (!retval.second.empty()) {
        REQUIRE(retval.second.back() == ')', "Found '(' for default input value without matching ')'");
        retval.second.pop_back();
    }
    return retval;
}


void
ExtractTranslators(std::string *rest, std::map<std::string, std::string> *translators) {
    // peel translators (e.g. "or string->MyTypeFromString") off the right side of the markup line

    for (;;) {
        // find last occurrence of separator string " or "
        auto orLoc = rest->rfind(" or ");
        if (orLoc == std::string::npos) {
            return;
        }
        auto tt = rest->substr(orLoc + 4);
        *rest = rest->substr(0, orLoc);

        // split the type and translator on "->"
        auto sepLoc = tt.find("->");
        if (sepLoc == std::string::npos) {
            translators->insert(std::make_pair(tt,
                                               std::string()));                     // no translator supplied, we'll autogenerate something
        } else {
            translators->insert(std::make_pair(tt.substr(0, sepLoc), tt.substr(sepLoc + 2)));
        }
    }
}


std::vector<std::string>::const_iterator
ReadArgument(const Info_ *parent,
             std::vector<std::string>::const_iterator line,
             std::vector<std::string>::const_iterator end,
             bool optional,
             Handle_ <Info_> *dst) {
    std::unique_ptr<Info_> arg(new Info_(parent, parent, std::string()));               // populate content later

    REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare argument");

    bool takes = false, isCol = false;
    auto is = line->find(IS_TOKEN), startRest = is + IS_TOKEN.size();
    if (is == std::string::npos) {
        is = line->find(TAKES_TOKEN);
        if (is != std::string::npos) {
            takes = true;
            startRest = is + TAKES_TOKEN.size();
        } else                                                                            // not that either
        {
            is = line->find(IS_COL_TOKEN);
            REQUIRE(is != std::string::npos,
                    "Input needs type declaration using 'is', 'takes', or 'is_column' (" + *line + ")");
            isCol = true;
            startRest = is + IS_COL_TOKEN.size();
        }
    }

    const std::string name = line->substr(0, is);
    std::string rest = line->substr(startRest);
    auto endType = rest.find_first_of(
            " \t[");                                          // anything that terminates the type
    const std::string type = rest.substr(0, endType);
    rest = rest.substr(type.size());
    int dim = 0;

    while (rest.substr(0, 2) == "[]") {
        ++dim;
        rest = rest.substr(2);
    }
    bool greedy = !rest.empty() && rest[0] == '+';
    if (greedy) {
        rest = rest.substr(1);
    }

    REQUIRE(StartsWithWhitespace(rest), "No space before subtype");

    std::map<std::string, std::string> inTranslators;
    ExtractTranslators(&rest, &inTranslators);
    auto sub_def = SubtypeAndDefault(AfterInitialWhitespace(rest));
    const std::string subtype = rest.empty() ? std::string() : AfterInitialWhitespace(rest);
    std::unique_ptr<Info_> help, insert;
    ++line;

    std::vector<Handle_ < Info_> > conditions;
    line = ReadHelp(arg.get(), parent->root_, line, end, &help, &conditions);
    line = ReadInsert(arg.get(), parent->root_, line, end, &insert);
    PopulateArgument(arg.get(), name, type, dim, takes, isCol, greedy, sub_def.first, optional, sub_def.second,
                     inTranslators, &help, conditions, &insert);
    dst->reset(arg.release());
    return line;
}


std::vector<std::string>::const_iterator
ReadCategory(Info_ *parent,
             std::vector<std::string>::const_iterator line,
             std::vector<std::string>::const_iterator end) {
    if (line != end && !line->empty() && line->substr(0, CATEGORY.size()) == CATEGORY) {
        Handle_ <Info_> category = Info::MakeLeaf(parent, parent->root_,
                                                  AfterInitialWhitespace(line->substr(CATEGORY.size())));
        parent->children_.insert(std::make_pair(CATEGORY, category));
        ++line;
    }
    return line;
}


std::vector<std::string>::const_iterator
ReadVolatile(Info_ *parent,
             std::vector<std::string>::const_iterator line,
             std::vector<std::string>::const_iterator end) {
    if (line != end && *line == VOLATILE) {
        Handle_ <Info_> child = Info::MakeLeaf(parent, parent->root_, "1");
        parent->children_.insert(std::make_pair(VOLATILE, child));
        ++line;
    }
    return line;
}


std::vector<std::string>::const_iterator
ReadSuppress(const Info_ *parent,
             std::vector<std::string>::const_iterator line,
             std::vector<std::string>::const_iterator end,
             Handle_ <Info_> *suppress) {
    std::unique_ptr<Info_> temp;
    while (line != end && !line->empty() && line->front() == '-') {
        if (!temp.get()) {
            temp.reset(new Info_(parent, parent, std::string()));
        }
        temp->children_.insert(std::make_pair(line->substr(1), Info::MakeLeaf(temp.get(), parent->root_, "_")));
        ++line;
    }
    if (temp.get()) {
        suppress->reset(temp.release());
    }
    return line;
}


struct ParsePublic_ : Info::Parser_ {
    ParsePublic_() {
        Info::RegisterParser(PUBLIC, *this);
    }

    Info_ *
    operator()(const std::string &info_name, const std::vector<std::string> &content) const {
        std::unique_ptr<Info_> retval(new Info_(0, 0, info_name));
        auto line = content.begin();

        // read the help
        std::unique_ptr<Info_> help;
        line = ReadHelp(retval.get(), retval.get(), line, content.end(), &help);
        if (help.get()) {
            retval->children_.insert(std::make_pair(HELP, Handle_<Info_>(help.release())));
        }
        line = ReadCategory(retval.get(), line,
                            content.end());                         // finds any category specification, inserts it and advances line
        line = ReadVolatile(retval.get(), line,
                            content.end());                         // handles volatile specification if present

        // any targets to suppress?
        Handle_ <Info_> suppress;
        line = ReadSuppress(retval.get(), line, content.end(), &suppress);
        if (suppress.get()) {
            retval->children_.insert(std::make_pair(SUPPRESS, suppress));
        }

        REQUIRE(line != content.end() && !line->empty() && line->front() == '&',
                "After function help, need '&inputs' or '&outputs'");

        if (*line ==
            START_INPUTS)                                                      // OK, there might be some inputs
        {
            ++line;
            bool optional = false;
            while (line != content.end() && *line != START_OUTPUTS && *line != START_CONDITIONS) {
                if (*line == START_OPTIONAL) {
                    optional = true;
                    ++line;
                    continue;
                }

                Handle_ <Info_> thisArg;
                line = ReadArgument(retval.get(), line, content.end(), optional, &thisArg);
                retval->children_.insert(std::make_pair(INPUT, thisArg));
            }
        }

        // maybe there are conditions
        if (line != content.end() && *line == START_CONDITIONS) {
            ++line;
            while (line != content.end() && *line != START_OUTPUTS) {
                Handle_ <Info_> thisCond;
                line = ReadCondition(retval.get(), line, content.end(), &thisCond);
                retval->children_.insert(std::make_pair(CONDITION, thisCond));
            }
        }

        // now should be at the outputs
        REQUIRE(line != content.end() && *line == START_OUTPUTS, "Expected '&outputs'");

        ++line;
        while (line != content.end() && *line != START_LINKS && *line != START_NOTES) {
            Handle_ <Info_> thisArg;
            line = ReadArgument(retval.get(), line, content.end(), false, &thisArg);
            retval->children_.insert(std::make_pair(OUTPUT, thisArg));
        }

        // maybe there are notes, or links
        if (line != content.end() && *line == START_NOTES) {
            std::unique_ptr<Info_> notes;
            line = ReadHelp(retval.get(), retval.get(), ++line, content.end(), &notes);
            if (notes.get()) {
                retval->children_.insert(std::make_pair(NOTES, Handle_<Info_>(notes.release())));
            }
        }
        if (line != content.end() && *line == START_LINKS) {
            ++line;
            while (line != content.end() && !line->empty() && line->front() != '&') {
                Handle_ <Info_> link;
                line = ReadLink(retval.get(), line, content.end(), &link);
                retval->children_.insert(std::make_pair(LINK, link));
            }
        }

        REQUIRE(line == content.end(), "Unexpected extra content beginning with '" + *line + "'");
        return retval.release();
    }
};

static ParsePublic_ TheParser;


// Now for the outputs
// here are all the basic types
enum {
    NUMBER = 0,
    INTEGER = 1,
    BOOLEAN = 2,
    STRING = 3,
    TIMEPOINT = 4,
    CELL = 5,
    DICTIONARY = 6,
    ENUM = 7,
    HANDLE = 8,
    SETTINGS = 9,
    DATE = 10,
    RECORD = 11,
    LONG_INT = 12,
    NUM_TYPES
};


int
TypeToIndex(const std::string &type) {
    static std::map<std::string, int> VALS;
    static bool first = true;

    if (first) {
        first = false;
        VALS["number"] = NUMBER;
        VALS["integer"] = INTEGER;
        VALS["boolean"] = BOOLEAN;
        VALS["string"] = STRING;
        VALS["timepoint"] = TIMEPOINT;
        VALS["cell"] = CELL;
        VALS["dictionary"] = DICTIONARY;
        VALS["enum"] = ENUM;
        VALS["handle"] = HANDLE;
        VALS["settings"] = SETTINGS;
        VALS["date"] = DATE;
        VALS["record"] = RECORD;
        VALS["long"] = LONG_INT;
    }
    auto pt = VALS.find(type);
    REQUIRE(pt != VALS.end(), "Invalid type '" + type + "'");
    return pt->second;
}


std::string
ScalarCType(const std::string &type,
            const std::string &subtype,
            bool suppress_subtype = false) {
    std::array<const char *, NUM_TYPES> RETVAL = {"double", "int", "bool", "String_", "TimePoint", "Cell_",
                                                  "Dictionary_", ":::", ":::", ":::", "Date_", ":::", "long long"};
    const int which = TypeToIndex(type);

    switch (which) {
        case ENUM: {
            return suppress_subtype ? "String_" : subtype + "_";
        }

        case HANDLE: {
            return "Handle_<" + (subtype.empty() || suppress_subtype ? std::string("Storable") : subtype) + "_>";
        }

        case SETTINGS:
        case RECORD: {
            return subtype + "_";
        }

        default: {
            return std::string(RETVAL[which]);
        }
    }
}


std::string
XDNType(const std::string &type, int dimension, int is_top = true) {
    const char *RETVAL[NUM_TYPES] = {"System::Double",
                                     "System::Int32",
                                     "System::Boolean",
                                     "System::String^",
                                     "System::DateTime^",
                                     "TALibNet::Atom_^",
                                     "System::Collections::Hashtable^",
                                     "System::String^",
                                     "TALibNet::Object_^",
                                     "System::Collections::Hashtable^",
                                     "System::DateTime^",
                                     "System::Collections::Hashtable^",
                                     "System::Int64"};

    return dimension == 0 ? RETVAL[TypeToIndex(type)] : "array<" + XDNType(type, dimension - 1, false) + " >^";
}


int
Dimension(const Info_ &arg) {
    const std::string dim = GetOptional(arg, DIMENSION);
    if (dim == "0" || dim.empty()) {
        return 0;
    }
    if (dim == "1") {
        return 1;
    }

    REQUIRE(dim == "2", "Unexpected dimension");
    return 2;
}


bool
HasCanonicalDefault(const std::string &scalar_type) {
    switch (TypeToIndex(scalar_type)) {
        case NUMBER:
        case INTEGER:
        case BOOLEAN:
        case TIMEPOINT:
        case DATE: {
            return false;
        }
    }
    return true;
}


std::string
IsCookie(const Info_ &src) {
    if (Dimension(src) == 0) {
        switch (TypeToIndex(GetMandatory(src, TYPE))) {
            case NUMBER:
            case INTEGER:
            case BOOLEAN:
            case TIMEPOINT:
            case DATE:
            case STRING: {
                return std::string();                                                   // these are the types we can pass directly
            }
        }
    }
    return "1";
}


std::string
XCppInType(const Info_ &src, bool suppress_subtype) {
    const std::string type = GetMandatory(src, TYPE);
    std::string scalar = ScalarCType(type, GetOptional(src, SUBTYPE), suppress_subtype);

    std::string retval;
    switch (Dimension(src)) {
        default:
        case 0: {
            retval = scalar;
            break;
        }

        case 1: {
            retval = "Vector_<" + scalar + ">";
            break;
        }

        case 2: {
            retval = "Matrix_<" + scalar + ">";
            break;
        }
    }
    const bool ptrArg = !GetOptional(src, OPTIONAL).empty() && GetOptional(src, DEFAULT).empty();

    return GetOptional(src, TAKES).empty() ? "const " + retval + (ptrArg ? '*' : '&') : "Carrier_<" + retval + '>' +
                                                                                        std::string(
                                                                                                ptrArg ? "*" : "&&");
}


std::string
XCppOutType(const Info_ &src, bool suppress_subtype) {
    std::string temp = XCppInType(src, suppress_subtype);
    temp.back() = '*';                                                                  // never a reference
    if (temp.substr(0, 6) == "const ") {
        return temp.substr(6);                                                          // never const
    }
    return temp;
}


std::string
CppOutType(const Info_ &src) {
    return XCppOutType(src, false);
}


std::string
CppLvalueType(const Info_ &src) {
    std::string retval = XCppOutType(src, true);
    while (!retval.empty() && (retval.back() == '*' || retval.back() == '&'))
        retval.pop_back();
    return retval;
}


std::string
CppDefault(const Info_ &src) {
    return GetOptional(src, TAKES).empty() ? CppLvalueType(src) +
                                             "()"                  // construct non-lvalue on the fly
                                           : src.content_ +
                                             "_default";                 // need an lvalue, which by convention gets this name
}


std::string
CppPassArg_ConvertPointers(const Info_ &src) {
    if (GetOptional(src, OPTIONAL).empty()) {
        return src.content_;                                                            // pass as-is
    }
    if (Dimension(src) == 0) {
        auto type = GetMandatory(src, TYPE);
        auto defval = GetOptional(src, DEFAULT);
        if (!HasCanonicalDefault(type)) {
            return defval.empty() ? "AsOptional(" + src.content_ + ')'                  // have to wrap in an optional
                                  : src.content_;                                       // default already supplied or unavailable
        }
        if ((TypeToIndex(type) == STRING || TypeToIndex(type) == ENUM) && !defval.empty()) {
            return src.content_;                                                        // default already supplied
        }
    }
    return '(' + src.content_ + " ? *" + src.content_ + " : " + CppDefault(src) + ")";
}


std::string
CppPassArg(const Info_ &src) {
    const std::string retval = CppPassArg_ConvertPointers(src);                         // might be all we need

    const std::string subtype = GetOptional(src, SUBTYPE);
    const std::string outtype = CppOutType(src);
    const std::string enumsubtype = outtype.substr(0, outtype.size() - 1);

    switch (TypeToIndex(GetMandatory(src, TYPE))) {
        case HANDLE: {
            if (!subtype.empty())                                                       // have to downcast the handles
            {
                return "CheckedCast_<" + subtype + "_>()(" + retval + ", \"" + subtype + "\")";
            }
            break;
        }

        case ENUM: {
            if (!Dimension(src)) {
                return subtype + "_(" + retval + ")";
            } else {
                return "Apply(ConstructCast_<" + subtype + "_>(), " + retval + ")";
            }
        }
    }
    if (!GetOptional(src, TAKES).empty()) {
        return "std::forward<" + CppLvalueType(src) + ">(" + retval + ")";
    }

    // ok, no special cases triggered
    return retval;
}


std::string
TypeForCat(const std::string &type, const std::string &subtype) {
    std::string retval = subtype.empty() ? type : subtype;
    retval[0] = toupper(retval[0]);
    return retval;
}


std::string
InTypeTranslator(const Info_ &src) {
    auto user = GetOptional(src, TRANSLATOR);
    if (!user.empty())                                                                  // user has supplied a translator
    {
        return user;
    }
    return TypeForCat(GetMandatory(*src.parent_, TYPE), GetOptional(*src.parent_, SUBTYPE)) + "::From" +
           TypeForCat(src.content_, std::string());
}


std::string
DotNetType(const Info_ &src) {
    const std::string type = GetMandatory(src, TYPE);
    std::string retval = XDNType(type, Dimension(src));
    return (!GetOptional(src, OPTIONAL).empty() && !HasCanonicalDefault(type) && Dimension(src) == 0) ?
           "TALibNet::Optional_<" + retval + ">^" : retval;
}


std::string
MMPassAsString(const Info_ &src) {
    const int dim = Dimension(src);
    switch (TypeToIndex(GetMandatory(src, TYPE))) {
        case STRING: {
            if (dim < 1) {
                break;
            }
        }

        case CELL:
        case DICTIONARY:
        case SETTINGS:
        case RECORD: {
            return std::string("1");
        }

        default: {
            break;
        }
    }
    return std::string();
}


std::string
DimensionString(std::size_t dim) {
    std::string retval("0");
    retval[0] += static_cast<char>(dim);
    return retval;
}


std::string
XMMType(const std::string &type, int dimension, int is_top = true) {
    const char *RETVAL[NUM_TYPES] = {"Real", "Integer", "\"Boolean\"", "\"UTF8String\"", "\"TIME\"", "\"CELL\"",
                                     "\"DICTIONARY\"", "\"UTF8String\"", "\"UTF8String\"", "\"SETTINGS\"",
                                     "\"UTF8String\"", "\"SETTINGS\"", "Integer"};

    return dimension == 0 ? RETVAL[TypeToIndex(type)] : "{" + XMMType(type, 0, false) + "," +
                                                        DimensionString(dimension) + ",Constant}";
}


std::string
XMMTypeShort(const std::string &type, int dimension, int is_top = true) {
    const char *RETVAL[NUM_TYPES] = {"Num", "Int", "Bool", "Str", "Time", "Cell", "Dict", "Str", "Str", "Dict", "Date",
                                     "Dict", "Int"};
    return RETVAL[TypeToIndex(type)] + (dimension == 0 ? std::string() : DimensionString(dimension));
}


std::string
MathematicaArgType(const Info_ &src) {
    if (!MMPassAsString(src).empty()) {
        return std::string("\"UTF8String\"");
    }

    const std::string type = GetMandatory(src, TYPE);
    std::string retval = XMMType(type, Dimension(src));
    return retval;                                                                      // postponed -- handle optional args?  maybe with overloading?
}


std::string
MathematicaArgTypeQ(const Info_ &src) {
    std::string retval = MathematicaArgType(src);
    if (!retval.empty() &&
        retval[0] == '"')                                            // retval is quoted, need to escape the quotes
    {
        retval.back() = '\\';
        return '\\' + retval + '"';
    }
    return retval;                                                                      // postponed -- handle optional args?  maybe with overloading?
}


std::string
MathematicaShortType(const Info_ &src) {
    const std::string type = GetMandatory(src, TYPE);
    std::string retval = XMMTypeShort(type, Dimension(src));
    return retval;                                                                      // postponed -- handle optional args?  maybe with overloading?
}


std::string
HashedArgNumbers(const Info_ &src) {
    const std::size_t nInputs = src.children_.count("input");
    std::string retval;

    for (std::size_t ii = 0; ii < nInputs; ++ii) {
        retval += '#' + DimensionString(ii + 1) + ", ";
    }
    while (!retval.empty() && (retval.back() == ' ' || retval.back() == ',')) {
        retval.pop_back();
    }
    return retval;
}


std::string
XJNISignatureType(const std::string &type, int dimension) {
    const char *RETVAL[NUM_TYPES] = {"D", "I", "B", "Ljava/lang/String;", "Ljava/util/Date;", "LDA/util/Cell;",
                                     "LDA/Dictionary;", "Ljava/lang/String;", "LDA/Handle;", "LDA/Dictionary;",
                                     "Ljava/util/Date;", "LDA/Dictionary;", "J"};
    std::string retval(RETVAL[TypeToIndex(type)]);

    for (int id = 0; id < dimension; ++id) {
        retval = '[' + retval + ')';
    }
    return retval;
}


std::string
JNI_SignatureType(const Info_ &src) {
    return XJNISignatureType(GetMandatory(src, TYPE), Dimension(src));
}


std::string
XJNINativeType(const std::string &type, int dimension) {
    const char *SCALAR_TYPE[NUM_TYPES] = {"jdouble", "jint", "jboolean", "jstring", "jobject", "jobject", "jobject",
                                          "jstring", "jobject", "jobject", "jobject", "jobject", "jlong"};
    const char *ELEMENT_TYPE[NUM_TYPES] = {"double", "int", "boolean", "", "", "", "", "", "", "", "", "", "long"};

    if (dimension > 0) {
        std::string e(ELEMENT_TYPE[TypeToIndex(type)]);
        if (dimension > 1 || e.empty())
            return "jobjectArray";
        return "j" + e + "Array";
    }
    return SCALAR_TYPE[TypeToIndex(type)];
}


std::string
JNI_NativeType(const Info_ &src) {
    return XJNINativeType(GetMandatory(src, TYPE), Dimension(src));
}


std::string
XJuliaType(const std::string &type, int dimension) {
    std::array<const char *, NUM_TYPES> SCALAR = {"Float64", "Int32", "Bool", "Ptr{Uint8}", "TIME", "CELL",
                                                  "DICTIONARY", "ENUM", "Handle", "SETTINGS", "DATE", "RECORD",
                                                  "Int64"};
    const std::string retval = SCALAR[TypeToIndex(type)];

    return dimension == 0 ? retval : "Array{" + retval + ", " + DimensionString(dimension) + "}";
}


std::string
JuliaType(const Info_ &src) {
    return XJuliaType(GetMandatory(src, TYPE), Dimension(src));
}


std::string
CookiePostfix(const Info_ &src) {
    std::array<const char *, NUM_TYPES> SCALAR = {"Double", "Int", "Bool", "String", "TimePoint", "Cell", "Dict",
                                                  "Enum", "Handle", "Settings", "Date", "Record", "Long"};
    std::string retval;

    switch (Dimension(src)) {
        case 1: {
            retval += "Vector";
            break;
        }
        case 2: {
            retval += "Matrix";
            break;
        }
    }
    retval += SCALAR[TypeToIndex(GetMandatory(src, TYPE))];
    return retval;
}


//--------------------------------------------------------------------------


struct MakePublicEmitter_ : Emitter::Source_ {
    MakePublicEmitter_() {
        Emitter::RegisterSource(PUBLIC, *this);
    }


    Emitter::Funcs_
    Parse(const std::vector<std::string> &lib, const std::string &path) const {
        // start with the library
        std::vector<std::string> tLines(lib);

        // add the template
        File::Read(path + "Public.mgt", &tLines);
        auto retval = Template::Parse(tLines);

        // now add C++ functions
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
        retval.ofInfo_["CppPassArg"].reset(EmitUnassisted(CppPassArg));

        return retval;
    }
};

static MakePublicEmitter_ TheEmitter_;

}  // namespace <un-named>

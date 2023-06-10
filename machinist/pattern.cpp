// parse pattern mark-up, emit results

// this machinist file has no associated header
#include "emitter.hpp"
#include "info.hpp"
#include "parseutils.hpp"
#include "template.hpp"
#include "file.hpp"

using namespace ParseUtils;

namespace {

    static const std::string PARTS("&parts");
    static const std::string ENTRY("entry");
    static const std::string PATTERN("pattern");
    static const std::string TYPE("type");
    static const std::string ARROW(" -> ");
    static const std::string DATEPATTERN("date");
    static const std::string DATETYPE("date");
    static const std::string RELDATETYPE("reldate");
    static const std::string SECONDVAR("secondvar");

    Info_* NewPattern(const Info_* parent,
                      const std::string& pattern,
                      const std::string& type,
                      const std::string& varName,
                      const std::string& secondVar) {
        unique_ptr<Info_> retval(new Info_(parent, parent, varName));

        retval->children_.insert(make_pair(PATTERN, Info::MakeLeaf(retval.get(), retval->root_, pattern)));
        retval->children_.insert(make_pair(TYPE, Info::MakeLeaf(retval.get(), retval->root_, type)));
        retval->children_.insert(make_pair(SECONDVAR, Info::MakeLeaf(retval.get(), retval->root_, secondVar)));

        return retval.release();
    }

    std::vector<std::string>::const_iterator ReadPattern(const Info_* parent,
                                               std::vector<std::string>::const_iterator line,
                                               std::vector<std::string>::const_iterator end,
                                               bool optional,
                                               Handle_<Info_>* dst) {
        REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare pattern");
        auto arrow = line->find(ARROW);
        REQUIRE(arrow != line->npos, "Input needs type declaration using '->'");
        std::string pattern = line->substr(0, arrow);
        std::string rest = line->substr(arrow + ARROW.size());
        auto endType = rest.find_first_of(" \t"); // anything that terminates the type
        const std::string type = rest.substr(0, endType);
        REQUIRE(type.size() != 0, "Required type of capture missing.");
        rest = AfterInitialWhitespace(rest.substr(type.size()));
        auto endVarName = rest.find_first_of(" \t"); // anything that terminates the variable name
        const std::string varName = rest.substr(0, endVarName);
        // see if there is a second var name (for creating relative dates)
        REQUIRE(varName.size() != 0, "Required variable name of capture missing.");

        rest = AfterInitialWhitespace(rest.substr(varName.size()));
        auto endSecondVar = rest.find_first_of(" \t"); // anything that terminates the type
        const std::string secondVar = rest.substr(0, endSecondVar);
        if (type == RELDATETYPE) {
            REQUIRE(pattern == DATEPATTERN && secondVar.size() != 0,
                    "Either pattern is not correct or relative date needs variable name for date it's relative to");
            pattern = std::string("\\\\d?\\\\d[DMYdmy]|\\\\d{8}"); // 4 \ required for C++ std string
        }
        if (type == DATETYPE) {
            REQUIRE(pattern == DATEPATTERN, "Date type and pattern must match.");
            pattern = std::string("\\\\d{8}");
        }

        dst->reset(NewPattern(parent, pattern, type, varName, secondVar));
        ++line;
        return line;
    }

    struct ParsePattern_ : Info::Parser_ {
        ParsePattern_() { Info::RegisterParser(PATTERN, *this); }

        Info_* operator()(const std::string& info_name, const std::vector<std::string>& content) const {

            unique_ptr<Info_> retval(new Info_(0, 0, info_name));

            auto line = content.begin();

            if (*line == PARTS) // these are the parts that will generate the regular expression
            {
                ++line;
                while (line != content.end()) {
                    Handle_<Info_> thisArg;

                    bool optional = false; // get rid of this

                    line = ReadPattern(retval.get(), line, content.end(), optional, &thisArg);

                    retval->children_.insert(make_pair(ENTRY, thisArg));
                }
            }
            return retval.release();
        }
    };

    const ParsePattern_ TheParser;

    enum { NUMBER = 0, INTEGER, STRING, DATE, NUM_TYPES };

    int TypeToIndex(const std::string& type) {
        static map<std::string, int> VALS;

        VALS["number"] = NUMBER;
        VALS["integer"] = INTEGER;
        VALS["string"] = STRING;
        VALS["date"] = DATE;
        VALS["reldate"] = DATE;

        auto pt = VALS.find(type);
        REQUIRE(pt != VALS.end(), "Invalid type for pattern '" + type + "'");
        return pt->second;
    }

    std::string CType(const Info_& src) {
        static const char* RETVAL[NUM_TYPES] = {"double", "int", "String_", "Date_"};

        const int which = TypeToIndex(GetMandatory(src, TYPE));
        return RETVAL[which];
    }
    std::string ConverterFromString(const Info_& src) {
        static const char* RETVAL[NUM_TYPES] = {"String::ToDouble", "String::ToInt", "String_", "NDate::FromString"};

        const int which = TypeToIndex(GetMandatory(src, TYPE));
        return RETVAL[which];
    }

    std::string SecondVar(const Info_& src) { return GetMandatory(src, SECONDVAR); }

    std::string IsRelDate(const Info_& src) { return (GetMandatory(src, TYPE) == RELDATETYPE) ? std::string("1") : std::string(); }

    struct MakePatternEmitter_ : Emitter::Source_ {
        MakePatternEmitter_() { Emitter::RegisterSource(PATTERN, *this); }
        Emitter::Funcs_ Parse(const std::vector<std::string>& lib, const std::string& path) const {
            // start with the library
            std::vector<std::string> tLines(lib);
            // add the template
            File::Read(path + "Pattern.mgt", &tLines);
            auto retval = Template::Parse(tLines);
            // now add C++ functions
            retval.ofInfo_["CType"].reset(EmitUnassisted(CType));
            retval.ofInfo_["ConverterFromString"].reset(EmitUnassisted(ConverterFromString));
            retval.ofInfo_["IsRelDate"].reset(EmitUnassisted(IsRelDate));
            retval.ofInfo_["SecondVar"].reset(EmitUnassisted(SecondVar));

            return retval;
        }
    };

    const MakePatternEmitter_ TheEmitter_;
} // namespace

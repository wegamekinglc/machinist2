
// parse enumeration mark-up, emit results

// this machinist file has no associated header
#include "emitter.hpp"
#include "file.hpp"
#include "info.hpp"
#include "parseutils.hpp"
#include "template.hpp"
#include <algorithm>
#include <iostream>
#include <filesystem>
using namespace ParseUtils;
using Info::MakeLeaf;

/* Mark-up format (filename is structure name, except for the trailing underscore)

        <help>
(alternative|default) <name> <alias>* [=numeric] [legacy shortname[|longname]]
(only one default -- can omit name for default, in which case the default constructor will create an uninitialized
state; can specify numeric value; can specify short/long names for legacy CORE_ENUMERATION_VALUE, but must |-separate
because the names themselves can contain spaces) <help> method <code> [switchable] [extensible] [legacy] prefix
*/

namespace {
    static const std::string ENUMERATION("enumeration");
    static const std::string HELP("help");
    static const std::string ALTERNATIVE("alternative");
    static const std::string DEFAULT("default");
    static const std::string METHOD("method");
    static const std::string SWITCHABLE("switchable");
    static const std::string EXTENSIBLE("extensible");
    static const std::string LEGACY("legacy");
    static const std::string LONGNAME("longname");
    static const std::string ALIAS("alias");
    static const std::string NUMERIC("numeric");

    Info_* NewMember(const Info_* parent, bool is_default, std::string all_names, unique_ptr<Info_>* help) {
        if (is_default)
            std::cout << "Enumeration has default\n";

        std::string lShort, lLong;
        // find any legacy names
        auto lLoc = all_names.find(LEGACY);
        if (lLoc != std::string::npos) {
            lShort =
                AfterInitialWhitespace(all_names.substr(lLoc + LEGACY.size())); // store it here, then split if needed
            all_names = all_names.substr(0, lLoc);
            auto sep = lShort.find('|');
            if (sep != std::string::npos) {
                lLong = AfterInitialWhitespace(lShort.substr(sep + 1));
                lShort = lShort.substr(0, sep);
            } else
                lLong = lShort; // easier output if these always exist together
        }

        std::string numeric;
        // find any numeric value
        auto eqLoc = all_names.find('=');
        if (eqLoc != std::string::npos) {
            numeric = AfterInitialWhitespace(all_names.substr(eqLoc + 1));
            all_names = all_names.substr(0, eqLoc);
        }

        // split names on whitespace
        std::vector<std::string> names;
        for (std::string rest = AfterInitialWhitespace(all_names); !rest.empty();) {
            auto stop = find_if(rest.begin(), rest.end(), IsWhite);
            names.push_back(std::string(rest.begin(), stop));
            rest = AfterInitialWhitespace(std::string(stop, rest.end()));
        }
        REQUIRE(!names.empty() || is_default, "Every alternative needs a name");

        unique_ptr<Info_> retval(new Info_(parent, parent, names.empty() ? "_NOT_SET" : EmbeddableForm(names[0])));
        for (auto pn = names.begin(); pn != names.end(); ++pn)
            retval->children_.insert(make_pair(ALIAS, MakeLeaf(retval.get(), retval->root_, *pn)));
        if (is_default)
            retval->children_.insert(make_pair(DEFAULT, MakeLeaf(retval.get(), retval->root_, "_")));

        if (help->get()) {
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

    std::vector<std::string>::const_iterator ReadAlternative(const Info_* parent,
                                                   std::vector<std::string>::const_iterator line,
                                                   std::vector<std::string>::const_iterator end,
                                                   Handle_<Info_>* dst) {
        REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare argument");
        const std::string start = UntilWhite(*line);
        const std::string rest = line->substr(start.size());

        unique_ptr<Info_> help;
        line = ReadHelp(0, parent, ++line, end, &help); // fix help's parent later
        dst->reset(NewMember(parent, start == DEFAULT, rest, &help));
        return line;
    }

    struct ParseEnumeration_ : Info::Parser_ {
        ParseEnumeration_() { Info::RegisterParser(ENUMERATION, *this); }

        Info_* operator()(const std::string& info_name, const std::vector<std::string>& content) const {
            unique_ptr<Info_> retval(new Info_(0, 0, info_name));
            auto line = content.begin();
            // read the help
            unique_ptr<Info_> help;
            line = ReadHelp(retval.get(), retval.get(), line, content.end(), &help);
            if (help.get())
                retval->children_.insert(make_pair(HELP, Handle_<Info_>(help.release())));

            while (line != content.end()) {
                // !a.find(b) is true iff a starts with b
                if (!line->find(ALTERNATIVE) || !line->find(DEFAULT)) {
                    Handle_<Info_> info;
                    line = ReadAlternative(retval.get(), line, content.end(), &info);
                    retval->children_.insert(
                        make_pair(ALTERNATIVE, info)); // note that DEFAULT is stored as an ALTERNATIVE
                } else if (!line->find(METHOD)) {
                    retval->children_.insert(
                        make_pair(METHOD, MakeLeaf(retval.get(), retval->root_,
                                                   AfterInitialWhitespace(line->substr(METHOD.size())))));
                    ++line;
                } else if (!line->find(SWITCHABLE)) {
                    retval->children_.insert(make_pair(SWITCHABLE, MakeLeaf(retval.get(), retval->root_, "_")));
                    ++line;
                } else if (!line->find(EXTENSIBLE)) {
                    retval->children_.insert(make_pair(EXTENSIBLE, MakeLeaf(retval.get(), retval->root_, "_")));
                    ++line;
                } else if (!line->find(LEGACY)) {
                    retval->children_.insert(
                        make_pair(LEGACY, MakeLeaf(retval.get(), retval->root_, line->substr(LEGACY.size() + 1))));
                    ++line;
                } else {
                    REQUIRE(0, "Unrecognized line: " + *line);
                }
            }
            return retval.release();
        }
    };
    const ParseEnumeration_ TheParser;

    std::string MaxNumericSentinel(const Info_& src) {
        int max = -1;
        auto alts = src.children_.equal_range(ALTERNATIVE);
        for (auto ia = alts.first; ia != alts.second; ++ia) {
            std::string num = GetOptional(*ia->second, NUMERIC);
            if (num.empty())
                break;
            int n = std::stoi(num);
            if (n > max)
                max = n;
        }
        const int startCount = static_cast<int>(src.children_.count(LEGACY));
        Template::SetGlobalCount(max < 0 ? startCount : max); // has side effect!
        return max > 0 ? "__MG_SENTINEL_VAL = " + std::to_string(max) + "\n\t\t" : std::string();
    }

    std::string Uppercase(const Info_& src) {
        std::string retval(src.content_);
        transform(retval.begin(), retval.end(), retval.begin(), toupper);
        return retval;
    }

    std::string SizeBase(const Info_& src) {
        if (src.children_.size() < 252)
            return " : char";
        return std::string();
    }

    struct MakeEnumerationEmitter_ : Emitter::Source_ {
        MakeEnumerationEmitter_() { Emitter::RegisterSource(ENUMERATION, *this); }
        Emitter::Funcs_ Parse(const std::vector<std::string>& lib, const std::string& path) const {
            // start with the library
            std::vector<std::string> tLines(lib);
            // add the template
            std::filesystem::path pl(path);
            File::Read((pl / "Enumeration.mgt").string(), &tLines);
            auto retval = Template::Parse(tLines);
            // now add C++ functions
            retval.ofInfo_["MaxNumericSentinel"].reset(EmitUnassisted(MaxNumericSentinel));
            retval.ofInfo_["Uppercase"].reset(EmitUnassisted(Uppercase));
            retval.ofInfo_["SizeBase"].reset(EmitUnassisted(SizeBase));
            // and library functions

            return retval;
        }
    };
    const MakeEnumerationEmitter_ TheEmitter_;
} // namespace

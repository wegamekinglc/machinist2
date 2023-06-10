
// parse Recipe mark-up, emit results

// this machinist file has no associated header
#include "emitter.hpp"
#include "file.hpp"
#include "info.hpp"
#include "parseutils.hpp"
#include "template.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <filesystem>
using namespace ParseUtils;
using namespace std;

namespace {
    static const std::string INGREDIENT("ingredient");
    static const std::string RECIPE("recipe");
    static const std::string MEASURE("measure");
    static const std::string NUMBER("number");
    static const std::string PROCESSED("processed");
    static const std::string ACTION("action");
    static const std::string CO("co");
    static const std::string PART("part");

    bool IsIngredientStart(const std::string& line) // see if it might be an ingredient
    {
        static const std::string FIRST("123456789#");
        return !line.empty() && FIRST.find(line[0]) != std::string::npos;
    }

    bool IsCO(const std::string& line) { return line.find("c/o") == 0 || line.find("&co") == 0; }

    bool IsNewPart(const std::string& line) { return line.substr(0, 6) == "+part " || line.substr(0, 6) == "&part "; }
    bool IsStopPart(const std::string& line) { return line.substr(0, 5) == "-part"; }
    std::string PartName(const std::string& line) { return line.substr(6); }

    bool StartsWith(const std::string& line, const std::string& token) { return line.find(token) == 0; }
    std::string StandardMeasure(const std::string& src) {
        std::string retval(src);
        transform(retval.begin(), retval.end(), retval.begin(), [](const unsigned char c) { return tolower(c); });
        if (StartsWith(retval, "tbsp") || StartsWith(retval, "tablespoon"))
            return "Tbsp";
        if (StartsWith(retval, "tsp") || StartsWith(retval, "teaspoon"))
            return "tsp";
        if (retval == "c." || StartsWith(retval, "cup"))
            return "c";
        if (retval == "g." || StartsWith(retval, "gram"))
            return "g";
        return retval;
    }

    Handle_<Info_> ParseIngredient(const std::string& line, const Info_* parent, const Info_* root) {
        assert(!line.empty());
        auto nStop = line.find_first_not_of("0123456789-/");
        std::string number = nStop == std::string::npos ? std::string() : line.substr(0, nStop);
        std::string rest = nStop == std::string::npos ? line : line.substr(nStop);
        if (!number.empty() && !rest.empty() && rest[0] == '~') // glue modifier to the number, e.g. 3~heaping
        {
            auto nSpace = rest.find(' ');
            REQUIRE(nSpace != std::string::npos, "Extended number fills whole ingredient line");
            number += line.substr(0, nSpace);
            rest = rest.substr(nSpace);
        }
        std::string measure;
        auto nHash = rest.find('#');
        if (nHash == std::string::npos) {
            // next thing is space-separated measure
            rest = AfterInitialWhitespace(rest);
            auto nSpace = rest.find(' ');
            REQUIRE(nSpace != std::string::npos, "Can't find measure for ingredient (" + line + ")");
            measure = StandardMeasure(rest.substr(0, nSpace));
            rest = TrimWhitespace(rest.substr(nSpace));
        } else {
            // no measure, just step past the hash
            rest = AfterInitialWhitespace(rest.substr(nHash + 1));
        }
        // next check for ", processed"
        auto comma = rest.find_first_of("(,");
        std::string processed;
        if (comma != std::string::npos) {
            processed = AfterInitialWhitespace(rest.substr(comma + 1));
            if (rest[comma] == '(') {
                // have to find the ')'
                auto ket = processed.find(')');
                REQUIRE(ket != std::string::npos, "'(' without matching ')'");
                processed[ket] = ' ';
            }
            rest = rest.substr(0, comma);
        }
        unique_ptr<Info_> retval(new Info_(parent, root, TrimWhitespace(rest)));
        if (!measure.empty())
            retval->children_.insert(make_pair(MEASURE, Info::MakeLeaf(retval.get(), root, measure)));
        if (!number.empty())
            retval->children_.insert(make_pair(NUMBER, Info::MakeLeaf(retval.get(), root, number)));
        if (!processed.empty())
            retval->children_.insert(make_pair(PROCESSED, Info::MakeLeaf(retval.get(), root, processed)));

        return retval.release();
    }

    Handle_<Info_> ParseCO(const std::string& line, const Info_* parent, const Info_* root) {
        return Info::MakeLeaf(parent, root, line.substr(4));
    }

    std::vector<std::string>::const_iterator AddIngredientsAndActions(Info_* info,
                                                            Info_* root,
                                                            std::vector<std::string>::const_iterator line,
                                                            std::vector<std::string>::const_iterator end) {
        // starts with ingredients
        while (line != end && IsIngredientStart(*line))
            info->children_.insert(make_pair(INGREDIENT, ParseIngredient(*line++, info, root)));
        // just finished ingredients
        REQUIRE(line != end, "Expected &do then actions after ingredients");
        REQUIRE(line->substr(0, 3) == "&do", "Expected &do then actions, not " + *line);
        while (++line != end && !IsNewPart(*line) && !IsStopPart(*line))
            info->children_.insert(make_pair(ACTION, Info::MakeLeaf(info, root, *line)));
        if (line != end && IsStopPart(*line))
            ++line; // skip this line
        return line;
    }

    struct ParseRecipe_ : Info::Parser_ {
        ParseRecipe_() { Info::RegisterParser(RECIPE, *this); }

        Info_* operator()(const std::string& info_name, const std::vector<std::string>& content) const {
            unique_ptr<Info_> retval(new Info_(0, 0, info_name));
            retval->root_ = retval.get();
            auto line = content.begin();
            // might have a c/o at the front
            if (line != content.end() && IsCO(*line))
                retval->children_.insert(make_pair(CO, ParseCO(*line++, retval.get(), retval.get())));
            // have to check for parts
            while (line != content.end() && IsNewPart(*line)) {
                unique_ptr<Info_> part(new Info_(retval.get(), retval.get(), PartName(*line)));
                line = AddIngredientsAndActions(part.get(), retval.get(), ++line, content.end());
                retval->children_.insert(make_pair(PART, part.release()));
            }
            cout << "Finished children at line " << (line - content.begin()) << "\n";
            AddIngredientsAndActions(retval.get(), retval.get(), line, content.end());

            return retval.release();
        }
    };
    const ParseRecipe_ TheParser;

    std::string NoUnderscores(const Info_& src) {
        std::string retval(src.content_);
        for (auto pc = retval.begin(); pc != retval.end(); ++pc)
            if (*pc == '_')
                *pc = ' ';
        return retval;
    }

    std::string TexNumber(const Info_& src) {
        auto slash = src.content_.find('/');
        if (slash == std::string::npos)
            return src.content_;
        // a fraction
        auto num = src.content_.substr(0, slash), den = src.content_.substr(slash + 1);
        auto dash = num.find('-');
        std::string whole;
        if (dash == std::string::npos) {
            whole = num.substr(0, num.length() - 1);
            num = num.substr(num.length() - 1); // just the last digit
        } else {
            whole = num.substr(0, dash);
            num = num.substr(dash + 1);
        }
        return '$' + whole + "\\frac{" + num + "}{" + den + "}$";
    }

    std::string TexSafeLC1(const Info_& src) {
        std::string retval = TexSafe(src.content_);
        retval[0] = tolower(retval[0]);
        return retval;
    }
    std::string TexSafeNoun(const Info_& src) {
        std::string retval = TexSafe(src.content_);
        while (!retval.empty() && retval[0] == '~') {
            int space = static_cast<int>(retval.find(' '));
            if (space == std::string::npos)
                retval.clear();
            else
                retval = retval.substr(space + 1);
        }
        return retval;
    }

    //--------------------------------------------------------------------------

    struct MakeRecipeEmitter_ : Emitter::Source_ {
        MakeRecipeEmitter_() { Emitter::RegisterSource(RECIPE, *this); }
        Emitter::Funcs_ Parse(const std::vector<std::string>& lib, const std::string& path) const {
            cout << "Parsing Recipe.mgt\n";
            // start with the library
            std::vector<std::string> tLines(lib);
            // add the template
            std::filesystem::path pl(path);
            File::Read((pl / "Recipe.mgt").string(), &tLines);
            auto retval = Template::Parse(tLines);
            // now add C++ functions
            retval.ofInfo_["TexSafeLC1"].reset(EmitUnassisted(TexSafeLC1));
            retval.ofInfo_["TexSafeNoun"].reset(EmitUnassisted(TexSafeNoun));
            retval.ofInfo_["NoUnderscores"].reset(EmitUnassisted(NoUnderscores));
            retval.ofInfo_["TexNumber"].reset(EmitUnassisted(TexNumber));

            return retval;
        }
    };

    const MakeRecipeEmitter_ TheEmitter_;
} // namespace

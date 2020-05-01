
// parse Recipe mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include <assert.h>
#include <iostream>
#include <algorithm>
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"
using namespace ParseUtils;
using namespace std;

namespace
{
	static const string INGREDIENT("ingredient");
	static const string RECIPE("recipe");
	static const string MEASURE("measure");
	static const string NUMBER("number");
	static const string PROCESSED("processed");
	static const string ACTION("action");
	static const string CO("co");
	static const string PART("part");

	bool IsIngredientStart(const string& line)	// see if it might be an ingredient
	{
		static const string FIRST("123456789#");
		return !line.empty()
			&& FIRST.find(line[0]) != string::npos;
	}

	bool IsCO(const string& line)
	{
		return line.find("c/o") == 0
			|| line.find("&co") == 0;
	}

	bool IsNewPart(const string& line)
	{
		return line.substr(0, 6) == "+part "
			|| line.substr(0, 6) == "&part ";
	}
	bool IsStopPart(const string& line)
	{
		return line.substr(0, 5) == "-part";
	}
	string PartName(const string& line)
	{
		return line.substr(6);
	}

	bool StartsWith(const string& line, const string& token)
	{
		return line.find(token) == 0;
	}
	string StandardMeasure(const string& src)
	{
		string retval(src);
		transform(retval.begin(), retval.end(), retval.begin(), tolower);
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

	Handle_<Info_> ParseIngredient(const string& line, const Info_* parent, const Info_* root)
	{
		assert(!line.empty());
		auto nStop = line.find_first_not_of("0123456789-/");
		string number = nStop == string::npos ? string() : line.substr(0, nStop);
		string rest = nStop == string::npos ? line : line.substr(nStop);
		if (!number.empty() && !rest.empty() && rest[0] == '~')	// glue modifier to the number, e.g. 3~heaping
		{
			auto nSpace = rest.find(' ');
			REQUIRE(nSpace != string::npos, "Extended number fills whole ingredient line");
			number += line.substr(0, nSpace);
			rest = rest.substr(nSpace);
		}
		string measure;
		auto nHash = rest.find('#');
		if (nHash == string::npos)
		{
			// next thing is space-separated measure
			rest = AfterInitialWhitespace(rest);
			auto nSpace = rest.find(' ');
			REQUIRE(nSpace != string::npos, "Can't find measure for ingredient (" + line + ")");
			measure = StandardMeasure(rest.substr(0, nSpace));
			rest = TrimWhitespace(rest.substr(nSpace));
		}
		else
		{
			// no measure, just step past the hash
			rest = AfterInitialWhitespace(rest.substr(nHash + 1));
		}
		// next check for ", processed"
		auto comma = rest.find_first_of("(,");
		string processed;
		if (comma != string::npos)
		{
			processed = AfterInitialWhitespace(rest.substr(comma + 1));
			if (rest[comma] == '(')
			{
				// have to find the ')'
				auto ket = processed.find(')');
				REQUIRE(ket != string::npos, "'(' without matching ')'");
				processed[ket] = ' ';
			}
			rest = rest.substr(0, comma);
		}
		auto_ptr<Info_> retval(new Info_(parent, root, TrimWhitespace(rest)));
		if (!measure.empty())
			retval->children_.insert(make_pair(MEASURE, Info::MakeLeaf(retval.get(), root, measure)));
		if (!number.empty())
			retval->children_.insert(make_pair(NUMBER, Info::MakeLeaf(retval.get(), root, number)));
		if (!processed.empty())
			retval->children_.insert(make_pair(PROCESSED, Info::MakeLeaf(retval.get(), root, processed)));

		return retval.release();
	}

	Handle_<Info_> ParseCO(const string& line, const Info_* parent, const Info_* root)
	{
		return Info::MakeLeaf(parent, root, line.substr(4));
	}

	vector<string>::const_iterator AddIngredientsAndActions
		(Info_* info,
		Info_* root,
		vector<string>::const_iterator line,
		vector<string>::const_iterator end)
	{
		// starts with ingredients
		while (line != end && IsIngredientStart(*line))
			info->children_.insert(make_pair(INGREDIENT, ParseIngredient(*line++, info, root)));
		// just finished ingredients
		REQUIRE(line != end, "Expected &do then actions after ingredients");
		REQUIRE(line->substr(0, 3) == "&do", "Expected &do then actions, not " + *line);
		while (++line != end && !IsNewPart(*line) && !IsStopPart(*line))
			info->children_.insert(make_pair(ACTION, Info::MakeLeaf(info, root, *line)));
		if (line != end && IsStopPart(*line))
			++line;	// skip this line
		return line;
	}

	struct ParseRecipe_ : Info::Parser_
	{
		ParseRecipe_()
		{
			Info::RegisterParser(RECIPE, *this);
		}

		Info_* operator()
			(const string& info_name,
			 const vector<string>& content)
		const
		{
			auto_ptr<Info_> retval(new Info_(0, 0, info_name));
			retval->root_ = retval.get();
			auto line = content.begin();
			// might have a c/o at the front
			if (line != content.end() && IsCO(*line))
				retval->children_.insert(make_pair(CO, ParseCO(*line++, retval.get(), retval.get())));
			// have to check for parts
			while (line != content.end() && IsNewPart(*line))
			{
				auto_ptr<Info_> part(new Info_(retval.get(), retval.get(), PartName(*line)));
				line = AddIngredientsAndActions(part.get(), retval.get(), ++line, content.end());
				retval->children_.insert(make_pair(PART, part.release()));
			}
			cout << "Finished children at line " << (line - content.begin()) << "\n";
			AddIngredientsAndActions(retval.get(), retval.get(), line, content.end());

			return retval.release();
		}
	};
	static const ParseRecipe_ TheParser;


	string NoUnderscores(const Info_& src)
	{
		string retval(src.content_);
		for (auto pc = retval.begin(); pc != retval.end(); ++pc)
			if (*pc == '_')
				*pc = ' ';
		return retval;
	}

	string TexNumber(const Info_& src)
	{
		auto slash = src.content_.find('/');
		if (slash == string::npos)
			return src.content_;	
		// a fraction
		auto num = src.content_.substr(0, slash), den = src.content_.substr(slash + 1);
		auto dash = num.find('-');
		string whole;
		if (dash == string::npos)
		{
			whole = num.substr(0, num.length() - 1);
			num = num.substr(num.length() - 1);	// just the last digit
		}
		else
		{
			whole = num.substr(0, dash);
			num = num.substr(dash + 1);
		}
		return '$' + whole + "\\frac{" + num + "}{" + den + "}$";
	}

	string TexSafeLC1(const Info_& src)
	{
		string retval = TexSafe(src.content_);
		retval[0] = tolower(retval[0]);
		return retval;
	}
	string TexSafeNoun(const Info_& src)
	{
		string retval = TexSafe(src.content_);
		while (!retval.empty() && retval[0] == '~')
		{
			int space = retval.find(' ');
			if (space == string::npos)
				retval.clear();
			else
				retval = retval.substr(space + 1);
		}
		return retval;
	}

//--------------------------------------------------------------------------

	struct MakeRecipeEmitter_ : Emitter::Source_
	{
		MakeRecipeEmitter_()
		{
			Emitter::RegisterSource(RECIPE, *this);
		}
		Emitter::Funcs_ Parse(const vector<string>& lib, const string& path) const
		{
			cout << "Parsing Recipe.mgt\n";
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Recipe.mgt", &tLines);
			auto retval = Template::Parse(tLines);
			// now add C++ functions
			retval.ofInfo_["TexSafeLC1"].reset(EmitUnassisted(TexSafeLC1));
			retval.ofInfo_["TexSafeNoun"].reset(EmitUnassisted(TexSafeNoun));
			retval.ofInfo_["NoUnderscores"].reset(EmitUnassisted(NoUnderscores));
			retval.ofInfo_["TexNumber"].reset(EmitUnassisted(TexNumber));

			return retval;
		}
	};
	static MakeRecipeEmitter_ TheEmitter_;
}	// leave local


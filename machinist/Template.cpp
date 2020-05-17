 
#include "Template.h"
#include <assert.h>
#include "Emitter.h"
#include "Info.h"
#include "ParseUtils.h"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <regex>

using ParseUtils::IsWhite;
using ParseUtils::IsAllWhite;

namespace
{
	static const char NEWLINE('\n');
	static const char BACKQ('`');
	static const char PCT('%');
	static const char COLON(':');

	bool IsComment(string::const_iterator letter, string::const_iterator line)
	{
		if (*letter == BACKQ)
		{
			// it's a literal BACKQ iff there's an odd number of PCT before it
			int np = 0;
			while (letter != line && *--letter == PCT)
				++np;
			return !(np & 1);
		}
		return false;
	}

	struct XInput_	// very primitive lexer, handles comments
	{
		const vector<string>& vals_;
		vector<string>::const_iterator outer_;
		string::const_iterator inner_;
		XInput_(const vector<string>& src) : vals_(src), outer_(src.begin()), inner_(src.begin()->begin()) {}

		void PutBack(int how_many)	// we read too many; back up
		{
			REQUIRE(inner_ - outer_->begin() >= how_many, "Can't back up through a line feed");
			inner_ -= how_many;
		}
		template<class P_> void ShowUntil(P_& record)
		{
			for ( ; ; )
			{
				if (inner_ == outer_->end() || IsComment(inner_, outer_->begin()))
				{
					if (inner_ == outer_->end())
						record(NEWLINE);	// comment does not trigger a newline
					if (++outer_ == vals_.end())
						break;
					inner_ = outer_->begin();
				}
				else
					record(*inner_++);

				if (record.Done())
					return;
			}
			// break means EOF
			record(EOF);
			REQUIRE(record.Done(), "EOF without completion");
		}
		bool IsEOF() const {return outer_ == vals_.end();}
	};

	struct MatchKet_	// gets from ShowUntil the characters up to closing brace -- discards the closing brace
	{
		char bra_, ket_;
		int depth_;	// number of unmatched bras
		string seen_;
		MatchKet_(char bra, char ket) : bra_(bra), ket_(ket), depth_(1) {}

		void operator()(char c)
		{
			assert(!Done());
			if (c == bra_)
				++depth_;
			else if (c == ket_)
				--depth_;
			if (depth_ > 0)
				seen_.push_back(c);
		}
		bool Done() const {return depth_ == 0;}
	};
	string::const_iterator MatchKet(string::const_iterator bra, char ket, string::const_iterator end)
	{
		MatchKet_ count(*bra, ket);
		for (auto pc = Next(bra); ; ++pc)
		{
			REQUIRE(pc != end, "Reached end before finding closing '" + string(1, ket) + "'");
			count(*pc);
			if (count.Done())
				return pc;
		}
	}

	struct MatchUntil_	// gets from ShowUntil up to a specific character string -- discards the string
	{
		string match_;
		bool eofOK_;
		int sofar_;
		string seen_;
		MatchUntil_(const string& match, bool eof_ok = true) : match_(match), eofOK_(eof_ok), sofar_(0) 
		{
			// we don't do the full O(N) test for semi-overlapping matches
			// so we won't find, e.g., "opinion" in "opiniopinion" because the second 'o' will back up to zero
			assert(!match.empty());
			assert(match.substr(1).find(match[0]) == string::npos); // Match algorithm not reliable when first character is repeated
		}

		void operator()(char c)
		{
			assert(!Done());
			if (sofar_ > 0 && c != match_[sofar_])
			{
				seen_ += match_.substr(0, sofar_);
				sofar_ = 0;
			}
			// reset is done if needed; proceed
			if (c == match_[sofar_])
				++sofar_;	// don't save anything yet
			else if (c == EOF && eofOK_)
			{
				seen_ += match_.substr(0, sofar_);
				sofar_ = match_.length();	// suddenly we are done
			}
			else
				seen_.push_back(c);
		}

		bool Done() const {return sofar_ == match_.length();}
	};

	struct FuncName_	// demands that ShowUntil contain a function name, or all whitespace to EOF
	{
		string name_;
		enum {WHITE, PERCENT, NAME, DONE} state_;
		FuncName_() : state_(WHITE) {}
		void operator()(char c) 
		{
			assert(!Done());
			switch (state_)
			{
			case WHITE:
				if (IsWhite(c))
					break;
				if (c == EOF)
				{
					state_ = DONE;
					break;
				}
				REQUIRE(c == PCT, "Expecting a function declaration but didn't find '%'");
				state_ = PERCENT;
				break;
			case PERCENT:
				REQUIRE(c == COLON, "Expecting a function declaration but '%' not followed by ':'");
				state_ = NAME;
				break;
			case NAME:
				if (c == COLON)
					state_ = DONE;
				else
					name_.push_back(c);
				break;
			}
		}
		bool Done() const {return state_ == DONE;}
	};

	struct FindNonwhite_
	{
		bool found_;
		bool eof_;
		FindNonwhite_() : found_(false), eof_(false) {}
		void operator()(char c)
		{
			if (c == EOF)
				eof_ = true;
			else if (!IsWhite(c))
				found_ = true;
		}
		bool Done() const {return found_ || eof_;}
	};

	string UnEscape(string::const_iterator start, string::const_iterator stop)
	{
		string retval;
		while (start < stop)
		{
			if (*start == '%')
				++start;
			if (start < stop)
				retval.push_back(*start++);
		}
		return retval;
	}

	//---------------------------------------------------------------------------

	struct Composite_ : Emitter_	// emit several things in series
	{
		vector<Handle_<Emitter_> > vals_;
		void Append(Emitter_* e)	// captures the input pointer
		{
			vals_.push_back(Handle_<Emitter_>(e));
		}
		vector<string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const override
		{
			vector<string> retval;
			for (auto pv = vals_.begin(); pv != vals_.end(); ++pv)
			{
				vector<string> more = (**pv)(arg, lib);
				retval.insert(retval.end(), more.begin(), more.end());
			}
			return retval;
		}
	};

	struct Literal_ : Emitter_	 // emit literal text
	{
		string literal_;
		Literal_(string l) : literal_(l) {}
		vector<string> operator()(const Info_&, const Emitter::Funcs_&) const override
		{
			return vector<string>(1, literal_);
		}
	};

	static const std::regex HAS_NONWHITE("\\S");
	static const std::regex IS_BLANK("^\\s*$");	// NOT USED -- I think something odd about the definition of what is a newline, causes this to match when it shouldn't
	struct Conditional_ : Emitter_	// if A, emit B
	{
		Handle_<Emitter_> cond_;
		Handle_<Emitter_> val_;
		std::regex match_;
		bool isNot_;
		bool isOr_;	

		Conditional_(Emitter_* c, Emitter_* v, bool p, bool io, const string& m) 
			: 
		cond_(c), 
		val_(v), 
		isNot_(p && m.empty()),
		isOr_(io),
		match_(p ? (m.empty() ? IS_BLANK : std::regex(m)) : HAS_NONWHITE) 
		{	}

		vector<string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const
		{			
			vector<string> save = (*cond_)(arg, lib);	// need to put this in an lvalue
			string test = accumulate(save.begin(), save.end(), string());	// search in a raw join, no whitespace
			bool matched = isNot_
					? IsAllWhite(test)
					: std::regex_search(test, match_);
			if (isOr_)
				return matched ? save : (*val_)(arg, lib);
			// conditional
			return matched ? (*val_)(arg, lib) : vector<string>();
		}
	};

	struct Iterate_ : Emitter_	// emit something for all children
	{
		string childName_;
		string separator_;
		Handle_<Emitter_> val_;
		bool sort_;
		Iterate_(const string& child, const string& separator, bool sort, Emitter_* val) : childName_(child), separator_(separator), sort_(sort), val_(val) {}

		template<class I_> void EmitRaw(const I_& start, const I_& stop, const Emitter::Funcs_& lib, vector<string>* dst) const
		{
			for (auto pc = start; pc != stop; ++pc)
			{
				if (pc != start && !separator_.empty())
					dst->push_back(separator_);
				auto contrib = (*val_)(*pc->second, lib);
				dst->insert(dst->end(), contrib.begin(), contrib.end());
			}
		}
      template<class R_> void EmitSorted(const R_& range, const Emitter::Funcs_& lib, vector<string>* dst) const
		{
			// copy the children into a map keyed on own content
			multimap<string, Handle_<Info_>> ordered;
			for (auto pc = range.first; pc != range.second; ++pc)
				ordered.insert(make_pair(pc->second->content_, pc->second));
			EmitRaw(ordered.begin(), ordered.end(), lib, dst);
		}

      vector<string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const override
		{
			vector<string> retval;
			// wow, this actually requires looking at the info
			auto children = arg.children_.equal_range(childName_);
			if (sort_)
				EmitSorted(children, lib, &retval);
			else
				EmitRaw(children.first, children.second, lib, &retval);
			return retval;
		}
	};

	struct Funcall_ : Emitter_	// call another emitter
	{
		string funcName_;
		Info::Path_ arg_;
		bool quiet_;
		Funcall_(const string& name, const string& path, bool quiet) : funcName_(name), arg_(path), quiet_(quiet) {}
		vector<string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const override
		{
			// have to navigate the info to the path
         return Emitter::Call(arg_(arg, quiet_), lib, funcName_);
		}
	};

	struct Transform_ : Emitter_	// call a string->string transformation
	{
		string funcName_;	// name of the transformation to do
		Handle_<Emitter_> arg_; // to evaluate and pass
		Transform_(const string& name, Emitter_* arg) : funcName_(name), arg_(arg) {}

      vector<string> operator()(const Info_& arg, const Emitter::Funcs_& lib) const override
		{
			vector<string> temp = (*arg_)(arg, lib);	// need to put this in an lvalue
			string src = accumulate(temp.begin(), temp.end(), string());	// function operates on combined string, doesn't see how it was accreted
			return Emitter::CallTransform(src, lib, funcName_);
		}
	};	

	bool IsOpener(char c) {return c == '(' || c == '<';}

	Emitter_* ParseFunc(const string& src, bool quiet = false)
	{
		assert(src.find(EOF) == src.npos);	// not prepared for EOF inside body
		auto_ptr<Composite_> retval(new Composite_);
		for (string::const_iterator here = src.begin(); here != src.end(); )
		{
			if (*here == PCT)
			{
				REQUIRE(src.end() - here > 1, "Template can't end with %");
				switch (*Next(here))
				{
				case PCT:	// literal %
					retval->Append(new Literal_("%"));
					here += 2;
					break;
				case '?':	// conditional insertion
				case '~':	// pattern-match insertion
				case '|':	// insertion with default
				{
					const bool patterned = *Next(here) == '~';
					const bool emitOne = *Next(here) == '|';
					REQUIRE(src.end() - here > 6, "'%?' too close to end of function");
					auto bra = here + 2;
					REQUIRE(*bra == '{', "'%" + string(1, *Next(here)) + "' must be followed by condition test enclosed in {}");
					auto ket = MatchKet(bra, '}', src.end());
					string cond(Next(bra), ket);	// text of condition function
					REQUIRE(src.end() - ket > 3, "End of condition too close to end of function");
					auto sep = bra = Next(ket);
					for ( ; ; )
					{
						bra = find(bra, src.end(), '{');
						REQUIRE(bra != src.end(), "Couldn't find body to emit when condition is satisfied");
						if (bra == sep || *(bra-1) != '%')
							break;
						// it was escaped, not a real '}', keep going
					}
					REQUIRE(patterned || bra == sep, "Can't supply a pattern to a simple %? query or %| emitter (pattern = " + string(here, bra) + ")");
					ket = MatchKet(bra, '}', src.end());
					string result(Next(bra), ket);	// text of emitted function
					retval->Append
							(new Conditional_
									(ParseFunc(cond, true), ParseFunc(result), patterned, emitOne, UnEscape(sep, bra)));
					here = Next(ket);
				}
					break;
				case '*':	// iteration over children
				case '^':	// iteration over children
					{
						REQUIRE(src.end() - here > 6, "'%*' too close to end of function");
						auto bra = here + 2;
						REQUIRE(*bra == '[', "'%*' must be followed by child name enclosed in []");
						auto ket = MatchKet(bra, ']', src.end());
						string child(Next(bra), ket);	// child name
						REQUIRE(src.end() - ket > 3, "End of child name too close to end of function");
						auto sep = Next(ket);
						bra = find(sep, src.end(), '{');
						REQUIRE(bra != src.end(), "Couldn't find body to emit for each child");
						ket = MatchKet(bra, '}', src.end());
						string task(Next(bra), ket);	// text of emitted function
						retval->Append
								(new Iterate_
										(child, string(sep, bra), *Next(here) == '^', ParseFunc(task)));
						here = Next(ket);
					}
					break;
				case ':':
					assert(!"Unreachable; function definition inside function");
				default:	// must be a function name
					{
						REQUIRE(src.end() - here > 2, "Function call too close to end of function");
						auto bra = find_if(Next(here), src.end(), IsOpener);
						string func(Next(here), bra);
						REQUIRE(src.end() - bra > 1, "End of function name ('" + string(here, Min(here + 30, bra)) + "') too close to end of function");
						if (*bra == '(')	// function call
						{
							auto ket = MatchKet(bra, ')', src.end());
							string arg(Next(bra), ket);
							if (func == "ENV")	// not a true function call -- resolve it now
								retval->Append(new Literal_(EnvironmentValue(arg)));
							else
								retval->Append(new Funcall_(func, arg, quiet || func == "_?"));	// magic function "_?" is always quiet
							here = Next(ket);
						}
						else
						{
							auto ket = MatchKet(bra, '>', src.end());
							string arg(Next(bra), ket);
							retval->Append(new Transform_(func, ParseFunc(arg)));
							here = Next(ket);
						}
					}
					break;
				}
			}
			else	// no '%' so it's just text up to the next '%'
			{
				auto pct = find(here, src.end(), PCT);
				retval->Append(new Literal_(string(here, pct)));
				here = pct;
			}
		}

		return retval.release();
	}

	struct EmitContent_ : Emitter_
	{
		vector<string> operator()
         (const Info_& arg, const Emitter::Funcs_&)
		const override
		{
			return vector<string>(1, arg.content_);
		}
	};

   string Stringify(const string& src)
	{
		string retval("\"");
		bool bs = false;
		for (auto ps = src.begin(); ps != src.end(); ++ps)
		{
			if (bs)	// just saw a backslash
			{
				if (*ps == '"' || *ps == '\\')	// it's already a \" or \\, we need to write \\"" or \\\\ 
				{
					retval.push_back('\\');
					retval.push_back('\\');
					retval.push_back(*ps);
					retval.push_back(*ps);
				}
				else
				{
					retval.push_back('\\');
					retval.push_back(*ps);
				}
				bs = false;
			}
			else if (*ps == '\\')
				bs = true;
			else if (*ps == '"')
				retval += "\\\"";	// add backslash before double-quote
			else
				retval.push_back(*ps);
		}
		if (bs)	// ended with backslash
			retval += "\\";
		retval += "\"";
		return retval;
	}

	// global counter shared all emitters
	static int THE_COUNT = 0;
	struct EmitCounter_ : Emitter_	
	{
		vector<string> operator()(const Info_&, const Emitter::Funcs_&) const override
		{
			return vector<string>(1, std::to_string(THE_COUNT++));
		}
	};
	struct ResetCounter_ : Emitter_
	{
      vector<string> operator()(const Info_&, const Emitter::Funcs_&) const override
		{
			THE_COUNT = 0;
			return vector<string>();
		}
	};

	// show a whole info (for debugging)
	string EmitRecursive(const Info_& src, const string& tabs)
	{
		string retval;
		// content
		retval += tabs + src.content_ + "\n";
		// children
		for (auto pc = src.children_.begin(); pc != src.children_.end(); ++pc)
		{
			retval += tabs + "\t" + pc->first + "\n";
			retval += EmitRecursive(*pc->second, tabs + "\t\t");
		}
		return retval;
	}
	string EmitRecursive0(const Info_& src) {return EmitRecursive(src, string());}

	static char** THE_ENV = 0;
}	// leave local

void Template::SetGlobalCount(int c)
{
	THE_COUNT = c;
}

Emitter::Funcs_ Template::Parse
	(const vector<string>& input_src)
{
	REQUIRE(!input_src.empty(), "Template is empty");
	Emitter::Funcs_ retval;
	XInput_ src(input_src);
	for ( ; !src.IsEOF(); )
	{
		// first extract the function name
		FuncName_ getName;
		src.ShowUntil(getName);
		const string funcName = getName.name_;
		if (funcName.empty())
			break;	// reached EOF 
		MatchUntil_ getBody("%:");	// tag for the next function
		src.ShowUntil(getBody);
		if (!src.IsEOF())
			src.PutBack(2);	// we looked ahead and grabbed the function tag; put it back
		retval.ofInfo_[funcName].reset(ParseFunc(getBody.seen_));
	}

	retval.ofInfo_["_"].reset(new EmitContent_());
	retval.ofInfo_["_?"].reset(new EmitContent_());		// but should be made quiet by construction, see above
	retval.ofInfo_["#"].reset(new EmitCounter_());
	retval.ofInfo_["#0"].reset(new ResetCounter_());
	retval.ofInfo_["<<"].reset(ParseUtils::EmitUnassisted(EmitRecursive0));
	// functions defined in ParseUtils, available to all templates
   retval.ofString_["\""].reset(ParseUtils::EmitTransform(Stringify));
   retval.ofString_["HtmlSafe"].reset(ParseUtils::EmitTransform(ParseUtils::HtmlSafe));
   retval.ofString_["TexSafe"].reset(ParseUtils::EmitTransform(ParseUtils::TexSafe));
   retval.ofString_["Condensed"].reset(ParseUtils::EmitTransform(ParseUtils::Condensed));
   retval.ofInfo_["WithParentName"].reset(ParseUtils::EmitUnassisted(ParseUtils::WithParentName));
	retval.ofInfo_["WithGrandparentName"].reset(ParseUtils::EmitUnassisted(ParseUtils::WithGrandparentName));
	return retval;
}


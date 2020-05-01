 
#include "ParseUtils.h"
#include "Info.h"
#include "Emitter.h"
#include <assert.h>

bool ParseUtils::IsWhite(char c)
{
	return c == ' ' || c == '\t' || c == '\n';
}
bool ParseUtils::StartsWithWhitespace(const string& line)
{
	return line.empty() || IsWhite(line[0]);
}
bool ParseUtils::IsAllWhite(const string& line)
{
	for (auto pc = line.begin(); pc != line.end(); ++pc)
		if (!IsWhite(*pc))
			return false;
	return true;
}

string ParseUtils::AfterInitialWhitespace(const string& line)
{
	if (line.empty())
		return string();
	auto offset = line.find_first_not_of(" \t");
	return offset == string::npos
			? string()
			: line.substr(offset);
}

string ParseUtils::TrimWhitespace(const string& src)
{
	string retval = AfterInitialWhitespace(src);
	while (!retval.empty() && IsWhite(retval.back()))
		retval.pop_back();
	return retval;
}

vector<string>::const_iterator ParseUtils::ReadHelp
	(const Info_* parent,
	 const Info_* root,
	 vector<string>::const_iterator line,
	 vector<string>::const_iterator end,
	 auto_ptr<Info_>* dst,
	 vector<Handle_<Info_>>* conditions)
{
	while (line != end && StartsWithWhitespace(*line))
	{
		string text = AfterInitialWhitespace(*line);
		REQUIRE(!text.empty(), "Line contains only whitespace");
		if (text[0] == '&')
		{	// local condition
			REQUIRE(conditions, "Unexpected '&' in context which does not allow local conditions");
			text = text.substr(1);
			auto bs = text.find('\\');
			auto_ptr<Info_> temp(new Info_(parent, root, text.substr(0, bs)));
			if (bs != string::npos)
				temp->children_.insert(std::make_pair("help", Info::MakeLeaf(temp.get(), root, text.substr(bs + 1))));
			conditions->push_back(temp.release());
		}
		else
		{	// actual help
			if (!dst->get())
				dst->reset(new Info_(parent, root, string()));
			else
				(*dst)->content_ += ' ';
			(*dst)->content_ += text;
		}
		++line;
	}
	return line;
}

vector<string>::const_iterator ParseUtils::ReadInsert
	(const Info_* parent,
	 const Info_* root,
	 vector<string>::const_iterator line,
	 vector<string>::const_iterator end,
	 auto_ptr<Info_>* dst)
{
	while (line != end && !line->empty() && line->front() == '+')
	{
		if (!dst->get())
			dst->reset(new Info_(parent, root, string()));
		else
			(*dst)->content_ += ' ';
		(*dst)->content_ += line->substr(1);
		++line;
	}
	return line;
}

bool ParseUtils::AddNonempty
	(Info_* info,
	 const string& tag,
	 const string& val)
{
	if (val.empty())
		return false;
	info->children_.insert(make_pair(tag, Info::MakeLeaf(info, info->root_, val)));
	return true;
}

namespace
{
	static const string HELP("help");

	Info_* NewCondition(const Info_* parent, const Info_* root, const string& code, auto_ptr<Info_>* help)
	{
		auto_ptr<Info_> retval(new Info_(parent, root, code));
		if (help->get())
		{
			(*help)->parent_ = retval.get();
			retval->children_.insert(make_pair(HELP, Handle_<Info_>(help->release())));
		}
		return retval.release();
	}
}	// leave local

vector<string>::const_iterator ParseUtils::ReadCondition
	(const Info_* parent,
	 vector<string>::const_iterator line,
	 vector<string>::const_iterator end,
	 Handle_<Info_>* dst)
{
	REQUIRE(!line->empty() && !StartsWithWhitespace(*line), "Expected un-indented line to declare condition");
	const string code = *line;
	auto_ptr<Info_> help;
	line = ReadHelp(0,parent, ++line, end, &help);	// correct the parent later
	dst->reset(NewCondition(parent, parent->root_, code, &help));
	return line;
}

vector<string>::const_iterator ParseUtils::ReadLink
	(const Info_* parent,
	 vector<string>::const_iterator line,
	 vector<string>::const_iterator end,
	 Handle_<Info_>* dst)
{
	REQUIRE(!line->empty(), "Expected non-empty line to declare link");
	string type, name = AfterInitialWhitespace(*line);
	auto space = name.find(' ');
	if (space != string::npos)
	{
		type = AfterInitialWhitespace(name.substr(space));
		name = name.substr(0, space);
	}
	auto_ptr<Info_> link(new Info_(parent, parent->root_, name));
	if (!type.empty())
		link->children_.insert(std::make_pair("type", Info::MakeLeaf(link.get(), parent->root_, type)));
	dst->reset(link.release());
	return ++line;
}

string ParseUtils::GetMandatory(const Info_& info, const string& child)
{
	auto r = info.children_.equal_range(child);
	REQUIRE(r.first != r.second, "Can't find '" + child + "' field");
	REQUIRE(r.first == --r.second, "'" + child + "' field is not unique");
	return r.first->second->content_;
}
string ParseUtils::GetOptional(const Info_& info, const string& child)
{
	auto r = info.children_.equal_range(child);
	if (r.first == r.second)
		return string();
	REQUIRE(r.first == --r.second, "'" + child + "' field is not unique");
	return r.first->second->content_;
}

string ParseUtils::EmbeddableForm(const string& src)
{
	string retval;
	auto ps = src.begin();
	retval.push_back(toupper(*ps));
	while (++ps != src.end())
	{
		if ((*ps >= 'a' && *ps <= 'z') || (*ps >= 'A' && *ps <= 'Z') || (*ps >= '0' && *ps <= '9') || *ps == '_')	// it's a letter or number, or underscore
			retval.push_back(*ps);
		else
			break;
	}
	while (!retval.empty() && retval.back() == '_')
		retval.pop_back();	// no trailing underscores
	return retval;
}

string ParseUtils::TexSafe(const string& src)	// wraps underscores in $
{
	string retval;
	bool mathMode = false;
	for (auto ps = src.begin(); ps != src.end(); ++ps)
	{
		if (*ps == '_' || *ps == '&')
		{
			if (!mathMode)
				retval.push_back('$');
			mathMode = true;
			retval.push_back('\\');
			retval.push_back(*ps);
		}
		else if (*ps == '\\')
		{
			if (!mathMode)
				retval.push_back('$');
			mathMode = true;
			retval += "\\backslash";
		}
		else if (*ps == '^')
		{
			if (!mathMode)
				retval.push_back('$');
			mathMode = true;
			retval.push_back(*ps);
			// need to eat the next character too
			if (++ps != src.end())
				retval.push_back(*ps);	
		}
		else if (*ps == '>' || *ps == '<')
		{
			if (!mathMode)
				retval.push_back('$');
			mathMode = true;
			retval.push_back(*ps);
		}
		else if (*ps == '%')
		{
			retval.push_back('\\');
			retval.push_back(*ps);
		}
		else
		{
			if (mathMode)
				retval.push_back('$');
			mathMode = false;
			retval.push_back(*ps);
		}
	}
	return retval;
}


string ParseUtils::HtmlSafe(const string& src)	// hides &gt; &lt:
{
	string retval;
	for (auto ps = src.begin(); ps != src.end(); ++ps)
	{
		if (*ps == '>')
			retval += "&gt;";
		else if (*ps == '<')
			retval += "&lt;";
		else if (*ps == '&')
			retval += "&amp;";
		else
			retval.push_back(*ps);
	}
	return retval;
}

string ParseUtils::Condensed(const string& src)
{
   static const string OTIOSE(" _\t");
   string retval;
   for (auto p = src.begin(); p != src.end(); ++p)
   {
      if (OTIOSE.find(*p) == string::npos)
         retval.push_back(toupper(*p));
   }
   return retval;
}


namespace
{
	string WithSubsName(const string& src, const Info_* subs)
	{
		string retval;
		for (auto ps = src.begin(); ps != src.end(); ++ps)
		{
			if (*ps == '$')
				retval += subs->content_;
			else
				retval.push_back(*ps);
		}
		return retval;
	}
}	// leave local

string ParseUtils::WithParentName(const Info_& src)
{
	return WithSubsName(src.content_, src.parent_);
}
string ParseUtils::WithGrandparentName(const Info_& src)
{
	return WithSubsName(src.content_, src.parent_->parent_);
}

namespace
{
	struct EmitUnassisted_ : Emitter_
	{
		ParseUtils::emit_from_info_t func_;
		EmitUnassisted_(ParseUtils::emit_from_info_t func) : func_(func) {}
		inline vector<string> operator()
			(const Info_& arg, const Emitter::Funcs_&)
		const override
		{
			return vector<string>(1, func_(arg));
		}
	};
	struct EmitTransform_ : StringTransform_
	{
		ParseUtils::emit_from_string_t func_;
		EmitTransform_(ParseUtils::emit_from_string_t func) : func_(func) {}
		inline vector<string> operator()
			(const string& src, const Emitter::Funcs_&)
		const override
		{
			return vector<string>(1, func_(src));
		}
	};
}

Emitter_* ParseUtils::EmitUnassisted(ParseUtils::emit_from_info_t func)
{
	return new EmitUnassisted_(func);
}

StringTransform_* ParseUtils::EmitTransform(ParseUtils::emit_from_string_t func)
{
	return new EmitTransform_(func);
}

string ParseUtils::UntilWhite(const string& src)
{
	string retval;
	for (auto pc = src.begin(); pc != src.end(); ++pc)
	{
		if (IsWhite(*pc))
			break;
		retval.push_back(*pc);
	}
	return retval;
}


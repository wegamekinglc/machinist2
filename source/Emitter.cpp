 
#include "Emitter.h"
#include "Info.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <assert.h>

Emitter_::~Emitter_()
{	}

StringTransform_::~StringTransform_()
{	}

vector<string> Emitter::Call
   (const Info_& arg, const Funcs_& lib, const string& which)
{
   auto pf = lib.ofInfo_.find(which);
   if (pf == lib.ofInfo_.end())
   {
      // can't find the function -- is there a transform-of-contents?
      auto pt = lib.ofString_.find(which);
      REQUIRE(pt != lib.ofString_.end(), "Can't find emitter function '" + which + "'");
      return (*pt->second)(arg.content_, lib);
   }
   return (*pf->second)(arg, lib);
}

vector<string> Emitter::CallTransform
	(const string& src, const Funcs_& lib, const string& which)
{
	auto pt = lib.ofString_.find(which);
	if (pt == lib.ofString_.end())
	{
		// has to be a Perl-style /before/after/ substitution
		REQUIRE(count(which.begin(), which.end(), '/') == 3 && which.front() == '/' && which.back() == '/', "Can't find emitter transformation '" + which + "'");
		auto mid = find(which.begin() + 1, which.end(), '/');
		std::regex before(string(which.begin() + 1, mid));
		string after(mid + 1, which.end() - 1);
		string retval = regex_replace(src, before, after);
		return vector<string>(1, retval);
	}
   return (*pt->second)(src, lib);
}

Emitter::Source_::~Source_()
{	}

namespace
{
	// static registry of emitter creators -- we do not own the pointers, they are assumed to point to static file-scope objects
	map<string, vector<const Emitter::Source_*> >& TheSources()
	{
		static map<string, vector<const Emitter::Source_*> > RETVAL;
		return RETVAL;
	}

	template<class K_, class V_> void MergeIn
		(map<K_, V_>* dst,
		 const map<K_, V_>& contrib)
	{
		for (auto pc = contrib.begin(); pc != contrib.end(); ++pc)
		{
			REQUIRE(!dst->count(pc->first), "Redefinition of emitter for " + pc->first);
			dst->insert(*pc);
		}
	}
}	// leave local

void Emitter::RegisterSource
	(const string& info_type,
	 const Source_& src)
{
	TheSources()[info_type].push_back(&src);
}

const Emitter::Funcs_& Emitter::GetAll
	(const string& info_type,
	 const string& path,
	 const vector<string>& lib)	// contents of library, not file paths
{
	std::cout << "Looking for " << info_type << " in " << path << "\n";
	// static registry of emitters themselves
	static map<string, Emitter::Funcs_> RETVALS;
	if (!RETVALS.count(info_type))
	{
		std::cout << "Parsing...";
		Emitter::Funcs_ retval;
		// parse all emitters for this name, combining results by merge
		for (auto s : TheSources()[info_type])	// auto-vivification is OK
		{
			auto temp = s->Parse(lib, path);
			std::cout << "Adding " << std::to_string(temp.ofInfo_.size()) << " functions\n";
			MergeIn(&retval.ofInfo_, temp.ofInfo_);
			MergeIn(&retval.ofString_, temp.ofString_);
		}
		RETVALS.insert(make_pair(info_type, retval));
	}
	std::cout << "Done\n";
	return RETVALS[info_type];
}


 
// parse directory as if it were mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"

using namespace ParseUtils;

namespace
{
	static const string DIR("dir");

	struct ParseDir_ : Info::Parser_
	{
		ParseDir_() 
		{
			Info::RegisterParser(DIR, *this);
		}

		Info_* operator()
			(const string& info_name,
			 const vector<string>& content)
		const
		{
			auto_ptr<Info_> retval(new Info_(0, 0, info_name));	// name is probably empty and never used...
			for (auto line = content.begin(); line != content.end(); ++line)
			{
				string type = File::InfoType(*line);
				auto_ptr<Info_> item(new Info_(retval.get(), retval.get(), File::InfoName(*line)));
				retval->children_.insert(make_pair(type, Handle_<Info_>(item.release())));
			}
			return retval.release();
		}
	};
	static ParseDir_ TheParser;

	// Now for the outputs

	struct MakeDirEmitter_ : Emitter::Source_
	{
		MakeDirEmitter_() 
		{
			Emitter::RegisterSource(DIR, *this);
		}
		Emitter::Funcs_ Parse
			(const vector<string>& lib,
			 const string& path) 
		const
		{
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Dir.mgt", &tLines);
			auto retval = Template::Parse(tLines);

			return retval;
		}
	};
	static MakeDirEmitter_ TheEmitter_;
}	// leave local


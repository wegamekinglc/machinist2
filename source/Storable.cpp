 
// parse storable mark-up, emit results

// this source file has no associated header
#include "Info.h"
#include "Template.h"
#include "Emitter.h"
#include "ParseUtils.h"
#include "File.h"

using namespace ParseUtils;

/* Mark-up format is exactly that of a settings!
*/

namespace
{
	static const string HELP("help");
	static const string CONDITION("condition");
	static const string MEMBER("member");
	static const string START_MEMBERS("&members");
	static const string START_CONDITIONS("&conditions");
	static const string IS(" is ");	// includes space separators, meaning tabs won't be recognizes
	static const string TYPE("type");
	static const string SUBTYPE("subtype");
	static const string DIMENSION("dimension");
	static const string REPEAT("repeat");
	static const string FROM("from");
	static const string MULTIPLE("multiple");
	static const string OPTIONAL("optional");
	static const string DEFAULT("default");
	static const string SETTINGS("settings");	// because we re-use the settings parser
	static const string STORABLE("storable");
	static const string RECORD("record");

	struct ParseStorable_ : Info::Parser_
	{
		ParseStorable_() 
		{
			Info::RegisterParser(STORABLE, *this);
		}

		Info_* operator()
			(const string& info_name,
			 const vector<string>& content)
		const
		{
			// settings parser should now be registered
			return Info::Parse(SETTINGS, info_name, content);
		}
	};
	static const ParseStorable_ TheParser;


//--------------------------------------------------------------------------

	struct MakeStorableEmitter_ : Emitter::Source_
	{
		MakeStorableEmitter_() 
		{
			Emitter::RegisterSource(STORABLE, *this);
		}
		Emitter::Funcs_ Parse
			(const vector<string>& lib,
			 const string& path) 
		const
		{
			// start with the library
			vector<string> tLines(lib);
			// add the template
			File::Read(path + "Storable.mgt", &tLines);	
			auto retval = Template::Parse(tLines);
			// now add C++ functions
			retval.ofInfo_["CType"].reset(EmitUnassisted(CType));
			retval.ofInfo_["SupplyDefault"].reset(EmitUnassisted(SupplyDefault));
			retval.ofInfo_["IsSettings"].reset(EmitUnassisted(IsSettings));
			retval.ofInfo_["IsEnum"].reset(EmitUnassisted(IsEnum));
			retval.ofInfo_["CoerceFromView"].reset(EmitUnassisted(CoerceFromView));

			return retval;
		}
	};
	static MakeStorableEmitter_ TheEmitter_;
}	// leave local


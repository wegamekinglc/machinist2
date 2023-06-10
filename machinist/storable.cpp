
// parse storable mark-up, emit results

// this machinist file has no associated header
#include "emitter.hpp"
#include "file.hpp"
#include "info.hpp"
#include "parseutils.hpp"
#include "template.hpp"

using namespace ParseUtils;

/* Mark-up format is exactly that of a settings!
 */

namespace {
    static const std::string HELP("help");
    static const std::string CONDITION("condition");
    static const std::string MEMBER("member");
    static const std::string START_MEMBERS("&members");
    static const std::string START_CONDITIONS("&conditions");
    static const std::string IS(" is "); // includes space separators, meaning tabs won't be recognizes
    static const std::string TYPE("type");
    static const std::string SUBTYPE("subtype");
    static const std::string DIMENSION("dimension");
    static const std::string REPEAT("repeat");
    static const std::string FROM("from");
    static const std::string MULTIPLE("multiple");
    static const std::string OPTIONAL("optional");
    static const std::string DEFAULT("default");
    static const std::string SETTINGS("settings"); // because we re-use the settings parser
    static const std::string STORABLE("storable");
    static const std::string RECORD("record");

    struct ParseStorable_ : Info::Parser_ {
        ParseStorable_() { Info::RegisterParser(STORABLE, *this); }

        Info_* operator()(const std::string& info_name, const std::vector<std::string>& content) const {
            // settings parser should now be registered
            return Info::Parse(SETTINGS, info_name, content);
        }
    };
    const ParseStorable_ TheParser;

    //--------------------------------------------------------------------------

    struct MakeStorableEmitter_ : Emitter::Source_ {
        MakeStorableEmitter_() { Emitter::RegisterSource(STORABLE, *this); }
        Emitter::Funcs_ Parse(const std::vector<std::string>& lib, const std::string& path) const {
            // start with the library
            std::vector<std::string> tLines(lib);
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
    const MakeStorableEmitter_ TheEmitter_;
} // namespace

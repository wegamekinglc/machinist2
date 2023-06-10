
// parse directory as if it were mark-up, emit results

// this machinist file has no associated header
#include "emitter.hpp"
#include "file.hpp"
#include "info.hpp"
#include "parseutils.hpp"
#include "template.hpp"
#include <filesystem>

using namespace ParseUtils;

namespace {
    static const std::string DIR("dir");

    struct ParseDir_ : Info::Parser_ {
        ParseDir_() { Info::RegisterParser(DIR, *this); }

        Info_* operator()(const std::string& info_name, const std::vector<std::string>& content) const {
            unique_ptr<Info_> retval(new Info_(0, 0, info_name)); // name is probably empty and never used...
            for (auto line = content.begin(); line != content.end(); ++line) {
                std::string type = File::InfoType(*line);
                unique_ptr<Info_> item(new Info_(retval.get(), retval.get(), File::InfoName(*line)));
                retval->children_.insert(make_pair(type, Handle_<Info_>(item.release())));
            }
            return retval.release();
        }
    };
    const ParseDir_ TheParser;

    // Now for the outputs

    struct MakeDirEmitter_ : Emitter::Source_ {
        MakeDirEmitter_() { Emitter::RegisterSource(DIR, *this); }
        Emitter::Funcs_ Parse(const std::vector<std::string>& lib, const std::string& path) const {
            // start with the library
            std::vector<std::string> tLines(lib);
            // add the template
            std::filesystem::path pl(path);
            File::Read((pl / "Dir.mgt").string(), &tLines);
            auto retval = Template::Parse(tLines);

            return retval;
        }
    };
    const MakeDirEmitter_ TheEmitter_;
} // namespace

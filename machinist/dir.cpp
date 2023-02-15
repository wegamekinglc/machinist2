// parse directory as if it were mark-up, emit results

// this source file has no associated header

#include "emitter.hpp"
#include "file.hpp"
#include "info.hpp"
#include "template.hpp"
#include "parseUtils.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>


using namespace ParseUtils;


namespace {

    static const std::string DIR("dir");

    struct ParseDir_ : Info::Parser_ {
        ParseDir_() {
            Info::RegisterParser(DIR, *this);
        }


        Info_ *
        operator()(const std::string &info_name, const std::vector<std::string> &content) const {
            std::unique_ptr<Info_> retval(
                    new Info_(0, 0, info_name));                  // name is probably empty and never used...
            for (auto line = content.begin(); line != content.end(); ++line) {
                std::string type = File::InfoType(*line);
                std::unique_ptr<Info_> item(new Info_(retval.get(), retval.get(), File::InfoName(*line)));
                retval->children_.insert(std::make_pair(type, Handle_<Info_>(item.release())));
            }
            return retval.release();
        }
    };

    static ParseDir_ TheParser;


    // Now for the outputs
    //
    struct MakeDirEmitter_ : Emitter::Source_ {
        MakeDirEmitter_() {
            Emitter::RegisterSource(DIR, *this);
        }

        Emitter::Funcs_
        Parse(const std::vector<std::string> &lib, const std::string &path) const {
            // start with the library
            std::vector<std::string> tLines(lib);

            // add the template
            File::Read(path + "Dir.mgt", &tLines);

            auto retval = Template::Parse(tLines);
            return retval;
        }
    };

    static MakeDirEmitter_ TheEmitter_;

}  // namespace <un-named>


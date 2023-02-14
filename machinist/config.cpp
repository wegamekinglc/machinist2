#include "config.hpp"

HERE

#include "info.hpp"
#include "file.hpp"
#include "template.hpp"

#include <cstddef>
#include <functional>
#include <iostream>
#include <numeric>
#include <regex>
#include <sstream>
#include <string>
#include <vector>



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//  the format for config files is minimal:
//  a line beginning with a tab names an emitter, which will be associated with the current markup type and file namer
//  a line beginning with "->" and containing '%' or '#' changes the current file namer:  the info name will be substituted for the '#' to create a file name
//  any other nonblank line changes the current markup type
//
//  e.g.
//  Enum
//  ->MG_#.cpp
//      EnumCppSource
//  ->MG_#.h
//      EnumCppClass
//      EnumCppMoreHeader
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //



namespace {

    FileSpec_t
    NamesFile(const std::string &line, bool targets_only = false) {
        bool result = line.size() > 3 && line[1] == '>' && line.find_first_of("#%") != line.npos;
        if (!result) {
            return FileSpec_t::Other;
        }

        switch (line[0]) {
            case '=' : {
                return FileSpec_t::Public;
            }

            case '-' : {
                return FileSpec_t::Private;
            }

            case '|' : {
                return FileSpec_t::Data;
            }


            default : {
                return FileSpec_t::Other;
            }
        }
    }


    bool
    NamesSources(const std::string &line) {
        return line.size() > 3 &&
               line[0] == '<' &&
               line[1] == '-';
    }


    std::vector<std::string>
    SourcePatterns(const std::string &line, std::vector<std::string> *reject) {
        std::string rest = line.substr(2);
        std::vector<std::string> retval;
        bool rejecting = false;
        while (!rest.empty()) {
            auto stop = rest.find_first_of(";!");
            (rejecting ? reject : &retval)->push_back(rest.substr(0, stop));
            if (stop == std::string::npos) {
                rest.clear();
            } else {
                if (rest[stop] == '!') {
                    rejecting = true;
                }
                rest = rest.substr(stop + 1);
            }
        }
        return retval;
    }


    std::vector<std::string>
    split(const std::string &input, char sep, std::vector<std::string> sofar = std::vector<std::string>()) {
        auto bar = input.find(sep);
        if (bar == std::string::npos) {
            sofar.push_back(input);
            return sofar;
        }
        sofar.push_back(input.substr(0, bar));
        return split(input.substr(bar + 1), sep, sofar);
    }


    void
    CommitAndReset(Config_ *dst,
                   const std::string &type,
                   Config_::Output_ *contrib) {
        if (!contrib->emitters_.empty()) {
            for (auto t: split(type, '|')) {
                dst->vals_[t].push_back(*contrib);
            }
            contrib->emitters_.clear();
        }
    }


    bool
    StartsWithBackquote(const std::string &line) {
        auto nonblank = line.find_first_not_of(" \t");

        return nonblank == std::string::npos                            // all blank
               || line[nonblank] == '`';
    }


    static const std::string FILENAME_FUNC("__OutputFilename");


    std::string
    AddEnvironment(const std::string &src,
                   std::size_t offset = 0,
                   const std::string &sofar = std::string()) {
        auto start = src.find("$(", offset);
        if (start == std::string::npos) {
            return sofar + src.substr(offset);
        }
        start += 2;
        auto stop = src.find(")", start);
        REQUIRE(stop != std::string::npos, "Nonterminated environment variable");
        return AddEnvironment(src, stop + 1, sofar + EnvironmentValue(src.substr(start, stop - start)));
    }


    void
    ToOS(std::string *s) {
        for (auto &c: *s) {
            if (c == '/' || c == '\\') {
                c = OS_SEP;
            }
        }
    }

}  // namespace <un-named>



Config_ Config::
Read(const std::string &filename, bool targets_only) {
    std::cout << "Reading configuration from " << filename << "\n";

    std::vector<std::string> src;
    File::Read(filename, &src);

    Config_ retval;
    retval.ownPath_ = File::PathOnly(filename);
    retval.targetsOnly_ = targets_only;
    std::string theType;
    std::vector<std::size_t> theSources;
    Config_::Output_ theFile;

    for (auto pl = src.begin(); pl != src.end(); ++pl) {
        const std::string &line = *pl;
        if (line.empty() || StartsWithBackquote(line)) {
            continue;
        } else if (line[0] == '@') {
            retval.templatePath_ = AddEnvironment(line.substr(1));
            ToOS(&retval.templatePath_);
        } else if (line[0] == '\t') {
            if (!theType.empty()) {
                // add emitters for this type
                theFile.emitters_.push_back(line.substr(1));

                REQUIRE(!theFile.emitters_.back().empty(), "Emitter name cannot be empty");
            } else if (!theSources.empty()) {
                const std::string token = line.substr(1);

                // add start/stop tokens for these sources
                for (auto ps = theSources.begin(); ps != theSources.end(); ++ps) {
                    auto &dst = retval.sources_[*ps];
                    if (dst.startToken_.empty()) {
                        dst.startToken_ = token;
                    } else {
                        REQUIRE(dst.stopToken_.empty(), "Too many tokens; can only specify start/stop");

                        dst.stopToken_ = token;
                    }
                }
            } else {
                REQUIRE(0, "Mark-up type must be supplied before emitters can be assigned to it");
            }
        } else if (NamesFile(line) != FileSpec_t::Other) {
            REQUIRE(!theType.empty(), "Mark-up type must be supplied before file namers can be assigned to it");

            // store the previous file output
            CommitAndReset(&retval, theType, &theFile);
            theFile.filespec_ = NamesFile(line, true);
            auto func = line;
            auto hash = line.find('#');
            if (hash != std::string::npos) {
                func = line.substr(0, hash) + "%_()" + line.substr(hash + 1);
            }

            // wrap it into a function definition to parse, then fish out the result
            auto bang = func.find('>');
            func = "%:" + FILENAME_FUNC + ":" + func.substr(bang + 1) + '`';
            auto temp = Template::Parse(std::vector<std::string>(1, func));
            theFile.fnFunc_ = temp.ofInfo_[FILENAME_FUNC];
            theSources.clear();
        } else if (NamesSources(line)) {
            theSources.clear();
            std::vector<std::string> reject;
            auto patterns = SourcePatterns(line, &reject);
            for (auto pp = patterns.begin(); pp != patterns.end(); ++pp) {
                theSources.push_back(retval.sources_.size());
                retval.sources_.push_back(*pp);
                retval.sources_.back().rejectPatterns_ = reject;
            }
            // theSources keeps locations to which start/stop tokens can be written
        } else {
            // store the previous file output
            CommitAndReset(&retval, theType, &theFile);
            theType = line;
            theSources.clear();
        }
    }

    // store the tail
    CommitAndReset(&retval, theType, &theFile);

    return retval;
}


//void
//Config ::
//Print(const Config_& cf, bool to_cerr)
//{
//    auto& dst = to_cerr ? std::cerr : std::cout;
//    for (auto s : cf.sources_)
//    {
//        dst << "Source is:\n";
//        dst << "\tmatch '" << s.filePattern_ << "'\n";
//        dst << "\treject ";
//        for (auto r : s.rejectPatterns_)
//        {
//            dst << "'" << r << "'";
//        }
//        dst << "\n\tin (" << s.startToken_ << ", " << s.stopToken_ << ")\n";
//    }
//    for (auto v : cf.vals_)
//    {
//        dst << v.first << " -> \n";
//        for (auto o : v.second)
//        {
//            dst << "\t " << (o.isPublic_ ? '=' : '-') << ">" << " (namer here) " << " <- \n";
//            for (auto e :o.emitters_)
//            {
//                dst << "\t\t <- " << e << "\n";
//            }
//        }
//    }
//    dst << "Own path = " << cf.ownPath_ << "\n";
//    dst << "Template path = " << cf.templatePath_ << std::endl;
//}


std::string
Config_::Output_::
FileName(const Info_ &info, const Emitter::Funcs_ &funcs) const {
    auto vs = (*fnFunc_)(info, funcs);
    std::string retval = std::accumulate(vs.begin(), vs.end(), std::string());

    REQUIRE(retval.find_first_not_of(" \t\n") != std::string::npos,
            "Filename must have non-whitespace (for '" + info.content_ + "')");

    return retval;
}

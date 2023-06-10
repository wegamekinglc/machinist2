
#include "config.hpp"
#include "file.hpp"
#include "template.hpp"
#include <functional>
#include <iostream>
#include <numeric>

/* the format for config files is minimal:
a line beginning with a tab names an emitter, which will be associated with the current markup type and file namer
a line beginning with "->" and containing '%' or '#' changes the current file namer:  the info name will be substituted
for the '#' to create a file name any other nonblank line changes the current markup type

e.g.
Enum
->MG_#.cpp
        EnumCppSource
->MG_#.h
        EnumCppClass
        EnumCppMoreHeader
*/

namespace {
    bool NamesFile(const std::string& line) {
        return line.size() > 3 && line[0] == '-' && line[1] == '>' && line.find_first_of("#%") != std::string::npos;
    }

    bool NamesSources(const std::string& line) { return line.size() > 3 && line[0] == '<' && line[1] == '-'; }

    std::vector<std::string> SourcePatterns(const std::string& line, std::vector<std::string>* reject) {
        std::string rest = line.substr(2);
        std::vector<std::string> ret_val;
        bool rejecting = false;
        while (!rest.empty()) {
            auto stop = rest.find_first_of(";!");
            (rejecting ? reject : &ret_val)->push_back(rest.substr(0, stop));
            if (stop == std::string::npos)
                rest.clear();
            else {
                if (rest[stop] == '!')
                    rejecting = true;
                rest = rest.substr(stop + 1);
            }
        }
        return ret_val;
    }

    std::vector<std::string> split(const std::string &input, std::vector<std::string> so_far = std::vector<std::string>()) {
        auto bar = input.find(124);
        if (bar == std::string::npos) {
            so_far.push_back(input);
            return so_far;
        }
        so_far.push_back(input.substr(0, bar));
        return split(input.substr(bar + 1), so_far);
    }

    void CommitAndReset(Config_* dst, const std::string& type, Config_::Output_* contrib) {
        if (!contrib->emitters_.empty()) {
            for (const auto& t : split(type))
                dst->vals_[t].push_back(*contrib);
            contrib->emitters_.clear();
        }
    }

    bool StartsWithBackQuote(const std::string& line) {
        auto non_blank = line.find_first_not_of(" \t");
        return non_blank == std::string::npos // all blank
               || line[non_blank] == '`';
    }

    const std::string FILENAME_FUNC("__OutputFile");

    std::string AddEnvironment(const std::string& src, int offset = 0, const std::string& so_far = std::string()) {
        auto start = src.find("$(", offset);
        if (start == std::string::npos)
            return so_far + src.substr(offset);
        start += 2;
        auto stop = src.find(')', start);
        REQUIRE(stop != std::string::npos, "Non-terminated environment variable");
        return AddEnvironment(src, static_cast<int>(stop + 1), so_far + EnvironmentValue(src.substr(start, stop - start)));
    }
} // namespace

Config_ Config::Read(const std::string& filename) {
    std::vector<std::string> src;
    std::cout << "Reading configuration from " << filename << "\n";
    File::Read(filename, &src);
    Config_ ret_val;
    ret_val.ownPath_ = File::PathOnly(filename);
    std::string theType;
    std::vector<int> theSources;
    Config_::Output_ theFile;

    for (auto & line : src) {
        if (line.empty() || StartsWithBackQuote(line))
            continue;
        else if (line[0] == '@') {
            ret_val.templatePath_ = AddEnvironment(line.substr(1));
        } else if (line[0] == '\t') {
            if (!theType.empty()) {
                REQUIRE(!theFile.dst_.funcs_.empty(),
                        "Nonempty file namer must be supplied before emitters can be assigned to it");
                // add emitters for this type
                theFile.emitters_.push_back(line.substr(1));
                REQUIRE(!theFile.emitters_.back().empty(), "Emitter name cannot be empty");
            } else if (!theSources.empty()) {
                const std::string token = line.substr(1);
                // add start/stop tokens for these sources
                for (auto source : theSources) {
                    auto& dst = ret_val.sources_[source];
                    if (dst.startToken_.empty())
                        dst.startToken_ = token;
                    else {
                        REQUIRE(dst.stopToken_.empty(), "Too many tokens; can only specify start/stop");
                        dst.stopToken_ = token;
                    }
                }
            } else {
                REQUIRE(0, "Mark-up type must be supplied before emitters can be assigned to it");
            }
        } else if (NamesFile(line)) {
            REQUIRE(!theType.empty(), "Mark-up type must be supplied before file namers can be assigned to it");
            // store the previous file output
            CommitAndReset(&ret_val, theType, &theFile);
            auto func = line;
            auto hash = line.find('#');
            if (hash != std::string::npos)
                func = line.substr(0, hash) + "%_()" + line.substr(hash + 1);
            // turn it into a template declaration and parse it
            auto bang = func.find('>');
            func = "%:" + FILENAME_FUNC + ":" + func.substr(bang + 1) + '`';
            theFile.dst_.funcs_ = Template::Parse(std::vector<std::string>(1, func)); // has access to built-ins
            theSources.clear();
        } else if (NamesSources(line)) {
            theSources.clear();
            std::vector<std::string> reject;
            auto patterns = SourcePatterns(line, &reject);
            for (auto & pattern : patterns) {
                theSources.push_back(static_cast<int>(ret_val.sources_.size()));
                ret_val.sources_.emplace_back(pattern);
                ret_val.sources_.back().rejectPatterns_ = reject;
            }
            // theSources keeps locations to which start/stop tokens can be written
        } else {
            // store the previous file output
            CommitAndReset(&ret_val, theType, &theFile);
            theFile.dst_.funcs_.clear();
            theType = line;
            theSources.clear();
        }
    }
    // store the tail
    CommitAndReset(&ret_val, theType, &theFile);
    return ret_val;
}

std::string Config_::Namer_::operator()(const Info_& info) const {
    auto vs = Emitter::Call(info, funcs_, FILENAME_FUNC);
    return std::accumulate(vs.begin(), vs.end(), std::string());
}

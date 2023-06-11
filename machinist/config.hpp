#pragma once

#include "handle.hpp"
#include "emitter.hpp"
#include <map>

using std::map;

/* The purpose of a config is to state, for each type of mark-up input:
        -- what files to write
        -- what emitter(s) to use for each

  Filenames are defined by a prefix and postfix which will be wrapped around the info name.
  Here is an example of the process:
        -- DayBasis.Enum.if is discovered
        -- the config for "Enum" is read
        -- it contains file descriptors of the form {"MG_", ".cpp"} which will cause MG_DayBasis.cpp to be created
        -- the emitters associated with that file descriptor are invoked and their output collated into that file

  Note that the reader for Enum blocks is not configurable; it is coded into this app.
*/

struct Config_ {
    struct Source_ {
        std::string filePattern_;
        std::vector<std::string> rejectPatterns_;
        std::string startToken_; // if empty, start of file
        std::string stopToken_;  // if empty, end of file
        Source_(const std::string& pattern) : filePattern_(pattern) {}
    };

    struct Namer_ // information to generate a filename
    {
        Emitter::Funcs_ funcs_;
        std::string operator()(const Info_& info) const;
    };

    struct Output_ {
        Namer_ dst_;
        std::vector<std::string> emitters_; // referenced by name
    };

    std::vector<Source_> sources_;
    map<std::string, std::vector<Output_>> vals_; // std::string key is the type of input ("Enum" above)
    std::string ownPath_;
    std::string templatePath_; // relative to config file, not to working directory!
};

namespace Config {
    Config_ Read(const std::string& filename);
}

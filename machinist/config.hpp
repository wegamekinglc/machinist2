#pragma once

#include "handle.hpp"
#include "emitter.hpp"

#include <map>
#include <string>
#include <vector>



// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //
//  The purpose of a config is to state, for each type of mark-up input:
//      -- what files to write
//      -- what emitter(s) to use for each
//
//  Filenames are defined by a prefix and postfix which will be wrapped around the info name.
//
//  Here is an example of the process:
//      -- DayBasis.Enum.if is discovered
//      -- the config for "Enum" is read
//      -- it contains file descriptors of the form {"MG_", ".cpp"} which will cause MG_DayBasis.cpp to be created
//      -- the emitters associated with that file descriptor are invoked and their output collated into that file
//
//  Note that the reader for Enum blocks is not configurable; it is coded into this app.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ //



enum struct FileSpec_t {
    Other = 0,
    Public = 1,
    Private = 2,
    Data = 3,
};


inline
const char *
as_string(FileSpec_t e) {
    switch (e) {
        case FileSpec_t::Public : {
            return "public";
        }

        case FileSpec_t::Private : {
            return "private";
        }

        case FileSpec_t::Data : {
            return "data";
        }


        default : {
            return "other";
        }
    }
}


struct Config_ {
    struct Source_ {
        Source_(const std::string &pattern)
                : filePattern_(pattern) {}


        std::string filePattern_;
        std::vector<std::string> rejectPatterns_;
        std::string startToken_;                                        // if empty, start of file
        std::string stopToken_;                                         // if empty, end of file
    };


    struct Output_ {
        std::string FileName(const Info_ &info, const Emitter::Funcs_ &funcs) const;


        FileSpec_t filespec_;
        Handle_ <Emitter_> fnFunc_;
        std::vector<std::string> emitters_;                             // referenced by name
    };


    std::vector<Source_> sources_;
    std::map<std::string, std::vector<Output_> > vals_;                 // string key is the type of input ("Enum" above)
    std::string ownPath_;
    std::string templatePath_;                                          // relative to config file, not to working directory!
    bool targetsOnly_;
};


namespace Config {

    Config_ Read(const std::string &filename, bool targets_only);

    void Print(const Config_ &cf, bool to_cerr = true);

}  // namespace Config

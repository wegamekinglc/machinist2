#pragma once

#include "handle.hpp"

#include <string>
#include <vector>


namespace File {
    std::string
    Path(const std::string &dir_in);

    std::string
    CombinedPath(const std::string &path1,
                 const std::string &path2);                               // cd path1; cd path2; -- has to see if path2 is relative

    std::string
    PathOnly(const std::string &filename);

    void
    Read(const std::string &filename,
         std::vector<std::string> *dst);                               // appends -- does not clear dst

    std::vector<std::string>
    List(const std::string &dir,
         const std::string &pattern,                                                                // default pattern is *.if
         const std::vector<std::string> &reject_patterns);                                          // lets us exclude MG_* from scan


    std::string
    InfoType(const std::string &filename);

    std::string
    InfoName(const std::string &filename);

    std::string
    DirInfoName(const std::string &dir);

}  // namespace File

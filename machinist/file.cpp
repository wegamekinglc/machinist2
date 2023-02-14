#include "file.hpp"

HERE

#ifdef _MSC_VER

# include <windows.h>

#else
# include <dirent.h>
#endif

#include <string>
#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <vector>


void
File::
Read(const std::string &filename, std::vector<std::string> *dst) {
    std::ifstream src(filename);
    char buf[2048];
    while (src.getline(buf, 2048)) {
        dst->push_back(std::string(buf));
    }
}


namespace {
    bool IsSlash(char c) {
        return c == '/' || c == '\\';
    }


    // open source globbing function, lightly edited
    bool
    wildcmp(const char *wild, const char *string) {
        // Written by Jack Handy - <A href="mailto:jakkhandy@hotmail.com">jakkhandy@hotmail.com</A>

        const char *cp = NULL, *mp = NULL;

        while (*string && *wild != '*') {
            if (*wild != *string && *wild != '?') {
                return false;
            }
            ++wild;
            ++string;
        }

        while (*string) {
            if (*wild == '*') {
                if (!*++wild) {
                    return true;
                }
                mp = wild;
                cp = string + 1;
            } else if (*wild == *string || *wild == '?') {
                ++wild;
                ++string;
            } else {
                wild = mp;
                string = cp++;
            }
        }

        while (*wild == '*') {
            ++wild;
        }
        return !*wild;
    }

}  // namespace <un-named>


std::string
File::Path(const std::string &dir_in) {
    return (dir_in.empty() || dir_in == ".") ? std::string()
                                             : (IsSlash(dir_in.back()) ? dir_in : dir_in + OS_SEP);
}


std::string
File::
CombinedPath(const std::string &path1, const std::string &path2) {
    return (path2.empty() || path2[0] == '.') ? path1 + path2                                   // path2 is relative
                                              : path2;
}


std::string
File::
PathOnly(const std::string &filename) {
    std::string retval(filename);
    while (!retval.empty() && !IsSlash(retval.back())) {
        retval.pop_back();
    }
    return retval;
}


std::vector<std::string>
File::
List(const std::string &dir,
     const std::string &pattern,
     const std::vector<std::string> &reject_patterns) {
    std::vector<std::string> retval;

#if _MSC_VER
    WIN32_FIND_DATAA found;
    HANDLE hfind = FindFirstFileA((Path(dir) + pattern).c_str(), &found);
    while (hfind != INVALID_HANDLE_VALUE) {
        bool reject = false;
        for (auto pr = reject_patterns.begin(); pr != reject_patterns.end() && !reject; ++pr) {
            reject = wildcmp(pr->c_str(), found.cFileName);
        }
        if (!reject) {
            retval.push_back(std::string(found.cFileName));
        }
        if (!FindNextFileA(hfind, &found)) {
            break;
        }
    }
    FindClose(hfind);
#else
    std::string searchPath = dir + '/' + pattern;
    DIR* dp = opendir(dir.c_str());

    REQUIRE(dp, "Can't open directory '" + dir + "'");

    struct dirent* dirp;
    while ((dirp = readdir(dp)))
    {
        bool reject = !wildcmp(pattern.c_str(), dirp->d_name);
        for (auto pr = reject_patterns.begin(); pr != reject_patterns.end() && !reject; ++pr)
        {
                reject = wildcmp(pr->c_str(), dirp->d_name);
        }
        if (!reject)
        {
                retval.push_back(std::string(dirp->d_name));
        }
    }
    closedir(dp);
#endif

    return retval;
}


std::string
File::
InfoType(const std::string &filename) {
    // filenames should have the form "<name>.<type>.if"
    auto dot = filename.find('.');

    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");

    std::string rest = filename.substr(dot + 1);
    dot = rest.find('.');

    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");

    return rest.substr(0, dot);
}


std::string
File::
InfoName(const std::string &filename) {
    // filenames should have the form "<name>.<type>.if"

    auto dot = filename.find('.');

    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");

    return filename.substr(0, dot);
}


std::string
File::
DirInfoName(const std::string &dir) {
    // only take the leaf directory name

    auto last = dir.find_last_of("/\\");
    return last == std::string::npos ? dir : dir.substr(last + 1);
}


#include "file.hpp"

HERE

#include <fstream>
#include <iostream>
#include <istream>
#include <string>
#include <regex>
#include <filesystem>



void File::Read(const std::string& filename, std::vector<std::string>* dst) {
    std::ifstream src(filename);
    char buf[2048];
    while (src.getline(buf, 2048)) {
        dst->push_back(std::string(buf));
    }
}

namespace {
    bool IsSlash(char c) { return c == '/' || c == '\\'; }
} // namespace

std::string File::Path(const std::string& dir_in) {
    return (dir_in.empty() || dir_in == ".") ? std::string() : (IsSlash(dir_in.back()) ? dir_in : dir_in + OS_SEP);
}
std::string File::CombinedPath(const std::string& path1, const std::string& path2) {
    return (path2.empty() || path2[0] == '.') ? path1 + path2 // path2 is relative
                                              : path2;
}

std::string File::PathOnly(const std::string& filename) {
    std::string retval(filename);
    while (!retval.empty() && !IsSlash(retval.back()))
        retval.pop_back();
    return retval;
}

namespace fs = std::filesystem;
std::vector<std::string> File::List(const std::string& dir, const std::string& pattern, const std::vector<std::string>& reject_patterns) {
    fs::recursive_directory_iterator it(dir);
    fs::recursive_directory_iterator endit;
    std::vector<std::string> ret_val;
    const std::regex filter(pattern);
    while (it != endit) {
        bool reject = false;
        std::smatch what;
        std::string file_name = it->path().filename().string();
        if (std::regex_match(file_name, what, filter)) {
            for (auto pr = reject_patterns.begin(); pr != reject_patterns.end() && !reject; ++pr) {
                const std::regex reject_pattern(*pr);
                reject = std::regex_match(file_name, what, reject_pattern);
            }
            if (!reject)
                ret_val.push_back(it->path().string());
        }
        ++it;
    }
    return ret_val;
}

std::string File::InfoType(const std::string& filename) {
    // filenames should have the form "<name>.<type>.if"
    auto dot = filename.find('.');
    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");
    std::string rest = filename.substr(dot + 1);
    dot = rest.find('.');
    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");
    return rest.substr(0, dot);
}
std::string File::InfoName(const std::string& filename) {
    // filenames should have the form "<name>.<type>.if"
    auto dot = filename.find('.');
    REQUIRE(dot != std::string::npos, "Input filename should have two separator periods");
    return filename.substr(0, dot);
}

std::string File::DirInfoName(const std::string& dir) {
    // only take the leaf directory name
    auto last = dir.find_last_of("/\\");
    return last == std::string::npos ? dir : dir.substr(last + 1);
}

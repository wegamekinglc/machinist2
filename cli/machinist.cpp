// Machinist.cpp : Defines the entry point for the console application.
//
#include "../machinist/handle.hpp"

#ifdef _WIN32

#include <direct.h>

#define getcwd _getcwd // stupid MSFT "deprecation" warning
#else
#include <unistd.h>
#endif

#include <iostream>
#include <fstream>
#include <cassert>
#include <numeric>
#include "../machinist/config.hpp"
#include "../machinist/emitter.hpp"
#include "../machinist/info.hpp"
#include "../machinist/file.hpp"

using std::pair;
using std::cout;
using std::endl;

namespace {
    void ReadCommandLine
            (int argc,
             char *argv[],
             std::string *config,
             std::vector<std::string> *libs,
             std::vector<std::string> *dirs) {
        for (int ii = 1; ii < argc; ++ii) {
            std::string arg(argv[ii]);
            REQUIRE(!arg.empty(), "machinist can't run without offering arguments");
            if (arg[0] == '-') {
                // command-line switch
                REQUIRE(arg.size() == 2, "Unrecognized command line switch");
                switch (arg[1]) {
                    case 'c':
                        REQUIRE(ii + 1 < argc, "-c switch must be followed by a config filename");
                        REQUIRE(config->empty(), "Can't supply multiple configs");
                        *config = argv[++ii];    // jump forward to this input
                        break;

                    case 'd':
                        REQUIRE(ii + 1 < argc, "-d switch must be followed by a working directory");
                        dirs->push_back(std::string(argv[++ii]));    // jump forward to this input
                        break;

                    case 'l':
                        REQUIRE(ii + 1 < argc, "-l must be followed by a template library filename");
                        libs->push_back(std::string(argv[++ii]));    // jump forward to this input
                        break;

                    default:
                        throw std::runtime_error("Unrecognized command line switch");
                }
            } else {
                // can't understand this
                REQUIRE(0, "Unexpected command line input");
            }
        }

        if (config->empty())
            *config = "config.ifc";    // the default
        if (dirs->empty())
            dirs->push_back(".");    // the default
    }

    bool SameExceptLF(const std::string &s1, const std::string &s2) {
        char LF(10);
        auto p1 = s1.begin(), p2 = s2.begin();
        while (p1 != s1.end() && p2 != s2.end()) {
            if (*p1 == LF)
                ++p1;
            else if (*p2 == LF)
                ++p2;
            else if (*p1++ != *p2++)
                return false;
        }
        // let p2 catch up, if needed
        while (p2 != s2.end() && *p2 == LF)
            ++p2;
        return p1 == s1.end() && p2 == s2.end();
    }

    bool StartsWith(const std::string &line, const std::string &token) {
        return line.find(token) == 0;
    }

    pair<std::string, std::string> TypeAndName(const std::string &line) {
        // just handle space-separated pair
        auto space = line.find(' ');
        REQUIRE(space != 0 && space != std::string::npos, "Can't find type/name separator");
        return make_pair(line.substr(0, space), line.substr(space + 1));
    }

    std::vector<std::string> AsDir(const std::vector<pair<std::string, std::string>> &types_and_names) {
        // reverses the operation of File::InfoType/Name,
        // just to get things back in a format the dir emitter understands
        std::vector<std::string> ret_val;
        ret_val.reserve(types_and_names.size());
        for (const auto &pair : types_and_names)
            ret_val.push_back(pair.second + "." + pair.first + ".if");
        return ret_val;
    }

    void WriteIfChanged
            (const std::string &dst_name,
             const std::vector<std::string> &output,
             int *n_written) {
        // I guess we have to read it
        std::vector<std::string> prior;
        File::Read(dst_name, &prior);
        const std::string allOld = accumulate(prior.begin(), prior.end(), std::string(), std::plus<>());
        const std::string allNew = accumulate(output.begin(), output.end(), std::string(), std::plus<>());

        if (!SameExceptLF(allOld, allNew)) {
            cout << "\tWriting " << dst_name << "\n";
            ++*n_written;
            std::ofstream dst(dst_name);
            for (const auto &po : output)
                dst << po;
        }
    }

    void WriteFromContents
            (const Config_ &config,
             const std::vector<std::string> &lib,
             const std::string &path,
             const pair<std::string, std::string> &type_and_name,
             const std::vector<std::string> &content,
             int *n_written) {
        const Emitter::Funcs_ &allFuncs = Emitter::GetAll(type_and_name.first,
                                                          File::CombinedPath(config.ownPath_, config.templatePath_),
                                                          lib);    // might parse a template file, lazily; will hold a static registry
        const Handle_<Info_> info = Info::Parse(type_and_name.first, type_and_name.second,
                                                content);    // will access a static registry of parsers
        auto find = config.vals_.find(type_and_name.first);
        if (find == config.vals_.end())
            return;
        const std::vector<Config_::Output_> &targets = find->second;
        for (const auto &target : targets) {
            std::string dstName = path + target.dst_(*info);
            std::vector<std::string> output;
            for (const auto &emitter : target.emitters_) {
                auto out = Emitter::Call(*info, allFuncs, emitter);
                output.insert(output.end(), out.begin(), out.end());
            }
            WriteIfChanged(dstName, output, n_written);
        }
    }

    // there are two ways to specify an input:
    //		-- in a file of its own, where the name and type will be read from the filename (e.g. MyFunc.public.if)
    //		-- embedded in another file between start and end tokens, where the first line after the start token will supply the name and type, e.g.:
    //					/*IF
    //					public MyFunc
    // the second method is slower because it requires scanning of every file which might have embedded markup
    // but may be more amenable to developers

    void WriteInfoFile
            (const Config_ &config,
             const std::vector<std::string> &lib,
             const std::string &path,
             const std::string &filename,
             std::vector<pair<std::string, std::string>> *things_read,
             int *n_written,
             int *n_lines) {
        cout << "Reading " << filename.c_str() << endl;
        const std::string infoType = File::InfoType(filename);
        if (!config.vals_.count(infoType)) {
            cout << "Skipping -- nothing to write" << endl;
            return;
        }
        std::vector<std::string> content;
        File::Read(path + filename, &content);
        *n_lines += static_cast<int>(content.size());
        things_read->push_back(make_pair(infoType, File::InfoName(filename)));
        WriteFromContents(config, lib, path, things_read->back(), content, n_written);
    }

    void WriteOneFile
            (const Config_ &config,
             const std::vector<std::string> &lib,
             const std::string &path,
             const std::string &filename,
             const std::string &start_token,
             const std::string &stop_token,
             std::vector<pair<std::string, std::string>> *things_read,
             int *n_written,
             int *n_lines) {
        if (start_token.empty()) {
            WriteInfoFile(config, lib, path, filename, things_read, n_written, n_lines);
            return;
        }
        cout << "Scanning " << filename.c_str() << "\n";
        cout.flush();
        std::vector<std::string> content;
        File::Read(filename, &content);
        *n_lines += static_cast<int>(content.size());
        for (auto pl = content.begin(); pl != content.end(); ++pl) {
            if (StartsWith(*pl, start_token)) {
                cout << "Found start token at " << (pl - content.begin()) << endl;
                // scan forward to find the stop token
                auto pStop = pl;
                for (;;) {
                    if (++pStop == content.end()) {
                        REQUIRE(stop_token.empty(), "Reached end of file without stop token");
                    }
                    if (pStop == content.end() || StartsWith(*pStop, stop_token))
                        break;
                }
                auto pFirst = Next(pl);
                REQUIRE(pStop > pFirst, "No content between start and end tokens");
                auto pContent = Next(pFirst);
                cout << "Found content under " << pFirst->c_str() << endl;
                auto typeAndName = TypeAndName(*pFirst);
                if (!config.vals_.count(typeAndName.first))
                    cout << "Skipping -- nothing to write" << endl;
                else {
                    things_read->push_back(typeAndName);
                    WriteFromContents(config, lib, path, typeAndName, std::vector<std::string>(pContent, pStop), n_written);
                }
            }
        }
    }
}    // leave local

void XMain(int argc, char *argv[]) {
    char buf[2048];
    char* buff = getcwd(buf, 2048);
    if (buff == NULL)
        THROW("get current working directory failed.")
    cout << "In directory " << buf << endl;

    std::string configFile;
    std::vector<std::string> libs, dirs;
    ReadCommandLine(argc, argv, &configFile, &libs, &dirs);
    Config_ config = Config::Read(configFile);
    // read library contents
    std::vector<std::string> libContents;
    for (auto &lib : libs)
        File::Read(lib, &libContents);

    // loop over directories
    for (auto &dir : dirs) {
        // keep track of actions in this directory only
        std::vector<pair<std::string, std::string>> thingsRead;
        int nWritten = 0, nRead = 0, nLines = 0;
        // set directory
        cout << "Scanning " << dir << "\n";
        const std::string path = File::Path(dir);

        // loop over input file types
        for (auto ps = config.sources_.begin(); ps != config.sources_.end(); ++ps) {
            std::vector<std::string> infoFiles = File::List(dir, ps->filePattern_, ps->rejectPatterns_);
            for (auto pi = infoFiles.begin(); pi != infoFiles.end(); ++pi, ++nRead)
                WriteOneFile(config, libContents, path, *pi, ps->startToken_, ps->stopToken_, &thingsRead, &nWritten,
                             &nLines);
        }
        cout << "Scanned " << nRead << " files (" << nLines << " lines), found " << thingsRead.size() << " blocks"
             << endl;
        // finally write the whole-directory info
        WriteFromContents(config, libContents, path, make_pair(std::string("dir"), File::DirInfoName(dir)),
                          AsDir(thingsRead), &nWritten);

        cout << "Wrote " << nWritten << " files" << endl;
        cout << "Machinist finished directory " << dir << endl;
    }
}

int main(int argc, char *argv[]) {
    try {
        XMain(argc, argv);
        return 0;
    }
    catch (std::exception &e) {
        std::cerr << "Error:  " << e.what() << endl;
        return -1;
    }
}



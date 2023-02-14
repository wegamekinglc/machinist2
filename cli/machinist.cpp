// Machinist.cpp : Defines the entry point for the console application.

#include "handle.hpp"

HERE

#ifdef _MSC_VER

# include <direct.h>

#else
# include <unistd.h>
#endif

#include "config.hpp"
#include "emitter.hpp"
#include "info.hpp"
#include "file.hpp"

#include <cassert>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace {
    void
    ReadCommandLine(int argc,
                    char *argv[],
                    bool *targets_only,
                    std::string *config,
                    std::vector<std::string> *libs,
                    std::vector<std::string> *dirs) {
        for (int ii = 1; ii < argc; ++ii) {
            std::string arg(argv[ii]);
            assert(!arg.empty());

            if (arg[0] == '-')                                                          // command-line switch
            {
                REQUIRE(arg.size() == 2, "Unrecognized command line switch");
                switch (arg[1]) {
                    case 'c' : {
                        REQUIRE(ii + 1 < argc, "-c switch must be followed by a config filename");
                        REQUIRE(config->empty(), "Can't supply multiple configs");
                        *config = argv[++ii];                                           // jump forward to this input
                        break;
                    }

                    case 'd' : {
                        REQUIRE(ii + 1 < argc, "-d switch must be followed by a working directory");
                        dirs->push_back(std::string(argv[++ii]));                       // jump forward to this input
                        break;
                    }

                    case 'l' : {
                        REQUIRE(ii + 1 < argc, "-l must be followed by a template library filename");
                        libs->push_back(std::string(argv[++ii]));                       // jump forward to this input
                        break;
                    }

                    case 't' : {
                        *targets_only = true;
                        break;
                    }


                    default : {
                        THROW("Unrecognized command line switch");
                    }
                }
            } else {
                // can't understand this
                REQUIRE(0, "Unexpected command line input");
            }
        }

        if (config->empty()) {
            *config = "config.ifc";                                                     // the default
        }
        if (dirs->empty()) {
            dirs->push_back(".");                                                       // the default
        }
    }


    bool
    SameExceptLF(const std::string &s1, const std::string &s2) {
        char LF(10);
        auto p1 = s1.begin(), p2 = s2.begin();
        while (p1 != s1.end() && p2 != s2.end()) {
            if (*p1 == LF) {
                ++p1;
            } else if (*p2 == LF) {
                ++p2;
            } else if (*p1++ != *p2++) {
                return false;
            }
        }

        // let p2 catch up, if needed
        while (p2 != s2.end() && *p2 == LF) {
            ++p2;
        }
        return p1 == s1.end() && p2 == s2.end();
    }


    bool
    StartsWith(const std::string &line, const std::string &token) {
        return line.find(token) == 0;
    }


    std::pair<std::string, std::string>
    TypeAndName(const std::string &line) {
        // just handle space-separated pair
        auto sep = line.find_first_of(" :");

        REQUIRE(sep != 0 && sep != std::string::npos, "Can't find type/name separator (space or colon)");

        return std::make_pair(line.substr(0, sep), line.substr(sep + 1));
    }


    Handle_ <Info_>
    AsDir(const std::vector<std::pair<std::string, Handle_ < Info_>>

    > things_read,
    const std::string &info_name
    ) {
    // combines infos into one massive Info_

    std::unique_ptr<Info_> retval(new Info_(0, 0, info_name));
    for (const auto &[type, info] : things_read) {
    retval->children_.
    emplace(type, info
    );
    // note this does NOT set the parent/root of each info
}
return retval.

release();

}


void
WriteIfChanged(const std::string &dst_name,
               const std::vector<std::string> &output,
               int *n_written) {
    // I guess we have to read it
    std::vector<std::string> prior;
    File::Read(dst_name, &prior);
    const std::string allOld = accumulate(prior.begin(), prior.end(), std::string(), std::plus<std::string>());
    const std::string allNew = accumulate(output.begin(), output.end(), std::string(), std::plus<std::string>());

    if (!SameExceptLF(allOld, allNew)) {
        std::cout << "\tWriting " << dst_name << "\n";

        std::filesystem::path dir = std::filesystem::path(dst_name).parent_path();
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        ++*n_written;
        std::ofstream dst(dst_name);
        for (auto po = output.begin(); po != output.end(); ++po) {
            dst << *po;
        }
    }
}


void
WriteFromInfo(const Config_ &config,
              const std::vector<std::string> &lib,
              const std::string &path,
              const std::string &src_filename,
              const std::string &info_type,
              const Handle_ <Info_> &info,
              int *n_written) {
    // might parse a template file, lazily; will hold a static registry
    const Emitter::Funcs_ &allFuncs = Emitter::GetAll(info_type,
                                                      File::CombinedPath(config.ownPath_, config.templatePath_), lib);

    auto it = config.vals_.find(info_type);
    if (it == config.vals_.end()) {
        REQUIRE(info_type == "dir", "Found markup for " + info_type + " but no emitters");
        return;
    }
    const std::vector<Config_::Output_> &targets = it->second;
    for (auto pt = targets.begin(); pt != targets.end(); ++pt) {
        std::string dstName = path + pt->FileName(*info, allFuncs);
        if (config.targetsOnly_) {
            std::cout << src_filename << ":" << as_string(pt->filespec_) << ":" << dstName << "\n";
        } else {
            std::vector<std::string> output;
            for (auto pToEmit = pt->emitters_.begin(); pToEmit != pt->emitters_.end(); ++pToEmit) {
                auto out = Emitter::Call(*info, allFuncs, *pToEmit);
                output.insert(output.end(), out.begin(), out.end());
            }
            WriteIfChanged(dstName, output, n_written);
        }
    }
}


Handle_ <Info_>
WriteFromContents(const Config_ &config,
                  const std::vector<std::string> &lib,
                  const std::string &path,
                  const std::string &src_filename,
                  const std::string &info_type,
                  const std::string &info_name,
                  const std::vector<std::string> &content,
                  int *n_written) {
    // might parse a template file, lazily; will hold a static registry
    const Emitter::Funcs_ &allFuncs [[maybe_unused]] = Emitter::GetAll(info_type, File::CombinedPath(config.ownPath_,
                                                                                                     config.templatePath_),
                                                                       lib);

    // will access a static registry of parsers
    const Handle_ <Info_> info = Info::Parse(info_type, info_name, content);
    WriteFromInfo(config, lib, path, src_filename, info_type, info, n_written);
    return info;
}


// there are two ways to specify an input:
//      -- in a file of its own, where the name and type will be read from the filename (e.g. MyFunc.public.if)
//      -- embedded in another file between start and end tokens, where the first line after the start token will supply the name and type, e.g.:
//                  /*IF
//                  public MyFunc
// the second method is slower because it requires scanning of every file which might have embedded markup
// but may be more amenable to developers


void WriteInfoFile(const Config_ &config,
                   const std::vector<std::string> &lib,
                   const std::string &path,
                   const std::string &filename,
                   std::vector<std::pair<std::string, Handle_ < Info_>>

>* things_read,
int *n_written,
        std::size_t
* n_lines)
{
if (filename.find_first_not_of(".") == std::string::npos)
{
return;                                                                     // . or ..
}

std::cout << "Reading " << filename.

c_str()

<< "\n";

const std::string infoType = File::InfoType(filename);
if (!config.vals_.
count(infoType)
)
{
std::cout << "Skipping -- nothing to write\n";
return;
}

std::vector<std::string> content;
File::Read(path
+ filename, &content);

*n_lines += content.

size();

std::string infoName = File::InfoName(filename);
Handle_ <Info_> info = WriteFromContents(config, lib, path, filename, infoType, infoName, content, n_written);
things_read->
push_back(std::make_pair(infoType, info)
);
if (config.targetsOnly_)
{
std::cout << "SOURCE:" << filename.

c_str()

<< "\n";
}
}


void
WriteFromFileInput(const Config_ &config,
                   const std::vector<std::string> &lib,
                   const std::string &path,
                   const std::string &filename,
                   const std::string &start_token,
                   const std::string &stop_token,
                   std::vector<std::pair<std::string, Handle_ < Info_>>

>* things_read,
int *n_written,
        std::size_t
* n_lines)
{
if (start_token.

empty()

)
{
WriteInfoFile(config, lib, path, filename, things_read, n_written, n_lines
);
return;
}
std::cout << "Scanning " << filename.

c_str()

<< "\n";
std::cout.

flush();

bool isSource = false;
std::vector<std::string> content;

File::Read(path
+ filename, &content);
*n_lines += content.

size();

for (
auto pl = content.begin();
pl != content.

end();

++pl)
{
if (
StartsWith(*pl, start_token
))
{
std::cout << "Found start token at " << (pl - content.

begin()

) << "\n";
// scan forward to find the stop token
auto pStop = pl;
for (;; )
{
if (++pStop == content.

end()

)
{
REQUIRE(stop_token
.

empty(),

"Reached end of file without stop token");
}
if (pStop == content.

end()

||
StartsWith(*pStop, stop_token
))
{
break;
}
}
auto pFirst = Next(pl);

REQUIRE(pStop
> pFirst, "No content between start and end tokens");

auto pContent = Next(pFirst);

std::cout << "Found content under " << pFirst->

c_str()

<< "\n";

auto [type, name] = TypeAndName(*pFirst);
if (!config.vals_.
count(type)
)
{
std::cout << "Skipping -- nothing to write for type " << type << "\n";
}
else
{
Handle_ <Info_> info = WriteFromContents(config, lib, path, filename, type, name,
                                         std::vector<std::string>(pContent, pStop), n_written);
things_read->

push_back (std::make_pair(type, info));

isSource = true;
}
}
}
if (
isSource &&config
.targetsOnly_)
{
std::cout << "SOURCE:" << filename.

c_str()

<< "\n";
}
}

}  // namespace <un-named>



void
XMain(int argc, char *argv[]) {
    std::cout << "In XMain" << std::endl;

    char buf[2048];
#ifdef _MSC_VER
    (void) _getcwd(buf, 2048);
    std::cout << "In directory " << buf << "\n";
#else
    auto cwd = getcwd(buf, sizeof(buf));
    if (cwd)
    {
        std::cout << "In directory " << cwd << "\n";
    }
#endif

    std::string configFile;
    std::vector<std::string> libs, dirs;
    bool targetsOnly = false;

    ReadCommandLine(argc, argv, &targetsOnly, &configFile, &libs, &dirs);
    Config_ config = Config::Read(configFile, targetsOnly);

    //Config::Print(config);

    // read library contents
    std::vector<std::string> libContents;
    for (auto pl = libs.begin(); pl != libs.end(); ++pl) {
        File::Read(*pl, &libContents);
    }

    // loop over directories
    for (auto pd = dirs.begin(); pd != dirs.end(); ++pd) {
        // keep track of actions in this directory only
        std::vector<std::pair<std::string, Handle_ < Info_>> > thingsRead;
        int nWritten = 0, nRead = 0;
        std::size_t nLines = 0;

        // set directory
        std::cout << "Scanning " << *pd << "\n";
        const std::string path = File::Path(*pd);

        // loop over input file types
        for (auto ps = config.sources_.begin(); ps != config.sources_.end(); ++ps) {
            std::vector<std::string> infoFiles = File::List(*pd, ps->filePattern_, ps->rejectPatterns_);
            for (auto pi = infoFiles.begin(); pi != infoFiles.end(); ++pi, ++nRead) {
                WriteFromFileInput(config, libContents, path, *pi, ps->startToken_, ps->stopToken_, &thingsRead,
                                   &nWritten, &nLines);
            }
        }

        std::cout << "Scanned " << nRead << " files (" << nLines << " lines), found " << thingsRead.size()
                  << " blocks\n";

        // finally write the whole-directory info
        WriteFromInfo(config, libContents, path, ".", "dir", AsDir(thingsRead, File::DirInfoName(*pd)), &nWritten);

        std::cout << "Wrote " << nWritten << " files\n";
        std::cout << "Machinist finished directory " << *pd << "\n";
    }
}


int main(int argc, char *argv[]) {
    try {
        char cb = '\\';
        std::string backslash(1, cb);
        std::cout << backslash << std::endl;
        std::cout << "Starting\n";
        XMain(argc, argv);
        return 0;
    }
    catch (std::exception &e) {
        std::cerr << "Error:  " << e.what() << "\n";
        return -1;
    }
}

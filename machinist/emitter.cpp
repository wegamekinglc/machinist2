
#include "emitter.hpp"
#include "info.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <regex>

std::vector<std::string> Emitter::Call(const Info_& arg, const Funcs_& lib, const std::string& which) {
    if (auto pf = lib.ofInfo_.find(which); pf != lib.ofInfo_.end())
        return (*pf->second)(arg, lib);
    // can't find the function -- is there a transform-of-contents?
    if (auto pt = lib.ofString_.find(which); pt != lib.ofString_.end())
        return (*pt->second)(arg.content_, lib);
    THROW("Can't find emitter function '" + which + "'");
}

std::vector<std::string> Emitter::CallTransform(const std::string& src, const Funcs_& lib, const std::string& which) {
    auto pt = lib.ofString_.find(which);
    if (pt == lib.ofString_.end()) {
        // has to be a Perl-style /before/after/ substitution
        REQUIRE(count(which.begin(), which.end(), '/') == 3 && which.front() == '/' && which.back() == '/',
                "Can't find emitter transformation '" + which + "'");
        auto mid = find(which.begin() + 1, which.end(), '/');
        std::regex before(std::string(which.begin() + 1, mid));
        std::string after(mid + 1, which.end() - 1);
        std::string retval = regex_replace(src, before, after);
        return std::vector<std::string>(1, retval);
    }
    return (*pt->second)(src, lib);
}

namespace {
    // static registry of emitter creators -- we do not own the pointers, they are assumed to point to static file-scope
    // objects
    std::map<std::string, std::vector<const Emitter::Source_*>>& TheSources() {
        static std::map<std::string, std::vector<const Emitter::Source_*>> RETVAL;
        return RETVAL;
    }

    template <class K_, class V_> void MergeIn(std::map<K_, V_>* dst, const std::map<K_, V_>& contrib) {
        for (auto pc = contrib.begin(); pc != contrib.end(); ++pc) {
            REQUIRE(!dst->count(pc->first), "Redefinition of emitter for " + pc->first);
            dst->insert(*pc);
        }
    }
} // namespace

void Emitter::RegisterSource(const std::string& info_type, const Source_& src) { TheSources()[info_type].push_back(&src); }

const Emitter::Funcs_& Emitter::GetAll(const std::string& info_type,
                                       const std::string& path,
                                       const std::vector<std::string>& lib) // contents of library, not file paths
{
    std::cout << "Looking for " << info_type << " in " << path << "\n";
    // static registry of emitters themselves
    static std::map<std::string, Emitter::Funcs_> RETVALS;
    if (!RETVALS.count(info_type)) {
        std::cout << "Parsing...";
        Emitter::Funcs_ retval;
        // parse all emitters for this name, combining results by merge
        for (auto s : TheSources()[info_type]) // auto-vivification is OK
        {
            auto temp = s->Parse(lib, path);
            std::cout << "Adding " << std::to_string(temp.ofInfo_.size()) << " functions\n";
            MergeIn(&retval.ofInfo_, temp.ofInfo_);
            MergeIn(&retval.ofString_, temp.ofString_);
        }
        RETVALS.insert(make_pair(info_type, retval));
    }
    std::cout << "Done\n";
    return RETVALS[info_type];
}

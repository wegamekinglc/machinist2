
#include "info.hpp"

HERE

#include <algorithm>
#include <cassert>
#include <iostream>

using std::unique_ptr;
using std::map;

bool Info::IsRoot(const Info_& i) {
    // two ways to check:  root is this, and parent is empty
    assert(!i.parent_ == (i.root_ == &i));
    return !i.parent_;
}

namespace {
    map<std::string, const Info::Parser_*>& TheParsers() {
        static map<std::string, const Info::Parser_*> RETVAL;
        return RETVAL;
    }

    //----------------------------------------------------------------------------

    // create an Info_ directly from a model of its contents, with no type-specific parsing
    // format of nested pairs, looks like the block below
    /*
    child1:child1_ownval
            gchild1:gchild1_ownval
            gchild2:_
    child2:child2_ownval
    ...
    */
    // top-level ownval (=info_name) comes from file name or first line of mark-up block, not seen here

    int Indent(const std::string& s) {
        int retval = 0;
        for (auto pc = s.begin(); pc != s.end(); ++pc, ++retval)
            if (*pc != '\t' && *pc != ' ')
                break;
        return retval;
    }

    Info_* XParseRaw(const Info_* parent,
                     const Info_* root,
                     const std::string& info_name,
                     std::vector<std::string>::const_iterator& line,
                     std::vector<std::string>::const_iterator end,
                     int tab_offset) {
        unique_ptr<Info_> retval(new Info_(parent, root, info_name));
        for (;;) {
            int indent = line == end ? -1 : Indent(*line);
            if (indent < tab_offset) // pop out to parent
                return retval.release();
            assert(indent == tab_offset);
            // new child starts here
            auto content = line->substr(indent);
            auto colon = content.find(':');
            auto childName = content.substr(0, colon);
            auto childVal = content.substr(colon + 1);
            retval->children_.insert(
                make_pair(childName, Handle_<Info_>(XParseRaw(retval.get(), root, childVal, ++line, end, indent + 1))));
        }
    }

    Info_* ParseRaw(const std::string& info_name, const std::vector<std::string>& content) {
        for (auto pc = content.begin(); pc != content.end(); ++pc)
            if (pc->find(':') == std::string::npos)
                return 0;
        // if any line does not contain a colon, then this function returns NULL and we can try some other parser
        // if every line contains a colon, we will try to parse, and throw an exception on failure
        auto beg = content.begin();
        auto end = content.begin();
        return XParseRaw(nullptr, nullptr, info_name, beg, end, 0);
    }

    void CheckBackPointers(const Info_& info, const Info_* parent = nullptr, const Info_* root = nullptr) {
        if (parent && info.parent_ != parent)
            std::cout << "Here";
        REQUIRE(!parent || info.parent_ == parent, "Bad parent of '" + info.content_ + "'");
        REQUIRE(!root || info.root_ == root, "Bad root");
        for (const auto& c : info.children_)
            CheckBackPointers(*c.second, &info, root ? root : info.root_);
    }
} // namespace

Info_* Info::Parse(const std::string& type, const std::string& name, const std::vector<std::string>& content) {
    if (Info_* raw = ParseRaw(name, content))
        return raw;
    // not in generic format; find the specialized parser
    REQUIRE(TheParsers().count(type), "No parsers for '" + type + "' info");
    std::unique_ptr<Info_> retval((*TheParsers()[type])(name, content));
    CheckBackPointers(*retval);
    return retval.release();
}

void Info::RegisterParser(const std::string& type, const Info::Parser_& parser) {
    std::cout << "Registering parser for " << type << "\n";
    assert(!TheParsers().count(type));
    TheParsers()[type] = &parser;
    std::cout << type << " done\n";
}

Info::Path_::Path_(const std::string& src) : absolute_(false) {
    for (auto start = src.begin(); start != src.end(); ++start) {
        if (*start == '/') {
            REQUIRE(start == src.begin(), "'//' not allowed in a path");
            absolute_ = true;
        } else {
            auto stop = find(start, src.end(), '/');
            childNames_.emplace_back(start, stop);
            start = stop; // don't need to skip the '/', the increment will do that
            if (start == src.end())
                break;
        }
    }
}

namespace {
    std::string ShowPath(const Info_& here) {
        std::string retval = here.content_;
        for (const Info_* parent = here.parent_; parent && parent != &here; parent = parent->parent_) {
            retval = parent->content_ + "/" + retval;
            if (parent->parent_ == parent) {
                break;
            }
        }
        return retval;
    }

}  // namespace <un-named>

const Info_& Info::Path_::operator()(const Info_& here, bool quiet) const {
    static const Handle_<Info_> FALSE = MakeLeaf(nullptr, nullptr, std::string());
    const Info_* retval = absolute_ ? here.root_ : &here;
    for (auto pc = childNames_.begin(); pc != childNames_.end(); ++pc) {
        const char step = '1' + static_cast<char>(pc - childNames_.begin());
        if (!retval) {
            REQUIRE(quiet, "Path navigation failed before step " + std::string(1, step) + " in " + ShowPath(here));
            return *FALSE;
        }
        if (*pc == "..") {
            REQUIRE(retval->parent_, "No parent to navigate to int " + ShowPath(here));
            retval = retval->parent_;
        } else if (*pc != ".") {
            auto cr = retval->children_.equal_range(*pc);
            if (cr.first == cr.second) {
                REQUIRE(quiet,
                        "Path navigation failed at step " + std::string(1, step) + ":  child '" + *pc + "' not found in " + ShowPath(here));
                return *FALSE;
            }
            REQUIRE(cr.first == --cr.second, "Path navigation failed at step " + std::string(1, step) + ":  child '" + *pc +
                                                 "' not unique in " + ShowPath(here)); // Next(cr.first) doesn't work due to compiler bug
            retval = cr.first->second.get();
        }
    }
    if (!retval) {
        REQUIRE(quiet, "Path navigation failed at final step in " + ShowPath(here));
        return *FALSE;
    }
    return *retval;
}

Handle_<Info_> Info::MakeLeaf(const Info_* parent, const Info_* root, const std::string& content) {
    return new Info_(parent, root, content);
}

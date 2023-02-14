#pragma once

#include "handle.hpp"

#include <map>
#include <string>
#include <vector>


// Info_ structure contains the high-level description of an object
//
struct Info_ {
    Info_(const Info_ *parent,
          const Info_ *root,
          const std::string &content)
            : content_(content), parent_(parent), root_(root) {
        if (!root) {
            root_ = this;
        }
    }


    std::string content_;                                                           // content at this level
    std::multimap<std::string, Handle_<Info_> > children_;
    const Info_ *parent_;
    const Info_ *root_;
};


namespace Info {
    bool
    IsRoot(const Info_ &i);


    // relies on an internal parser registry for each type
    //
    Info_ *
    Parse(const std::string &type,
          const std::string &name,
          const std::vector<std::string> &content);


    struct Parser_ {
        virtual
        ~Parser_();

        virtual
        Info_ *
        operator()(const std::string &info_name, const std::vector<std::string> &content) const = 0;
    };


    void
    RegisterParser(const std::string &info_type,
                   const Parser_ &parser);                // parser:  assumed to be a file static -- we hold a pointer to the parser


    struct Path_ {
        Path_(const std::string &text);                                                 // parses a path -- '/' is the separator

        const Info_ &
        operator()(const Info_ &here, bool quiet) const;

        bool absolute_;
        std::vector<std::string> childNames_;
    };


    // a generally useful function
    Handle_<Info_>
    MakeLeaf(const Info_ *parent,
             const Info_ *root,
             const std::string &content);
}


#ifndef MACHINIST_PARSEUTILS__
#define MACHINIST_PARSEUTILS__

#include "handle.hpp"

using std::unique_ptr;
struct Info_;
class Emitter_;
class StringTransform_;

namespace ParseUtils {
    bool IsWhite(char c);
    bool StartsWithWhitespace(const std::string& line);
    bool IsAllWhite(const std::string& line);
    std::string AfterInitialWhitespace(const std::string& line);
    std::string UntilWhite(const std::string& src);
    std::string TrimWhitespace(const std::string& src);

    std::vector<std::string>::const_iterator ReadHelp(const Info_* parent,
                                            const Info_* root,
                                            std::vector<std::string>::const_iterator line,
                                            std::vector<std::string>::const_iterator end,
                                            unique_ptr<Info_>* dst,
                                            std::vector<Handle_<Info_>>* local_conditions = 0);

    std::vector<std::string>::const_iterator ReadInsert(const Info_* parent,
                                              const Info_* root,
                                              std::vector<std::string>::const_iterator line,
                                              std::vector<std::string>::const_iterator end,
                                              unique_ptr<Info_>* dst);

    bool AddNonempty(Info_* info, const std::string& tag, const std::string& val);

    std::vector<std::string>::const_iterator ReadCondition(const Info_* parent,
                                                 std::vector<std::string>::const_iterator line,
                                                 std::vector<std::string>::const_iterator end,
                                                 Handle_<Info_>* dst);

    std::vector<std::string>::const_iterator ReadLink(const Info_* parent,
                                            std::vector<std::string>::const_iterator line,
                                            std::vector<std::string>::const_iterator end,
                                            Handle_<Info_>* dst);

    // support output too
    std::string GetMandatory(const Info_& info, const std::string& child);
    std::string GetOptional(const Info_& info, const std::string& child);
    std::string EmbeddableForm(const std::string& src); // strips out things that cannot be in a C++ identifier
    std::string HtmlSafe(const std::string& src);       // makes embeddable within HTML
    std::string TexSafe(const std::string& src);        // makes embeddable within TeX
    std::string Condensed(const std::string& src);

    std::string WithParentName(const Info_& src);      // substitutes parent name for '$'
    std::string WithGrandparentName(const Info_& src); // substitutes grandparent name for '$'

    // one kind of emitter just wraps a function(Info_)
    typedef std::string (*emit_from_info_t)(const Info_&);
    Emitter_* EmitUnassisted(emit_from_info_t func);
    typedef std::string (*emit_from_string_t)(const std::string&);
    StringTransform_* EmitTransform(emit_from_string_t func);
} // namespace ParseUtils

#endif
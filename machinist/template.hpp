
#ifndef MACHINIST_TEMPLATE__
#define MACHINIST_TEMPLATE__

#include "handle.hpp"
#include <map>

using std::map;
class Emitter_;
namespace Emitter {
    struct Funcs_;
}

namespace Template {
    Emitter::Funcs_ Parse(const std::vector<std::string>& src);

    void SetGlobalCount(int c);
} // namespace Template

#endif

/* A template file defines a set of emitters -- functions that transform info to text.
These may refer to other functions outside the template file:  functions are not resolved until execution.

The % key is reserved for function definitions and function calls.
Literal % should be written as %%.
The ` (backquote) key is reserved for comments -- the rest of the line will be ignored.
Literal ` should be written as %`.

"%:function_name:function_body" defines a function.  The function body may contain newlines.
        This should be placed at the start of a line; the preceding newline will be ignored, not considered as part of
the previous function
"%function_name()" calls a function with the current info as its argument
"%function_name(path)" calls a function with some other info, described by the path, as its argument
        paths are analogous to Unix filesystem navigation
                / is a separator
                leading / means root
                .. means parent
                . means this
                children are named
        in this context _ is the special function which outputs the content_ of the input info
                                        " is the special function which "stringifies" the content, enclosing in
double-quotes and escaping existing double-quotes
"%ENV(env_variable)", though it looks like a function call, resolves at parse time to the value of the given environment
variable in the system
"%?{test}{emit}" first evaluates the text to test, then if it is nonempty (e.g. if a given child exists) returns the
text to emit equivalent to ?~{test}.{emit}
"%|{text1}{text2} first emits text1, then iff it is empty also emits text2
        equivalent to text1%~{text1}{text2}
"%~{test}pattern{emit}" first evaluates the text to test, then if it matches the given pattern, returns the text to emit
        if the pattern is omitted, "^$" is used -- i.e., only the empty std::string is matched, and this reduces the the
'not' operator braces within the pattern must be escaped with '%'
"%*[child_name]{emit}" concatenates the emitted text for each child of the given name
        the emit function is called repeatedly, with the input info set to each child in turn
"%*[child_name]sep{emit}" concatenates the emitted text for each child of the given name
        as above, but inserts the separator between successive emissions
"%^[child_name]sep{emit}" concatenates the emitted text for each child of the given name
        as "%*" syntax, but sorts the children in alphabetic order by their (top-level) contents
"%transform<text_before>" emits the text into a temporary buffer, then concatenates the result of applying the
transformation function to that text a call to %func(...), if the function is not found, will next try %func<%_(...)>;
thus a transform defines a function acting on content_ if the transformation has the form /<before>/<after>/ (like a
Perl substitution), that regex substitution is applied
*/

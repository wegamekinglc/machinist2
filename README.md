# User guide to Machinist

<table>
<tr>
  <td>Build Status</td>
  <td>
    <a href="https://travis-ci.org/alpha-miner/Finance-Python">
    <img src="https://travis-ci.org/wegamekinglc/machinist2.svg?branch=master" alt="travis build status" />
    </a>
  </td>
</tr>
<tr>
  <td>Coverage</td>
  <td><img src="https://coveralls.io/repos/wegamekinglc/machinist2/badge.svg?branch=master&service=github" alt="coverage" /></td>
</tr>
</table>

* Tom Hyer, Cheng Li
* 5 March 2015

## Overview of Execution

Machinist scans files in a directory, looking for parseable mark-up blocks.  
Each block has an a priori type which is used to find the relevant parser.
The result of parsing is a property tree with pointers to parent and root.
Each type has an emitter, which defines functions producing text from a ptree.
Emitters are defined in C++ and a template language; each can call the other.
Template library files define functions available to all emitters.
The user configures which emitter functions to call and where to write the results.

## About This Document

Strings in CAPS are to be understood as placeholders for user-supplied text or function arguments.
Optional text is enclosed in double-braces; e.g., `FUNC(ARG1[[, ARG2]])`.  The expressions \n and \t
in examples indicate newline and tab, not literal escape sequences.


## Platforms

~Machinist uses the Windows API to read and write files; it will not run on any other OS.~

Now **Machinist** works both on windows and unix style OS.


## The Command Line

Machinist takes command-line inputs specifying:
* what directory to operate in (may be used more than once):  `-d DIR`
* what configuration file to use:  `-c CFG`
* what library file to read in (may be used more than once): `-l LIB`

If no directory is specified, `.` is used; if no configuration file is specified, "config.ifc" 
is used.  The configuration file's relative path is based on the working directory (where 
execution was launched), not from any directory specified by `-d`.

Machinist scans each file in each specified directory.


## Configuration

The configuration file specifies:
* the directory in which to find template files:  "@DIR\"
   * this is the relative path from the location of the config file
   * the final '\' is mandatory
   * environment variables can be specified using "$(VARNAME)" as part of the DIR
   
* which file types to scan: "<-GLOB[[;GLOB]][[!GLOB]]"
    * e.g., "<-*.if" causes every file matching "*.if" to be scanned
		 multiple patterns can be specified, separated by ';'
    * patterns to reject can be placed after the scan definition, prefixed with '!'
         e.g., "<-*.cpp;*.h!MG_*" scans .cpp and .h files except those matching "MG_*"
    * tokens marking begin/end of a mark-up block can be placed on the next two lines, indented
         e.g. "<-*.cpp\n\t/*IF\n\t-IF" indicates mark-up is found between '/*IF' and '-IF'
         these tokens must be at the start of lines; the rest of the line is ignored
* for each mark-up type, which functions to emit and where to place them
    * an unindented line not beginning with "->" introduces a new type
			multiple types can be supplied, separated with "|"
    * following unindented lines beginning with "->FILE" cause creation of a file
               the template syntax should be used to compute the filename
			   '#' in the file name is an abbreviation for %_()
               every filename specification should use the info to be unique
			   a file will be created for each mark-up block of the current type
    * following indented lines are the names of functions to emit into that file
               supply type first, then filename, then function name


Here is an example config.ifc:

```
@$(TEMPLATE_DIR)
<-*.if
<-*.cpp;*.h;*.hpp!MG_*
	/*IF
	-IF
public
->MG_#.cpp
	ExcelSource
	PythonSource
->MG_#.h
	DotNetHeader
->MG_#.htm
	HtmlHelp
object|settings
->MG_#.cpp
	Constructor
```
            
In English, this reads "Find templates in the directory defined by environment variable 
TEMPLATE_DIR.  Scan each .if file as a single mark-up block; scan *.cpp and *.h.*.hpp files 
(except those beginning with MG_), searching for mark-up blocks between '/*IF' and '-IF'.  For 
each block of type 'public', create three files and write the outputs of the specified functions 
there; for each block of type 'object' or 'settings', create a file and write the output of its 
emitter's 'Constructor' function there."

The config file parser expects its contents in the order described here; it is not clever enough 
to deal with permutations.


## Emitters

An emitter is a function which produces text based on the contents of an input ptree.  To let 
emitters call other emitters, we provide as inputs both the argument (ptree or text) and a function 
library.  This library is a collection of emitters indexed by a string name, plus a separate 
collection of string-to-string transformation functions, also indexed by name.  

An emitter written in C++ is an instance of the abstract class Emitter_, implementing its virtual
operator() function.  It may call other library functions using Emitter::Call (for functions of 
ptree) and Emitter::CallTransform (for string-to-string transforms), providing the name of the
function being called.  Thus functions are bound to names only during emission.

Several standard emitters are already part of Machinist, and are added to the function library for
each mark-up type.  They include EmitContent_ which emits the top-level contents of a ptree; 
EmitCounter_ which emits the value of a single global counter, and increments the counter;
ResetCounter_ which resets the global counter to zero and emits nothing; and EmitRecursive which
emits the entire contents of the ptree, with each child on a separate line and indentation of one
tab for each level of nesting; and WithParentName_/WithGrandparentName_ which emits the top-level 
contents, but with '$' replaced by the parent's or grandparent's contents.

Several standard transforms are also included:  Stringify which encloses a string in double-quotes
and escapes internal double-quotes; HtmlSafe which substitutes character codes for '<', '>' and '&';
and TexSafe which wraps underscores and math characters in '$'.

If Emitter::Call cannot find a function with the name specified, it also looks for a string
transform with the same name, and calls that transform on the top-level contents of the ptree.
If Emitter::CallTransform cannot find a transformation with the name specified, it attempts to
interpret the name as a Perl-style substitution instruction (without the 's') like "/BEFORE/AFTER/".
The parts of such a substitution must be literal; function calls within the pattern are not
supported.  The C++ standard library is used for all regular expression matching and substitution.


## Templates

Emitters and transforms can also be written in Machinist's own template language.  A template is 
a sequence of function definitions, each introduced by "%:NAME:".  Each mark-up type has its own 
template; shared functions can be defined in library templates, see below.  Function names may not 
contain ':', '[', or '{'.  All text after the introduction, until the next function is introduced, 
is taken to be the function's definition.  Each defined function produces an emitter, which is 
stored in the library using the NAME as its key.

Text within the function definition, unless part of a control construct introduced using '%', is
emitted literally, including all whitespace.  Thus a function definition can provide a skeleton 
into which specific computed text is to be inserted.  The comment character '`' (backquote) 
suppresses the rest of its line, including itself and the newline.  For a literal backquote, use
"%`".

The syntax "%NAME([[PATH]])" calls the named function in the library, and inserts the emitted 
output at the point of the function call.  The standard emitters are available within templates:  
"_" emits the contents, "#" emits the counter, "#0" resets the counter, and "WithParentName" and 
"WithGrandparentName" call the corresponding C++ functions.  If no path is supplied, the function
argument is used.  A Unix-like path within the ptree can also be supplied to emit some function of
a parent or child ptree; ".." for the parent, "../.." for the grandparent, "/" for the root, and
"childkey" for a particular child.  Note that downward path navigation (to children) will fail at
emission time if the child is not unique. 

A path navigation to a non-existent child will produce an error at emission time, with two 
exceptions.  Within the COND clause of a conditional emission using %?, %~ or %| (see below), a 
missing child will be treated as empty; and the magic function %_?(PATH) is equivalent to 
%_(PATH) except that it succeeds and emits an empty string if the path navigation fails.

The special syntax %ENV(NAME)% (here ENV is a literal all-caps keyword) is not a function of the 
ptree; it is resolved during parsing to the value of the named environment variable.

The syntax %subs<BEFORE>% evaluates the output of before, then applies the (named or pattern) 
substitution and emits the result.  Because of the implementation of Emitter::Call (see above),
"%SUBS([[PATH]])" can be used in place of "%SUBS<%_([[PATH]])>" unless there is a function named 
SUBS.  In particular, the built-in substitutions are available as functions of the same name, 
except for Stringify which is abbreviated to '"'.    

Conditional emission is supported by several constructs. "%?{COND}{TEXT}" emits the result of TEXT 
iff the result of COND is not all whitespace.  "%~{COND}{TEXT}" does the opposite, emitting only 
if COND evaluates to all whitespace.  "%~{COND}PATT{TEXT}" emits iff the result of evaluating COND 
matches the pattern PATT.  "%|{TEXT1}{TEXT2}" emits the result of TEXT1, and if it contains only 
whitespace also emits the result of TEXT2; this is sugar for "%?{TEXT1}{TEXT1}%~{TEXT1}{TEXT2}".

The other major feature of the template language is iteration over children.  "%*[KEY]{TEXT}"
emits the result of TEXT for every child node of the argument with key KEY, concatenating all
the emitted text at the function call point.  Note that if TEXT itself contains function calls,
they will be called on each child ptree, rather than the ptree on which the "%*" construct is
operating.  A separator may be placed between the ']' and '{'; thus "%*[arg], {%_()}" emits a 
comma-separated list of arguments (if the ptree stores its arguments as children with key 'arg').
"%^" is equivalent to "%*" except that it iterates through multiple children alphabetically (by
their top-level contents) rather than in the order of their insertion into the ptree.

Each template files must be named "TYPE.mgt" where TYPE is the mark-up type (see below) for which 
it supplies emitters, and must be stored in the directory indicated in the configuration file (see 
above).

Here is an example template function:
%:TexArgHelp:`
\item{\verb!%_()!}{{\small\ (%TypeForHelp()%*[optional]{ (Optional)})} %*[help]{%TexSafe()}.}

This is meant to emit TeX code, to become part of a user manual, for one argument to an API 
function.  It emits the following
      the literal "\item{\verb!"
      the argument name
      the literal "!}{{\small\ ("
      the result of the function call %TypeForHelp()
      for each child with key "optional", the string " (Optional)"
            i.e., the presence of such a child marks the argument as optional
            this could also be written as "%?{%_(optional)}{ (Optional)}"
      the literal ")} "
      for each child with key "help", the result of the function call %TexSafe() on that child
            this applies the string transformation TexSafe to the child's name
            this could also be written %TexSafe<%_()>, making its operation more explicit
      the literal ".}"


## Library Templates

Common functions to be used by multiple mark-up types may be defined in library templates, which
by convention have the extension ".mgl".  Each library specified at the command line (using the "-l"
option; see above) is amalgamated into a single block of text, which is prepended to each template
(.mgt) file before parsing.  

Libraries are specified on the command line, rather than in the configuration, so that they can
be used for directory-specific information; e.g., an export directive which is different for each
DLL in a project can be defined appropriately in a directory-specific library.


## Mark-up Types

Each mark-up block has an associated type, in addition to the ptree of its contents.  The top-level
content of the root node is deemed to be the name associated with that block.  It is this name which
is used in constructing the output filenames based on the configuration (see above).  For a mark-up
block stored in its own file, the type and name are deduced from the filename, which is expected to 
be of the form NAME.TYPE.EXT where the extension (".if" in the example above) is given in the 
configuration file.  For mark-up identified by delimiters within a file, the first line is expected
to have the format "TYPE NAME".

The mark-up type will be used to determine what functions to emit and where to place the result
(as described in the configuration, see above); what template library to use in emitting text (see
above); and what parser to use to convert the input mark-up to the intermediate ptree (see below).

Emitters are registered without parsing, then parsed lazily the first time a mark-up block of the
corresponding type is found.  Mark-up blocks with no outputs specified in the configuration will be
skipped with no such parsing.


## Parsers

Once the type is known, we look up the appropriate parser to read the remainder of the mark-up and
construct the ptree.  These parsers are written directly in C++, and registered in a global static
repository when Machinist is launched.  There are built-in parsers for the following types:  dir
(see below), enumeration, public, recipe, settings, storable.  

However, if every line in the mark-up (except the first line giving type and name) contains a colon 
(':'), then the mark-up block is presumed to be provided directly in ptree format, with each element
on a single line and nesting corresponding to the tabs of indentation.  This 'raw' format is parsed
to a ptree, whether or not a registered parser is found.  Thus it is possible to work with new types
without providing any corresponding parsers; however, when available, parsers allow inputs in a 
terser and more readable format.

The emitter functions, especially those involving iteration over children, will depend on advance
knowledge of what child names may be present in the ptree.  These "magic words" force a tight 
coupling between the parser (or the ptree contents, if entered directly) and the emitter.


## The "dir" Type

During the execution of Machinist on one target directory, a ptree object of type "dir" is 
accumulated.  Its name (top-level content) is the leaf part of the path as supplied at the command
line (using "-d"; see above).  If the "-d" option is not specified and Machinist is simply
operating on the working directory, this will be empty.  Each mark-up block found in the directory
is inserted as a child of this ptree's root node, with key equal to the block's type and contents
equal to the block's top-level contents.  

The "dir" type has its own template file, dir.mgt, and the configuration file specifies the outputs
to be created.  This is useful for, e.g., HTML help index pages or master API headers.  Indexing of
the contents of multiple directories is beyond the scope of Machinist.


## Errors and Messages

Machinist produces high-level status updates before starting a top-level task:  scanning a file,
parsing a mark-up block or template, or writing a file.  Looking at the most recent update before
an error message is often sufficient to localize the error.

Machinist has no "-v" setting; it is always equally verbose.

A commonly seen class of error contains the string "too close to end", meaning that a control
construct has been found without enough characters remaining to complete it.  Such errors are 
often triggered by imbalanced braces or brackets, which cause the template parser to consume more 
characters than the user intended.








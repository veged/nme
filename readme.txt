   Nyctergatis Markup Engine

Nyctergatis Markup Engine (NME) is a library for parsing text with
markup and converting it to other text formats. The markup has the
following goals:

 - A set of simple rules a user can learn and rely on.
 - Robust specifications with room for evolution without breaking
compatibility.

The markup is based on Creole. Creole is a collaborative work in
progress; only its core features are frozen. This is why the
documentation of the markup used by NME is not Creole itself, but the
documentation provided with the engine.

  Usage

The library core is made of two source files in C:

NME.h
  header file with all extern declarations and detailed usage
information, compatible with C++;
NME.c
  function and data definitions.

In addition, the following optional code is provided:

NMEStyle.h and NMEStyle.c
  functions to collect style information separately
NMEAutolink.h and NMEAutolink.c
  automatic conversion of CamelCase words and URLs to links;
NMEPluginCalendar.h and NMEPluginCalendar.c
  plugin to create the calendar corresponding to a given month and
year in a table.
NMEPluginRaw.h and NMEPluginRaw.c
  plugin to embed raw data copied verbatim to the output stream;
NMEPluginReverse.h and NMEPluginReverse.c
  plugin to reverse text;
NMEPluginRot13.h and NMEPluginRot13.c
  plugin to apply ROT-13 conversion to text (letters A-M and N-Z are
switched);
NMEPluginUppercase.h and NMEPluginUppercase.c
  plugin to convert text to uppercase;
NMECpp.h
  C++ glue.

The last files are applications:

NMEMain.c
  command-line application which filters its input (stdin), which is
text with markup, and writes to its output (stdout) the result of the
conversion to HTML or several other formats. To build it, cd to the
NME distribution directory and type make.
NMETest.cpp
  simple test application for NMECpp.h. To build it, cd to the NME
distribution directory and type make nmecpp.
NMEGtkTest.c
  test application for NMEStyle which displays styled text in a GTK
window. It requires GTK+ 2.0, available at http://www.gtk.org. To
build it, cd to the NME distribution directory and type make nmegtk.

  License

Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

Neither the name of Yves Piguet nor the names of its contributors may
be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------

  Release notes

 19 November 2008

 - new hook function charHookFun called when a character is copied to
output; useful to match text fragments in input and output
 - new hook function getVarFun for custom variables in expressions
 - character = was skipped in headings even when it wasn't the
optional trailing heading markup; fixed

 25 August 2008

 - new variable p in output format strings for output offset
 - new format NMEOutputFormatTextCompact with less blank lines than
NMEOutputFormatText
 - reduced right margin for RTF output
 - tables should be followed by an empty line; fixed
 - --easylink option in NMEMain.c

 15 October 2007

 - unnested unnumbered or numbered list items weren't recognized when
following sublists; fixed

 12 October 2007

 - NME output better escapes characters when it has to, but usually
doesn't unless required
 - in NME output, support for mixed nested lists
 - double-slash is considered as a part of URL instead of italic
markup only if it follows a digit, letter, '+', '-' or '.', and a
colon; and it precedes a nonblank character; therefore, a tilde
before the colon, such as abc~://..., restores the meaning of the
double-slash sequence as italic markup

 9 October 2007

 - in lists, item markup must match, else it is interpreted as plain
characters or as bold or monospace markup
 - NMEMFC.cpp compiles correctly without _UNICODE

 4 October 2007

 - link encoding for RTF output
 - NMEGtk tuning
 - parHook and divHook bugs, revealed by new option --checkhooks of
NMEMain.c, fixed

 29 September 2007

 - new high-level function NMEGtkInsert to insert NME text directly
into a GtkTextBuffer; NMEGtkTest.c modified to use it
 - NMEProcess optionally returns the output length in unicode
characters, assuming UTF-8 input
 - in NMEMain.c, option --strictcreole doesn't disable escape
character anymore
 - NMECpp.h uses C++ exceptions for error handling only if
UseNMECppException is defined; else method getOutput returns an error
code (the choice is useful for platforms where exceptions are not
supported or have a large cost)
 - minor bug fixes in NMEStyle.c (plain paragraphs and tables)
 - refined NMEMFC.cpp
 - error codes in hook callbacks weren't handled correctly; fixed
 - support for links and images in span hook and NMEStyle
 - support for links in NMEGtk and NMEMFC
 - in NMECpp and NMEStyleCpp, copy constructors and assignment
operators
 - NMEEncodeURLFun has an additional argument for
application-specific data, and structure NMEOutputFormat has an
additional field for it

 23 August 2007

 - bug in unicode offset computation fixed
 - kNMEProcessOptXRef wasn't handled correctly; fixed
 - blanks following starting = or preceding ending = in titles are
ignored
 - in NMEGtkApplyStyle, additional argument for the offset of NME
output in GTK+ text buffer
 - in LaTeX output, braces are now escaped and bold italic is
suported
 - monospace text wasn't signalled to spanHookFun; fixed
 - MFC glue with NMEMFC.cpp and NMEMFC.h (function NMEMFCSetRichText
which renders NME text into a CRichEditCtrl)

 5 July 2007

 - option to have verbatim rendered as monospace
(kNMEProcessOptVerbatimMono)

 4 July 2007

 - callback to check valid wordwrap points
 - bad unicode encoding for RTF fixed
 - tilde is an escape character before any nonblank character,
including alphanumeric

 19 June 2007

 - support for UCS-16 offset (16-bit unicode offsets when the input
is encoded in UTF-8)
 - C++ glue for NMEStyle (NMEStyleCpp.h)
 - in block preformatted, when 3 closing braces follow spaces at the
beginning of a line, one space is removed

 7 June 2007

 - NMEStyle to separate text from style
 - NMEGtkTest.c, a test application for NMEStyle which displays a
text file with NME markup in a GTK+ 2 window
 - support for placeholders (plugin option to use triple angle
brackets instead of double angle brackets)
 - bug fixes

 30 May 2007

 - heading of level 1 and 2 following other paragraphs without a
blank line were always numbered; fixed
 - XHTML-compatible image tag in HTML output
 - plugins source code in separate files
 - in plugin tables, single field for option flags instead of
separate boolean fields
 - in plugin tables, option flag for plugins between paragraphs
 - characters weren't encoded in preformatted blocks; now there is a
separate callback
 - new opaque structure NMEContext for output data and other
contextual information, to reduce the number of arguments passed
around and to support string output with embedded expressions from
plugin and autoconvert functions
 - variable o (offset in source code) in embedded expressions, useful
as unique identifiers for hyperlinks
 - hook functions for paragraphs and sections
 - functions to retrieve current output format and options in plugin,
autoconvert and hook functions, and to copy input text from input to
output
 - change in the arguments of NMEProcess, to keep original source
code
 - table of contents plugin (sample code for the new features)
 - C++ glue (NMECpp.h)

 24 April 2007

 - optional automatic links for URLs beginning with http://,
https://, ftp://, mailto:
 - optional separate source file for automatic links for camelcase
and URL
 - options to disable indented paragraphs and definitions lists
 - in NMEMain.c, option --strictnme to disable features not in Creole
1.0

 19 April 2007

 - optional numbering of headings, level 1 and/or 2
 - images weren't supported in links; fixed
 - empty cells weren't output correctly in text format; fixed
 - in NMEMain.c, most plugins have reparseOutput set, so that
word-wrap is applied to output
 - Python glue for the command-line application (nme.py)

 18 April 2007

 - plugins now use the "placeholder" syntax, with double angle
brackets
 - references to plugin globally-unique identifiers removed
 - option to match only the beginning of plugin names

 16 April 2007

 - doc header and trailer suppressed in plugin output
 - less references to Creole in source code
 - "null" format for producing no output
 - better handling of plugins, especially those which produce NME
code which must be processed again
 - autoconversion plugins which make substitutions before NME
conversion (as an example, NMEMain.c has an option to convert
camelCase words to links)

 30 March 2007

 - wrong encoding of unicode characters >= 32768 for RTF output;
fixed
 - optional processing of plugin output
 - calendar plugin in NMEMain.c

 23 March 2007

 - support for parenthesis in expressions in output strings
 - replicated strings themselves can contain expressions, and syntax
for expressions in output strings has been changed to support it
 - default font size in structure NMEOutputFormat and support for
default font size (with nonpositive values, HTML does not contain
font size specifications at all, relying on the browser defaults)
 - argument withPreAndPostDoc of NMEProcess replaced by options
 - images
 - option to consider line breaks as paragraph separators
 - minor documentation fixes

 20 March 2007

 - Initial release

----------

Copyright Yves Piguet, 25 March 2008. May be distributed with NME or
cited with proper attribution.


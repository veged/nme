/**
 *	@file NME.h
 *	@brief Nyctergatis Markup Engine (simple text markup formatting based on Creole)
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet
 *	@see @ref nme
 */
 
/**	@page nme NME Usage
 *
 *	Nyctergatis Markup Engine (NME) - Simple text markup formatting based on Creole.
 *
 *	@author Yves Piguet
 *	@see http://www.nyctergatis.com (home of NME project)
 *	and http://www.wikicreole.org (Creole site).
 *
 *	@section Markup Markup
 *
 *	The following markup is recognized:
 *	- =main title=
 *	- ==section title==
 *	- ===subsection title=== etc.
 *	- empty-line-separated paragraphs
 *	- * (asterisk) for lists (multiple lines allowed, merged)
 *	- # for numbered lists (multiple lines allowed, merged)
 *	- ; : for definition lists (sublists are obtained by repeating *#;)
 *	- : for indented paragraphs
 *	- lines between {{{ and }}} alone in lines:
 *    preformatted (tabs are replaced with spaces with 4-char alignment)
 *	- **bold**
 *	- //italic//
 *	- __underline__
 *	- ## monospace ##
 *	- ^^superscript^^
 *	- ,,subscript,,
 *	- {{{verbatim}}}
 *	- ~x (escaped character, where x is nonblank)
 *	- ---- alone in a line for a horizontal rule
 *	- [[link]] or [[link|text]]
 *	- {{image}} or {{image|alt text}}
 *	- &lt;&lt;plugins>>, or block plugins where << and >> are alone in lines
 *	- &lt;&lt;&lt;placeholder>>>
 *
 *	@section File File overview
 *
 *	- NME.c, NME.h: main source code of NME; everything else is optional
 *	- NMEAutolink.c, NMEAutolink.h: optional support for automatic conversion
 *	  of CamelCase words (aka wiki words, i.e. words with mixed lowercase and
 *	  uppercase letters used in some wikis as page names) and/or URL to links
 *	  without requiring the double-bracket markup
 *	- NMEMain.c: source code of a command-line application which filters
 *	  input text with NME markup, with support for many options
 *
 *	@section Usage Usage
 *
 *	To convert text with NME markup to another format, such as HTML or
 *	RTF, only NME.c and NME.h are required. The code below shows how
 *	to convert markup to HTML.
 *
 *	@code
 *	#include "NME.h"
 *
 *	NMEText input;
 *	NMEInt inputLength;
 *	(read source of length inputLength into input)
 *	NMEInt size = ...; // buffer size, typically initialized to 2x inputLength
 *	NMEText buf;
 *	buf = malloc(size);
 *	NMEText output;
 *	NMEInt outputLength;
 *	NMEErr err;
 *	for (;;)
 *	{
 *		err = NMEProcess(input, inputLength,
 *			buf, size,
 *			kNMEProcessOptDefault, "\n", &NMEOutputFormatHTML, 0,
 *			&output, &outputLength);
 *		if (err == kNMEErrNotEnoughMemory)	// unlikely
 *		{
 *			(increase size)
 *			(realloc buf)
 *		}
 *		else
 *			break;
 *	}
 *	if (err == kNMEErrOk)
 *		(write outputLength first bytes of output[])
 *	else
 *		(handle error)
 *	free(buf);
 *	@endcode
 *
 *	@section Security Security
 *
 *	Inline images are subject to cross site scripting if links to
 *	arbitrary sources are supported. Hypertext links are less dangerous,
 *	because they must be clicked by the user and they are loaded in
 *	separate pages.
 */
 
/**	@page nmelicense NME License
 *
 *	Copyright (c) 2007-2008, Yves Piguet.
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

Neither the name of Yves Piguet nor the names of its contributors may be
used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	@section Remark Remarks
 *
 *	Sorry for shouting; we keep the new BSD license disclaimer verbatim.
 *
 *	The license above is known as the "new BSD license", i.e. without
 *	advertising clause. For a discussion on different open-source and
 *	"free" ("copyleft") licenses, please visit  http://www.opensource.org
 *	and http://www.gnu.org.
 */

#ifndef __NME__
#define __NME__

#ifdef __cplusplus
extern "C" {
#endif

/// Character (8-bit; processing possible with compatible charsets like UTF-8 or Shift-JIS)
typedef char NMEChar;

/// Text
typedef NMEChar *NMEText;

/// Constant text
typedef NMEChar const *NMEConstText;

/// Integer
typedef int NMEInt;

/// Boolean
typedef int NMEBoolean;

#if !defined(NULL)
/// null pointer
#	define NULL 0
#endif

#if !defined(FALSE)
#	define FALSE 0	///< false boolean value
#	define TRUE 1	///< true boolean value
#endif

/// Error codes
typedef enum
{
	kNMEErrOk = 0,	///< no error
	kNMEErrNotEnoughMemory,	///< not enough memory
	kNMEErrBadMarkup,	///< bad markup (can be considered as OK)
	kNMEErrInternal,	///< internal error (should never occur)
	
	kNMEErr1stNMEOpt,	///< first error code used by NME options (plugins etc.)
	
	kNMEErr1stUser = 10000	///< first error code reserved to user for plugins etc.
} NMEErr;

/// Options for NMEProcess (change the way input is parsed)
enum
{
	kNMEProcessOptDefault = 0,	///< default options
	kNMEProcessOptNoPreAndPost = 0x1,	///< no doc header and trailer
	kNMEProcessOptNoH1 = 0x4,	///< no heading level 1 (replaced by level 2)
	kNMEProcessOptH1Num = 0x8,	///< numbering of heading level 1
	kNMEProcessOptH2Num = 0x10,	///< numbering of heading level 2
	kNMEProcessOptNoDL = 0x20,	///< no definition list
	kNMEProcessOptNoIndentedPar = 0x40,	///< no indented paragraph
	kNMEProcessOptNoMultilinePar = 0x80,	///< line breaks are paragraph separators
	kNMEProcessOptNoEscape = 0x100,	///< no escape character
	kNMEProcessOptNoHRule = 0x200,	///< no horizontal rule
	kNMEProcessOptNoLink = 0x400,	///< no hypertext link
	kNMEProcessOptNoImage = 0x800,	///< no image
	kNMEProcessOptNoTable = 0x1000,	///< no table
	kNMEProcessOptNoUnderline = 0x2000,	///< no underline
	kNMEProcessOptNoMonospace = 0x4000,	///< no monospace
	kNMEProcessOptNoSubSuperscript = 0x8000,	///< no subscript or superscript
	kNMEProcessOptNoBold = 0x10000,	///< no bold
	kNMEProcessOptNoItalic = 0x20000,	///< no italic
	kNMEProcessOptNoPlugin = 0x40000,	///< no plugin
	kNMEProcessOptVerbatimMono = 0x100000,	///< inline verbatim is rendered in monospace
	kNMEProcessOptXRef = 0x200000	///< headings have hyperlink target labels
};

/** Opaque structure for NMEAddString and functions which call it
*/
typedef struct NMEContextStruct NMEContext;

/** Callback to encode a single character (typically UTF-8)
	@param[in] src input characters
	@param[in] srcLen size of src in bytes
	@param[in,out] srcIx index in src (should be updated by one character)
	@param[in] dest address of encoded text
	@param[in] destSize size of dest
	@param[in,out] destIx index in dest (should be updated by encoded character length)
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
typedef NMEErr (*NMEEncodeCharFun)(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context,
		void *data);

/// Wordwrap permission
typedef enum
{
	kNMEWordwrapNo = 0,	///< wordwrap not permitted here
	kNMEWordwrapReplaceChar,	///< linebreak should replace character (typically a space)
	kNMEWordwrapInsert	///< linebreak should be inserted
} NMEWordwrapPermission;

/** Callback to check valid wordwrap point
	@param[in] txt output text
	@param[in] len length of output text in bytes
	@param[in] i line break to check (space to be removed or before char to
	push to next line)
	@param[in,out] data value specific to the callback
	@return wordwrap kind (kNMEWordwrapNo if not permitted here)
*/
typedef NMEWordwrapPermission (*NMEWordwrapCheckFun)(NMEConstText txt,
		NMEInt len, NMEInt i,
		void *data);

/** Callback to encode link to a URL (removing or encoding spaces etc.)
	@param[in] link input characters
	@param[in] linkLen length of link
	@param[in] dest address of encoded text
	@param[in] destSize size of dest
	@param[in,out] destIx index in dest (should be updated by encoded character length)
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
typedef NMEErr (*NMEEncodeURLFun)(NMEConstText link, NMEInt linkLen,
		NMEContext *context, void *data);

/// Special values for argument level of NMEProcessHookFun
enum
{
	kNMEHookLevelPar = 0,
	kNMEHookLevelSpan = -1
};

/** Hook called when a character is added to output; can be used
	to match text fragments in input and output or add data to output
	stream.
	@param[in] srcIndex current index in source code
	@param[in,out] context current context
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
typedef NMEErr (*NMECharHookFun)(NMEInt srcIndex,
		NMEContext *context,
		void *data);

/** Hook called when a construct is entered or exited; can be used
	to collect information or add data to output stream.
	@param[in] level heading or list level (1 = topmost, par=kNMEHookLevelPar,
	span=kNMEHookLevelSpan)
	@param[in] item list item or heading counter
	@param[in] enter TRUE when entering construct, FALSE when exiting
	@param[in] markup null-terminated string for initial markup ("p" for par,
	"*" for unnumbered list of any level, "//" for italic, "{{{" for preformatted)
	@param[in] srcIndex current index in source code
	@param[in,out] context current context
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
typedef NMEErr (*NMEProcessHookFun)(NMEInt level,
		NMEInt item,
		NMEBoolean enter,
		NMEConstText markup,
		NMEInt srcIndex,
		NMEContext *context,
		void *data);

/** Callback for plugins
	@param[in] name plugin identifier, such as "calendar"
	@param[in] nameLen length of name
	@param[in] data data text
	@param[in] dataLen length of data
	@param[in] eol null-terminated string used for end-of-line
	@param[out] dest result (formatted text)
	@param[in] destSize maximum size of dest
	@param[out] destLen formatted text length
	@param[in] userData pointer passed from the parser, as specified in NMEPlugin
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
typedef NMEErr (*NMEPluginFun)(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// Options for NMEPlugin
enum
{
	kNMEPluginOptDefault = 0,
	kNMEPluginOptPartialName = 0x1,	/**< if set, name must match the beginning of what's
			found in NME input; if FALSE, name must match the whole word following
			"<<" **/
	kNMEPluginOptReparseOutput = 0x2,	///< if set, output should be parsed again
	kNMEPluginOptBetweenPar = 0x4,	///< if set, forced outside paragraphs or lists
	kNMEPluginOptTripleAngleBrackets = 0x8	/**< if set, used with triple angle brackets
		(placeholders) */
};

/// Structure for plugins
typedef struct
{
	NMEConstText name;	///< plugin identifier, such as "calendar"
	NMEInt options;	///< kNMEPluginOptDefault or sum of other options
	NMEPluginFun cb;	///< callback
	void *userData;	///< pointer passed to cb
} NMEPlugin;

/// End-of-table marker for table of plugins
#define NMEPluginTableEnd {NULL, kNMEPluginOptDefault, NULL, NULL}

/** Callback for autoconvert
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in,out] i index in src (token to parse on input, next token on output)
	@param[in] eol null-terminated string used for end-of-line
	@param[out] dest result (formatted text)
	@param[in] destSize maximum size of dest
	@param[in,out] destLen formatted text length
	@param[in] userData pointer passed from the parser, as specified in NMEAutoconvert
	@return TRUE for conversion, else FALSE
*/
typedef NMEBoolean (*NMEAutoconvertFun)(NMEConstText src, NMEInt srcLen,
		NMEInt *i,
		NMEContext *context,
		void *userData);

/// Structure for autoconvert
typedef struct
{
	NMEAutoconvertFun cb;	///< callback
	void *userData;	///< pointer passed to cb
} NMEAutoconvert;

/**	Callback for application-specific variables with uppercase name
	@param[in] name variable name ('A'-'Z')
	@param[in] userData pointer passed to the function,
	field getVarData in NMEOutputFormat
	@return value (default value 0 for undefined variables)
*/
typedef NMEInt (*NMEGetVarFun)(NMEChar name, void *userData);

/// End-of-table marker for table of interwikis
#define NMEAutoconvertTableEnd {NULL, NULL}

/// Structure for interwiki
typedef struct
{
	NMEConstText alias;	///< alias, including ending colon if any
	NMEConstText urlPrefix;	///< URL fragment alias is replaced with
} NMEInterwiki;

/// End-of-table marker for table of interwikis
#define NMEInterwikiTableEnd {NULL, NULL}

/** Structure of output format fragments used by NMEProcess.
	All strings may contain control sequences which are processed before
	being copied to the output. There are three kinds of control sequences:
	- Numerical expression, whose result is converted to a decimal integer;
	  expression is given between %{ and }. Example: "%{2+3}" is replaced by "5".
	- Replicated string, i.e. a string which is replicated 0 or more times
	  depending on the result of a numerical expression. The control
	  sequence starts with %%{, followed by the numerical expression,
	  followed by }, followed by the string to replicate, followed by %% .
	  The replicated string itself may contain numerical expressions, but
	  not replicated strings. The string is not output if the numerical
	  expression gives a zero or negative result. Example: "%%{2+3}-%%" is
	  replaced by "-----", "%%{2>3}foo%%" by "", and "%%{5+6>10}%{5+6}pt%%"
	  by "11pt".
	- Special string %L for the current list nesting as NME markup characters,
	  as produced by NMECurrentListNesting.
	
	Character shown as % above is actually specified by ctrlChar; it can
	be replaced with any other 8-bit character, should it conflict with
	output markup.
	
	Expressions are made of signed integers, parameters, infix
	operators with the expected priority, and parenthesis for subexpressions.
	Comparisons yeld 1 for true and 0 for false; logical operators consider
	any nonzero value as true. Spaces and tabs are ignored.
	
	Parameters:
	- i current item index in lists (1=first)
	- l current list or heading level
	- s font size of normal text in points (nonpositive values should be
	  considered as the default value, but they can be filtered out if the
	  defFontSize field of NMEOutputFormat is positive)
	- o current offset in source code (can be used as a unique identifier for
	  hyperlinks)
	- p current offset in output
	
	Operators by increasing priority:
	- | logical OR (first op if nonzero, else second op)
	- & logical AND (first op if zero, else second op)
	- = ! < > comparison operators (! is "not equal to")
	- + - addition and subtraction
	- * / multiplication and integer division
	
	Conditional expression written "a ? b : c" in C can be written "a & b | c"
	
	@see NMEProcess
*/
typedef struct
{
	NMEConstText space;	///< one space
	NMEInt indentSpaces;	/**< spaces per level to indent lists and indented text (not first line),
		 0 to rely on target markup */
	NMEInt defFontSize;	/**< default font size used when fontSize is <= 0
		(may be 0 if all expressions handle the case s=0) */
	NMEChar ctrlChar;	///< control character for embedded expressions (usually %)
	NMEConstText beginDoc;	///< document preamble
	NMEConstText endDoc;	///< document postamble
	NMEInt maxHeadingLevel;	///< highest heading level
	NMEConstText beginHeading;	///< beginning of H1 header
	NMEConstText endHeading;	///< end of H1 header
	NMEConstText beginPar;	///< beginning of text paragraph
	NMEConstText endPar;	///< end of text paragraph
	NMEConstText lineBreak;	///< line break in text paragraphs
	NMEConstText beginPre;	///< beginning of block of preformatted text
	NMEConstText endPre;	///< end of block of preformatted text
	NMEConstText beginPreLine;	///< beginning of line of preformatted text (between beginPre/endPre)
	NMEConstText endPreLine;	///< end of line of preformatted text
	NMEConstText beginUL;	///< beginning of unnumbered list
	NMEConstText endUL;	///< end of unnumbered list
	NMEConstText beginULItem;	///< beginning of list item (between beginUL/endUL)
	NMEConstText endULItem;	///< end of list item
	NMEConstText beginOL;	///< beginning of numbered list
	NMEConstText endOL;	///< end of numbered list
	NMEConstText beginOLItem;	///< beginning of numbered list item, including number (between beginOL/endOL)
	NMEConstText endOLItem;	///< end of numbered list item
	NMEConstText beginDL;	///< beginning of definition list
	NMEConstText endDL;	///< end of definition list
	NMEConstText beginDT;	///< beginning of definition title
	NMEConstText endDT;	///< end of definition title
	NMEConstText emptyDT;	///< written before beginDD if it does not follow DT
	NMEConstText beginDD;	///< beginning of definition
	NMEConstText endDD;	///< end of definition
	NMEConstText beginIndented;	///< beginning of indented section
	NMEConstText endIndented;	///< end of indented section
	NMEConstText beginIndentedPar;	///< beginning of indented paragraph
	NMEConstText endIndentedPar;	///< end of indented paragraph
	NMEConstText beginTable;	///< beginning of a table
	NMEConstText endTable;	///< end of a table
	NMEConstText beginTableRow;	///< beginning of a table row
	NMEConstText endTableRow;	///< end of a table row
	NMEConstText beginTableHCell;	///< beginning of a header cell
	NMEConstText endTableHCell;	///< end of a header cell
	NMEConstText beginTableCell;	///< beginning of a normal cell
	NMEConstText endTableCell;	///< end of a normal cell
	NMEConstText horRule;	///< horizontal rule
	NMEConstText beginBold;	///< beginning of bold text span
	NMEConstText endBold;	///< end of bold text span
	NMEConstText beginItalic;	///< beginning of italic text span
	NMEConstText endItalic;	///< end of italic text span
	NMEConstText beginUnderline;	///< beginning of underline text span
	NMEConstText endUnderline;	///< end of underline text span
	NMEConstText beginSuperscript;	///< beginning of superscript text span
	NMEConstText endSuperscript;	///< end of superscript text span
	NMEConstText beginSubscript;	///< beginning of subscript text span
	NMEConstText endSubscript;	///< end of subscript text span
	NMEConstText beginCode;	///< beginning of monospace text span
	NMEConstText endCode;	///< end of monospace text span
	NMEConstText beginLink;	///< beginning of link
	NMEConstText endLink;	///< end of link
	NMEConstText sepLink;	///< separator between link and text of link, or NULL if no link
	NMEBoolean linkAfterSep;	///< TRUE if link (URL) is after sepLink, FALSE if before
	NMEConstText beginImage;	///< beginning of image
	NMEConstText endImage;	///< end of image
	NMEConstText sepImage;	///< separator between image and alt text of image, or NULL if no image
	NMEBoolean imageAfterSep;	///< TRUE if image (URL) is after sepImage, FALSE if before
	NMEBoolean noStyleInAlt;	///< TRUE if style markup is ignored in alt text of images
	NMEInterwiki const *interwikis;	///< array of interwikis, terminated by alias=NULL (NULL if none)
	NMEEncodeURLFun encodeURLFun;	///< URL encoder (NULL if none; not used for interwiki urlPrefix)
	void *encodeURLData;	///< data passed to encodeURLFun
	NMEEncodeCharFun encodeCharFun;	///< character encoder everywhere except for pre blocks (NULL if none)
	void *encodeCharData;	///< data passed to encodeCharFun
	NMEEncodeCharFun encodeCharPreFun;	///< character encoder in preformatted blocks (NULL if none)
	void *encodeCharPreData;	///< data passed to encodeCharPreFun
	NMEInt textWidth;	///< text width (-1 for no wordwrap)
	NMEWordwrapCheckFun wordwrapPermFun;	///< check if wordwrap is permitted
	void *wordwrapPermData;	///< data passed to wordwrapPermFun
	NMECharHookFun charHookFun;	///< char hook
	void *charHookData;	///< data passed to charHookFun
	NMEProcessHookFun divHookFun;	/**< process hook from heading to same-level heading,
		whole lists or tables */
	NMEProcessHookFun parHookFun;	/**< process hook for par, pre blocks, headings, list items,
		cell items, interparagraphs plugins */
	NMEProcessHookFun spanHookFun;	/**< process hook for spans of text */
	void *hookData;	///< data passed to sectionHookFun, parHookFun and spanHookFun
	NMEPlugin const *plugins;	///< array of plugins, terminated by name=NULL (NULL if none)
	NMEAutoconvert const *autoconverts;	///< array of autoconverts, terminated by cb=NULL (NULL if none)
	NMEGetVarFun getVarFun;	///< function which gets custom variable values ('A'-'Z') in expressions
	void *getVarData;	///< data passed to getVarFun
} NMEOutputFormat;

/** Structure for elements of table used by NMEEncodeCharFunDict.
	@see NMEEncodeCharFunDict
*/
typedef struct
{
	NMEChar ch;	///< character
	NMEConstText str;	///< replacement string
} NMEEncodeCharDict;

/** NMEEncodeCharFun function which replaces characters listed in a table with
	strings. Characters not in the table are copied unmodified. Its address should be
	stored in the encodeCharFun field of NMEOutputFormat, while the adresse of a table
	of type NMEEncodeCharDict[] ending with {0,NULL} should be stored in the
	encodeCharData field.
	@param[in] src input characters
	@param[in] srcLen size of src in bytes
	@param[in,out] srcIx index in src (updated by one character)
	@param[in,out] context context for embedded expressions
	@param[in,out] data address of substitution table, of type NMEEncodeCharDict[]
	@return error code (kNMEErrOk for success)
	@see NMEOutputFormat
*/
NMEErr NMEEncodeCharFunDict(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context, void *data);

/// Format strings for plain text output
extern NMEOutputFormat const NMEOutputFormatText;

/// Format strings for plain text output with fewer blank lines
extern NMEOutputFormat const NMEOutputFormatTextCompact;

/// Format strings for no output
extern NMEOutputFormat const NMEOutputFormatNull;

/// Format strings for NME text output
extern NMEOutputFormat const NMEOutputFormatNME;

/// Format strings for HTML output
extern NMEOutputFormat const NMEOutputFormatHTML;

/// Format strings for RTF output
extern NMEOutputFormat const NMEOutputFormatRTF;

/// Format strings for LaTeX output
extern NMEOutputFormat const NMEOutputFormatLaTeX;

/** Format strings for man page output
	@bug NMEOutputFormatMan needs more work or it will be discarded from
	the main code
*/
extern NMEOutputFormat const NMEOutputFormatMan;

/** Transform text by interpreting markup.
	This is the main and only required extern function of the parser.
	@param[in] nmeText source text with markup
	@param[in] nmeTextLen source text length
	@param[out] buf buffer used during conversion
	@param[in] bufSize size of buf
	@param[in] options kNMEProcessOptDefault or sum of options
	@param[in] eol null-terminated string used for end-of-line
	@param[in] outputFormat format strings, or NULL for default
	(NMEOutputFormatText)
	@param[in] fontSize font size of plain text in points (nonpositive -> default)
	@param[out] output formatted text (in buf), followed by null byte
	@param[out] outputLen formatted text length, excluding final null byte
	@param[out] outputUCS16Len formatted text length in 16-bit unicode characters
	assuming input is in UTF-8,	excluding final null byte (may be NULL)
	@return error code (kNMEErrOk for success)
	@bug Links are copied verbatim, without processing the escape character
	(this means that pipes and double-closing-brackets cannot be included
	in a link).
*/
NMEErr NMEProcess(NMEConstText nmeText, NMEInt nmeTextLen,
		NMEText buf, NMEInt bufSize,
		NMEInt options,
		NMEConstText eol,
		NMEOutputFormat const *outputFormat,
		NMEInt fontSize,
		NMEText *output,
		NMEInt *outputLen,
		NMEInt *outputUCS16Len);

/** Add a string to output, converting eol and embedded expressions.
	@param[in] str null-terminated string to append
	@param[in] strLen length of str, or -1 for null-terminated string
	@param[in] ctrlChar control character for embedded expressions
	@param[in,out] context current context
	@return TRUE for success, FALSE for failure (not enough space)
*/
NMEBoolean NMEAddString(NMEConstText str,
		NMEInt strLen,
		NMEChar ctrlChar,
		NMEContext *context);

/** Copy or skip source to output.
	@param[in] length number of bytes of source to consume
	@param[in] copy TRUE to copy source, FALSE to skip
	@param[in] encodeChar TRUE to use encodeCharFun, FALSE for plain copy
	@param[in,out] context current context
	@return TRUE for success, FALSE for failure (not enough space)
*/
NMEErr NMECopySource(NMEInt length,
		NMEBoolean copy,
		NMEBoolean encodeChar,
		NMEContext *context);

/** Get temporary memory which can be used in plugin, autolink and
	hook functions.
	@param[in] context current context
	@param[out] addr memory address
	@param[out] len number of bytes
*/
void NMEGetTempMemory(NMEContext const *context,
		NMEText *addr,
		NMEInt *len);

/** Get current output format and options.
	@param[in] context current context
	@param[out] outputFormat output format (not set if pointer is null)
	@param[out] options kNMEProcessOptDefault or sum of options
	@param[out] fontSize font size (not set if pointer is null)
*/
void NMEGetFormat(NMEContext const *context,
		NMEOutputFormat const **outputFormat,
		NMEInt *options,
		NMEInt *fontSize);

/** Accessor for output index.
	@param[in] context current context
	@return current output index
*/
NMEInt NMECurrentOutputIndex(NMEContext const *context);

/** Accessor for output index in unicode characters, assuming UTF-8 input.
	@param[in] context current context
	@return current output index
*/
NMEInt NMECurrentOutputIndexUCS16(NMEContext const *context);

/** Accessor for current link or image (can be called from hook "[["
	or "{{"), before any processing by NMEEncodeURLFun
	@param[in] context current context
	@param[out] linkOffset offset of link in NME source text
	@param[out] linkLength length of link
*/
void NMECurrentLink(NMEContext const *context,
		NMEInt *linkOffset, NMEInt *linkLength);

/** Accessor for output produced until now (provides context which can be used
	by to decide if a character should be escaped).
	@param[in] context current context
	@param[out] output address of output (not provided if NULL)
	@param[out] outputLength current length of output in bytes (not provided if NULL)
*/
void NMECurrentOutput(NMEContext const *context,
		NMEConstText *output, NMEInt *outputLength);

/**	Accessor for current list nesting as a string of NME markup characters.
	@param[in] context current context
	@return string (constant, valid until next call)
*/
NMEConstText NMECurrentListNesting(NMEContext const *context);

#ifdef __cplusplus
}
#endif

#endif

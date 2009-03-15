/**
 *	@file NME.c
 *	@brief Nyctergatis Markup Engine (simple text markup formatting based on Creole)
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see header file) */

#include "NME.h"

#define kMaxNesting 8	///< maximum nesting of lists

/** Special codes for list items (positive values are item numbers for OL) */
enum
{
	kNMEListNumUL = -100,	///< unnumbered list item
	kNMEListNumDT,	///< title in definition list
	kNMEListNumDD,	///< definition in definition list
	kNMEListIndented,	///< indented paragraph
	kNMEListNumTableCell,	///< table cell
	kNMEListNumTableHCell	///< table heading cell
};

/// Test if a character is a space or a tab
#define isBlank(c) ((c) == ' ' || (c) == '\t')

/// Test if a character is an eol
#define isEol(c) ((c) == '\r' || (c) == '\n')

/// Test if a character is a digit
#define isDigit(c) ((c) >= '0' && (c) <= '9')

/// Test if a character is alphabetic (ASCII) or numeric
#define isAlphaNum(c) \
	((c) >= 'a' && (c) <= 'z' || (c) >= 'A' && (c) <= 'Z' || isDigit(c))

/// Test if a byte is the first byte of a UTF-8 character
#define isFirstUTF8Byte(c) \
	(((c) & 0x80) == 0 || ((c) & 0xe0) == 0xc0 || ((c) & 0xf0) == 0xe0)

/// Maximum number of numbered heading levels
#define kMaxNumberedHeadingLevels 2

/// Maximum number of heading-separated section levels
#define kMaxSectionLevels 4

/// Tabulator width
#define kTabWidth 4

/// Assign return value to err and return it unless kNMEErrOk
#define CheckError(c) \
	do { err = (c); if (err != kNMEErrOk) return err; } while (0)

/** Parser states (the whole state includes the current nested lists with
item numbers for ordered lists, the stack of styles in the current paragraph
and the URL location in the source code while decoding the link text) */
typedef enum NMEState
{
	kNMEStatePar,	// or UL/OL depending on nesting
	kNMEStatePre,
	kNMEStateHeading,
	kNMEStateParAfterEol,
	kNMEStatePreAfterEol,
	kNMEStateBetweenPar
} NMEState;

/** Parsed token */
typedef enum NMEToken
{
	kNMETokenChar,	///< any other character
	kNMETokenSpace,	///< space
	kNMETokenTab,	///< tab
	kNMETokenEOL,	///< CR or LF or CRLF
	kNMETokenHeading,	///< one or more '='
	kNMETokenLineBreak,	///< double backslash
	kNMETokenLI,	///< *, #, ;, or : (indenting) at beginning of line (not ** without space)
	kNMETokenDD,	///< : after a space in the context of kNMEListNumDT
	kNMETokenTableCell,	///< single pipe
	kNMETokenTableHCell,	///< double pipe
	kNMETokenHR,
	kNMETokenPre,	///< {{{ or }}} alone in a line
	kNMETokenStyle,	///< any style (including verbatim) but link
	kNMETokenLinkBegin,	///< [[
	kNMETokenLinkEnd,	///< ]]
	kNMETokenImageBegin,	///< {{
	kNMETokenImageEnd,	///< }}
	kNMETokenPlugin,	///< << with other data on the same line
	kNMETokenPluginBlock,	///< << alone on a line (end tag must also be alone)
	kNMETokenPlaceholder,	///< <<< with other data on the same line
	kNMETokenPlaceholderBlock	///< <<< alone on a line (end tag must also be alone)
} NMEToken;

/** Text style */
typedef enum NMEStyle
{
	kNMEStyleBold = 0,	///< bold
		// must begin at 0 so that kNMEStylesCount is correct
	kNMEStyleItalic,	///< italic
	kNMEStyleUnderline,	///< underline
	kNMEStyleSuperscript,	///< superscript
	kNMEStyleSubscript,	///< subscript
	kNMEStyleMonospace,	///< monospace (code)
	kNMEStyleVerbatim,	///< verbatim
	kNMEStyleLink,	///< link text
	kNMEStyleImage,	///< image alt text
	kNMEStylesCount	///< number of different styles (max. nesting)
} NMEStyle;

/** Context used by NMEAddString */
struct NMEContextStruct
{
	NMEText dest;	///< address of encoded text
	NMEInt destLen;	///< current length of dest
	
	NMEInt destLenUCS16;	///< destLen in UCS16 (16-bit unicode), assuming UTF-8 input
	
	NMEText src;	///< NME source text
	NMEInt srcIndex;	///< index in src[]
	NMEInt srcIndexOffset;	///< offset between srcIndex and index in original nmeText
	NMEInt srcLen;	///< length of source code
	
	NMEInt bufSize;	///< size of dest and src
	
	NMEInt currentIndent;	///< current indenting (0=none, 1=next one, etc.)
	NMEInt col;	///< current column
	
	NMEInt listNum[kMaxNesting];	///< current number or kNMEListNumUL/DT/DD/Indented
	NMEInt nesting;	///< level of list nesting (0 outside)
	
	NMEOutputFormat const *outputFormat;	///< output format strings
	NMEConstText eol;	///< null-terminated string used for end-of-line
	NMEChar ctrlChar;	///< control character for embedded expressions (usually %)
	NMEInt options;	///< kNMEProcessOptDefault or sum of options
	
	NMEInt fontSize;	///< font size of text in points
	
	NMEInt level;	///< level of heading, indenting or list nesting
	NMEInt item;	///< list item number
	NMEInt linkOffset;	///< offset of link/image in src for the current kNMEStyleLink/kNMEStyleImage
	NMEInt linkLength;///< length of link/image in src for the current kNMEStyleLink/kNMEStyleImage
	
	NMEBoolean xref;	///< TRUE if headings should have labels for hyperlink targets
};

/// Set the context level and item number
#define setContext(c, l, i) do { (c).level = l; (c).item = (i) < 0 ? 0 : (i); } while (0)

/** Skip spaces and tabs
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in,out] i index in src
*/
static void skipBlanks(NMEConstText src, NMEInt srcLen, NMEInt *i)
{
	while (*i < srcLen && isBlank(src[*i]))
		(*i)++;
}

/** Execute operator.
	@param[in,out] stack operand stack
	@param[in,out] stackDepth operand stack depth
	@param[in] op operator
*/
static void execOperator(NMEInt stack[], NMEInt *stackDepth, NMEChar op)
{
	switch (op)
	{
		case '+':
			stack[*stackDepth-2] += stack[*stackDepth-1];
			break;
		case '-':
			stack[*stackDepth-2] -= stack[*stackDepth-1];
			break;
		case '*':
			stack[*stackDepth-2] *= stack[*stackDepth-1];
			break;
		case '/':
			stack[*stackDepth-2] /= stack[*stackDepth-1];
			break;
		case '=':
			stack[*stackDepth-2] = stack[*stackDepth-2] == stack[*stackDepth-1];
			break;
		case '!':
			stack[*stackDepth-2] = stack[*stackDepth-2] != stack[*stackDepth-1];
			break;
		case '<':
			stack[*stackDepth-2] = stack[*stackDepth-2] < stack[*stackDepth-1];
			break;
		case '>':
			stack[*stackDepth-2] = stack[*stackDepth-2] > stack[*stackDepth-1];
			break;
		case '&':
			if (stack[*stackDepth-2] != 0)
				stack[*stackDepth-2] = stack[*stackDepth-1];
			break;
		case '|':
			if (stack[*stackDepth-2] == 0)
				stack[*stackDepth-2] = stack[*stackDepth-1];
			break;
	}
	(*stackDepth)--;
}

/** Evaluate expression (Are supported: + - * / = !(ne) > < & |, parenthesis,
	integers, l=level, i=item, s=size).
	@param[in] src source code
	@param[in] srcLen length of source code
	@param[in] context context for variables
	@return result, or 1 if error
*/
static NMEInt evalExpression(NMEConstText src, NMEInt srcLen,
		NMEContext const *context)
{
#define kExprStackSize 16	///< size of operand and operator stacks
#define kExprErrorValue 1	///< default value if error
	static struct
	{
		NMEChar op;	///< operator character (nul for end of list)
		NMEInt priority;	///< operator priority
	} const opList[] =
	{
		{'|', 1},
		{'&', 2},
		{'=', 3}, {'!', 3}, {'<', 3}, {'>', 3},
		{'+', 4}, {'-', 4},
		{'*', 5}, {'/', 5},
		{'\0', 0}
	};
	NMEInt stack[kExprStackSize];	// stack of operands (intermediate results)
	NMEInt stackDepth;
	NMEInt opStack[kExprStackSize];
		// stack of waiting operators (index in opList, -1=parenthesis)
	NMEInt opStackDepth;
	NMEInt i, j;
	
	for (i = stackDepth = opStackDepth = 0; ; )
	{
		skipBlanks(src, srcLen, &i);
		
		if (i >= srcLen)
			return 1;	// unexpected end of expression
		
		// decode opening parenthesis
		while (i < srcLen && src[i] == '(' && opStackDepth < kExprStackSize)
		{
			opStack[opStackDepth++] = -1;
			i++;
			skipBlanks(src, srcLen, &i);
		}
		
		// decode number or variable
		if (isDigit(src[i]))
			for (stack[stackDepth] = 0; i < srcLen && isDigit(src[i]); i++)
				stack[stackDepth] = 10 * stack[stackDepth] + src[i] - '0';
		else
			switch (src[i++])
			{
				case 'l':
					stack[stackDepth] = context->level;
					break;
				case 'i':
					stack[stackDepth] = context->item;
					break;
				case 's':
					stack[stackDepth] = context->fontSize;
					break;
				case 'o':
					stack[stackDepth] = context->srcIndex;
					break;
				case 'p':
					stack[stackDepth] = context->destLen;
					break;
				case 'x':
					stack[stackDepth] = context->xref;
					break;
				default:
					if (src[i - 1] >= 'A' && src[i - 1] <= 'Z')
					{
						// custom variable
						if (context->outputFormat->getVarFun)
							stack[stackDepth]
									= context->outputFormat->getVarFun(src[i - 1],
											context->outputFormat->getVarData);
						else
							stack[stackDepth] = 0;
					}
					else
						return kExprErrorValue;	// unknown variable, or garbage
			}
		stackDepth++;
		
		skipBlanks(src, srcLen, &i);
		
		// stop if end of expression
		if (i >= srcLen)
		{
			// flush all waiting operators, including parenthesis not closed
			for ( ; opStackDepth > 0; opStackDepth--)
				if (opStack[opStackDepth - 1] >= 0)
					execOperator(stack, &stackDepth,
							opList[opStack[opStackDepth - 1]].op);
			return stack[0];
		}
		
		// decode closing parenthesis (those without matching opening parenthesis are ignored)
		while (i < srcLen && src[i] == ')')
		{
			// flush waiting operators until matching opening parenthesis
			for ( ; opStackDepth > 0 && opStack[opStackDepth - 1] >= 0; opStackDepth--)
				execOperator(stack, &stackDepth,
						opList[opStack[opStackDepth - 1]].op);
			if (opStackDepth > 0)
				opStackDepth--;	// opening parenthesis
			
			// skip parenthesis and spaces in source
			i++;
			skipBlanks(src, srcLen, &i);
		}
		
		// decode operator
		for (j = 0; opList[j].op && src[i] != opList[j].op; j++)
			;
		if (!opList[j].op)
			return kExprErrorValue;	// unknown operator, or garbage
		
		// execute operators with higher priority
		for ( ; opStackDepth > 0
					&& opStack[opStackDepth - 1] >= 0
					&& opList[j].priority <= opList[opStack[opStackDepth - 1]].priority;
				opStackDepth--)
			execOperator(stack, &stackDepth, opList[opStack[opStackDepth - 1]].op);
		
		// give up if op stack is full
		if (opStackDepth >= kExprStackSize)
			return kExprErrorValue;
		
		// store operator in opStack
		opStack[opStackDepth++] = j;
		
		// skip operator in source
		i++;
	}
}

NMEBoolean NMEAddString(NMEConstText str,
		NMEInt strLen,
		NMEChar ctrlChar,
		NMEContext *context)
{
	NMEInt k;
	
	if (!str)
		return TRUE;	// no op if str is NULL
	
	if (strLen < 0)
		for (strLen = 0; str[strLen]; strLen++)
			;
	
	for (k = 0; k < strLen; )
		if (context->destLen
					+ (str[k] == '\n' ? (context->eol[1] ? 1 : 0)
						+ context->currentIndent : 0)
				>= context->bufSize)
			return FALSE;
		else if (str[k] == '\n')
		{
			NMEInt i;
			
			context->dest[context->destLen++] = context->eol[0];
			context->destLenUCS16++;
			if (context->eol[1])
			{
				context->dest[context->destLen++] = context->eol[1];
				context->destLenUCS16++;
			}
			k++;
			for (i = 0; i < context->currentIndent; i++)
			{
				context->dest[context->destLen++] = ' ';
				context->destLenUCS16++;
			}
			context->col = context->currentIndent;
		}
		else if (k + 2 < strLen
				&& str[k] == ctrlChar
				&& (str[k + 1] == '{' || str[k + 1] == ctrlChar && str[k + 2] == '{'))
		{
			NMEBoolean replicate;
			NMEText repStr;
			NMEInt len, result, i, repStrLen, col0, destLenUCS160;
			
			replicate = str[k + 1] == ctrlChar;
			if (replicate)
				k++;
			for (k += 2, len = 0; k + len < strLen && str[k + len] != '}'; len++)
				;
			if (k + len >= strLen)	// unexpected end of string
				return TRUE;	// don't report error (enough space)
			result = evalExpression(str + k, len, context);
			k += len + 1;	// skip after }
			if (replicate)
			{
				// find string, until double-ctrlChar
				for (len = 0;
						k + len + 1 < strLen
							&& (str[k + len] != ctrlChar
									|| str[k + len + 1] != ctrlChar);
						len++)
					;
				// copy once, evaluating expressions
				// (cannot have more recursive calls, because they occur only when
				// the string contains double-ctrlChar which cannot happen here)
				repStr = context->dest + context->destLen;	// repl. string after expr substitutions
				destLenUCS160 = context->destLenUCS16;
				col0 = context->col;
				if (!NMEAddString(str + k, len, context->ctrlChar, context))
					return FALSE;
				repStrLen = context->dest + context->destLen - repStr;
				context->destLen = repStr - context->dest;
				context->destLenUCS16 = destLenUCS160;
				context->col = col0;
				
				// copy result times dest[*destLen..*destLen+len-1]
				if (result > 100)
					result = 100;	// avoid overflows
				if (result > 0 && context->destLen + result * repStrLen > context->bufSize)
					return FALSE;
				for (; result > 0; result--)
				{
					for (i = 0; i < repStrLen; i++)
					{
						context->dest[context->destLen++] = repStr[i];
						if (isFirstUTF8Byte(repStr[i]))
							context->destLenUCS16++;
					}
					context->col += repStrLen;
				}
				// skip rep string and double ctrlChar
				k += len + 2;
			}
			else
			{
				// write result
				if (context->destLen + 12 > context->bufSize)
					return FALSE;
				if (result < 0)
				{
					context->dest[context->destLen++] = '-';
					context->destLenUCS16++;
					result = -result;
					context->col++;
				}
				for (i = 1000000000; i >= 1; i /= 10)
					if (result >= i || i == 1)
					{
						context->dest[context->destLen++] = '0' + (result / i) % 10;
						context->destLenUCS16++;
						context->col++;
					}
			}
		}
		else if (k + 2 < strLen && str[k] == ctrlChar && str[k + 1] == 'L')
		{
			NMEConstText str = NMECurrentListNesting(context);
			NMEInt i;
			
			if (context->destLen + kMaxNesting > context->bufSize)
				return FALSE;
			for (i = 0; str[i]; i++)
			{
				context->dest[context->destLen++] = str[i];
				context->destLenUCS16++;
				context->col++;
			}
			k += 2;
		}
		else
		{
			if (isFirstUTF8Byte(str[k]))
				context->destLenUCS16++;
			context->dest[context->destLen++] = str[k++];
			context->col++;
		}
	
	return TRUE;
}

NMEErr NMECopySource(NMEInt length,
		NMEBoolean copy,
		NMEBoolean encodeChar,
		NMEContext *context)
{
	if (context->srcIndexOffset + context->srcIndex + length > context->srcLen)
		length = context->srcLen - (context->srcIndexOffset + context->srcIndex);
	if (copy)
	{
		NMEInt i;
		NMEErr err;
		
		if (encodeChar && context->outputFormat->encodeCharFun)
		{
			NMEEncodeCharFun fun = context->outputFormat->encodeCharFun;
			void *data = context->outputFormat->encodeCharData;
			
			for (i = context->srcIndex; i < context->srcIndex + length; )
				CheckError(fun(context->src, context->srcLen, &i, context, data));
		}
		else
		{
			NMEConstText s = context->src + context->srcIndex;
			NMEText d = context->dest + context->destLen;
			
			if (context->destLen + length > context->bufSize)
				return kNMEErrNotEnoughMemory;
			
			for (i = 0; i < length; i++)
			{
				d[i] = s[i];
				if (isFirstUTF8Byte(s[i]))
					context->destLenUCS16++;
			}
			context->srcIndex += length;
			context->destLen += length;
		}
	}
	
	context->srcIndex += length;
	context->col = 0;
	return kNMEErrOk;
}

/** Check wordwrap, inserting an end-of-line and spaces for indenting if
	required.
	@param[in,out] context current context
	@param[in] outputFormat format strings (can be NULL)
	@return error code (kNMEErrOk for success)
*/
static NMEErr checkWordwrap(NMEContext *context,
		NMEOutputFormat const *outputFormat)
{
	if (outputFormat && outputFormat->textWidth > 0
			&& context->col >= outputFormat->textWidth)
	{
		// current line longer that textWidth: do wordwrap
		
		NMEInt i, j, dist;
		NMEWordwrapPermission perm;
		
		perm = kNMEWordwrapNo;
		if (outputFormat->wordwrapPermFun)
		{
			// find last wordwrap point on current line
			for (i = context->destLen - 1;
					i >= 0 && !isEol(context->dest[i]);
					i--)
			{
				perm = outputFormat->wordwrapPermFun(context->dest, context->destLen, i,
						outputFormat->wordwrapPermData);
				if (perm != kNMEWordwrapNo)
					break;
			}
		}
		else
		{
			// find last space on current line
			for (i = context->destLen - 1;
					i >= 0 && !isEol(context->dest[i]);
					i--)
				if (isBlank(context->dest[i]))
				{
					perm = kNMEWordwrapReplaceChar;
					break;
				}
		}
		
		// ret. if none
		if (perm == kNMEWordwrapNo)
			return kNMEErrOk;
		
		// if eol has two char or spaceBeforeWordWrap, insert enough space
		dist = (context->eol[1] ? 2 : 1)
				+ (perm == kNMEWordwrapReplaceChar ? -1 : 0)
				+ context->currentIndent;
		if (dist > 0)
		{
			if (context->destLen + dist > context->bufSize)
				return kNMEErrNotEnoughMemory;
			for (j = context->destLen - 1; j > i; j--)
				context->dest[j + dist] = context->dest[j];
			context->destLen += dist;
			context->destLenUCS16 += dist;
		}
		
		// insert eol
		if (perm == kNMEWordwrapInsert)
			i++;	// keep character
		context->dest[i++] = context->eol[0];
		if (context->eol[1])
			context->dest[i++] = context->eol[1];
		for (j = 0; j < context->currentIndent; j++)
			context->dest[i++] = ' ';
		context->col = context->destLen - i + context->currentIndent;
	}
	
	return kNMEErrOk;
}

/** Table of character substitutions for HTML or XML */
static NMEEncodeCharDict const htmlCharDict[] =
{
	{'<', "&lt;"},
	{'>', "&gt;"},
	{'"', "&quot;"},
	{'&', "&amp;"},
	{0, NULL}
};

/** Table of character substitutions for LaTeX */
static NMEEncodeCharDict const latexCharDict[] =
{
	{'#', "\\#"},
	{'^', "$\\,\\hat{}\\,$"},
	{'~', "$\\,\\tilde{}\\,$"},
	{'\\', "$\\backslash$"},
	{'|', "$|$"},
	{'\'', "\'{}"},
	{'`', "`{}"},
	{'<', "$<$"},
	{'>', "$>$"},
	{'{', "\\{"},
	{'}', "\\}"},
	{0, NULL}
};

NMEErr NMEEncodeCharFunDict(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context, void *data)
{
	NMEInt i;
	(void)srcLen;
	
	for (i = 0; ((NMEEncodeCharDict const *)data)[i].str; i++)
		if (src[*srcIx] == ((NMEEncodeCharDict const *)data)[i].ch)
		{
			if (!NMEAddString(((NMEEncodeCharDict const *)data)[i].str, -1,
					context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			(*srcIx)++;
			return kNMEErrOk;
		}
	if (!NMEAddString(&src[(*srcIx)++], 1, context->ctrlChar, context))
		return kNMEErrNotEnoughMemory;
	return kNMEErrOk;
}

/** NMEEncodeCharFun function for NME output; characters are left unescaped when
	possible.
	@param[in] src input characters
	@param[in] srcLen size of src in bytes
	@param[in,out] srcIx index in src (updated by one character)
	@param[in,out] context context used to deciding whether to escape
	@param[in,out] data not used (NULL)
	@return error code (kNMEErrOk for success)
	@see NMEOutputFormat
*/
NMEErr encodeCharFunNME(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context, void *data)
{
	NMEInt i;
	NMEConstText output;
	NMEInt outputLength;
	NMEChar prev, str[2];
	static const struct
	{
		NMEChar ch;	///< character which must be escaped
		NMEChar prev;	/**< previous character,
			or '\n' for line beginning (w/ leading spaces), or 0 for any */
	} dict[] =
	{
		{'*', '\n'},	// first nonblank character of the line
		{'#', '\n'},
		{';', '\n'},
		{':', '\n'},
		{'=', '\n'},
		{'-', '\n'},
		{'*', '*'},	// double characters
		{'/', '/'},
		{'#', '#'},
		{'_', '_'},
		{'^', '^'},
		{',', ','},
		{'[', '['},
		{']', ']'},
		{'{', '{'},
		{'}', '}'},
		{'<', '<'},
		{'>', '>'},
		{'\\', '\\'},
		{'~', '\0'},	// other
		{'|', '\0'},	// other
		{0, 0}
	};
	(void)srcLen;
	(void)data;
	
	// get preceding character
	NMECurrentOutput(context, &output, &outputLength);
	prev = outputLength > 0 ? output[outputLength - 1] : '\n';
	for (i = outputLength - 1; i >= 0 && isBlank(output[i] == ' '); i--)
		;
	if (i < 0 || isEol(output[i]))
		prev = '\n';
	
	// look for a matching entry in dict[]
	for (i = 0; dict[i].ch; i++)
		if (src[*srcIx] == dict[i].ch && (!dict[i].prev || prev == dict[i].prev))
		{
			str[0] = '~';
			str[1] = dict[i].ch;
			if (!NMEAddString(str, 2, '\0', context))
				return kNMEErrNotEnoughMemory;
			(*srcIx)++;
			return kNMEErrOk;
		}
	if (!NMEAddString(&src[(*srcIx)++], 1, '\0', context))
		return kNMEErrNotEnoughMemory;
	return kNMEErrOk;
}

NMEOutputFormat const NMEOutputFormatText =
{
	" ",	// space
	3,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"", "",	// doc
	4,	// highest heading level
	"%%{4-l} %%%%{i>0}%{i}. %%", "\n\n",	// heading
	"", "\n\n",	// par
	"\n",	// line break
	"", "\n",	// pre
	"", "\n",	// pre line
	"", "%%{l=1}\n%%",	// UL
	"%%{3*l-2} %%- ", "\n",	// UL line
	"", "%%{l=1}\n%%",	// OL
	"%%{3*l-3} %%%{i}. ", "\n",	// OL line
	"", "%%{l=1}\n%%",	// DL
	"%%{3*l-3} %%", "\n",	// DT
	NULL,	// emptyDT
	"%%{3*l-1} %%", "\n",	// DD
	"", "%%{l=1}\n%%",	// indented section
	"%%{3*l} %%", "\n",	// indented par
	"", "\n",	// table
	"", "\n",	// table row
	"", "\t",	// table header cell
	"", "\t",	// table normal cell
	"%%{10}-%%\n\n",	// hr
	"", "",	// bold
	"", "",	// italic
	"", "",	// underline
	"", "",	// superscript
	"", "",	// subscript
	"", "",	// monospace
	"", "", NULL, FALSE,	// link
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwikis
	NULL, NULL,	// encodeURLFun
	NULL, NULL,	// char encoder
	NULL, NULL,	// char pre encoder
	70, NULL, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

NMEOutputFormat const NMEOutputFormatTextCompact =
{
	" ",	// space
	3,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"", "",	// doc
	4,	// highest heading level
	"%%{p>0}\n%%%%{4-l} %%%%{i>0}%{i}. %%", "\n",	// heading
	"", "\n",	// par
	"\n",	// line break
	"", "",	// pre
	"", "\n",	// pre line
	"", "%%{l=1}%%",	// UL
	"%%{3*l-2} %%- ", "\n",	// UL line
	"", "%%{l=1}%%",	// OL
	"%%{3*l-3} %%%{i}. ", "\n",	// OL line
	"", "%%{l=1}%%",	// DL
	"%%{3*l-3} %%", "\n",	// DT
	NULL,	// emptyDT
	"%%{3*l-1} %%", "\n",	// DD
	"", "%%{l=1}%%",	// indented section
	"%%{3*l} %%", "\n",	// indented par
	"", "",	// table
	"", "\n",	// table row
	"", "\t",	// table header cell
	"", "\t",	// table normal cell
	"%%{10}-%%\n",	// hr
	"", "",	// bold
	"", "",	// italic
	"", "",	// underline
	"", "",	// superscript
	"", "",	// subscript
	"", "",	// monospace
	"", "", NULL, FALSE,	// link
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwikis
	NULL, NULL,	// encodeURLFun
	NULL, NULL,	// char encoder
	NULL, NULL,	// char pre encoder
	70, NULL, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

/** NMEEncodeURLFun function which encodes link to a URL for null output,
	producing no output.
	@param[in] link input characters (ignored)
	@param[in] linkLen length of link (ignored)
	@param[in,out] context current context (ignored)
	@param[in,out] data value specific to the callback (ignored)
	@return error code (always kNMEErrOk)
*/
static NMEErr encodeURLFunNull(NMEConstText link, NMEInt linkLen,
		NMEContext *context, void *data)
{
	return kNMEErrOk;
}

/** NMEEncodeCharFun function which encodes characters for null output, producing
	no output; the encodeCharData field is ignored (can be set to NULL).
	@param[in] src input characters
	@param[in] srcLen size of src in bytes
	@param[in,out] srcIx index in src (should be updated by one character)
	@param[in,out] context current context
	@param[in,out] data ignored 
	@return error code (kNMEErrOk for success)
	@see NMEOutputFormat
*/
static NMEErr encodeCharFunNull(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context,
		void *data)
{
	(*srcIx)++;
	return kNMEErrOk;
}

NMEOutputFormat const NMEOutputFormatNull =
{
	"",	// space
	0,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"", "",	// doc
	4,	// highest heading level
	"", "",	// heading
	"", "",	// par
	"",	// line break
	"", "",	// pre
	"", "",	// pre line
	"", "",	// UL
	"", "",	// UL line
	"", "",	// OL
	"", "",	// OL line
	"", "",	// DL
	"", "",	// DT
	NULL,	// emptyDT
	"", "",	// DD
	"", "",	// indented section
	"", "",	// indented par
	"", "",	// table
	"", "",	// table row
	"", "",	// table header cell
	"", "",	// table normal cell
	"",	// hr
	"", "",	// bold
	"", "",	// italic
	"", "",	// underline
	"", "",	// superscript
	"", "",	// subscript
	"", "",	// monospace
	"", "", NULL, FALSE,	// link
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwikis
	encodeURLFunNull, NULL,	// encodeURLFun
	encodeCharFunNull, NULL,	// char encoder
	encodeCharFunNull, NULL,	// char pre encoder
	-1, NULL, NULL,	// no wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

/** NMEWordwrapCheckFun function to check valid wordwrap point for NME
	(don't break line before stars, sharps, eq, pipes)
	@param[in] txt output text
	@param[in] len length of output text in bytes
	@param[in] i line break to check (space to be removed or before char to
	push to next line)
	@param[in,out] data value specific to the callback
	@return wordwrap kind (kNMEWordwrapNo if not permitted here)
*/
static NMEWordwrapPermission wordwrapCheckNMEFun(NMEConstText txt,
		NMEInt len, NMEInt i,
		void *data)
{
	(void)data;
	
	return txt[i] == ' ' && i + 1 < len
				&& txt[i + 1] != '*'
				&& txt[i + 1] != '#'
				&& txt[i + 1] != '|'
				&& txt[i + 1] != '='
			? kNMEWordwrapReplaceChar
			: kNMEWordwrapNo;
}

NMEOutputFormat const NMEOutputFormatNME =
{
	" ",	// space
	0,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"", "",	// doc
	6,	// highest heading level
	"%%{l}=%%", "%%{l}=%%\n",	// heading
	"", "\n\n",	// par
	"\\\\",	// line break
	"{{{\n", "}}}\n\n",	// pre
	"", "\n",	// pre line
	"", "%%{l=1}\n%%",	// UL
	"%L ", "\n",	// UL line
	"", "%%{l=1}\n%%",	// OL
	"%L ", "\n",	// OL line
	"", "%%{l=1}\n%%",	// DL
	"%L ", "\n",	// DT
	NULL,	// emptyDT
	"%L ", "\n",	// DD
	"", "%%{l=1}\n%%",	// indented section
	"%%{l}:%% ", "\n",	// indented par
	"", "\n",	// table
	"", "\n",	// table row
	"|=", "",	// table header cell
	"|", "",	// table normal cell
	"----\n\n",	// hr
	"**", "**",	// bold
	"//", "//",	// italic
	"__", "__",	// underline
	"^^", "^^",	// superscript
	",,", ",,",	// subscript
	"##", "##",	// monospace
	"[[", "]]", "|", FALSE,	// URL
	"{{", "}}", "|", FALSE, FALSE,	// image
	NULL,	// interwiki
	NULL, NULL,	// encodeURLFun
	encodeCharFunNME, NULL,	// char encoder
	NULL, NULL,	// char pre encoder
	70, wordwrapCheckNMEFun, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

NMEOutputFormat const NMEOutputFormatHTML =
{
	" ",	// space
	2,	// indentSpaces
	0,	// defFontSize
	'%',	// ctrlChar
	"<!-- Generated by Nyctergatis Markup Engine, "
			__DATE__ " " __TIME__ " -->\n"
	"<html><body>\n", "</body></html>\n",	// doc
	4,	// highest heading level
	"<h%{l}%%{s>0} style=\"font-size:%{l=1&3*s|l=2&2*s|l=3&3*s/2|5*s/4}pt\"%%>"
			"%%{x}<a name=\"h%{o}\">%%"
			"%%{i>0}%{i}. %%",
		"%%{x}</a>%%"
			"</h%{l}>\n",	// heading
	"<p%%{s>0} style=\"font-size:%{s}pt\"%%>", "</p>\n",	// par
	"<br />",	// line break
	"<pre%%{s>0} style=\"font-size:%{s}pt\"%%>\n", "</pre>\n",	// pre
	"", "\n",	// pre line
	"<ul>\n", "</ul>\n",	// UL
	"<li%%{s>0} style=\"font-size:%{s}pt\"%%>", "</li>\n",	// UL line
	"<ol>\n", "</ol>\n",	// OL
	"<li%%{s>0} style=\"font-size:%{s}pt\"%%>", "</li>\n",	// OL line
	"<dl>\n", "</dl>\n",	// DL
	"<dt%%{s>0} style=\"font-size:%{s}pt\"%%>", "</dt>\n",	// DT
	NULL,	// emptyDT
	"<dd%%{s>0} style=\"font-size:%{s}pt\"%%>", "</dd>\n",	// DD
	"<div style=\"margin-left:2em%%{s>0}; font-size:%{s}pt%%\">\n",
		"</div>\n",	// indented section
	"<p%%{s>0} style=\"font-size:%{s}pt\"%%>", "</p>\n",	// indented par
	"<table>\n", "</table>\n",	// table
	"<tr>", "</tr>\n",	// table row
	"<th%%{s>0} style=\"font-size:%{s}pt\"%%>", "</th>\n",	// table header cell
	"<td%%{s>0} style=\"font-size:%{s}pt\"%%>", "</td>\n",	// table normal cell
	"<hr />\n",	// hr
	"<b>", "</b>",	// bold
	"<i>", "</i>",	// italic
	"<u>", "</u>",	// underline
	"<sup>", "</sup>",	// superscript
	"<sub>", "</sub>",	// subscript
	"<tt>", "</tt>",	// monospace
	"<a href=\"", "</a>", "\">", FALSE,	// URL
	"<img src=\"", "\" />", "\" alt=\"", FALSE, TRUE,	// image
	NULL,	// interwiki
	NULL, NULL,	// encodeURLFun
	NMEEncodeCharFunDict, (void *)htmlCharDict,	// char encoder
	NMEEncodeCharFunDict, (void *)htmlCharDict,	// char pre encoder
	70, NULL, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

/** NMEEncodeCharFun function which encodes characters for RTF. Special
	RTF characters (backslash and braces) are escaped and multibyte
	characters are written in hexadecimal. Its address should be
	stored in the encodeCharFun field of NMEOutputFormat; the encodeCharData
	field is ignored (can be set to NULL).
	@param[in] src input characters
	@param[in] srcLen size of src in bytes
	@param[in,out] srcIx index in src (should be updated by one character)
	@param[in,out] context current context
	@param[in,out] data ignored 
	@return error code (kNMEErrOk for success)
	@see NMEOutputFormat
*/
static NMEErr encodeCharRTFFun(NMEConstText src, NMEInt srcLen, NMEInt *srcIx,
		NMEContext *context, void *data)
{
	NMEInt ch, i;
	(void)data;
	
	if ((src[*srcIx] & 0x80) == 0)	// one byte, ASCII
	{
		if (context->destLen + 1 >= context->bufSize)
			return kNMEErrNotEnoughMemory;
		if (src[*srcIx] == '\\' || src[*srcIx] == '{' || src[*srcIx] == '}')
		{
			context->dest[context->destLen++] = '\\';	// \, { and } must be escaped
			context->destLenUCS16++;
		}
		context->dest[context->destLen++] = src[(*srcIx)++];
		context->destLenUCS16++;
		return kNMEErrOk;
	}
	else if (*srcIx + 1 < srcLen && (src[*srcIx] & 0xe0) == 0xc0	// two bytes
			&& (src[*srcIx + 1] & 0xc0) == 0x80)
	{
		ch = (((NMEInt)src[*srcIx] & 0x1f) << 6) | (src[*srcIx + 1] & 0x3f);
		*srcIx += 2;
	}
	else if (*srcIx + 2 < srcLen && (src[*srcIx] & 0xf0) == 0xe0	// three bytes
			&& (src[*srcIx + 1] & 0xc0) == 0x80 && (src[*srcIx + 2] & 0xc0) == 0x80)
	{
		ch = ((NMEInt)src[*srcIx] << 12) | (((NMEInt)src[*srcIx + 1] & 0x3f) << 6)
				| src[*srcIx + 2] & 0x3f;
		*srcIx += 3;
	}
	else
	{
		*srcIx += 1;	// ignore byte
		return kNMEErrOk;
	}
	
	if (context->destLen + 9 >= context->bufSize)	// worst case (\u-30000?)
		return kNMEErrNotEnoughMemory;
	
	// unsigned -> signed
	if (ch >= 32768)
		ch -= 65536;
	
	context->dest[context->destLen++] = '\\';
	context->dest[context->destLen++] = 'u';
	if (ch < 0)
	{
		context->dest[context->destLen++] = '-';
		context->destLenUCS16++;
		ch = -ch;
	}
	for (i = 1; i < ch; i *= 10)
		;
	for (i /= 10; i > 0; i /= 10)
	{
		context->dest[context->destLen++] = '0' + (ch / i) % 10;
		context->destLenUCS16++;
	}
	context->dest[context->destLen++] = '?';	// ANSI representation
	context->destLenUCS16 += 3;	// \u?
	
	return kNMEErrOk;
}

/** NMEEncodeURLFun function which encodes link to a URL for RTF output.
	@param[in] link input characters
	@param[in] linkLen length of link
	@param[in,out] context current context
	@param[in,out] data value specific to the callback (ignored)
	@return error code
*/
static NMEErr encodeURLFunRTF(NMEConstText link, NMEInt linkLen,
		NMEContext *context, void *data)
{
	NMEInt i;
	NMEErr err;
	(void)data;
	
	// encode each character with encodeCharRTFFun 
	for (i = 0; i < linkLen; )
		CheckError(encodeCharRTFFun(link, linkLen, &i, context, NULL));
	return kNMEErrOk;
}

/** NMEWordwrapCheckFun function to check valid wordwrap point for RTF (keep space)
	@param[in] txt output text
	@param[in] len length of output text in bytes
	@param[in] i line break to check (space to be removed or before char to
	push to next line)
	@param[in,out] data value specific to the callback
	@return wordwrap kind (kNMEWordwrapNo if not permitted here)
*/
static NMEWordwrapPermission wordwrapCheckRTFFun(NMEConstText txt,
		NMEInt len, NMEInt i,
		void *data)
{
	(void)data;
	
	return txt[i] == ' ' ? kNMEWordwrapInsert : kNMEWordwrapNo;
}

NMEOutputFormat const NMEOutputFormatRTF =
{
#define SIZE "%{2*s}"	///< size of text
#define SIZEH "%{l=1&3*s|l=2&5*s/2|l=3&2*s|3*s/2}"	///< size of heading
	" ",	// space
	0,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"{\\rtf1\\ansi\\deff0"
			"{\\fonttbl"
				"{\\f0\\froman Times;}"
				"{\\f1\\fswiss Helvetica;}"
				"{\\f2\\fmodern Courier;}"
			"}\n",	// beginDoc
		"\n}\n",	// endDoc
	4,	// highest heading level
	"{\\pard\\sb%{500-100*l}\\li60\\sa40%%{l=1}\\qc%%\\f1"
			"\\fs" SIZEH "%%{l!2}\\b%% %%{i>0}%{i}. %%",	// beginHeading
		"\\par}\n",	// endHeading
	"{\\pard\\sb80\\li60\\qj\\fi160\\f0\\fs" SIZE " ",	// beginPar
		"\\par}\n",	// endPar
	"\\line ",	// line break
	"{\\pard\\sb80\\li160\\f2\\fs" SIZE " ",	// beginPre
		"}\n",	// endPre
	"",	// beginPreLine
		"\\par\n",	// endPreLine
	"",
		"",	// UL
	"{\\pard\\sb80\\li%{60+100*l}\\qj\\fi160\\f0\\fs" SIZE " * ",
		"\\par}\n",	// UL line
	"",
		"",	// OL
	"{\\pard\\sb80\\li%{60+100*l}\\qj\\fi160\\f0\\fs" SIZE " %{i}",
		"\\par}\n",	// OL line
	"", "",	// DL
	"{\\pard\\sb80\\li%{60+100*l}\\qj\\f0\\fs" SIZE "\\i ",
		"\\par}\n",	// DT
	NULL,	// emptyDT
	"{\\pard\\sb80\\qj\\fi160\\f0\\fs" SIZE "\\li320 ",
		"\\par}\n",	// DD
	"", "",	// indented section
	"{\\pard\\sb80\\li%{60+100*l}\\qj\\fi160\\f0\\fs" SIZE " ",
		"\\par}\n",	// indented par
	"{\\par\\li60 ", "\\pard}\n",	// table
	"\\trowd\\trautofit1 ", "\\row\n",	// table row
	"\\pard\\intbl\\sb80\\qc\\fi160\\f0\\fs" SIZE " {\\b ",
		"}\\cell\n",	// table header cell
	"\\pard\\intbl\\sb80\\qj\\fi160\\f0\\fs" SIZE " ",
		"\\cell\n",	// table normal cell
	"\\hrule\n",	// hr
	"{\\b ",
		"}",	// bold
	"{\\i ",
		"}",	// italic
	"{\\ul ",
		"}",	// underline
	"{\\super ",
		"}",	// superscript
	"{\\sub ",
		"}",	// subscript
	"{\\f2 ",
		"}",	// monospace
	"{\\field{\\*\\fldinst{HYPERLINK \"", "}}", "\"}}{\\fldrslt ", FALSE,	// URL
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwiki
	encodeURLFunRTF, NULL,	// encodeURLFun
	encodeCharRTFFun, NULL,	// char encoder
	encodeCharRTFFun, NULL,	// char pre encoder
	70, wordwrapCheckRTFFun, NULL,	// wordwrap (keep space)
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
#undef SIZE
#undef SIZEH
};

NMEOutputFormat const NMEOutputFormatLaTeX =
{
	" ",	// space
	2,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	"\\documentclass[%{s}pt]{article}\n"
	"\\usepackage{hyperref}\n"
	"\\begin{document}\n",	// beginDoc
		"\n\\end{document}\n",	// endDoc
	4,	// highest heading level
	"\n\\%%{l>3&2|l-1}sub%%section%%{l>3|i<1}*%%{",	// beginHeading
		"}\n",	// endHeading
	"\n",	// beginPar
		"\n",	// endPar
	"\\\\",	// line break
	"\n\\begin{verbatim}\n",	// beginPre
		"\\end{verbatim}\n",	// endPre
	"",	// beginPreLine
		"\n",	// endPreLine
	"\\begin{itemize}\n",
		"\\end{itemize}\n",	// UL
	"\\item ",
		"\n",	// UL line
	"\\begin{itemize}\n",
		"\\end{itemize}\n",	// OL
	"\\item[%{i}] ",
		"\n",	// OL line
	"\\begin{itemize}\n",
		"\\end{itemize}\n",	// DL
	"\\item[] {\\bf ", "} \\hspace{1em} ",	// DT
	NULL,	// emptyDT
	"\n", "\n",	// DD
	"\\begin{itemize}\n", "\\end{itemize}\n",	// indented section
	"\\item[] ", "\n",	// indented par
	"\\begin{tabular}{llllllllllllllll}\n", "\\end{tabular}\n",	// table
	"", "\\\\\n",	// table row
	"{\\bf ", "} & ",	// table header cell
	"", " & ",	// table normal cell
	"",	// hr
	"{\\bfseries ",
		"}",	// bold
	"{\\itshape ",
		"}",	// italic
	"\\underline{",
		"}",	// underline
	"\\textsuperscript{",
		"}",	// superscript
	"\\ensuremath{_{\\mbox{",
		"}}}",	// subscript
	"{\\ttfamily ",
		"}",	// monospace
	"\\href{", "}", "}{", FALSE,	// URL
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwiki
	NULL, NULL,	// encodeURLFun
	NMEEncodeCharFunDict, (void *)latexCharDict,	// char encoder
	NULL, NULL,	// char pre encoder
	70, NULL, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

NMEOutputFormat const NMEOutputFormatMan =
{
	" ",	// space
	0,	// indentSpaces
	10,	// defFontSize
	'%',	// ctrlChar
	".TH title 1\n", "",	// doc
	2,	// highest heading level
	"%%{l=1}.SH%%%%{l>1}.SS%% ", "\n",	// heading
	".P\n", "\n",	// par
	"",	// line break
	"", "",	// pre
	" ", "\n",	// pre line
	"", "",	// UL
	".IP *\n", "\n",	// UL line
	"", "",	// OL
	".IP %{i}\n", "\n",	// OL line
	"", "",	// DL
	".IP ", "\n",	// DT
	NULL,	// emptyDT
	"", "\n",	// DD
	"", "",	// indented section
	"\n.P\n", "\n",	// indented par
	"", "",	// table
	"", "\n",	// table row
	"", " ",	// table header cell
	"", " ",	// table normal cell
	"\n",	// hr
	"\n.B ", "\n",	// bold
	"\n.I ", "\n",	// italic
	"", "",	// underline
	"", "",	// superscript
	"", "",	// subscript
	"", "",	// monospace
	"", "", NULL, FALSE,	// URL
	"", "", NULL, FALSE, FALSE,	// image
	NULL,	// interwiki
	NULL, NULL,	// encodeURLFun
	NULL, NULL,	// char encoder
	NULL, NULL,	// char pre encoder
	70, NULL, NULL,	// wordwrap
	NULL, NULL,	// char hook
	NULL, NULL, NULL, NULL,	// process hooks
	NULL,	// plugins
	NULL,	// autoconverts
	NULL, NULL	// getVar
};

/** Add link to dest, substituting interwiki if necessary.
	@param[in,out] context current context where link position and source is stored
	@param[in] outputFormat format strings, or NULL for default
	@return error code (kNMEErrOk for success)
*/
static NMEErr addLink(NMEContext *context,
		NMEOutputFormat const *outputFormat)
{
	NMEInt i;
	NMEConstText link = context->src + context->linkOffset;
	NMEInt linkLen = context->linkLength;
	NMEErr err;
	
	if (outputFormat->interwikis)
	{
		NMEInt iw;
		
		// find interwiki whose alias matches the beginning of link
		for (iw = 0; outputFormat->interwikis[iw].alias; iw++)
		{
			for (i = 0;
					i < linkLen
						&& outputFormat->interwikis[iw].alias[i] != '\0'
						&& link[i] == outputFormat->interwikis[iw].alias[i];
					i++)
				;
			if (outputFormat->interwikis[iw].alias[i] == '\0')
			{
				// found
				if (!NMEAddString(outputFormat->interwikis[iw].urlPrefix, -1,
							context->ctrlChar, context))
					return kNMEErrNotEnoughMemory;
				CheckError(checkWordwrap(context, outputFormat));
				link += i;
				linkLen -= i;
				break;
			}
		}
	}
	
	// write (remaining) part of link using encodeURLFun if defined
	if (outputFormat->encodeURLFun)
		CheckError(outputFormat->encodeURLFun(link, linkLen, context,
				outputFormat->encodeURLData));
	else
	{
		// copy link as is
		if (context->destLen + linkLen > context->bufSize)
			return kNMEErrNotEnoughMemory;
		for (i = 0; i < linkLen; i++)
		{
			context->dest[context->destLen++] = link[i];
			context->destLenUCS16++;
		}
	}
	
	return kNMEErrOk;
}

/** Find style in stack of styles.
	@param[in] styleStack stack of styles
	@param[in] styleNesting number of styles in stack
	@param[in] style style to find
	@param[out] i set to index of index matching style if found and i not NULL
	@return TRUE if found, FALSE if not found
*/
static NMEBoolean findStyleInStyleStack(
		NMEStyle const styleStack[],
		NMEInt styleNesting,
		NMEStyle style,
		NMEInt *i)
{
	NMEInt j;
	
	for (j = 0; j < styleNesting; j++)
		if (styleStack[j] == style)
		{
			if (i)
				*i = j;
			return TRUE;
		}
	return FALSE;
}

/** Get style marker string associated to a style constant.
	@param[in] style style
	@return style marker string, or NULL for nonbasic style
*/
static NMEConstText styleMarkerFromStyleID(NMEStyle style)
{
	switch (style)
	{
		case kNMEStyleBold:
			return "**";
		case kNMEStyleItalic:
			return "//";
		case kNMEStyleUnderline:
			return "__";
		case kNMEStyleSuperscript:
			return "^^";
		case kNMEStyleSubscript:
			return ",,";
		case kNMEStyleMonospace:
			return "##";
		case kNMEStyleLink:
			return "[[";
		case kNMEStyleImage:
			return "{{";
		default:
			return NULL;
	}
}

/** Process style tag.
	@param[in,out] styleStack stack of styles
	@param[in,out] styleNesting number of styles in stack
	@param[in] style new style (not kNMEStyleLink or kNMEStyleImage)
	@param[in] i0 index in src of beginning of tag
	@param[in] outputFormat format strings
	@param[in,out] context current context
	@return error code (kNMEErrOk for success)
*/
static NMEErr processStyleTag(
		NMEStyle styleStack[/* kNMEStylesCount */],
		NMEInt *styleNesting,
		NMEStyle style,
		NMEInt i0,
		NMEOutputFormat const *outputFormat,
		NMEContext *context)
{
	NMEInt i, j;
	NMEErr err;
	
#define HOOK(e, st) \
	do { \
		if (outputFormat->spanHookFun) \
		{ \
			NMEConstText styleStr = styleMarkerFromStyleID(st); \
			if (styleStr) \
				CheckError(outputFormat->spanHookFun(kNMEHookLevelSpan, 0, e, styleStr, \
						i0 + context->srcIndexOffset, \
						context, \
						outputFormat->hookData)); \
		} \
	} while (0)
	
	// check if s is in styleStack
	if (findStyleInStyleStack(styleStack, *styleNesting, style, &i))
	{
		// remove it as well as all badly nested styles
		for (j = *styleNesting - 1; j >= i; j--)
			if (styleStack[j] != kNMEStyleVerbatim
					|| context->options & kNMEProcessOptVerbatimMono
						&& !findStyleInStyleStack(styleStack, *styleNesting,
								kNMEStyleMonospace, NULL))
			{
				// write separator and link if they must be after the text
				if (styleStack[j] == kNMEStyleLink
						&& outputFormat->sepLink && outputFormat->linkAfterSep)
				{
					if (!NMEAddString(outputFormat->sepLink, -1,
								context->ctrlChar, context))
						return kNMEErrNotEnoughMemory;
					CheckError(addLink(context, outputFormat));
					CheckError(checkWordwrap(context, outputFormat));
				}
				else if (styleStack[j] == kNMEStyleImage
						&& outputFormat->sepImage && outputFormat->imageAfterSep)
				{
					if (!NMEAddString(outputFormat->sepImage, -1,
								context->ctrlChar, context))
						return kNMEErrNotEnoughMemory;
					CheckError(addLink(context, outputFormat));
					CheckError(checkWordwrap(context, outputFormat));
				}
				
				// write end tag
				if (!NMEAddString(styleStack[j] == kNMEStyleBold
								? outputFormat->endBold
							: styleStack[j] == kNMEStyleItalic
								? outputFormat->endItalic
							: styleStack[j] == kNMEStyleUnderline
								? outputFormat->endUnderline
							: styleStack[j] == kNMEStyleSuperscript
								? outputFormat->endSuperscript
							: styleStack[j] == kNMEStyleSubscript
								? outputFormat->endSubscript
							: styleStack[j] == kNMEStyleLink
								? outputFormat->endLink
							: styleStack[j] == kNMEStyleImage
								? outputFormat->endImage
								: outputFormat->endCode,
							-1,
							context->ctrlChar, context))
					return kNMEErrNotEnoughMemory;
				CheckError(checkWordwrap(context, outputFormat));
				
				// call hook
				HOOK(FALSE, styleStack[j]);
			}
		// add back badly nested styles, except for kNMEStyleLink or kNMEStyleImage
		for (j = i + 1; j < *styleNesting; j++)
			if (styleStack[j] != kNMEStyleLink && styleStack[j] != kNMEStyleImage)
			{
				HOOK(TRUE, styleStack[j]);
				
				styleStack[i] = styleStack[j];
				if (styleStack[i] != kNMEStyleVerbatim)
				{
					if (!NMEAddString(styleStack[i] == kNMEStyleBold
								? outputFormat->beginBold
							: styleStack[i] == kNMEStyleItalic
								? outputFormat->beginItalic
							: styleStack[i] == kNMEStyleUnderline
								? outputFormat->beginUnderline
							: styleStack[i] == kNMEStyleSuperscript
								? outputFormat->beginSuperscript
							: styleStack[i] == kNMEStyleSubscript
								? outputFormat->beginSubscript
								: outputFormat->beginCode,
							-1,
							context->ctrlChar, context))
						return kNMEErrNotEnoughMemory;
					CheckError(checkWordwrap(context, outputFormat));
				}
				i++;
			}
		*styleNesting -= j - i;
		return kNMEErrOk;
	}
	
	// not found; add it unless it's kNMEStyleLink or kNMEStyleImage
	//  or any style in image alt text if noStyleInAlt
	if (style == kNMEStyleLink || style == kNMEStyleImage
			|| outputFormat->noStyleInAlt
				&& findStyleInStyleStack(styleStack, *styleNesting,
						kNMEStyleImage, NULL))
		return kNMEErrOk;	// ignore
	styleStack[(*styleNesting)++] = style;
	
	// add beginning tag
	if (style == kNMEStyleVerbatim
			&& (!(context->options & kNMEProcessOptVerbatimMono)
				|| findStyleInStyleStack(styleStack, *styleNesting,
								kNMEStyleMonospace, NULL)))
		return kNMEErrOk;
	HOOK(TRUE, style);
	if (!NMEAddString(style == kNMEStyleBold ? outputFormat->beginBold
					: style == kNMEStyleItalic ? outputFormat->beginItalic
					: style == kNMEStyleUnderline ? outputFormat->beginUnderline
					: style == kNMEStyleSuperscript ? outputFormat->beginSuperscript
					: style == kNMEStyleSubscript ? outputFormat->beginSubscript
					: outputFormat->beginCode,
				-1,
				context->ctrlChar, context))
		return kNMEErrNotEnoughMemory;
	CheckError(checkWordwrap(context, outputFormat));
	return kNMEErrOk;

#undef HOOK
}

/** Flush waiting style tags.
	@param[in,out] styleStack stack of styles
	@param[in,out] styleNesting number of styles in stack
	@param[in] i0 index in src of beginning of tag
	@param[in] outputFormat format strings
	@param[in,out] context current context
	@return error code (kNMEErrOk for success)
*/
static NMEErr flushStyleTags(
		NMEStyle styleStack[kNMEStylesCount],
		NMEInt *styleNesting,
		NMEInt i0,
		NMEOutputFormat const *outputFormat,
		NMEContext *context)
{
	NMEInt i;
	NMEErr err;
	
	for (i = *styleNesting - 1; i >= 0; i--)
		if (styleStack[i] != kNMEStyleVerbatim
				|| context->options & kNMEProcessOptVerbatimMono
						&& !findStyleInStyleStack(styleStack, *styleNesting,
								kNMEStyleMonospace, NULL))
		{
			if (styleStack[i] == kNMEStyleLink
					&& outputFormat->sepLink && outputFormat->linkAfterSep)
			{
				if (!NMEAddString(outputFormat->sepLink, -1,
							context->ctrlChar, context))
					return kNMEErrNotEnoughMemory;
				CheckError(addLink(context, outputFormat));
				CheckError(checkWordwrap(context, outputFormat));
			}
			else if (styleStack[i] == kNMEStyleImage
					&& outputFormat->sepImage && outputFormat->imageAfterSep)
			{
				if (!NMEAddString(outputFormat->sepImage, -1,
							context->ctrlChar, context))
					return kNMEErrNotEnoughMemory;
				CheckError(addLink(context, outputFormat));
				CheckError(checkWordwrap(context, outputFormat));
			}
			
			if (!NMEAddString(styleStack[i] == kNMEStyleBold
									? outputFormat->endBold
								: styleStack[i] == kNMEStyleItalic
									? outputFormat->endItalic
								: styleStack[i] == kNMEStyleUnderline
									? outputFormat->endUnderline
								: styleStack[i] == kNMEStyleSuperscript
									? outputFormat->endSuperscript
								: styleStack[i] == kNMEStyleSubscript
									? outputFormat->endSubscript
								: styleStack[i] == kNMEStyleLink
									? outputFormat->endLink
								: styleStack[i] == kNMEStyleImage
									? outputFormat->endImage
								: outputFormat->endCode,
							-1,
							context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
				
			// call hook
			if (outputFormat->spanHookFun)
			{
				NMEConstText styleStr = styleMarkerFromStyleID(styleStack[i]);
				if (styleStr)
					CheckError(outputFormat->spanHookFun(kNMEHookLevelSpan, 0,
							FALSE, styleStr,
							i0 + context->srcIndexOffset,
							context,
							outputFormat->hookData));
			}
		}
	*styleNesting = 0;
	return kNMEErrOk;
}

/** Add an endPar, endIndented, endULItem or endOLItem.
	@param[in] forceEndOfList if TRUE, all current lists are ended
	@param[in] outputFormat format strings
	@param[in,out] context current context
	@return error code (kNMEErrOk for success)
*/
static NMEErr addEndPar(NMEBoolean forceEndOfList,
		NMEOutputFormat const *outputFormat,
		NMEContext *context)
{
	NMEErr err;
	
#define HOOK(cb, l, it, e, m) \
	do { \
		if (outputFormat->cb) \
			CheckError(outputFormat->cb(l, it, e, m, \
					context->srcIndexOffset + context->srcIndex, \
					context, \
					outputFormat->hookData)); \
	} while (0)
	
	if (context->nesting > 0)
	{
		NMEInt level0, item0;
		
		level0 = context->level;
		item0 = context->item;
		setContext(*context, context->nesting, context->listNum[context->nesting - 1]);
		
		// end of item
		if (context->listNum[context->nesting - 1] >= 0)
		{
			context->listNum[context->nesting - 1]++;
			if (!NMEAddString(outputFormat->endOLItem, -1,
						context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
			HOOK(parHookFun, context->level, context->item, FALSE, "#");
		}
		else if (context->listNum[context->nesting - 1] == kNMEListNumTableCell
				|| context->listNum[context->nesting - 1] == kNMEListNumTableHCell)
		{
			if (!NMEAddString(context->listNum[context->nesting - 1] == kNMEListNumTableCell
							? outputFormat->endTableCell
							: outputFormat->endTableHCell,
						-1,
						context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
			HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE,
					context->listNum[context->nesting - 1] == kNMEListNumTableCell ? "|" : "|=");
			if (!NMEAddString(outputFormat->endTableRow, -1,
							context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
		}
		else
		{
			if (!NMEAddString(context->listNum[context->nesting - 1] == kNMEListNumUL
							? outputFormat->endULItem
						: context->listNum[context->nesting - 1] == kNMEListNumDT
							? outputFormat->endDT
						: context->listNum[context->nesting - 1] == kNMEListNumDD
							? outputFormat->endDD
							: outputFormat->endIndentedPar,
						-1,
						context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
			HOOK(parHookFun, context->level, context->item, FALSE,
					context->listNum[context->nesting - 1] == kNMEListNumUL ? "*"
						: context->listNum[context->nesting - 1] == kNMEListNumDT ? ";"
						: context->listNum[context->nesting - 1] == kNMEListNumDD ? ";:"
						: ":");
		}
		// end of list or table
		if (forceEndOfList)
			while (context->nesting > 0)
			{
				setContext(*context, context->nesting, context->listNum[context->nesting - 1]);
				if (!NMEAddString(context->listNum[context->nesting - 1] == kNMEListNumUL
								? outputFormat->endUL
							: context->listNum[context->nesting - 1] == kNMEListNumDT
									|| context->listNum[context->nesting - 1] == kNMEListNumDD
								? outputFormat->endDL
							: context->listNum[context->nesting - 1] == kNMEListIndented
								? outputFormat->endIndented
							: context->listNum[context->nesting - 1] == kNMEListNumTableCell
									|| context->listNum[context->nesting - 1] == kNMEListNumTableHCell
								? outputFormat->endTable
								: outputFormat->endOL,
							-1,
							context->ctrlChar, context))
					return kNMEErrNotEnoughMemory;
				CheckError(checkWordwrap(context, outputFormat));
				HOOK(divHookFun,
						context->listNum[context->nesting - 1] == kNMEListNumTableCell
								|| context->listNum[context->nesting - 1] == kNMEListNumTableHCell
							? kNMEHookLevelPar : context->level,
						0, FALSE,
						context->listNum[context->nesting - 1] == kNMEListNumUL ? "*"
							: context->listNum[context->nesting - 1] == kNMEListNumDT
								|| context->listNum[context->nesting - 1] == kNMEListNumDD ? ";"
							: context->listNum[context->nesting - 1] == kNMEListIndented ? ":"
							: context->listNum[context->nesting - 1] > 0 ? "#"
							: "|");
				context->nesting--;
			}
		
		setContext(*context, level0, item0);
		return kNMEErrOk;
	}
	else
	{
		if (!NMEAddString(outputFormat->endPar, -1, context->ctrlChar, context))
			return kNMEErrNotEnoughMemory;
		CheckError(checkWordwrap(context, outputFormat));
		HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE, "p");
		return kNMEErrOk;
	}
#undef HOOK
}

/** Parse the beginning of a link and leave the parsing point
	at the beginning of the link text.
	@param[in] isImage TRUE for image, FALSE for link
	@param[in,out] styleStack stack of styles
	@param[in,out] styleNesting number of styles in stack
	@param[in] i0 index in src of beginning of tag
	@param[in] outputFormat format strings, or NULL for default
	@param[in,out] context current context (where link position is stored)
	@return error code (kNMEErrOk for success)
*/
static NMEErr addLinkBegin(NMEBoolean isImage,
		NMEStyle styleStack[kNMEStylesCount],
		NMEInt *styleNesting,
		NMEInt i0,
		NMEOutputFormat const *outputFormat,
		NMEContext *context)
{
	NMEInt j, k;
	NMEErr err;
	
	// check if already parsing a link or image
	for (j = 0; j < *styleNesting; j++)
		if (styleStack[j] == kNMEStyleLink && !isImage	// image in links are ok
				|| styleStack[j] == kNMEStyleImage)
			return kNMEErrOk;
	
	// skip spaces
	skipBlanks(context->src, context->srcLen, &context->srcIndex);
	
	// find | or ]]/}}, stopping at double end of line
	for (j = context->srcIndex; j < context->srcLen
				&& (!isEol(context->src[j])
						|| j + 1 < context->srcLen
							&& (!isEol(context->src[j + 1])
								|| context->src[j] == '\r' && context->src[j + 1] =='\n'))
				&& (j + 1 >= context->srcLen
					|| context->src[j] != (isImage ? '}' : ']')
					|| context->src[j + 1] != context->src[j])
				&& context->src[j] != '|';
			j++)
		;
	
	// gobble back trailing spaces and eol
	for (k = j;
			k > context->srcIndex
				&& (isBlank(context->src[k - 1]) || isEol(context->src[k - 1]));
			k--)
		;
	
	// give up if no link
	if (k <= context->srcIndex)
	{
		context->srcIndex = j;
		return kNMEErrOk;
	}
	
	// set link location
	context->linkOffset = context->srcIndex;
	context->linkLength = k - context->srcIndex;
	
	// call hook, if any
	if (outputFormat->spanHookFun)
		CheckError(outputFormat->spanHookFun(kNMEHookLevelSpan, 0, TRUE,
				isImage ? "{{" : "[[",
				i0 + context->srcIndexOffset,
				context,
				outputFormat->hookData));
	
	if (isImage ? outputFormat->sepImage : outputFormat->sepLink)
	{
		// write beginning of link
		if (!NMEAddString(isImage ? outputFormat->beginImage : outputFormat->beginLink, -1,
					context->ctrlChar, context))
			return kNMEErrNotEnoughMemory;
		CheckError(checkWordwrap(context, outputFormat));
		// write link unless linkAfterSep or imageAfterSep
		if (!(isImage ? outputFormat->imageAfterSep : outputFormat->linkAfterSep))
		{
			CheckError(addLink(context, outputFormat));
			if (!NMEAddString(isImage ? outputFormat->sepImage : outputFormat->sepLink, -1,
					context->ctrlChar, context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(context, outputFormat));
		}
	}
	
	// continue with link text or image alt text
	if (j < context->srcLen && context->src[j] == '|')
	{
		context->srcIndex = j + 1;
		skipBlanks(context->src, context->srcLen, &context->srcIndex);
	}
	
	// add link or image "style", which indicates we're expecting an
	//  end-of-link marker
	styleStack[(*styleNesting)++] = isImage ? kNMEStyleImage : kNMEStyleLink;
	
	return kNMEErrOk;
}

/** Find plugin.
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in] i parsing point
	@param[in] isPlaceholder if TRUE, end tag must be triple right angle brackets
	@param[in] outputFormat format strings, or NULL for default
	@return index of plugin in outputFormat->plugins, or -1 if not found
*/
static NMEInt findPlugin(NMEConstText src, NMEInt srcLen, NMEInt i,
		NMEBoolean isPlaceholder,
		NMEOutputFormat const *outputFormat)
{
	NMEConstText name;
	NMEInt nameLen, j, k;
	
	// find name
	skipBlanks(src, srcLen, &i);
	name = src + i;
	for (nameLen = 0;
			i + nameLen < srcLen
					&& !isBlank(src[i + nameLen])
					&& !isEol(src[i + nameLen])
					&& src[i + nameLen] != '>';
			nameLen++)
		;
	
	// find plugin
	if (outputFormat->plugins && nameLen > 0)
		for (j = 0; outputFormat->plugins[j].name; j++)
			if (isPlaceholder
					^ (outputFormat->plugins[j].options & kNMEPluginOptTripleAngleBrackets) == 0)
			{
				// compare plugin name
				for (k = 0; k < nameLen && outputFormat->plugins[j].name[k]; k++)
					if (name[k] != outputFormat->plugins[j].name[k])
						goto continueMainLoop;
				if (outputFormat->plugins[j].name[k] != '\0'
						|| k < nameLen
							&& !(outputFormat->plugins[j].options & kNMEPluginOptPartialName))
					goto continueMainLoop;
				
				return j;
continueMainLoop:
				;
			}
	
	// not found
	return -1;
}

/** Parse and process a plugin tag.
	@param[in] isBlock if TRUE, end tag must be alone in a line
	@param[in] isPlaceholder if TRUE, end tag must be triple right angle brackets
	@param[in] options kNMEProcessOptDefault or sum of options
	@param[in] outputFormat format strings, or NULL for default
	@param[in,out] context current context
	@param[out] reparseOutput TRUE if plugin's output is NME which should be parsed again
	@return error code (kNMEErrOk for success)
*/
static NMEErr addPlugin(NMEBoolean isBlock,
		NMEBoolean isPlaceholder,
		NMEInt options,
		NMEOutputFormat const *outputFormat,
		NMEContext *context,
		NMEBoolean *reparseOutput)
{
	NMEConstText name, data;
	NMEInt nameLen, dataLen;
	NMEInt j, k;
	NMEErr err;
	
	// find name
	skipBlanks(context->src, context->srcLen, &context->srcIndex);
	name = context->src + context->srcIndex;
	for (nameLen = 0;
			context->srcIndex + nameLen < context->srcLen
					&& !isBlank(context->src[context->srcIndex + nameLen])
					&& !isEol(context->src[context->srcIndex + nameLen])
					&& context->src[context->srcIndex + nameLen] != '>';
			nameLen++)
		;
	context->srcIndex += nameLen;
	skipBlanks(context->src, context->srcLen, &context->srcIndex);
	if (context->srcIndex < context->srcLen && context->src[context->srcIndex] == '\r')
		context->srcIndex++;
	if (context->srcIndex < context->srcLen && context->src[context->srcIndex] == '\n')
		context->srcIndex++;
	
	// find data
	data = context->src + context->srcIndex;
	if (isBlock)
		for (dataLen = 0;
				context->srcIndex + dataLen + (isPlaceholder ? 2 : 1) < context->srcLen;
				dataLen++)
		{
			if (isEol(context->src[context->srcIndex + dataLen - 1])
					&& context->src[context->srcIndex + dataLen] == '>'
					&& context->src[context->srcIndex + dataLen + 1] == '>'
					&& (!isPlaceholder
						|| context->src[context->srcIndex + dataLen + 2] == '>'))
			{
				// >> or >>> at the beginning of a line; check what follows
				j = context->srcIndex + dataLen + (isPlaceholder ? 3 : 2);
				skipBlanks(context->src, context->srcLen, &j);
				if (j >= context->srcLen || isEol(context->src[j]))
					break;	// nothing but blanks: end marker
			}
		}
	else	// inline plugin
		for (dataLen = 0;
				context->srcIndex + dataLen + (isPlaceholder ? 2 : 1) < context->srcLen
						&& (context->src[context->srcIndex + dataLen] != '>'
							|| context->src[context->srcIndex + dataLen + 1] != '>'
							|| isPlaceholder
								&& context->src[context->srcIndex + dataLen + 2] != '>');
				dataLen++)
			;
	context->srcIndex += dataLen;
	if (context->srcIndex + (isPlaceholder ? 3 : 2) <= context->srcLen)
		context->srcIndex += isPlaceholder ? 3 : 2;	// skip >>> or >>
	
	// remove trailing eol
	if (dataLen > 0 && data[dataLen - 1] == '\n')
		dataLen--;
	if (dataLen > 0 && data[dataLen - 1] == '\r')
		dataLen--;
	
	// find plugin
	if (outputFormat->plugins && nameLen > 0)
		for (j = 0; outputFormat->plugins[j].name; j++)
			if (isPlaceholder
					^ (outputFormat->plugins[j].options & kNMEPluginOptTripleAngleBrackets) == 0)
			{
				// compare plugin name
				for (k = 0; k < nameLen && outputFormat->plugins[j].name[k]; k++)
					if (name[k] != outputFormat->plugins[j].name[k])
						goto continueMainLoop;
				if (outputFormat->plugins[j].name[k] != '\0'
						|| k < nameLen
							&& !(outputFormat->plugins[j].options & kNMEPluginOptPartialName))
					goto continueMainLoop;
				
				// execute plugin
				CheckError(outputFormat->plugins[j].cb(name, nameLen,
						data, dataLen,
						context,
						outputFormat->plugins[j].userData));
				
				*reparseOutput
						= (outputFormat->plugins[j].options & kNMEPluginOptReparseOutput) != 0;
				
				return kNMEErrOk;
continueMainLoop:
				;
			}
	
	// not found: ignore
	return kNMEErrOk;
}

/** Update heading index and Increment heading number at the specified level,
	resetting higher levels.
	@param [in,out] headingFlags bit field with 1 if inside a section at that level
	([0]=level 1)
	@param [in,out] headingNum array of heading numbers ([0]=level 1)
	@param [in] headingLevel heading level to increment (1=level 1)
*/
static void nextHeading(NMEInt *headingFlags,
		NMEInt headingNum[/* kMaxNumberedHeadingLevels */],
		NMEInt headingLevel)
{
	NMEInt i;
	
	if (headingLevel <= sizeof(*headingFlags))
	{
		// set flag corresponding to current level (headingLevel-1)
		*headingFlags |= 1 << headingLevel - 1;
		// clear flags for i >= headingLevel
		*headingFlags &= ~((~(NMEInt)0) << headingLevel);
	}
	
	if (headingLevel <= kMaxNumberedHeadingLevels)
	{
		headingNum[headingLevel - 1]++;
		for (i = headingLevel; i < kMaxNumberedHeadingLevels; i++)
			headingNum[i] = 0;
	}
}

/** Parse next token, skipping it in the source code.
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in,out] i index in src (token to parse on input, next token on output)
	@param[in] state current state of the parser
	@param[in] verbatim TRUE if in inline verbatim mode
	@param[in] nesting current list nesting
	@param[in] listNum array of current list item numbers or kNMEListNumUL/DT/DD/Indented
	@param[in] styleStack stack of styles
	@param[in] styleNesting number of styles in stack
	@param[in] outputFormat format strings (can be NULL)
	@param[out] token kind of parsed token (details are in src[*i-1], *headingLevel,
	*itemNesting, *style)
	@param[out] headingLevel if token is a starting kNMETokenHeading, set to its level
	@param[out] itemNesting if token is kNMETokenLI, set to its nesting level
	@param[out] style if token is kNMETokenStyle, set to its style
	@param[in] options passed to NMEProcess
	@return TRUE to continue, FALSE if end of source has been reached
*/
static NMEBoolean parseNextToken(NMEConstText src, NMEInt srcLen,
		NMEInt *i,
		NMEState state,
		NMEBoolean verbatim,
		NMEInt nesting,
		NMEInt const listNum[],
		NMEStyle const styleStack[],
		NMEInt styleNesting,
		NMEOutputFormat const *outputFormat,
		NMEToken *token,
		NMEInt *headingLevel,
		NMEInt *itemNesting,
		NMEStyle *style,
		NMEInt options)
{
	NMEInt k;	// temp. index in src
	
	// skip blanks before trailing = in headings (must do it here)
	if (state == kNMEStateHeading && isBlank(src[*i]))
	{
		k = *i;
		skipBlanks(src, srcLen, &k);
		if (k < srcLen && src[k] == '=')
		{
			NMEInt p;
			
			// trailing '='?
			for (p = k + 1; p < srcLen && src[p] == '='; p++)
				;
			skipBlanks(src, srcLen, &p);
			if (p >= srcLen || isEol(src[p]))	// yes
				*i = k;	// skip blanks
		}
	}
	
	// parse token (sensitive to context)
	switch (src[*i])
	{
		case ' ':
			*token = kNMETokenSpace;
			(*i)++;
			return TRUE;
		case '\t':
			*token = kNMETokenTab;
			(*i)++;
			return TRUE;
		case '\r':
			*token = kNMETokenEOL;
			(*i)++;
			if (*i < srcLen && src[*i] == '\n')
				(*i)++;
			return TRUE;
		case '\n':
			*token = kNMETokenEOL;
			(*i)++;
			return TRUE;
		case '\\':
			if (verbatim)
				break;
			switch (state)
			{
				case kNMEStatePar:
				case kNMEStateHeading:
				case kNMEStateBetweenPar:
				case kNMEStateParAfterEol:
					if (*i + 1 >= srcLen || src[*i + 1] != '\\')
						break;	// not double backslash -> plain character
					*token = kNMETokenLineBreak;
					*i += 2;
					skipBlanks(src, srcLen, i);
					return TRUE;
				default:
					// plain character
					break;
			}
			break;
		case '=':
			if (state == kNMEStatePar || state == kNMEStatePre
					|| state == kNMEStatePreAfterEol || verbatim)
				break;	// plain character
			if (state == kNMEStateHeading)	// ignore number of ending =
			{
				// trailing?
				for (k = *i; k < srcLen && src[k] == '='; k++)
					;
				skipBlanks(src, srcLen, &k);
				if (k < srcLen && !isEol(src[k]))
					break;	// no, plain character
				while (*i < srcLen && src[*i] == '=')
					(*i)++;
			}
			else
			{
				for (*headingLevel = 0;
						*i < srcLen && src[*i] == '=';
						(*i)++, (*headingLevel)++)
					;
				if (options & kNMEProcessOptNoH1 && *headingLevel == 1)
					*headingLevel = 2;
				if (*headingLevel > outputFormat->maxHeadingLevel)
					*headingLevel = outputFormat->maxHeadingLevel;
			}
			*token = kNMETokenHeading;
			return TRUE;
		case '*':
		case '#':
			if (verbatim)
				break;	// plain character
			switch (state)
			{
				case kNMEStatePar:
				case kNMEStateHeading:
					if (*i + 1 >= srcLen || src[*i + 1] != src[*i])
						break;	// not bold/monospace
					if (options & (src[*i] == '*'
							? kNMEProcessOptNoBold : kNMEProcessOptNoMonospace))
						break;
					*token = kNMETokenStyle;
					*style = src[*i] == '*' ? kNMEStyleBold : kNMEStyleMonospace;
					*i += 2;
					return TRUE;
				case kNMEStateBetweenPar:
				case kNMEStateParAfterEol:
					if (nesting == 0
							|| src[*i] == (listNum[0] == kNMEListNumUL ? '*' : '#')
								&& (nesting == 1 || *i + 1 >= srcLen || src[*i] != src[*i + 1]
									|| (src[*i] == '*'
										? options & kNMEProcessOptNoBold || listNum[1] == kNMEListNumUL
										: src[*i] == '#'
										? options & kNMEProcessOptNoMonospace || listNum[1] > 0
										: TRUE)))
					{
						// no list mismatch at top or second level
						if (*i + 1 >= srcLen	// eof after one star or sharp
								|| src[*i + 1] != src[*i]	// not ** or ##
								|| nesting > 0 && *i + 2 < srcLen	// already in lists
								|| options & (src[*i] == '*'
										? kNMEProcessOptNoBold
										: kNMEProcessOptNoMonospace))
							goto tokenLI;
					}
					
					if (*i + 1 < srcLen && src[*i + 1] == src[*i])
					{
						*token = kNMETokenStyle;
						*style = src[*i] == '*' ? kNMEStyleBold : kNMEStyleMonospace;
						*i += 2;
						return TRUE;
					}
					// plain character
				default:
					// plain character
					break;
			}
			break;
		case '/':
			if (*i >= 2 && src[*i - 1] == ':'
					&& (isAlphaNum(src[*i - 2])
						|| src[*i - 2] == '+' || src[*i - 2] == '-' || src[*i - 2] == '.')
					&& *i + 2 < srcLen
					&& !isBlank(src[*i + 2]) && !isEol(src[*i + 2]))
				break;	// plain character in URL (see RFC 1738)
			// continue
		case '_':
		case '^':
		case ',':
			if (verbatim
					|| options & kNMEProcessOptNoItalic && src[*i] == '/'
					|| options & kNMEProcessOptNoUnderline && src[*i] == '_'
					|| options & kNMEProcessOptNoSubSuperscript && src[*i] == '^'
					|| options & kNMEProcessOptNoSubSuperscript && src[*i] == ',')
				break;	// plain character
			switch (state)
			{
				case kNMEStatePar:
				case kNMEStateHeading:
				case kNMEStateBetweenPar:
				case kNMEStateParAfterEol:
					if (*i + 1 >= srcLen || src[*i + 1] != src[*i])
						break;	// not double character -> plain character
					*token = kNMETokenStyle;
					*style = src[*i] == '/' ? kNMEStyleItalic
							: src[*i] == '^' ? kNMEStyleSuperscript
							: src[*i] == ',' ? kNMEStyleSubscript
							: kNMEStyleUnderline;
					*i += 2;
					return TRUE;
				default:
					// plain character
					break;
			}
			break;
		case ';':
			if (verbatim
					|| options & kNMEProcessOptNoDL
					|| state != kNMEStateBetweenPar && state != kNMEStateParAfterEol)
				break;	// not at beginning of line -> plain character
			// check that markup matches list kinds
			if (nesting == 0 || listNum[0] == kNMEListNumDT || listNum[0] == kNMEListNumDD)
				goto tokenLI;
			// doesn't match, plain character
			break;
		case ':':
			switch (state)
			{
				case kNMEStatePar:
					if (verbatim || nesting == 0 || listNum[nesting - 1] != kNMEListNumDT)
						break;	// not the end of a definition title: plain character
					*token = kNMETokenDD;
					(*i)++;
					skipBlanks(src, srcLen, i);
					return TRUE;
				case kNMEStateParAfterEol:
					if (verbatim || nesting > 0
							&& listNum[0] != kNMEListNumDT && listNum[0] != kNMEListNumDD
							&& listNum[0] != kNMEListIndented)
						break;	// plain character
					if (nesting > 0 && listNum[0] != kNMEListIndented)
						goto tokenLI;	// definition
					// indented par
				case kNMEStateBetweenPar:
					if (options & kNMEProcessOptNoIndentedPar)
						break;
					// count colons
					for (*itemNesting = 0;
							*i < srcLen && src[*i] == ':';
							(*i)++, (*itemNesting)++)
						;
					// limit
					if (*itemNesting > kMaxNesting)
						*itemNesting = kMaxNesting;
					*token = kNMETokenLI;
					return TRUE;
				default:
					// plain character
					break;
			}
			break;
		case '|':
			if (verbatim
					|| options & kNMEProcessOptNoTable)
				break;	// plain character
			switch (state)
			{
				case kNMEStatePar:
					if (nesting == 0
							|| listNum[nesting - 1] != kNMEListNumTableCell
								&& listNum[nesting - 1] != kNMEListNumTableHCell)
						break;	// not in a table -> plain character
					// continue
				case kNMEStateParAfterEol:
				case kNMEStateBetweenPar:
					if (*i + 1 < srcLen && src[*i + 1] == '=')
					{
						*token = kNMETokenTableHCell;
						*i += 2;
					}
					else
					{
						*token = kNMETokenTableCell;
						(*i)++;
					}
					skipBlanks(src, srcLen, i);
					if (state == kNMEStatePar)
						if (*i >= srcLen)
							return FALSE;	// nothing more on line: ignore
						else if (isEol(src[*i]))
						{
							// nothing more on line: ignore
							(*i)++;
							if (src[*i - 1] == '\r' && *i < srcLen && src[*i] == '\n')
								(*i)++;	// skip CRLF
							*token = kNMETokenEOL;
							return TRUE;
						}
					return TRUE;
				default:
					// plain character
					break;
			}
			break;
		case '}':
			if (!(options & kNMEProcessOptNoImage)
					&& !verbatim && *i + 1 < srcLen && src[*i + 1] == '}'
					&& findStyleInStyleStack(styleStack, styleNesting,
							kNMEStyleImage, NULL)
					&& state != kNMEStatePre && state != kNMEStatePreAfterEol)
			{
				*token = kNMETokenImageEnd;
				*i += 2;
				return TRUE;
			}
			// continue
		case '{':
			// matches "{{{" or "}}}", or "{{" for images
			if (*i + 2 >= srcLen || src[*i + 1] != src[*i]
					|| src[*i + 2] != src[*i])
			{
				if (!(options & kNMEProcessOptNoImage)
						&& !verbatim && *i + 1 < srcLen && src[*i + 1] == '{'
						&& !findStyleInStyleStack(styleStack, styleNesting,
								kNMEStyleImage, NULL)
						&& state != kNMEStatePre && state != kNMEStatePreAfterEol)
				{
					*token = kNMETokenImageBegin;
					*i += 2;
					return TRUE;
				}
				break;	// plain character
			}
			if (src[*i] == '}' && *i + 3 < srcLen && src[*i + 3] == '}')
				break;	// '}' before "}}}" -> still inside verbatim
			switch (state)
			{
				case kNMEStatePar:
				case kNMEStateHeading:
					if (verbatim ? src[*i] == '{' : src[*i] == '}')
						break;	// not the expected one
					*token = kNMETokenStyle;
					*style = kNMEStyleVerbatim;
					*i += 3;
					return TRUE;
				case kNMEStateBetweenPar:
				case kNMEStateParAfterEol:
					// check if there is something else on the line
					k = *i + 3;
					skipBlanks(src, srcLen, &k);
					if (k < srcLen && !isEol(src[k]))
					{
						*token = kNMETokenStyle;	// yes
						*style = kNMEStyleVerbatim;
					}
					else
						*token = kNMETokenPre;	// no
					*i += 3;
					return TRUE;
				case kNMEStatePreAfterEol:
					if (src[*i] == '}')
					{
						*token = kNMETokenPre;
						*i += 3;
						return TRUE;
					}
					break;
				default:
					// plain character
					break;
			}
			break;
		case ']':
			if (!findStyleInStyleStack(styleStack, styleNesting,
					kNMEStyleLink, NULL))
				break;	// not parsing a link -> plain character
			// continue
		case '[':
			// matches "[[" or "]]"
			if (verbatim || options & kNMEProcessOptNoLink)
				break;	// plain character
			switch (state)
			{
				case kNMEStatePar:
				case kNMEStateHeading:
				case kNMEStateBetweenPar:
				case kNMEStateParAfterEol:
					if (*i + 1 >= srcLen || src[*i + 1] != src[*i])
						break;	// not double char -> plain character
					*token = src[*i] == '[' ? kNMETokenLinkBegin : kNMETokenLinkEnd;
					*i += 2;
					return TRUE;
				default:
					// plain character
					break;
			}
			break;
		case '<':
			// matches "<<" or "<<<"
			if (verbatim
					|| options & kNMEProcessOptNoPlugin
					|| state == kNMEStatePre || state == kNMEStatePreAfterEol
					|| *i + 2 >= srcLen
					|| src[*i + 1] != '<')
				break;	// in verbatim or not double char -> plain character
			if (*i + 2 < srcLen && src[*i + 2] == '<')
			{
				*token = kNMETokenPlaceholder;
				*i += 3;
			}
			else
			{
				*token = kNMETokenPlugin;
				*i += 2;
			}
			if (state == kNMEStateBetweenPar || state == kNMEStateParAfterEol)
			{
				// check if there is something else on the line
				k = *i;
				skipBlanks(src, srcLen, &k);
				if (k >= srcLen || isEol(src[k]))	// no -> block syntax
					if (*token == kNMETokenPlaceholder)
						*token = kNMETokenPlaceholderBlock;
					else
						*token = kNMETokenPluginBlock;
			}
			return TRUE;
		case '-':
			// matches regex "^----"
			if (options & kNMEProcessOptNoHRule
					|| state != kNMEStateBetweenPar && state != kNMEStateParAfterEol
					|| *i + 3 >= srcLen
					|| src[*i + 1] != '-' || src[*i + 2] != '-'
					|| src[*i + 3] != '-')
				break;	// doesn't match -> plain character
			// skip all '-'
			while (*i < srcLen && src[*i] == '-')
				(*i)++;
			*token = kNMETokenHR;
			return TRUE;
		case '~':
			// escape character, before any non-blank character
			if (verbatim
					|| options & kNMEProcessOptNoEscape
					|| state == kNMEStatePre || state == kNMEStatePreAfterEol
					|| *i + 1 >= srcLen
					|| isEol(src[*i + 1]) || isBlank(src[*i + 1]))
				break;	// no -> plain character
			*i += 2;
			*token = kNMETokenChar;
			return TRUE;
	}
	
	*token = kNMETokenChar;
	(*i)++;
	return TRUE;
	
tokenLI:
	// count *, #, ;, : which match current list
	for (*itemNesting = 0;
			*i < srcLen
				&& (src[*i] == '*' || src[*i] == '#' || src[*i] == ';'
					|| src[*i] == ':' && *itemNesting < nesting)
				&& (*itemNesting >= nesting
					|| (listNum[*itemNesting] == kNMEListNumUL ? src[*i] == '*'
						: listNum[*itemNesting] == kNMEListNumDT
								|| listNum[*itemNesting] == kNMEListNumDD
							? src[*i] == ';' || src[*i] == ':'
						: src[*i] == '#'));
			(*i)++, (*itemNesting)++)
		;
	// limit
	if (*itemNesting > kMaxNesting)
		*itemNesting = kMaxNesting;
	*token = src[*i - 1] == ':' ? kNMETokenDD : kNMETokenLI;
	return TRUE;
}

/** Swap source and destination buffers after some plugin or autoconvert
	output must be reparsed.
	@param[in,out] src input characters
	@param[in,out] srcLen length of src in bytes
	@param[in,out] context current context
	@param[in,out] commonLen number of bytes at beginning of dest already copied in src
	@param[in] destLen0 value of destLen before plugin or autoconvert call
	@return error code (kNMEErrOk for success)
	@see NMEProcess
*/
static NMEErr swapBuffers(NMEText *src, NMEInt *srcLen,
		NMEContext *context,
		NMEInt *commonLen,
		NMEInt destLen0)
{
	NMEInt k;
	NMEText tmp;
	
	// check size
	if (*srcLen + context->destLen - context->srcIndex > context->bufSize
			|| destLen0 > context->bufSize)
		return kNMEErrNotEnoughMemory;
	
	// see comment at beginning of NMEProcess
	for (k = 0; k < *srcLen - context->srcIndex; k++)
		context->dest[context->destLen + k] = (*src)[context->srcIndex + k];
	for (k = 0; k < destLen0 - *commonLen; k++)
		(*src)[*commonLen + k] = context->dest[*commonLen + k];
	*commonLen = destLen0;
	*srcLen += context->destLen - context->srcIndex;
	context->srcIndexOffset -= context->destLen - context->srcIndex;
	context->srcIndex = context->destLen = destLen0;
	tmp = *src; *src = context->dest; context->dest = tmp;
	
	return kNMEErrOk;
}

NMEErr NMEProcess(NMEConstText nmeText, NMEInt nmeTextLen,
		NMEText buf, NMEInt bufSize,
		NMEInt options,
		NMEConstText eol,
		NMEOutputFormat const *outputFormat,
		NMEInt fontSize,
		NMEText *output,
		NMEInt *outputLen,
		NMEInt *outputUCS16Len)
{
	/*
	NMEProcess uses two buffers for input and output and swaps them when
	it must reprocess output, be it for plugins or autoconvert.
	src[0..commonLen-1]: partial copy of processed output, same as dest[0..commonLen-1]
	src[commonLen..i0-1]: processed input, can be discarded
	src[i0..srcLen-1]: input still to be processed
	dest[0..destLen0-1]: processed output
	When having to consume src[i0..i-1] and replace it with dest[destLen0..destLen-1] which must
	be processed again, the following steps occur:
	- copy src[i..srcLen-1] to dest[destLen..destLen+srcLen-i-1]
	- copy dest[commonLen..destLen0-1] to src[commonLen..destLen0-1]
	- set commonLen to destLen0
	- set srcLen to srcLen + destLen - i
	- set i to destLen0
	- set destLen to destLen0
	- swap src and dest
	*/
	NMEInt destLenTmp;	// temp. destLen used with plugins and autoconvert
	NMEInt commonLen;	// length of processed text shared in src and dest
	NMEInt i0;	// value of srcIndex before parsing current token
	NMEInt noAutoOrPluginLen;	// initial span of src protected against autoconvert and plugins
	NMEBoolean reparseOutput;	// TRUE if plugin's output must be parsed again
	NMEState state;	// current state
	NMEToken token;	// next token
	NMEInt headingNum[kMaxNumberedHeadingLevels];	// last heading number
	NMEInt headingFlags;	// bit i-1 is 1 if currently in a section at level i
	NMEInt currentIndent;	// number of spaces to prepend to each line
	NMEStyle styleStack[kNMEStylesCount];	// stack of styles
	NMEInt styleNesting;	// number of styles in styleStack
	NMEStyle newStyle = (NMEStyle)0;
	NMEInt itemNesting = 0;
	NMEInt headingLevel = 0;	// current heading level (1=top-level heading)
	NMEInt headingLevel0 = 0;	// heading level before current token
	NMEContext context;	// context used for expressions in output strings
	NMEErr err;
	
#define HOOK(cb, l, it, e, m) \
	do { \
		if (outputFormat->cb) \
		{ \
			err = outputFormat->cb(l, it, e, m, i0 + context.srcIndexOffset, \
					&context, \
					outputFormat->hookData); \
			if (err != kNMEErrOk) \
				return err; \
		} \
	} while (0)
	
	// set up format
	if (!outputFormat)
		outputFormat = &NMEOutputFormatText;
	context.fontSize = fontSize > 0 ? fontSize : outputFormat->defFontSize;
	context.options = options;
	context.eol = eol;
	context.ctrlChar = outputFormat->ctrlChar;
	context.xref = (options & kNMEProcessOptXRef) != 0;
	setContext(context, 0, 0);
	
	// set up buffers
	if (nmeTextLen > bufSize / 2)
		return kNMEErrNotEnoughMemory;
	context.src = buf;
	context.srcLen = nmeTextLen;
	for (i0 = 0; i0 < nmeTextLen; i0++)
		buf[i0] = nmeText[i0];
	context.dest = buf + bufSize / 2;
	context.bufSize = bufSize / 2;
	
	// set up parser state
	context.outputFormat = outputFormat;
	context.destLen = context.col = 0;
	context.destLenUCS16 = 0;
	commonLen = noAutoOrPluginLen = 0;
	context.currentIndent = 0;
	state = kNMEStateBetweenPar;
	context.nesting = 0;
	styleNesting = 0;
	headingNum[0] = -1;
	nextHeading(&headingFlags, headingNum, 1);
	headingFlags = 0;
	
	// beginning of doc
	if (!(options & kNMEProcessOptNoPreAndPost)
			&& !NMEAddString(outputFormat->beginDoc, -1, context.ctrlChar, &context))
		return kNMEErrNotEnoughMemory;
	
	// single pass main loop
	for (context.srcIndex = context.srcIndexOffset = 0; context.srcIndex < context.srcLen; )
	{
		// check enough memory for worst case
		if (context.destLen + kNMETokenTab >= context.bufSize)
			return kNMEErrNotEnoughMemory;
		
		// autoconvert
		if (state != kNMEStatePre && state != kNMEStatePreAfterEol
				&& context.srcIndex >= noAutoOrPluginLen
				&& !(options & kNMEProcessOptNoPlugin)
				&& outputFormat->autoconverts)
		{
			NMEInt k;
			
			for (k = 0; outputFormat->autoconverts[k].cb; k++)
			{
				destLenTmp = context.destLen;
				if (outputFormat->autoconverts[k].cb(context.src, context.srcLen, &context.srcIndex,
						&context,
						outputFormat->autoconverts[k].userData))
				{
					noAutoOrPluginLen = context.destLen;
					CheckError(swapBuffers(&context.src, &context.srcLen,
							&context,
							&commonLen,
							destLenTmp));
					break;
				}
			}
		}
		
		// parse token (sensitive to context)
		i0 = context.srcIndex;
		headingLevel0 = headingLevel;
		if (!parseNextToken(context.src, context.srcLen, &context.srcIndex,
				state,
				styleNesting > 0 && styleStack[styleNesting - 1] == kNMEStyleVerbatim,
				context.nesting, context.listNum,
				styleStack, styleNesting,
				outputFormat,
				&token,
				&headingLevel,
				&itemNesting,
				&newStyle,
				options))
			break;	// nothing more on line: ignore
		
		// state machine
		switch (state)
		{
			case kNMEStateBetweenPar:
				switch (token)
				{
					case kNMETokenChar:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "p");
						if (!NMEAddString(outputFormat->beginPar, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						if (outputFormat->charHookFun)
							CheckError(outputFormat->charHookFun(i0 + context.srcIndexOffset,
									&context,
									outputFormat->charHookData));
						if (outputFormat->encodeCharFun)
						{
							context.srcIndex--;
							CheckError(outputFormat->encodeCharFun(context.src,
									context.srcLen, &context.srcIndex,
									&context,
									outputFormat->encodeCharData));
						}
						else
						{
							context.dest[context.destLen++] = context.src[context.srcIndex - 1];
							if (isFirstUTF8Byte(context.src[context.srcIndex - 1]))
								context.destLenUCS16++;
							context.col++;
						}
						CheckError(checkWordwrap(&context, outputFormat));
						state = kNMEStatePar;
						break;
					case kNMETokenSpace:
					case kNMETokenTab:
						// ignore
						break;
					case kNMETokenEOL:
						// ignore
						break;
					case kNMETokenHeading:
						for (; headingLevel0 > headingLevel; headingLevel0--)
							if ((headingFlags >> headingLevel0 - 1) & 1)
								HOOK(divHookFun, headingLevel0, 0, FALSE, "=");
						nextHeading(&headingFlags, headingNum, headingLevel);
						context.level = headingLevel;
						context.item = headingLevel <= kMaxNumberedHeadingLevels
									&& options & (headingLevel == 1
											? kNMEProcessOptH1Num : kNMEProcessOptH2Num)
								? headingNum[headingLevel - 1]
								: 0;
						HOOK(divHookFun, context.level, context.item, TRUE, "=");
						HOOK(parHookFun, context.level, context.item, TRUE, "=");
						if (!NMEAddString(outputFormat->beginHeading, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						context.level = 0;
						state = kNMEStateHeading;
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						break;
					case kNMETokenLineBreak:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "p");
						if (!NMEAddString(outputFormat->beginPar, -1,
									context.ctrlChar, &context)
								|| !NMEAddString(outputFormat->lineBreak, -1,
										context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						state = kNMEStatePar;
						break;
					case kNMETokenPre:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "{{{");
						if (!NMEAddString(outputFormat->beginPre, -1,
									context.ctrlChar, &context)
								|| !NMEAddString(outputFormat->beginPreLine, -1,
										context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						state = kNMEStatePreAfterEol;
						// skip next eol
						if (context.srcIndex < context.srcLen && context.src[context.srcIndex] == '\r')
							context.srcIndex++;
						if (context.srcIndex < context.srcLen && context.src[context.srcIndex] == '\n')
							context.srcIndex++;
						break;
					case kNMETokenLI:
						// begin list(s)
						for (context.nesting = 0; context.nesting < itemNesting; context.nesting++)
						{
							context.listNum[context.nesting]
									= context.src[context.srcIndex - itemNesting + context.nesting] == '*'
										? kNMEListNumUL
										: context.src[context.srcIndex - itemNesting + context.nesting] == ';'
											? kNMEListNumDT
											: context.src[context.srcIndex - itemNesting + context.nesting] == ':'
												? kNMEListIndented
												: 1;
							context.level = context.nesting + 1;
							HOOK(divHookFun, context.level, 0, TRUE,
									context.listNum[context.nesting] == kNMEListNumUL ? "*"
										: context.listNum[context.nesting] == kNMEListNumDT ? ";"
										: context.listNum[context.nesting] == kNMEListIndented ? ":"
										: "#");
							if (!NMEAddString(context.listNum[context.nesting] == kNMEListNumUL
										? outputFormat->beginUL
									: context.listNum[context.nesting] == kNMEListNumDT
										? outputFormat->beginDL
									: context.listNum[context.nesting] == kNMEListIndented
										? outputFormat->beginIndented
										: outputFormat->beginOL,
									-1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							context.level = 0;
						}
						// skip spaces
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						// begin item
						currentIndent = context.nesting * outputFormat->indentSpaces;
						setContext(context, context.nesting, context.listNum[context.nesting - 1]);
						HOOK(parHookFun, context.level, context.item, TRUE,
								context.listNum[context.nesting - 1] == kNMEListNumUL ? "*"
									: context.listNum[context.nesting - 1] == kNMEListNumDT ? ";"
									: context.listNum[context.nesting - 1] == kNMEListIndented ? ":"
									: "#");
						if (!NMEAddString(context.listNum[context.nesting - 1] == kNMEListNumUL
									? outputFormat->beginULItem
								: context.listNum[context.nesting - 1] == kNMEListNumDT
									? outputFormat->beginDT
								: context.listNum[context.nesting - 1] == kNMEListIndented
									? outputFormat->beginIndentedPar
									: outputFormat->beginOLItem,
								-1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						setContext(context, 0, 0);
						state = kNMEStatePar;
						break;
					case kNMETokenTableCell:
					case kNMETokenTableHCell:
						// start new table
						context.listNum[context.nesting++]
								= token == kNMETokenTableCell
									? kNMEListNumTableCell
									: kNMEListNumTableHCell;
						currentIndent = context.nesting * outputFormat->indentSpaces;
						state = kNMEStatePar;
						context.level = context.nesting - 1;
						HOOK(divHookFun, kNMEHookLevelPar, 0, TRUE, "|");
						if (!NMEAddString(outputFormat->beginTable, -1,
									context.ctrlChar, &context)
								|| !NMEAddString(outputFormat->beginTableRow, -1,
										context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE,
								token == kNMETokenTableCell ? "|" : "|=");
						if (!NMEAddString(token == kNMETokenTableCell
									? outputFormat->beginTableCell
									: outputFormat->beginTableHCell,
								-1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						context.level = 0;
						break;
					case kNMETokenHR:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "----");
						if (!NMEAddString(outputFormat->horRule, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE, "----");
						break;
					case kNMETokenStyle:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "p");
						if (!NMEAddString(outputFormat->beginPar, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(processStyleTag(styleStack, &styleNesting,
								newStyle,
								i0,
								outputFormat, &context));
						state = kNMEStatePar;
						break;
					case kNMETokenLinkBegin:
					case kNMETokenImageBegin:
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "p");
						if (!NMEAddString(outputFormat->beginPar, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(addLinkBegin(token == kNMETokenImageBegin,
								styleStack, &styleNesting,
								i0, outputFormat, &context));
						state = kNMEStatePar;
						break;
					case kNMETokenPlugin:
					case kNMETokenPluginBlock:
					case kNMETokenPlaceholder:
					case kNMETokenPlaceholderBlock:
						{
							NMEInt pluginIndex;
							
							pluginIndex = findPlugin(context.src, context.srcLen, context.srcIndex,
									token == kNMETokenPlaceholder
											|| token == kNMETokenPlaceholderBlock,
									outputFormat);
							if (pluginIndex >= 0
									&& !(outputFormat->plugins[pluginIndex].options
											& kNMEPluginOptBetweenPar))
							{
								// start a new paragraph
								HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "p");
								if (!NMEAddString(outputFormat->beginPar, -1,
										context.ctrlChar, &context))
									return kNMEErrNotEnoughMemory;
								state = kNMEStatePar;
							}
							destLenTmp = context.destLen;
							CheckError(addPlugin(token == kNMETokenPluginBlock
										|| token == kNMETokenPlaceholderBlock,
									token == kNMETokenPlaceholder
										|| token == kNMETokenPlaceholderBlock,
									options, outputFormat, &context,
									&reparseOutput));
							if (reparseOutput)
							{
								CheckError(swapBuffers(&context.src, &context.srcLen,
										&context,
										&commonLen,
										destLenTmp));
								// noAutoOrPluginLen = context.destLen;
							}
						}
						break;
					case kNMETokenDD:
					case kNMETokenLinkEnd:
					case kNMETokenImageEnd:
						// should never occur between paragraphs
						return kNMEErrInternal;
				}
				break;
			case kNMEStatePar:
				switch (token)
				{
					case kNMETokenChar:
						if (outputFormat->charHookFun)
							CheckError(outputFormat->charHookFun(i0 + context.srcIndexOffset,
									&context,
									outputFormat->charHookData));
						if (outputFormat->encodeCharFun)
						{
							context.srcIndex--;
							CheckError(outputFormat->encodeCharFun(context.src, context.srcLen, &context.srcIndex,
									&context,
									outputFormat->encodeCharData));
						}
						else
						{
							context.dest[context.destLen++] = context.src[context.srcIndex - 1];
							if (isFirstUTF8Byte(context.src[context.srcIndex - 1]))
								context.destLenUCS16++;
							context.col++;
						}
						CheckError(checkWordwrap(&context, outputFormat));
						break;
					case kNMETokenSpace:
					case kNMETokenTab:
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						// single space provided not last of line
						if (context.srcIndex < context.srcLen && !isEol(context.src[context.srcIndex]))
						{
							if (!NMEAddString(outputFormat->space, -1,
										context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							CheckError(checkWordwrap(&context, outputFormat));
						}
						break;
					case kNMETokenLineBreak:
						if (!NMEAddString(outputFormat->lineBreak, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						break;
					case kNMETokenEOL:
						state = kNMEStateParAfterEol;
						break;
					case kNMETokenDD:
						context.level = context.nesting;
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(FALSE, outputFormat, &context));
						HOOK(parHookFun, context.level, context.item, TRUE, ";:");
						if (!NMEAddString(outputFormat->beginDD, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						context.level = 0;
						context.listNum[context.nesting - 1] = kNMEListNumDD;
						break;
					case kNMETokenTableCell:
					case kNMETokenTableHCell:
						// gobble back spaces (keep tabs)
						while (context.destLen > 0 && context.dest[context.destLen - 1] == ' ')
						{
							context.destLen--;
							context.destLenUCS16--;
						}
						// end last cell and begin new one
						context.level = context.nesting;
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						if (!NMEAddString(context.listNum[context.nesting - 1] == kNMEListNumTableCell
									? outputFormat->endTableCell
									: outputFormat->endTableHCell,
								-1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE,
								context.listNum[context.nesting - 1] == kNMEListNumTableCell
									? "|" : "|=");
						CheckError(checkWordwrap(&context, outputFormat));
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE,
								token == kNMETokenTableCell ? "|" : "|=");
						if (!NMEAddString(token == kNMETokenTableCell
									? outputFormat->beginTableCell
									: outputFormat->beginTableHCell,
								-1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						context.level = 0;
						context.listNum[context.nesting - 1] = token == kNMETokenTableCell
								? kNMEListNumTableCell
								: kNMEListNumTableHCell;
						break;
					case kNMETokenStyle:
					case kNMETokenLinkEnd:
					case kNMETokenImageEnd:
						CheckError(processStyleTag(styleStack, &styleNesting,
								token == kNMETokenLinkEnd ? kNMEStyleLink
									: token == kNMETokenImageEnd ? kNMEStyleImage : newStyle,
								i0,
								outputFormat, &context));
						break;
					case kNMETokenLinkBegin:
					case kNMETokenImageBegin:
						CheckError(addLinkBegin(token == kNMETokenImageBegin,
								styleStack, &styleNesting,
								i0, outputFormat, &context));
						break;
					case kNMETokenPlugin:
					case kNMETokenPluginBlock:
					case kNMETokenPlaceholder:
					case kNMETokenPlaceholderBlock:
						{
							NMEInt pluginIndex;
							
							pluginIndex = findPlugin(context.src, context.srcLen, context.srcIndex,
									token == kNMETokenPlaceholder
											|| token == kNMETokenPlaceholderBlock,
									outputFormat);
							if (pluginIndex >= 0
									&& outputFormat->plugins[pluginIndex].options
											& kNMEPluginOptBetweenPar)
							{
								// end paragraph
								CheckError(flushStyleTags(styleStack, &styleNesting,
											i0,
											outputFormat, &context));
								CheckError(addEndPar(TRUE, outputFormat, &context));
								state = kNMEStateBetweenPar;
							}
							destLenTmp = context.destLen;
							CheckError(addPlugin(token == kNMETokenPluginBlock
										|| token == kNMETokenPlaceholderBlock,
									token == kNMETokenPlaceholder
										|| token == kNMETokenPlaceholderBlock,
									options, outputFormat, &context,
									&reparseOutput));
							if (reparseOutput)
							{
								CheckError(swapBuffers(&context.src, &context.srcLen,
										&context,
										&commonLen,
										destLenTmp));
								// noAutoOrPluginLen = context.destLen;
							}
						}
						break;
					case kNMETokenHeading:
					case kNMETokenLI:
					case kNMETokenHR:
					case kNMETokenPre:
						// should never occur inside a paragraph
						return kNMEErrInternal;
				}
				break;
			case kNMEStateParAfterEol:
				switch (token)
				{
					case kNMETokenChar:
						if (options & kNMEProcessOptNoMultilinePar
								|| context.nesting > 0
									&& (context.listNum[context.nesting - 1] == kNMEListNumTableCell
										|| context.listNum[context.nesting - 1] == kNMEListNumTableHCell))
						{
							// new paragraph
							CheckError(flushStyleTags(styleStack, &styleNesting,
									i0,
									outputFormat, &context));
							CheckError(addEndPar(TRUE, outputFormat, &context));
							if (!NMEAddString(outputFormat->beginPar, -1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							currentIndent = 0;
						}
						else
							if (!NMEAddString(outputFormat->space, -1,
										context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						if (outputFormat->charHookFun)
							CheckError(outputFormat->charHookFun(i0 + context.srcIndexOffset,
									&context,
									outputFormat->charHookData));
						if (outputFormat->encodeCharFun)
						{
							context.srcIndex--;
							CheckError(outputFormat->encodeCharFun(context.src, context.srcLen, &context.srcIndex,
									&context,
									outputFormat->encodeCharData));
						}
						else
						{
							context.dest[context.destLen++] = context.src[context.srcIndex - 1];
							if (isFirstUTF8Byte(context.src[context.srcIndex - 1]))
								context.destLenUCS16++;
							context.col++;
						}
						state = kNMEStatePar;
						break;
					case kNMETokenSpace:
					case kNMETokenTab:
						// ignore
						break;
					case kNMETokenEOL:
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(TRUE, outputFormat, &context));
						state = kNMEStateBetweenPar;
						currentIndent = context.nesting * outputFormat->indentSpaces;
						break;
					case kNMETokenDD:
						context.level = context.nesting;
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(FALSE, outputFormat, &context));
						if (context.listNum[context.nesting - 1] != kNMEListNumDT	// prev par wasn't DT
								&& outputFormat->emptyDT)
						{
							if (!NMEAddString(outputFormat->emptyDT, -1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							CheckError(checkWordwrap(&context, outputFormat));
						}
						// skip spaces
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						// begin DD
						HOOK(parHookFun, context.level, context.item, TRUE, ";:");
						if (!NMEAddString(outputFormat->beginDD, -1, context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						context.level = 0;
						context.listNum[context.nesting - 1] = kNMEListNumDD;
						state = kNMEStatePar;
						break;
					case kNMETokenLineBreak:
						if (!NMEAddString(outputFormat->lineBreak, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						break;
					case kNMETokenPre:
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(TRUE, outputFormat, &context));
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "{{{");
						if (!NMEAddString(outputFormat->beginPre, -1,
										context.ctrlChar, &context)
								|| !NMEAddString(outputFormat->beginPreLine, -1,
										context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						state = kNMEStatePreAfterEol;
						currentIndent = 0;
						// skip next eol
						if (context.srcIndex < context.srcLen && context.src[context.srcIndex] == '\r')
							context.srcIndex++;
						if (context.srcIndex < context.srcLen && context.src[context.srcIndex] == '\n')
							context.srcIndex++;
						break;
					case kNMETokenHeading:
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(TRUE, outputFormat, &context));
						for (; headingLevel0 > headingLevel; headingLevel0--)
							if ((headingFlags >> headingLevel0 - 1) & 1)
								HOOK(divHookFun, headingLevel0, 0, FALSE, "=");
						nextHeading(&headingFlags, headingNum, headingLevel);
						context.level = headingLevel;
						context.item = headingLevel <= kMaxNumberedHeadingLevels
									&& options & (headingLevel == 1
											? kNMEProcessOptH1Num : kNMEProcessOptH2Num)
								? headingNum[headingLevel - 1]
								: 0;
						HOOK(divHookFun, context.level, context.item, TRUE, "=");
						HOOK(parHookFun, context.level, context.item, TRUE, "=");
						if (!NMEAddString(outputFormat->beginHeading, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						currentIndent = 0;
						context.level = 0;
						state = kNMEStateHeading;
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						break;
					case kNMETokenLI:
						// end last paragraph or list item
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(FALSE, outputFormat, &context));
						// if new item less nested than current level, end lists(s)
						while (context.nesting > itemNesting)
						{
							setContext(context, context.nesting, context.listNum[context.nesting - 1]);
							context.nesting--;
							if (!NMEAddString(context.listNum[context.nesting] == kNMEListNumUL
										? outputFormat->endUL
									: context.listNum[context.nesting] == kNMEListNumDT
										? outputFormat->endDL
									: context.listNum[context.nesting] == kNMEListIndented
										? outputFormat->endIndented
										: outputFormat->endOL,
									-1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							HOOK(divHookFun, context.level, context.item, FALSE,
									context.listNum[context.nesting] == kNMEListNumUL ? "*"
									: context.listNum[context.nesting] == kNMEListNumDT ? ";"
									: context.listNum[context.nesting] == kNMEListIndented ? ":"
									: "#");
						}
						setContext(context, 0, 0);
						// if new item more nested than current level, begin list(s)
						for (; context.nesting < itemNesting; context.nesting++)
						{
							context.listNum[context.nesting]
									= context.src[context.srcIndex - itemNesting + context.nesting] == '*'
										? kNMEListNumUL
										: context.src[context.srcIndex - itemNesting + context.nesting] == ';'
											? kNMEListNumDT
											: context.src[context.srcIndex - itemNesting + context.nesting] == ':'
												? kNMEListIndented
												: 1;
							context.level = context.nesting + 1;
							HOOK(divHookFun, context.level, context.item, TRUE,
									context.listNum[context.nesting] == kNMEListNumUL ? "*"
									: context.listNum[context.nesting] == kNMEListNumDT ? ";"
									: context.listNum[context.nesting] == kNMEListIndented ? ":"
									: "#");
							if (!NMEAddString(context.listNum[context.nesting] == kNMEListNumUL
										? outputFormat->beginUL
									: context.listNum[context.nesting] == kNMEListNumDT
										? outputFormat->beginDL
									: context.listNum[context.nesting] == kNMEListIndented
										? outputFormat->beginIndented
										: outputFormat->beginOL,
									-1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
						}
						currentIndent = context.nesting * outputFormat->indentSpaces;
						// skip spaces
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
						// replace DD with DT
						if (context.listNum[context.nesting - 1] == kNMEListNumDD)
							context.listNum[context.nesting - 1] = kNMEListNumDT;
						// begin item
						setContext(context, context.nesting, context.listNum[context.nesting - 1]);
						HOOK(parHookFun, context.level, context.item, TRUE,
								context.listNum[context.nesting - 1] == kNMEListNumUL ? "*"
								: context.listNum[context.nesting - 1] == kNMEListNumDT ? ";"
								: context.listNum[context.nesting - 1] == kNMEListIndented ? ":"
								: "#");
						if (!NMEAddString(context.listNum[context.nesting - 1] == kNMEListNumUL
									? outputFormat->beginULItem
								: context.listNum[context.nesting - 1] == kNMEListNumDT
									? outputFormat->beginDT
								: context.listNum[context.nesting - 1] == kNMEListIndented
									? outputFormat->beginIndentedPar
									: outputFormat->beginOLItem,
								-1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						setContext(context, 0, 0);
						state = kNMEStatePar;
						break;
					case kNMETokenTableCell:
					case kNMETokenTableHCell:
						// end last paragraph or list item
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(FALSE, outputFormat, &context));
						if (context.nesting == 0
								|| context.listNum[context.nesting - 1] != kNMEListNumTableCell
									&& context.listNum[context.nesting - 1] != kNMEListNumTableHCell)
						{
							// start new table
							context.nesting++;	// context.listNum set below
							context.level = context.nesting - 1;
							HOOK(divHookFun, kNMEHookLevelPar, 0, TRUE, "|");
							if (!NMEAddString(outputFormat->beginTable, -1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
						}
						currentIndent = context.nesting * outputFormat->indentSpaces;
						// set context.listNum type
						context.listNum[context.nesting - 1] = token == kNMETokenTableCell
								? kNMEListNumTableCell
								: kNMEListNumTableHCell;
						// new row
						context.level = context.nesting - 1;
						if (!NMEAddString(outputFormat->beginTableRow, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE,
								token == kNMETokenTableCell ? "|" : "|=");
						if (!NMEAddString(token == kNMETokenTableCell
										? outputFormat->beginTableCell
										: outputFormat->beginTableHCell,
									-1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						context.level = 0;
						state = kNMEStatePar;
						break;
					case kNMETokenHR:
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						CheckError(addEndPar(TRUE, outputFormat, &context));
						HOOK(parHookFun, kNMEHookLevelPar, 0, TRUE, "----");
						if (!NMEAddString(outputFormat->horRule, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE, "----");
						currentIndent = 0;
						state = kNMEStateBetweenPar;
						break;
					case kNMETokenStyle:
					case kNMETokenLinkEnd:
					case kNMETokenImageEnd:
						if (!NMEAddString(outputFormat->space, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						CheckError(processStyleTag(styleStack, &styleNesting,
								token == kNMETokenLinkEnd
										? kNMEStyleLink
								: token == kNMETokenImageEnd
										? kNMEStyleImage
										: newStyle,
								i0,
								outputFormat, &context));
						state = kNMEStatePar;
						break;
					case kNMETokenLinkBegin:
					case kNMETokenImageBegin:
						if (!NMEAddString(outputFormat->space, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(addLinkBegin(token == kNMETokenImageBegin,
								styleStack, &styleNesting,
								i0, outputFormat, &context));
						state = kNMEStatePar;
						break;
					case kNMETokenPlugin:
					case kNMETokenPluginBlock:
					case kNMETokenPlaceholder:
					case kNMETokenPlaceholderBlock:
						{
							NMEInt pluginIndex;
							
							pluginIndex = findPlugin(context.src, context.srcLen, context.srcIndex,
									token == kNMETokenPlaceholder
											|| token == kNMETokenPlaceholderBlock,
									outputFormat);
							if (pluginIndex >= 0
									&& outputFormat->plugins[pluginIndex].options
											& kNMEPluginOptBetweenPar)
							{
								// end paragraph
								CheckError(flushStyleTags(styleStack, &styleNesting,
										i0,
										outputFormat, &context));
								CheckError(addEndPar(TRUE, outputFormat, &context));
								state = kNMEStateBetweenPar;
							}
							else
							{
								// begin line (insert space)
								if (!NMEAddString(outputFormat->space, -1,
										context.ctrlChar, &context))
									return kNMEErrNotEnoughMemory;
								state = kNMEStatePar;
							}
							destLenTmp = context.destLen;
							CheckError(addPlugin(token == kNMETokenPluginBlock
										|| token == kNMETokenPlaceholderBlock,
									token == kNMETokenPlaceholder
										|| token == kNMETokenPlaceholderBlock,
									options, outputFormat, &context,
									&reparseOutput));
							if (reparseOutput)
							{
								CheckError(swapBuffers(&context.src, &context.srcLen,
										&context,
										&commonLen,
										destLenTmp));
								// noAutoOrPluginLen = context.destLen;
							}
						}
						break;
				}
				break;
			case kNMEStatePreAfterEol:
				if (token == kNMETokenPre)	// end of pre
				{
					if (!NMEAddString(outputFormat->endPre, -1,
							context.ctrlChar, &context))
						return kNMEErrNotEnoughMemory;
					HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE, "{{{");
					state = kNMEStateBetweenPar;
					break;
				}
				// beginning of pre line
				if (!NMEAddString(outputFormat->beginPreLine, -1,
						context.ctrlChar, &context))
					return kNMEErrNotEnoughMemory;
				state = kNMEStatePre;
				
				if (token == kNMETokenSpace)
				{
					// followed by spaces and three closing braces?
					NMEInt k;
					
					for (k = context.srcIndex; k < context.srcLen && context.src[k] == ' '; k++)
						;
					if (k + 3 <= context.srcLen && context.src[k] == '}'
							&& context.src[k + 1] == '}' && context.src[k + 2] == '}')
						break;	// yes: ignore first space
				}				
				// continue
			case kNMEStatePre:
				switch (token)
				{
					case kNMETokenChar:
						if (outputFormat->encodeCharPreFun)
						{
							context.srcIndex--;
							CheckError(outputFormat->encodeCharPreFun(context.src, context.srcLen, &context.srcIndex,
									&context,
									outputFormat->encodeCharPreData));
						}
						else
						{
							context.dest[context.destLen++] = context.src[context.srcIndex - 1];
							if (isFirstUTF8Byte(context.src[context.srcIndex - 1]))
								context.destLenUCS16++;
							context.col++;
						}
						break;
					case kNMETokenSpace:
						if (outputFormat->encodeCharPreFun)
						{
							NMEInt tmp = 0;
							
							CheckError(outputFormat->encodeCharPreFun(" ", 1, &tmp,
									&context,
									outputFormat->encodeCharPreData));
						}
						else
						{
							context.dest[context.destLen++] = ' ';
							context.destLenUCS16++;
							context.col++;
						}
						break;
					case kNMETokenTab:
						do
						{
							if (outputFormat->encodeCharPreFun)
							{
								NMEInt tmp = 0;
								
								CheckError(outputFormat->encodeCharPreFun(" ", 1, &tmp,
										&context,
										outputFormat->encodeCharPreData));
							}
							else
							{
								context.dest[context.destLen++] = ' ';
								context.destLenUCS16++;
								context.col++;
							}
						} while (context.col % kTabWidth != 0);
						break;
					case kNMETokenEOL:
						if (!NMEAddString(outputFormat->endPreLine, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						state = kNMEStatePreAfterEol;
						break;
					default:
						// should never occur in preformatted blocks
						return kNMEErrInternal;
				}
				break;
			case kNMEStateHeading:
				switch (token)
				{
					case kNMETokenChar:
						if (outputFormat->charHookFun)
							CheckError(outputFormat->charHookFun(i0 + context.srcIndexOffset,
									&context,
									outputFormat->charHookData));
						if (outputFormat->encodeCharFun)
						{
							context.srcIndex--;
							CheckError(outputFormat->encodeCharFun(context.src, context.srcLen, &context.srcIndex,
									&context,
									outputFormat->encodeCharData));
						}
						else
						{
							context.dest[context.destLen++] = context.src[context.srcIndex - 1];
							if (isFirstUTF8Byte(context.src[context.srcIndex - 1]))
								context.destLenUCS16++;
							context.col++;
						}
						CheckError(checkWordwrap(&context, outputFormat));
						break;
					case kNMETokenSpace:
					case kNMETokenTab:
						if (state == kNMEStatePar)	// pack multiple spaces for par
							skipBlanks(context.src, context.srcLen, &context.srcIndex);
						// single space provided not last of line
						if (context.srcIndex < context.srcLen && !isEol(context.src[context.srcIndex]))
						{
							if (!NMEAddString(outputFormat->space, -1,
									context.ctrlChar, &context))
								return kNMEErrNotEnoughMemory;
							CheckError(checkWordwrap(&context, outputFormat));
						}
						break;
					case kNMETokenHeading:
						// end of heading
						skipBlanks(context.src, context.srcLen, &context.srcIndex);
					case kNMETokenEOL:
						context.level = headingLevel;
						CheckError(flushStyleTags(styleStack, &styleNesting,
								i0,
								outputFormat, &context));
						if (!NMEAddString(outputFormat->endHeading, -1,
									context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						HOOK(parHookFun, context.level, 0, FALSE, "=");
						context.level = 0;
						state = kNMEStateBetweenPar;
						break;
					case kNMETokenLineBreak:
						if (!NMEAddString(outputFormat->lineBreak, -1,
								context.ctrlChar, &context))
							return kNMEErrNotEnoughMemory;
						CheckError(checkWordwrap(&context, outputFormat));
						break;
					case kNMETokenStyle:
					case kNMETokenLinkEnd:
					case kNMETokenImageEnd:
						CheckError(processStyleTag(styleStack, &styleNesting,
								token == kNMETokenLinkEnd
									? kNMEStyleLink
									: token == kNMETokenImageEnd
										? kNMEStyleImage : newStyle,
								i0,
								outputFormat, &context));
						break;
					case kNMETokenLinkBegin:
					case kNMETokenImageBegin:
						CheckError(addLinkBegin(token == kNMETokenImageBegin,
								styleStack, &styleNesting,
								i0, outputFormat, &context));
						break;
					case kNMETokenPlugin:
					case kNMETokenPluginBlock:
					case kNMETokenPlaceholder:
					case kNMETokenPlaceholderBlock:
						destLenTmp = context.destLen;
						CheckError(addPlugin(token == kNMETokenPluginBlock
										|| token == kNMETokenPlaceholderBlock,
									token == kNMETokenPlaceholder
										|| token == kNMETokenPlaceholderBlock,
								options, outputFormat, &context,
								&reparseOutput));
						if (reparseOutput)
						{
							CheckError(swapBuffers(&context.src, &context.srcLen,
									&context,
									&commonLen,
									destLenTmp));
							// noAutoOrPluginLen = context.destLen;
						}
						break;
					case kNMETokenLI:
					case kNMETokenDD:
					case kNMETokenTableCell:
					case kNMETokenTableHCell:
					case kNMETokenHR:
					case kNMETokenPre:
						// should never occur in a heading
						return kNMEErrInternal;
				}
				break;
		}
	}
	
	// end: flush pending constructs
	switch (state)
	{
		case kNMEStatePar:
		case kNMEStateParAfterEol:
			CheckError(flushStyleTags(styleStack, &styleNesting,
					i0,
					outputFormat, &context));
			CheckError(addEndPar(TRUE, outputFormat, &context));
			break;
		case kNMEStatePre:
			if (!NMEAddString(outputFormat->endPre, -1,
					context.ctrlChar, &context))
				return kNMEErrNotEnoughMemory;
			HOOK(parHookFun, kNMEHookLevelPar, 0, FALSE, "{{{");
			break;
		case kNMEStateHeading:
			context.level = headingLevel;
			CheckError(flushStyleTags(styleStack, &styleNesting,
					i0,
					outputFormat, &context));
			if (!NMEAddString(outputFormat->endHeading, -1,
							context.ctrlChar, &context))
				return kNMEErrNotEnoughMemory;
			CheckError(checkWordwrap(&context, outputFormat));
			HOOK(parHookFun, context.level, 0, FALSE, "=");
			context.level = 0;
			break;
		default:
			break;
	}
	
	// end of doc
	if (!(options & kNMEProcessOptNoPreAndPost)
			&& !NMEAddString(outputFormat->endDoc, -1,
					context.ctrlChar, &context))
		return kNMEErrNotEnoughMemory;
	if (context.destLen + 1 >= context.bufSize)
		return kNMEErrNotEnoughMemory;
	context.dest[context.destLen] = '\0';
	
	// set result
	*output = context.dest;
	*outputLen = context.destLen;
	if (outputUCS16Len)
		*outputUCS16Len = context.destLenUCS16;
	return kNMEErrOk;
}

void NMEGetTempMemory(NMEContext const *context,
		NMEText *addr,
		NMEInt *len)
{
	*addr = context->src + context->srcLen;
	*len = context->bufSize - context->srcLen;
}

void NMEGetFormat(NMEContext const *context,
		NMEOutputFormat const **outputFormat,
		NMEInt *options,
		NMEInt *fontSize)
{
	if (outputFormat)
		*outputFormat = context->outputFormat;
	if (options)
		*options = context->options;
	if (fontSize)
		*fontSize = context->fontSize;
}

NMEInt NMECurrentOutputIndex(NMEContext const *context)
{
	return context->destLen;
}

NMEInt NMECurrentOutputIndexUCS16(NMEContext const *context)
{
	return context->destLenUCS16;
}

void NMECurrentLink(NMEContext const *context,
		NMEInt *linkOffset, NMEInt *linkLength)
{
	*linkOffset = context->srcIndexOffset + context->linkOffset;
	*linkLength = context->linkLength;
}

void NMECurrentOutput(NMEContext const *context,
		NMEConstText *output, NMEInt *outputLength)
{
	if (output)
		*output = context->dest;
	if (outputLength)
		*outputLength = context->destLen;
}

NMEConstText NMECurrentListNesting(NMEContext const *context)
{
	NMEInt i;
	static NMEChar str[kMaxNesting + 1];
	
	for (i = 0; i < context->level; i++)
		str[i] = context->listNum[i] == kNMEListNumUL ? '*'
				: context->listNum[i] == kNMEListNumDT ? ';'
				: context->listNum[i] == kNMEListNumDD ? ':'
				: context->listNum[i] == kNMEListIndented ? ':' : '#';
	str[i] = '\0';
	return str;
}

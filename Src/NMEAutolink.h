/**
 *	@file NMEAutolink.h
 *	@brief NME optional automatic link recognition (CamelCase and URL)
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 *
 *	To convert camelCase words to links (i.e. transform any word with
 *	at least one uppercase letter following a lowercase letter, such
 *	as camelCase, to a link as if it were enclosed between double-brackets,
 *	as [[camelCase]]), add NMEAutoconvertCamelCaseEntry to the table
 *	autoconverts[] in NMEOutputFormat.
 *	To convert URLs to links (i.e. transform any sequence beginning
 *	with http://, https://, ftp:// or mailto: to a link as if it were
 *	enclosed between double-brackets), add NMEAutoconvertURLEntry to the
 *	table autoconverts[] in NMEOutputFormat.
 *	To have both:
 *	@code
 *	NMEAutoconvert const autoconverts[] =
 *	{
 *		NMEAutoconvertCamelCaseEntry,
 *		NMEAutoconvertURLEntry,
 *		NMEAutoconvertTableEnd
 *	};
 *	NMEOutputFormat f = NMEOutputFormatHTML;
 *	f.autoconverts = autoconverts;
 *	... NMEProcess(..., &f, ...);
 *	@endcode
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEAutolink__
#define __NMEAutolink__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

/** Autoconvert implementation for camelCase words
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in,out] i index in src (token to parse on input, next token on output)
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEAutoconvert
	@return TRUE for conversion, else FALSE
*/
NMEBoolean NMEAutoconvertCamelCase(NMEConstText src, NMEInt srcLen,
		NMEInt *i,
		NMEContext *context,
		void *userData);

/// NMEAutoconvertCamelCase entry for table of NMEAutoconvert
#define NMEAutoconvertCamelCaseEntry \
	{NMEAutoconvertCamelCase, NULL}

/** Autoconvert implementation for URLs
	@param[in] src source text with markup
	@param[in] srcLen source text length
	@param[in,out] i index in src (token to parse on input, next token on output)
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEAutoconvert
	@return TRUE for conversion, else FALSE
*/
NMEBoolean NMEAutoconvertURL(NMEConstText src, NMEInt srcLen,
		NMEInt *i,
		NMEContext *context,
		void *userData);

/// NMEAutoconvertURL entry for table of NMEAutoconvert
#define NMEAutoconvertURLEntry \
	{NMEAutoconvertURL, NULL}

#ifdef __cplusplus
}
#endif

#endif

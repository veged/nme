/**
 *	@file NMEPluginTOC.h
 *	@brief NME optional plugin for table of contents in HTML.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEPluginTOC__
#define __NMEPluginTOC__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

/** User data for hook (opaque, typically allocated as an automatic (local)
	variable and passed by address to NMESetTOCOutputFormat)
*/
typedef struct
{
	NMEOutputFormat *outputFormat;	///< private
	NMEInt level1;	///< private
	NMEInt level2;	///< private
	NMEBoolean links;	///< private
} HookTOCData;

/** Set output format structure for TOC generation
	@param[in] f address of output format structure, whose contents are filled
	@param[out] d paragraph hook user data
*/
void NMESetTOCOutputFormat(NMEOutputFormat *f, HookTOCData *d);

/** User data of NMEPluginTOCEntry
*/
typedef struct
{
	NMEConstText src;	///< original NME source code
	NMEInt srcLen;	///< length of src
} NMEPluginTocData;

/** Plugin implementation for calendar using table markup (plugin's data:
	year month)
	@param[in] name plugin name, such as "calendar"
	@param[in] nameLen length of name
	@param[in] data data text
	@param[in] dataLen length of data
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEPlugin
	@return error code (kNMEErrOk for success)
	@test @code
	<< toc level
	>>
	@endcode
	or @code
	<< toc level1 level2
	>>
	@endcode
	or @code
	<< toc level1 level2 title
	>>
	@endcode
*/
NMEErr NMEPluginTOC(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// NMEPlugin entry for table of plugins
#define NMEPluginTOCEntry(data) \
	{"toc", kNMEPluginOptBetweenPar, NMEPluginTOC, (void *)data}

#ifdef __cplusplus
}
#endif

#endif

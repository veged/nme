/**
 *	@file NMEPluginRaw.h
 *	@brief NME optional plugin for keeping text unchanged.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEPluginRaw__
#define __NMEPluginRaw__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

/** Plugin implementation for keeping text unchanged
	(useful to embed XML or HTML)
	@param[in] name plugin name, such as "raw"
	@param[in] nameLen length of name
	@param[in] data data text
	@param[in] dataLen length of data
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEPlugin
	@return error code (kNMEErrOk for success)
	@test @code
	<< raw
	<span style="color:#ff0000">RED</span>
	>>
	@endcode
*/
NMEErr NMEPluginRaw(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// NMEPlugin entry for table of plugins
/// (opt should be kNMEPluginOptDefault or kNMEPluginOptBetweenPar)
#define NMEPluginRawEntry(name, opt) \
	{name, opt, NMEPluginRaw, NULL}

#ifdef __cplusplus
}
#endif

#endif

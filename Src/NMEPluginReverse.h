/**
 *	@file NMEPluginReverse.h
 *	@brief NME optional plugin for reversing text.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEPluginReverse__
#define __NMEPluginReverse__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

/** Plugin implementation for reversing text
	@param[in] name plugin name, such as "reverse"
	@param[in] nameLen length of name
	@param[in] data data text
	@param[in] dataLen length of data
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEPlugin
	@return error code (kNMEErrOk for success)
	@test @code
	<< reverse
	abcdef 123456
	>>
	@endcode
*/
NMEErr NMEPluginReverse(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// NMEPlugin entry for table of plugins
#define NMEPluginReverseEntry \
	{"reverse", kNMEPluginOptReparseOutput, NMEPluginReverse, NULL}

#ifdef __cplusplus
}
#endif

#endif

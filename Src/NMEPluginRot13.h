/**
 *	@file NMEPluginRot13.h
 *	@brief NME optional plugin for ROT-13 conversion.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEPluginRot13__
#define __NMEPluginRot13__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

/** Plugin implementation for encrypting or decrypting text with ROT-13
	@param[in] name plugin name, such as "rot13"
	@param[in] nameLen length of name
	@param[in] data data text
	@param[in] dataLen length of data
	@param[in,out] context current context
	@param[in] userData pointer passed from the parser, as specified in NMEPlugin
	@return error code (kNMEErrOk for success)
	@test @code
	<< rot13
	Guvf vf n grfg.
	>>
	@endcode
*/
NMEErr NMEPluginRot13(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// NMEPlugin entry for table of plugins
#define NMEPluginRot13Entry \
	{"rot13", kNMEPluginOptReparseOutput, NMEPluginRot13, NULL}

#ifdef __cplusplus
}
#endif

#endif

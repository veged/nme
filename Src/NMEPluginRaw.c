/**
 *	@file NMEPluginRaw.c
 *	@brief NME optional plugin for keeping text unchanged.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see NME.h) */

#include "NMEPluginRaw.h"

NMEErr NMEPluginRaw(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData)
{
	(void)name;
	(void)nameLen;
	(void)userData;
	
	if (!NMEAddString(data, dataLen, '\0', context))
		return kNMEErrNotEnoughMemory;
	
	return kNMEErrOk;
}

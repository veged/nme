/**
 *	@file NMEPluginReverse.c
 *	@brief NME optional plugin for reversing text.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see NME.h) */

#include "NMEPluginReverse.h"

NMEErr NMEPluginReverse(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData)
{
	NMEInt i;
	(void)name;
	(void)nameLen;
	(void)userData;
	
	for (i = 0; i < dataLen; i++)
		if (!NMEAddString(&data[dataLen - 1 - i], 1, '\0', context))
			return kNMEErrNotEnoughMemory;
	
	return kNMEErrOk;
}

/**
 *	@file NMEPluginUppercase.c
 *	@brief NME optional plugin for cnverting text to uppercase.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see header file) */

#include "NMEPluginUppercase.h"

NMEErr NMEPluginUppercase(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData)
{
	NMEInt i;
	NMEChar c;
	(void)name;
	(void)nameLen;
	(void)userData;
	
	for (i = 0; i < dataLen; i++)
	{
		c = data[i] >= 'a' && data[i] <= 'z'
				? data[i] + 'A' - 'a' : data[i];
		if (!NMEAddString(&c, 1, '\0', context))
			return kNMEErrNotEnoughMemory;
	}
	
	return kNMEErrOk;
}

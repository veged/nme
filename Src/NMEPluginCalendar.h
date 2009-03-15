/**
 *	@file NMEPluginCalendar.h
 *	@brief NME optional plugin for calendar.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */
 
/* License: new BSD license (see NME.h) */

#ifndef __NMEPluginCalendar__
#define __NMEPluginCalendar__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"

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
	<< calendar
	2007 4
	>>
	@endcode
*/
NMEErr NMEPluginCalendar(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData);

/// NMEPlugin entry for table of plugins
#define NMEPluginCalendarEntry \
	{"calendar", kNMEPluginOptReparseOutput | kNMEPluginOptBetweenPar, \
		NMEPluginCalendar, NULL}

#ifdef __cplusplus
}
#endif

#endif

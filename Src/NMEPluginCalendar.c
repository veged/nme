/**
 *	@file NMEPluginCalendar.c
 *	@brief NME optional plugin for calendar.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see header file) */

#include "NMEPluginCalendar.h"
#include <string.h>
#include <stdio.h>

NMEErr NMEPluginCalendar(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData)
{
	char *endp;
	int year, month;
	int weekday;	// weekday of first day of month (0=monday)
	int m1, y1;	// temp. values used to compute weekday
	int d, dmax;
	char str[16];
	static int const nd[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	(void)name;
	(void)nameLen;
	(void)userData;
	
	// read year and month in data
	year = strtol(data, &endp, 0);
	month = strtol(endp, NULL, 0);
	if (year < 0)
		year = 0;
	if (month < 1 || month > 12)
		month = 1;
	
	// find week day of 1st day of month
	m1 = (month - 14) / 12;
	y1 = year + 4800;
	weekday = (1461 * (y1 + m1) / 4 + 367 * (month - 2 - 12 * m1) / 12
				- 3 * ((y1 + m1 + 100) / 100) / 4 - 32074) % 7;
	// write calendar to dest: "\n|=Mon|=Tue...\n|||1|2|..."
	if (!NMEAddString("\n|=Mon|=Tue|=Wed|=Thu|=Fri|=Sat|=Sun\n", -1,
			'\0', context))
		return kNMEErrNotEnoughMemory;
	dmax = nd[month - 1];
	if (month == 2 && (year % 4 == 0 && year % 100 != 0 || year % 400 == 0))
		dmax = 29;
	for (d = 0; d < weekday; d++)
		if (!NMEAddString("|", 1, '\0', context))
			return kNMEErrNotEnoughMemory;
	for (d = 1; d <= dmax; d++)
	{
		sprintf(str, "|%d", d);
		if (!NMEAddString(str, -1, '\0', context))
			return kNMEErrNotEnoughMemory;
		if ((d + weekday) % 7 == 0)
			if (!NMEAddString("\n", 1, '\0', context))
				return kNMEErrNotEnoughMemory;
	}
	if (!NMEAddString("\n\n", 2, '\0', context))
		return kNMEErrNotEnoughMemory;
	
	return kNMEErrOk;
}

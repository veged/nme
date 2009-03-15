/**
 *	@file NMEPluginTOC.c
 *	@brief NME optional plugin for table of contents in HTML.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see NME.h) */

#include "NMEPluginTOC.h"

/// Test if a character is a space, tab, cr or lf
#define isBlankOrEol(c) ((c) == ' ' || (c) == '\t' || (c) == 10 || (c) == 13)

static void switchOutputFormatTOC(HookTOCData *hookData,
		NMEBoolean heading);

/** Paragraph hook for TOC (switch output on for titles and off for everything else)
	@param[in] level heading or list level (1 = topmost)
	@param[in] item list item or heading counter
	@param[in] enter TRUE when entering construct, FALSE when exiting
	@param[in] markup null-terminated string for initial markup ("p" for par,
	"*" for unnumbered list of any level, "//" for italic, "{{{" for preformatted)
	@param[in] srcIndex current index in source code
	@param[in,out] context current context
	@param[in,out] data value specific to the callback
	@return error code (kNMEErrOk for success)
*/
static NMEErr parHookTOC(NMEInt level,
		NMEInt item,
		NMEBoolean enter,
		NMEConstText markup,
		NMEInt srcIndex,
		NMEContext *context,
		void *data)
{
	HookTOCData *d = (HookTOCData *)data;
	
	if (markup[0] == '=' && level >= d->level1 && level <= d->level2)
		switchOutputFormatTOC(d, enter);
	
	return kNMEErrOk;
}

/** Switch output on for titles and off for everything else)
	@param[in,out] hookData hook user data
	@param[in] heading TRUE for heading start, FALSE for heading end
*/
static void switchOutputFormatTOC(HookTOCData *hookData,
		NMEBoolean heading)
{
	if (heading)
	{
		*hookData->outputFormat = NMEOutputFormatHTML;
		hookData->outputFormat->beginHeading
				= "%%{l}&nbsp;%%<a href=\"#h%{o}\">%%{i>0}%{i}. %%";
		hookData->outputFormat->endHeading = "</a><br />\n";
		hookData->outputFormat->lineBreak = " ";
		hookData->outputFormat->beginLink = "";
		hookData->outputFormat->endLink = "";
		hookData->outputFormat->sepLink = NULL;
		hookData->outputFormat->beginImage = "";
		hookData->outputFormat->endImage = "";
		hookData->outputFormat->sepImage = NULL;
	}
	else
	{
		*hookData->outputFormat = NMEOutputFormatNull;
		hookData->outputFormat->beginDoc
				= "<html>\n<body>\n<p%%{s>0} style=\"font-size:%{s}pt\"%%>\n";
		hookData->outputFormat->endDoc = "</p>\n</body>\n</html>\n";
	}
	hookData->outputFormat->parHookFun = parHookTOC;
	hookData->outputFormat->hookData = (void *)hookData;
}

void NMESetTOCOutputFormat(NMEOutputFormat *f, HookTOCData *d)
{
	d->level1 = 1;
	d->level2 = 4;
	d->links = TRUE;
	d->outputFormat = f;
	switchOutputFormatTOC(d, FALSE);
}

NMEErr NMEPluginTOC(NMEConstText name, NMEInt nameLen,
		NMEConstText data, NMEInt dataLen,
		NMEContext *context,
		void *userData)
{
	char *endp;
	HookTOCData hookData;
	NMEText title, buf, dest;
	NMEInt titleLen, bufLen, destLen;
	NMEOutputFormat outputFormat;
	NMEInt options, fontSize;
	NMEErr err;
	(void)name;
	(void)nameLen;
	
	NMESetTOCOutputFormat(&outputFormat, &hookData);
	
	// read levels in data
	hookData.level1 = strtol(data, &endp, 0);
	hookData.level2 = strtol(endp, &endp, 0);
	if (hookData.level1 < 1)
		return kNMEErrOk;
	if (hookData.level2 < hookData.level1)
		hookData.level2 = hookData.level1;
	
	// read TOC title
	for (title = endp; title < data + dataLen && isBlankOrEol(title[0]); title++)
		;
	for (titleLen = dataLen - (title - data);
			titleLen > 0 && isBlankOrEol(title[titleLen - 1]);
			titleLen--)
		;
	
	// make TOC
	NMEGetTempMemory(context, &buf, &bufLen);
	NMEGetFormat(context, NULL, &options, &fontSize);
	err = NMEProcess(((NMEPluginTocData *)userData)->src,
			((NMEPluginTocData *)userData)->srcLen,
			buf, bufLen,
			options | kNMEProcessOptNoPreAndPost, "\n", &outputFormat, fontSize,
			&dest, &destLen, NULL);
	if (err != kNMEErrOk)
		return err;
	
	// write TOC title, if any
	if (titleLen > 0)
		if (!NMEAddString("<h2%%{s>0} style=\"font-size:%{2*s}pt\"%%>", -1, '%', context)
				|| !NMEAddString(title, titleLen, '\0', context)
				|| !NMEAddString("</h2>\n", -1, '%', context))
			return kNMEErrNotEnoughMemory;
	
	// write TOC to output
	if (!NMEAddString("<p%%{s>0} style=\"font-size:%{s}pt\"%%>\n", -1, '%', context)
			|| !NMEAddString(dest, destLen, '\0', context)
			|| !NMEAddString("</p>\n", -1, '%', context))
		return kNMEErrNotEnoughMemory;
	
	return kNMEErrOk;
}

/**
 *	@file NMEMFC.cpp
 *	@brief Support for MFC (Windows) .
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see file NME.h) */

#include "NMEMFC.h"
#include "NMEStyleCpp.h"

/// Test if a byte is the first byte of a UTF-8 character
#define isFirstUTF8Byte(c) \
	(((c) & 0x80) == 0 || ((c) & 0xe0) == 0xc0 || ((c) & 0xf0) == 0xe0)

/** Convert unicode characters encoded as 16-bit UTF-8 to a CString.
	@param[in] utf8 UTF-8 string
	@param[in] len length of utf8 in bytes, or -1 if null-terminated
*/
static CString utf8ToCString(char const *utf8, int len = -1)
{
	int i, j, wlen;
	WCHAR *wstr;
	CString s;
	
	// count len
	if (len < 0)
		for (len = 0; utf8[len]; len++)
			;
	
	// count characters
	for (i = wlen = 0; i < len; i++)
		if (isFirstUTF8Byte(utf8[i]))
			wlen++;
	
	// allocate room for null-terminated array of WCHAR
	wstr = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));

	// convert string
	for (i = j = 0; j < len; i++)
	{
		if ((utf8[j] & 0x80) == 0)	// 7 bits
			wstr[i] = (WCHAR)utf8[j++];
		else if (j + 1 < len	// 11 bits
			&& (utf8[j] & 0xe0) == 0xc0 && (utf8[j + 1] & 0xc0) == 0x80)
		{
			wstr[i] = ((WCHAR)utf8[j++] & 0x1f) << 6;
			wstr[i] |= utf8[j++] & 0x3f;
		}
		else if (j + 2 < len	// 16 bits
			&& (utf8[j] & 0xf0) == 0xe0 && (utf8[j + 1] & 0xc0) == 0x80
			&& (utf8[j + 2] & 0xc0) == 0x80)
		{
			wstr[i] = (WCHAR)utf8[j++] << 12;
			wstr[i] |= (utf8[j++] & 0x3f) << 6;
			wstr[i] |= utf8[j++] & 0x3f;
		}
		else
		{
			j++;	// ignore
			i--;
		}
	}
	wstr[i] = 0;

	// make CString
	s = wstr;

	// free array of WCHAR
	free((void *)wstr);
	
	return s;
}

/** Convert UCS-16 string to UTF-8.
	@param[in] wstr string with 16-bit unicode char (UCS-16)
	@param[in] len number of characters in wstr (-1 if null-terminated)
	@return newly allocated null-terminated string converted to UTF-8
	(should be freed with delete [])
*/
static char *cStringToUtf8(WCHAR const *wstr, int len = -1)
{
	int len8;	// number of UTF-8 bytes, excluding ending null
	int i, j;
	char *s;
	
	// count number of UTF-8 bytes
	for (i = len8 = 0; len < 0 ? wstr[i] : i < len; i++)
		if (!(wstr[i] & 0xff80))
			len8++;
		else if (!(wstr[i] & 0xf800))
			len8 += 2;
		else
			len8 += 3;
	
	// alloc output string
	s = new char [len8 + 1];
	if (!s)
		return s;
	
	// convert to UTF-8
	for (i = j = 0; len < 0 ? wstr[i] : i < len; i++)
		if (!(wstr[i] & 0xff80))
			s[j++] = (char)wstr[i];
		else if (!(wstr[i] & 0xf800))
		{
			s[j++] = 0xc0 | (wstr[i] >> 6);
			s[j++] = 0x80 | wstr[i] & 0x3f;
		}
		else
		{
			s[j++] = 0xe0 | (wstr[i] >> 12) & 0x0f;
			s[j++] = 0x80 | (wstr[i] >> 6) & 0x3f;
			s[j++] = 0x80 | wstr[i] & 0x3f;
		}
	
	// add ending null and return
	s[j] = '\0';
	return s;
}

void NMEMFCSetRichText(CRichEditCtrl &c,
		char const *input, int inputLength,
		bool replaceSel,
		CHARFORMAT const *plainTextCharFormat,
		bool links)
{
	// convert input to text + style table
	NMEStyle nme(input, inputLength);
	NMEOutputFormat f = NMEOutputFormatBasicText;
	f.parHookFun = NMEStyleSpanHook;
	if (links)
		f.sepLink = "|";
	nme.setFormat(f);
#if defined(_UNICODE)
	nme.setUnicodeStyleOffsets(TRUE);
#endif
	NMEConstText output;
	NMEInt outputLength;
	if (nme.getOutput(&output, &outputLength) != kNMEErrOk)
		return;
	NMEStyleTable const *styleTable = nme.getStyleTable();
	
	// replace whole text or selection
	long offset = 0;
	long length = outputLength;
#if defined(_UNICODE)
	CString str = utf8ToCString(output, outputLength);
	length = str.GetLength();
	if (replaceSel)
	{
		long endSel;	// ignored
		
		c.GetSel(offset, endSel);
		c.ReplaceSel(str);
	}
	else
		c.SetWindowText(str);
#else
	length = outputLength;
	if (replaceSel)
	{
		long endSel;	// ignored
		
		c.GetSel(offset, endSel);
		c.ReplaceSel(CString(output, outputLength));
	}
	else
		c.SetWindowText(CString(output, outputLength));
#endif
	
	// apply style
	CHARFORMAT cf;
	PARAFORMAT pf;
	if (plainTextCharFormat)
		cf = *plainTextCharFormat;
	else
		c.GetDefaultCharFormat(cf);
	c.SetSel(offset, offset + length);
	c.SetSelectionCharFormat(cf);
	cf.cbSize = sizeof(cf);
	pf.cbSize = sizeof(pf);
	for (int i = 0; i < styleTable->n; i++)
	{
		cf.dwMask = 0;
		pf.dwMask = 0;
		switch (styleTable->span[i].style)
		{
			case kNMEStyleCharBold:
			case kNMEStyleCharDT:
			case kNMEStyleCharTH:
				cf.dwMask = CFM_BOLD;
				cf.dwEffects = CFE_BOLD;
				break;
			case kNMEStyleCharItalic:
				cf.dwMask = CFM_ITALIC;
				cf.dwEffects = CFE_ITALIC;
				break;
			case kNMEStyleCharUnderline:
				cf.dwMask = CFM_UNDERLINE;
				cf.dwEffects = CFE_UNDERLINE;
				break;
			case kNMEStyleCharSuperscript:
				cf.dwMask = CFM_OFFSET;
				cf.yOffset = 100;
				break;
			case kNMEStyleCharSubscript:
				cf.dwMask = CFM_OFFSET;
				cf.yOffset = -100;
				break;
			case kNMEStyleCharMonospace:
				cf.dwMask = CFM_FACE;
				strcpy(cf.szFaceName, "Courier");
				break;
			case kNMEStyleCharLink:
				if (links)
				{
					// hide target and set both target and link to CFM_LINK
					CHARFORMAT2 c2;
					int j;
#if defined(_UNICODE)
					for (j = 0; j < length && str.GetAt(styleTable->span[i].begin + j) != _T('|'); j++)
						;
#else
					for (j = 0; j < length && output[j] != '|'; j++)
						;
#endif
					if (j < length)
						j++;
					c2.cbSize = sizeof(c2);
					c2.dwMask = CFM_HIDDEN;
					c2.dwEffects = CFE_HIDDEN;
					c.SetSel(offset + styleTable->span[i].begin, offset + styleTable->span[i].begin + j);
					::SendMessage(c.m_hWnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&c2);
					cf.dwMask = CFM_LINK;
					cf.dwEffects = CFE_LINK;
					c.SetSel(offset + styleTable->span[i].begin, offset + styleTable->span[i].end);
					c.SetSelectionCharFormat(cf);
					cf.dwMask = 0;
				}
				break;
			case kNMEStyleParPlain:
				pf.dwMask = PFM_OFFSETINDENT | PFM_OFFSET;
				pf.dxStartIndent = 300;
				pf.dxOffset = -300;
				break;
			case kNMEStyleParIndentedPar:
				pf.dwMask = PFM_OFFSETINDENT | PFM_OFFSET;
				pf.dxStartIndent = 600 * styleTable->span[i].level + 300;
				pf.dxOffset = -300;
				break;
			case kNMEStyleParHeading:
				cf.dwMask = CFM_SIZE;
				cf.yHeight = 20 * (20 - 2 * styleTable->span[i].level);
				if (styleTable->span[i].level == 1)
				{
					pf.dwMask = PFM_ALIGNMENT;
					pf.wAlignment = PFA_CENTER;
				}
				break;
		}
		if (cf.dwMask != 0 || pf.dwMask != 0)
			c.SetSel(offset + styleTable->span[i].begin, offset + styleTable->span[i].end);
		if (cf.dwMask != 0)
			c.SetSelectionCharFormat(cf);
		if (pf.dwMask != 0)
			c.SetParaFormat(pf);
	}
	
	c.SetSel(0, 0);	// don't leave the last span selected
}

void NMEMFCSetRichText(CRichEditCtrl &c,
		WCHAR const *input, int inputLength,
		bool replaceSel,
		CHARFORMAT const *plainTextCharFormat,
		bool links)
{
	char *str8 = cStringToUtf8(input, inputLength);
	NMEMFCSetRichText(c, str8, -1, replaceSel, plainTextCharFormat, links);
	delete [] str8;
}

void NMEMFCEnLink(NMHDR const *pNMHDR, LRESULT *pResult,
		NMEMFCLinkFun linkFun, void *linkFunData)
{
	ENLINK *event = (ENLINK *)pNMHDR;
	if (event->msg == WM_LBUTTONDOWN)
	{
		// extract link (8-bit char!)
		TEXTRANGE range;
		range.chrg = event->chrg;
		range.lpstrText = new char[range.chrg.cpMax - range.chrg.cpMin + 1];
		::SendMessage(pNMHDR->hwndFrom, EM_GETTEXTRANGE, 0 , (LPARAM)&range);
		CString s(range.lpstrText);
		if (s.Find('|') >= 0)
			s = s.Left(s.Find('|'));
#if defined(_UNICODE)
		char *url = cStringToUtf8(s);
#else
		char const *url = s;
#endif
		if (url && linkFun)
			linkFun(url, linkFunData);
#if defined(_UNICODE)
		if (url)
			delete [] url;
#endif
		ShellExecute(NULL, _T("open"), s, NULL, NULL, SW_SHOWNORMAL);
		delete [] range.lpstrText;
	}
	*pResult = 0;
}

void NMEMFCLinkFunURL(NMEConstText link, void *data)
{
	ShellExecute(NULL, _T("open"), utf8ToCString(link), NULL, NULL, SW_RESTORE);
}

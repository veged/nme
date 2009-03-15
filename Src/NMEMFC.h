/**
 *	@file NMEMFC.h
 *	@brief Support for MFC (Windows) .
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet
 *
 *	@section Links Links
 *
 *	Here is how to support links in a CRichEditView (assuming the subclass name
 *	is S and the control id is ID):
 *	- Enable links in method S::OnCreate, e.g. with
 *	@code
 *	GetRichEditCtrl().SetEventMask(GetRichEditCtrl().GetEventMask() | ENM_LINK);
 *	@endcode
 *	- Add the following protected method prototype in the header file:
 *	@code
 *	afx_msg void OnEnLink(NMHDR* pNMHDR, LRESULT* pResult);
 *	@endcode
 *	- Add the following method definition to CRichEditView's subclass (you can write
 *	your own NMEMFCLinkFun function if you want to process links in a special way):
 *	@code
 *	void S::OnEnLink(NMHDR* pNMHDR, LRESULT* pResult)
 *	{
 *	  NMEMFCEnLink(pNMHDR, pResult, NMEMFCLinkFunURL);
 *	}
 *	@endcode
 *	- Add the following line between BEGIN_MESSAGE_MAP and END_MESSAGE_MAP:
 *	@code
 *	ON_NOTIFY(EN_LINK, ID, OnEnLink)
 *	@endcode
 *	- call NMEMFCSetRichText with argument link set to TRUE
 *
 *	You should replace ON_NOTIFY with ON_NOTIFY_RANGE and define
 *	S::OnEnLinkRange(UINT id, NMHDR* pNMHDR, LRESULT* pResult) if you have to
 *	handle multiple CRichEditView controls with one method.
 */
 
#ifndef __NMEMFC__
#define __NMEMFC__

#include "NME.h"
#include "NMEStyle.h"

#include <afxrich.h>

/** Callback to handle link clicks.
	@param[in] link raw link, as in NME source text, without leading and
	trailing spaces (null-terminated string)
	@param[in,out] data data value specific to the callback
*/
typedef void (*NMEMFCLinkFun)(NMEConstText link, void *data);

/** Replace selection in a CRichEditCtrl with NME text converted
	to styled text.
	@param[in,out] c MFC rich text control
	@param[in] input text with NME markup
	@param[in] inputLength length of input, or negative if input is
	null-terminated
	@param[in] replaceSel if TRUE, replace selection, else replace whole text
	@param[in] plainTextCharFormat character format of plain text (if NULL, use
	default character format of c)
	@param[in] links TRUE to enable links, else FALSE
*/
void NMEMFCSetRichText(CRichEditCtrl &c,
		char const *input, int inputLength = -1,
		bool replaceSel = FALSE,
		CHARFORMAT const *plainTextCharFormat = NULL,
		bool links = FALSE);

/** Replace selection in a CRichEditCtrl with NME text converted
	to styled text.
	@param[in,out] c MFC rich text control
	@param[in] input text as UCS-16 with NME markup
	@param[in] inputLength length of input, or negative if input is
	null-terminated
	@param[in] replaceSel if TRUE, replace selection, else replace whole text
	@param[in] plainTextCharFormat character format of plain text (if NULL, use
	default character format of c)
	@param[in] links TRUE to enable links, else FALSE
*/
void NMEMFCSetRichText(CRichEditCtrl &c,
		WCHAR const *input, int inputLength = -1,
		bool replaceSel = FALSE,
		CHARFORMAT const *plainTextCharFormat = NULL,
		bool links = FALSE);

/**	Handle links (should be called when an EN_LINK notification is received).
	@param[in] pNMHDR notification message
	@param[out] pResult result code
	@param[in] linkFun callback which handles link clicks
	@param[in] linkFunData value passed to linkFun
*/
void NMEMFCEnLink(NMHDR const *pNMHDR, LRESULT* pResult,
		NMEMFCLinkFun linkFun, void *linkFunData = NULL);

/**	NMEMFCLinkFun callback to let Windows process URL (can be used as is
	or called from a user-written callback which recognizes other kinds of
	links).
	@param[in] link raw URL, as in NME source text, without leading and
	trailing spaces (null-terminated string)
	@param[in,out] data not used
*/
void NMEMFCLinkFunURL(NMEConstText link, void *data = NULL);

#endif

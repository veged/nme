/**
 *	@file NMEGtk.h
 *	@brief Support for GTK+ .
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet
 */
 
#ifndef __NMEGtk__
#define __NMEGtk__

#ifdef __cplusplus
extern "C" {
#endif

#include "NME.h"
#include "NMEStyle.h"

#include <gtk/gtk.h>

#define kMaxHeadingLevel 4
#define kMaxListLevel 4

/** Callback to handle link clicks.
	@param[in] link raw link, as in NME source text, without leading and
	trailing spaces (null-terminated string)
	@param[in,out] data data value specific to the callback
*/
typedef void (*NMEGtkLinkFun)(NMEConstText link, void *data);

/// Private structure
typedef struct
{
	NMEGtkLinkFun fun;	///< private
	void *data;	///< private
} NMEGtkLinkCB;

/// Private structure
typedef struct
{
	GtkTextBuffer *textBuffer;	///< private
	NMEGtkLinkCB *linkCB;	///< private
	GtkTextTag *plainTag;	///< private
	GtkTextTag *boldTag;	///< private
	GtkTextTag *italicTag;	///< private
	GtkTextTag *underlineTag;	///< private
	GtkTextTag *superTag;	///< private
	GtkTextTag *subTag;	///< private
	GtkTextTag *monoTag;	///< private
	GtkTextTag *headingTag[kMaxHeadingLevel];	///< private
	GtkTextTag *parTag;	///< private
	GtkTextTag *indentTag[kMaxListLevel];	///< private
} NMEGtk;

/** Initialize NMEGtk data structure.
	@param[out] nmegtk data structure used by NMEGtkApplyStyle
	@param[in] textBuffer text buffer style is applied to
	@param[in] charSize standard text character size, used as the basis for
	heading sizes and superscript/subscript offsets
*/
void NMEGtkInit(NMEGtk *nmegtk, GtkTextBuffer *textBuffer, int charSize);

/** Set the callback which processes clicks on links
	@param[in,out] nmegtk data structure used by NMEGtkApplyStyle
	@param[in] linkFun callback which handles link clicks
	@param[in] linkFunData value passed to linkFun
*/
void NMEGtkSetLinkFun(NMEGtk *nmegtk, NMEGtkLinkFun linkFun, void *linkFunData);

/** Apply styles collected by NMEStyle to a GTK+ text buffer
	@param[in] nmegtk data structure initialized by NMEGtkInit
	@param[in] spanTable table of style spans created by NMEProcess with NMEStyle
	@param[in] offset offset of NME output in textBuffer, in unicode characters
	@param[in] length length of NME output in unicode characters
	@param[in] nmeTextForLinks set hypertext links if source NME text, don't if NULL
*/
void NMEGtkApplyStyle(NMEGtk const *nmegtk,
		NMEStyleTable const *spanTable,
		NMEInt offset, NMEInt length,
		NMEConstText nmeTextForLinks);

/** Insert text with NME markup in a GtkTextBuffer, calling NMEGtkApplyStyle.
	@param[in,out] textBuffer GTK+ text buffer
	@param[in] nmegtk NMEGtk structure for textBuffer, initialized with
	NMEGtkInit
	@param[in] str text with NME markup
	@param[in] len length of str in bytes, or -1 if str is null-terminated
	@param[in] replaceSel if TRUE, replace selection, else replace whole text
	@param[in] links set hypertext links if TRUE
*/
NMEErr NMEGtkInsert(GtkTextBuffer *textBuffer,
		NMEGtk const *nmegtk,
		NMEConstText str, NMEInt len,
		NMEBoolean replaceSel,
		NMEBoolean links);

#ifdef __cplusplus
}
#endif

#endif

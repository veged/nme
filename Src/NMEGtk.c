/**
 *	@file NMEGtk.c
 *	@brief Support for GTK+ .
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see file NME.h) */

#include "NMEGtk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GdkCursor *textCursor = NULL;	///< cursor over nonlink text
static GdkCursor *linkCursor = NULL;	///< cursor over links

void NMEGtkInit(NMEGtk *nmegtk, GtkTextBuffer *textBuffer,
		int charSize)
{
	int i;
	
	nmegtk->textBuffer = textBuffer;
	nmegtk->linkCB = NULL;
	
	nmegtk->plainTag = gtk_text_buffer_create_tag(textBuffer, "plain", NULL);
	g_object_set(G_OBJECT(nmegtk->plainTag), "font", "serif", NULL);
	nmegtk->boldTag = gtk_text_buffer_create_tag(textBuffer, "bold", NULL);
	g_object_set(G_OBJECT(nmegtk->boldTag), "weight", PANGO_WEIGHT_BOLD, NULL);
	nmegtk->italicTag = gtk_text_buffer_create_tag(textBuffer, "italic", NULL);
	g_object_set(G_OBJECT(nmegtk->italicTag), "style", PANGO_STYLE_ITALIC, NULL);
	nmegtk->underlineTag = gtk_text_buffer_create_tag(textBuffer, "underline", NULL);
	g_object_set(G_OBJECT(nmegtk->underlineTag), "underline", PANGO_UNDERLINE_SINGLE, NULL);
	nmegtk->superTag = gtk_text_buffer_create_tag(textBuffer, "superscript", NULL);
	g_object_set(G_OBJECT(nmegtk->superTag), "rise", charSize / 2, NULL);
	nmegtk->subTag = gtk_text_buffer_create_tag(textBuffer, "subscript", NULL);
	g_object_set(G_OBJECT(nmegtk->subTag), "rise", -charSize / 3, NULL);
	nmegtk->monoTag = gtk_text_buffer_create_tag(textBuffer, "monospace", NULL);
	g_object_set(G_OBJECT(nmegtk->monoTag), "font", "monospace", NULL);
	nmegtk->headingTag[0] = gtk_text_buffer_create_tag(textBuffer, "h1", NULL);
	g_object_set(G_OBJECT(nmegtk->headingTag[0]),
			"weight", PANGO_WEIGHT_BOLD,
			"scale", PANGO_SCALE_XX_LARGE,
			"pixels-above-lines", charSize / PANGO_SCALE,
			"justification", GTK_JUSTIFY_CENTER,
			NULL);
	nmegtk->headingTag[1] = gtk_text_buffer_create_tag(textBuffer, "h2", NULL);
	g_object_set(G_OBJECT(nmegtk->headingTag[1]),
			"weight", PANGO_WEIGHT_BOLD,
			"scale", PANGO_SCALE_X_LARGE,
			"pixels-above-lines", 2 * charSize / 3 / PANGO_SCALE,
			NULL);
	nmegtk->headingTag[2] = gtk_text_buffer_create_tag(textBuffer, "h3", NULL);
	g_object_set(G_OBJECT(nmegtk->headingTag[2]),
			"weight", PANGO_WEIGHT_BOLD,
			"scale", PANGO_SCALE_LARGE,
			"pixels-above-lines", charSize / 2 / PANGO_SCALE,
			NULL);
	nmegtk->headingTag[3] = gtk_text_buffer_create_tag(textBuffer, "h4", NULL);
	g_object_set(G_OBJECT(nmegtk->headingTag[3]), "weight", PANGO_WEIGHT_BOLD,
			"pixels-above-lines", charSize / 3 / PANGO_SCALE,
			NULL);
	nmegtk->parTag = gtk_text_buffer_create_tag(textBuffer, "plainpar", NULL);
	g_object_set(G_OBJECT(nmegtk->parTag), "indent", 2 * charSize / PANGO_SCALE, NULL);
	for (i = 0; i < kMaxListLevel; i++)
	{
		char name[16];
		
		sprintf(name, "indent%d", i + 1);
		nmegtk->indentTag[i] = gtk_text_buffer_create_tag(textBuffer, name, NULL);
		g_object_set(G_OBJECT(nmegtk->indentTag[i]),
				"left-margin", (3 * i + 1) * charSize / PANGO_SCALE,
				"indent", -charSize / PANGO_SCALE,
				NULL);
	}
	
	// initialize cursors
	if (!linkCursor)
		linkCursor = gdk_cursor_new(GDK_LEFT_PTR);
	if (!textCursor)
		textCursor = gdk_cursor_new(GDK_XTERM);
}

void NMEGtkSetLinkFun(NMEGtk *nmegtk, NMEGtkLinkFun linkFun, void *linkFunData)
{
	nmegtk->linkCB = g_malloc(sizeof(NMEGtkLinkCB));
	nmegtk->linkCB->fun = linkFun;
	nmegtk->linkCB->data = linkFunData;
	g_object_set_data_full(G_OBJECT(nmegtk->textBuffer), "link_cb",
			nmegtk->linkCB, g_free);
}

/**	Signal handler for GTK+ link events.
	@param[in] tag GTK+ tag of the link
	@param[in] w GTK+ textview widget
	@param[in] event event description
	@param[in] iter location of the event in the textbuffer
	@param[in] d adress of a NMEGtkLinkCB structure
*/
static gboolean linkEvent(GtkTextTag *tag, GtkWidget *w,
		GdkEventButton *event, GtkTextIter *iter,
		gpointer d)
{
	NMEGtkLinkCB *l = (NMEGtkLinkCB *)d;
	
	switch (event->type)
	{
		case GDK_BUTTON_RELEASE:
			if (l)
			{
				l->fun(g_object_get_data(G_OBJECT(tag), "link_url"),
						l->data);
				return TRUE;
			}
			break;
// what follows doesn't work...
		case GDK_ENTER_NOTIFY:
			if (linkCursor)
				gdk_window_set_cursor(gtk_widget_get_parent_window(w), linkCursor);
			return TRUE;
		case GDK_LEAVE_NOTIFY:
			if (textCursor)
				gdk_window_set_cursor(gtk_widget_get_parent_window(w), textCursor);
			return TRUE;
	}
	
	return TRUE;	// FALSE; (TRUE to avoid assertion error)
}

void NMEGtkApplyStyle(NMEGtk const *nmegtk,
		NMEStyleTable const *spanTable,
		NMEInt offset, NMEInt length,
		NMEConstText nmeTextForLinks)
{
	GtkTextIter start, end;
	GtkTextTag *tag;
	int i;
	
	gtk_text_buffer_get_iter_at_offset(nmegtk->textBuffer, &start,
			offset);
	gtk_text_buffer_get_iter_at_offset(nmegtk->textBuffer, &end,
			offset + length);
	gtk_text_buffer_apply_tag(nmegtk->textBuffer, nmegtk->plainTag, &start, &end);
	
	for (i = 0; i < spanTable->n; i++)
	{
		switch (spanTable->span[i].style)
		{
			case kNMEStyleCharBold:
			case kNMEStyleCharDT:
			case kNMEStyleCharTH:
				tag = nmegtk->boldTag;
				break;
			case kNMEStyleCharItalic:
				tag = nmegtk->italicTag;
				break;
			case kNMEStyleCharUnderline:
				tag = nmegtk->underlineTag;
				break;
			case kNMEStyleCharSuperscript:
				tag = nmegtk->superTag;
				break;
			case kNMEStyleCharSubscript:
				tag = nmegtk->subTag;
				break;
			case kNMEStyleCharMonospace:
				tag = nmegtk->monoTag;
				break;
			case kNMEStyleParHeading:
				if (spanTable->span[i].level <= kMaxHeadingLevel)
					tag = nmegtk->headingTag[spanTable->span[i].level - 1];
				else
					tag = nmegtk->headingTag[kMaxHeadingLevel - 1];
				break;
			case kNMEStyleParPlain:
				tag = nmegtk->parTag;
				break;
			case kNMEStyleParUL:
			case kNMEStyleParOL:
			case kNMEStyleParDL:
			case kNMEStyleParDT:
			case kNMEStyleParIndentedPar:
				if (spanTable->span[i].level <= kMaxListLevel)
					tag = nmegtk->indentTag[spanTable->span[i].level - 1];
				else
					tag = nmegtk->indentTag[kMaxHeadingLevel - 1];
				break;
			case kNMEStyleCharLink:
				if (nmeTextForLinks)
				{
					tag = gtk_text_buffer_create_tag(nmegtk->textBuffer, NULL,
							"foreground", "blue",
							"underline", PANGO_UNDERLINE_SINGLE,
							NULL);
					g_object_set_data_full(G_OBJECT(tag),
							"link_url",
							g_strndup(nmeTextForLinks + spanTable->span[i].linkOffset,
									spanTable->span[i].linkLength),
							g_free);
					g_signal_connect(G_OBJECT(tag), "event",
							G_CALLBACK(linkEvent), (gpointer)nmegtk->linkCB);
					break;
				}
				// else no hyperlink
			default:
				tag = NULL;
				break;
		}
		
		if (tag)
		{
			gtk_text_buffer_get_iter_at_offset(nmegtk->textBuffer, &start,
					spanTable->span[i].begin + offset);
			gtk_text_buffer_get_iter_at_offset(nmegtk->textBuffer, &end,
					spanTable->span[i].end + offset);
			gtk_text_buffer_apply_tag(nmegtk->textBuffer, tag, &start, &end);
		}
	}
}

NMEErr NMEGtkInsert(GtkTextBuffer *textBuffer,
		NMEGtk const *nmegtk,
		NMEConstText str, NMEInt len,
		NMEBoolean replaceSel,
		NMEBoolean links)
{
	NMEText buf, dest;
	NMEInt bufSize, destLen, destLenUCS16;
	NMEOutputFormat f;
	NMEErr err;
	int length;
	GtkTextIter iter;
	
	if (len < 0)
		len = strlen(str);
	
	bufSize = 1024 + 2 * len;
	f = NMEOutputFormatBasicText;
	f.spanHookFun = NMEStyleSpanHook;
	f.parHookFun = NMEStyleSpanHook;
	
tryAgain:
	buf = malloc(bufSize);
	if (!buf)
		return kNMEErrNotEnoughMemory;
	f.hookData = malloc(bufSize);
	if (!f.hookData)
	{
		free((void *)buf);
		return kNMEErrNotEnoughMemory;
	}
	NMEStyleInit((NMEStyleTable *)f.hookData, bufSize, TRUE);
	
	err = NMEProcess(str, len, buf, bufSize,
			kNMEProcessOptDefault, "\n", &f, 0,
			&dest, &destLen, &destLenUCS16);
	if (err == kNMEErrNotEnoughMemory || err == kNMEErrStyleTableTooSmall)
	{
		free((void *)buf);
		free((void *)f.hookData);
		if (bufSize < 65536 + 10 * len)
		{
			bufSize *= 2;
			goto tryAgain;
		}
		else
			return kNMEErrNotEnoughMemory;
	}
	
	if (replaceSel)
	{
		length = gtk_text_buffer_get_char_count(textBuffer);
		gtk_text_buffer_get_iter_at_offset(textBuffer, &iter, length);
		gtk_text_buffer_insert(textBuffer, &iter, dest, destLen);
		NMEGtkApplyStyle(nmegtk, (NMEStyleTable *)f.hookData,
				length, destLenUCS16, links ? str : NULL);
	}
	else
	{
		gtk_text_buffer_set_text(textBuffer, dest, destLen);
		NMEGtkApplyStyle(nmegtk, (NMEStyleTable *)f.hookData,
				0, destLenUCS16, links ? str : NULL);
	}
	
	free((void *)buf);
	free((void *)f.hookData);
	
	return kNMEErrOk;
}

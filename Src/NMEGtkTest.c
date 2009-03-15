/**
 *	@file NMEGtkTest.c
 *	@brief NME viewer for GTK+ 2 (test of NMEStyle).
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>

#include "NME.h"
#include "NMEStyle.h"
#include "NMEGtk.h"

/** Quit GTK+ event loop.
*/
static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

/** Link callback.
*/
static void linkFun(NMEConstText link, void *data)
{
	printf("%s\n", link);
}

/** Create a text window and fill it with read-only styled text converted from
	NME input.
	@param[in] title window title
	@param[in] input NME text with NME markup (UTF-8)
	@param[in] inputLen length in input in bytes
*/
static void makeWindow(char const *title, NMEConstText input, NMEInt inputLen)
{
	GtkWidget *window, *sw, *view;
	GtkTextBuffer *textBuffer;
	int charSize;
	static NMEGtk nmegtk;
	NMEErr err;
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(window), "destroy",
			G_CALLBACK(destroy), NULL);
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_widget_set_usize(GTK_WIDGET(window), 500, 400);
	
	view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), view);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
			GTK_POLICY_NEVER,
			GTK_POLICY_ALWAYS);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 4);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 4);
	textBuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_container_add(GTK_CONTAINER(window), sw);
	
	charSize = pango_font_description_get_size(view->style->font_desc);
	NMEGtkInit(&nmegtk, textBuffer, charSize);
	NMEGtkSetLinkFun(&nmegtk, linkFun, NULL);
	
	gtk_widget_show_all(window);
	
	err = NMEGtkInsert(textBuffer, &nmegtk, input, inputLen, FALSE, TRUE);
	
	if (err != kNMEErrOk)
	{
		fprintf(stderr, "Error %d\n", err);
		exit(1);
	}
}

/** Main function (optional argument: path of text file with NME markup; else
	stdin is read).
	@param[in] argc number of arguments, including application name
	@param[in] argv array of arguments
	@return 0 for success, nonzero for error
*/
int main(int argc, char **argv)
{
	NMEText input, output, buf;
	NMEInt inputLen, inputSize, outputLen, bufSize, n;
	FILE *fp;
	
	gtk_init(&argc, &argv);
	
	if (argc > 2)
	{
		fprintf(stderr, "Usage: %s [file]\n", argv[0]);
		exit(1);
	}
	
	// read file
	if (argc == 2)
	{
		fp = fopen(argv[1], "r");
		if (!fp)
		{
			fprintf(stderr, "Cannot open file \"%s\"\n", argv[1]);
			exit(1);
		}
	}
	else
		fp = stdin;
	
	inputSize = 1024;
	input = (NMEText)malloc(inputSize);
	for (inputLen = 0; !feof(fp); )
	{
		n = fread(input + inputLen, 1, inputSize - inputLen, fp);
		if (n < 0)
			break;
		inputLen += n;
		inputSize += 2 * n;
		input = (NMEText)realloc(input, inputSize);
	}
	
	if (argc == 2)
		fclose(fp);
	
	// create text window
	makeWindow("Nyctergatis Markup Engine", input, inputLen);
	
	// run GTK+ main event loop
	gtk_main();
	
	return 0;
}

/**
 *	@file NMEStyleCpp.h
 *	@brief C++ class wrapper for NME.h and NMEStyle.h (inherits from class NME).
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 *
 *	NMEStyleCpp.h implements a C++ class wrapper for NME.h, NME.c,
 *	NMEStyle.h and NMEStyle.c.
 *	If UseNMECppException is defined, errors are reported with
 *	C++ exceptions, else with error codes.
 */

#ifndef __NMEStyleCpp__
#define __NMEStyleCpp__

#if defined(UseNMECppException)
	// class NME is used without exceptions
#	undef UseNMECppException
#	include "NMECpp.h"
#	define UseNMECppException
	// still define class NMEError
#	include "NMEErrorCpp.h"
#else
#	include "NMECpp.h"
#endif

#include "NMEStyle.h"

/** Handler for link clicks.
*/
class NMELinkHandler
{
	public:
		/** Called by NMEStyle object to handle a hyperlink click.
		@param[in] link link (not null-terminated)
		@param[in] linkLength length of link
		*/
		virtual void click(NMEConstText link, NMEInt linkLength) = 0;
};

/** NMEStyle parser class with separate text and style (objects can be
used for multiple conversions, by changing input and/or output format
before getting output again).

NMEStyleC is a C++ class wrapper for NME.h, NME.c,
NMEStyle.h and NMEStyle.cpp. While class NME converts text with NME
markup to some other text format, often with another markup such as
HTML or RTF, class NMEStyle usually provides two separate outputs: plain
text, and a table of style specifications with the location of the related
text span.
*/
class NMEStyle: public NME
{
	public:
		
		/** Default constructor.
		*/
		NMEStyle(): NME()
		{
			styleTable = NULL;
		}
		
		/** Constructor with input.
		@param[in] input address of input
		@param[in] inputLength input length in bytes (if not specified, input
		is a null-terminated string)
		*/
		NMEStyle(char const *input, int inputLength = -1): NME(input, inputLength)
		{
			styleTable = NULL;
		}
		
		/** Copy constructor.
		@param[in] nme object to be copied
		*/
		NMEStyle(NMEStyle const &nme): NME(nme)
		{
			unicodeStyleOffsets = nme.unicodeStyleOffsets;
			if (nme.styleTable)
			{
				format.parHookFun = format.spanHookFun = NMEStyleSpanHook;
				styleTableSize = nme.styleTableSize;
				styleTable = (NMEStyleTable *) new char[styleTableSize];
				NMEStyleInit(styleTable, styleTableSize,
						unicodeStyleOffsets);
				styleTable->n = nme.styleTable->n;
				int i;
				for (i = 0; i < styleTable->n; i++)
					styleTable->span[i] = nme.styleTable->span[i];
				format.hookData = (void *)styleTable;
			}
			else
				styleTable = NULL;
		}
		
		/** Destructor. */
		~NMEStyle()
		{
			if (styleTable)
				delete [] (char *)styleTable;
		}
		
		/** Copy operator.
		@param[in] nme object to be copied
		@return *this
		*/
		NMEStyle NMEStyle::operator = (NMEStyle const &nme)
		{
			if (this != &nme)
			{
				NME::operator = (nme);
				if (styleTable)
					delete [] (char *)styleTable;
				if (nme.styleTable)
				{
					format.parHookFun = format.spanHookFun = NMEStyleSpanHook;
					styleTableSize = nme.styleTableSize;
					styleTable = (NMEStyleTable *) new char[styleTableSize];
					NMEStyleInit(styleTable, styleTableSize,
							unicodeStyleOffsets);
					styleTable->n = nme.styleTable->n;
					int i;
					for (i = 0; i < styleTable->n; i++)
						styleTable->span[i] = nme.styleTable->span[i];
					format.hookData = (void *)styleTable;
				}
				else
					styleTable = NULL;
			}
			return *this;
		}
		
		/** Set style table offset mode.
		@param[in] unicodeStyleOffsets true to have offsets in style table in UCS-16
		(16-bit unicode), false to have offsets in bytes (UTF-8 or any other ASCII extension)
		*/
		void setUnicodeStyleOffsets(bool unicodeStyleOffsets = true)
		{
			this->unicodeStyleOffsets = unicodeStyleOffsets;
		}
		
		/** Get parser output, generating it if needed.
		@param[out] output address of output (null-terminated)
		@param[out] outputLength length of output in bytes, excluding null terminator
		(optional)
		*/
		NMEErr getOutput(NMEConstText *output, NMEInt *outputLength = NULL)
		{
			if (!styleTable)
			{
				format.parHookFun = format.spanHookFun = NMEStyleSpanHook;
				styleTableSize = sizeof(NMEStyleTable) + inputLength;
				styleTable = (NMEStyleTable *) new char[styleTableSize];
				NMEStyleInit(styleTable, styleTableSize,
						unicodeStyleOffsets);
				format.hookData = (void *)styleTable;
			}
			
			for (;;)
			{
				NMEErr err = NME::getOutput(output, outputLength);
				if (err == kNMEErrOk)
					break;
				else if (err == (NMEErr)kNMEErrStyleTableTooSmall)
				{
					if (styleTableSize >= 65536 + 10 * inputLength)
#if defined(UseNMECppException)
						throw NMEError((NMEErr)kNMEErrStyleTableTooSmall);
#else
						return (NMEErr)kNMEErrStyleTableTooSmall;
#endif
					delete [] (char *)styleTable;
					styleTableSize *= 2;
					styleTable = (NMEStyleTable *) new char[styleTableSize];
					NMEStyleInit(styleTable, styleTableSize,
							unicodeStyleOffsets);
					format.hookData = (void *)styleTable;
				}
				else
#if defined(UseNMECppException)
					throw NMEError(err);
#else
					return err;
#endif
			}
			
			return kNMEErrOk;
		}
		
		/** Get style table.
		@return style table
		*/
		NMEStyleTable const *getStyleTable()
		{
			// parse input if not done yet
			if (!output)
				if (getOutput(NULL) != kNMEErrOk)	// ignore output
				{
					// getOutput has allocated styleTable; reset it
					NMEStyleInit(styleTable, styleTableSize,
							unicodeStyleOffsets);
					return NULL;
				}
			return styleTable;
		}
		
		/** Handle a click in text, calling link handler if offset is located
		in a link.
		@param[in] offset click location as a character offset in output text
		@param[in] handler object whose click method is called if offset is
		in a link (can be NULL)
		@return TRUE if offset is in a link, else FALSE
		*/
		NMEBoolean handleLinkClick(NMEInt offset, NMELinkHandler *handler)
		{
			// check table
			if (!styleTable)
				(void)getOutput(NULL);
			if (styleTable)
			{
				NMEInt i;
				
				for (i = 0; i < styleTable->n; i++)
					if (styleTable->span[i].style == kNMEStyleCharLink
							&& offset >= styleTable->span[i].begin
							&& offset < styleTable->span[i].end)
					{
						if (handler)
							handler->click(input + styleTable->span[i].linkOffset,
									styleTable->span[i].linkLength);
						return TRUE;
					}
			}
			
			return FALSE;
		}
		
	private:
		
		bool unicodeStyleOffsets;
		NMEStyleTable *styleTable;
		NMEInt styleTableSize;
};

#endif

/**
 *	@file NMECpp.h
 *	@brief C++ class wrapper for NME.h.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 *
 *	NMECpp.h implements a C++ class wrapper for NME.h and NME.c.
 *	If UseNMECppException is defined, errors are reported with
 *	C++ exceptions, else with error codes.
 */

#ifndef __NMECpp__
#define __NMECpp__

#include "NME.h"
#include "NMEErrorCpp.h"
#include <string.h>

/** @brief NME parser class (objects can be used for multiple
conversions, by changing input and/or output format before getting
output again).

In addition to the conversion of text with NME markup to
some text output format, it handles memory allocation with
C++ operators new and delete, which simplifies its use with
respect to the C interface defined in NME.h. Errors are
handled via C++ exceptions.
*/
class NME
{
	public:
		
		/** Default constructor.
		*/
		NME()
		{
			input = NULL;
			buf = NULL;
			output = NULL;
			format = NMEOutputFormatText;
			fontSize = 0;
		}
		
		/** Constructor with input.
		@param[in] input address of input
		@param[in] inputLength input length in bytes (if not specified, input
		is a null-terminated string)
		*/
		NME(char const *input, int inputLength = -1)
		{
			this->input = input;
			if (inputLength >= 0)
				this->inputLength = inputLength;
			else
				this->inputLength = strlen(input);
			buf = output = NULL;
			format = NMEOutputFormatText;
			fontSize = 0;
		}
		
		/** Copy constructor.
		@param[in] nme object to be copied
		*/
		NME(NME const &nme)
		{
			input = nme.input;
			inputLength = nme.inputLength;
			buf = output = NULL;	// will be created when required
			format = nme.format;
			fontSize = nme.fontSize;
		}
		
		/** Destructor. */
		~NME()
		{
			if (buf)
				delete [] buf;
		}
		
		/** Copy operator.
		@param[in] nme object to be copied
		@return *this
		*/
		NME NME::operator = (NME const &nme)
		{
			if (this != &nme)
			{
				input = nme.input;
				inputLength = nme.inputLength;
				if (buf)
					delete [] buf;
				buf = output = NULL;	// will be created when required
				format = nme.format;
				fontSize = nme.fontSize;
			}
			return *this;
		}
		
		/** Set (or change) input.
		@param[in] input address of input
		@param[in] inputLength input length in bytes (if not specified, input
		is a null-terminated string)
		*/
		void setInput(char const *input, int inputLength = -1)
		{
			this->input = input;
			if (inputLength >= 0)
				this->inputLength = inputLength;
			else
				this->inputLength = strlen(input);
			output = NULL;
		}
		
		/** Set (or change) output format.
		@param[in] format output format
		*/
		void setFormat(NMEOutputFormat const &format)
		{
			this->format = format;
			output = NULL;
		}
		
		/** Set (or change) output font size.
		@param[in] fontSize font size (default is default output font size)
		*/
		void setFontSize(NMEInt fontSize = 0)
		{
			this->fontSize = fontSize;
			output = NULL;
		}
		
		/** Get parser output, generating it if needed.
		@param[out] output address of output (null-terminated)
		@param[out] outputLength length of output in bytes, excluding null terminator
		(optional)
		*/
		NMEErr getOutput(NMEConstText *output, NMEInt *outputLength = NULL)
		{
			if (this->output)
			{
				if (output)
					*output = this->output;
				if (outputLength)
					*outputLength = this->outputLength;
				return kNMEErrOk;
			}
			
			if (!input || inputLength == 0)
			{
				if (output)
					*output = "";
				if (outputLength)
					*outputLength = 0;
			}
			
			if (!buf)
			{
				bufSize = 1024 + 2 * inputLength;
				buf = new NMEChar[bufSize];
			}
			
			for (;;)
			{
				NMEErr err = NMEProcess(input, inputLength,
						buf, bufSize,
						kNMEProcessOptDefault, "\n", &format, fontSize,
						&this->output, &this->outputLength, NULL);
				if (err == kNMEErrOk)
				{
					if (output)
						*output = this->output;
					if (outputLength)
						*outputLength = this->outputLength;
					return kNMEErrOk;
				}
				else if (err == kNMEErrNotEnoughMemory)
				{
					if (bufSize >= 65536 + 10 * inputLength)
#if defined(UseNMECppException)
						throw NMEError(kNMEErrNotEnoughMemory);
#else
						return kNMEErrNotEnoughMemory;
#endif
					delete [] buf;
					bufSize *= 2;
					buf = new NMEChar[bufSize];
				}
				else
#if defined(UseNMECppException)
					throw NMEError(err);
#else
					return err;
#endif
			}
		}
		
	protected:
		
		NMEConstText input;	///< NME text input (belong to caller)
		NMEInt inputLength;	///< length of input in bytes
		
		NMEText buf;	///< buffer used during conversion (belong to NME object)
		NMEInt bufSize;	///< length of buf
		
		NMEText output;	///< address of processed output (in buf)
		NMEInt outputLength;	///< length of processed output
		
		NMEOutputFormat format;	///< NME output format
		NMEInt fontSize;	///< font size (0 for default value)
};

#endif

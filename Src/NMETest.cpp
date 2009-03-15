/**
 *	@file NMETest.cpp
 *	@brief Test program for NME C++ class.
 *	@author Yves Piguet. Copyright 2007-2008, Yves Piguet.
 */

/* License: new BSD license (see NME.h) */

#include <iostream>

#define UseNMECppException
#include "NMEStyleCpp.h"

using namespace std;

int main()
{
	NMEStyle nme;
	
	nme.setInput("=Test\n"
		"==Section A\n"
		"Some text...\n"
		"==Section B\n"
		"*First item\n"
		"*Second item\n"
		"**Sublist item\n");
	char const *output;
	nme.getOutput(&output);
	cout << output;
	return 0;
}

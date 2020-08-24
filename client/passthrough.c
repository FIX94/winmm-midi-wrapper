/*
 * Copyright (C) 2020 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include "passthrough.h"

// list of function names to import
#define PASS_MACRO(funcname) #funcname,
char *names[] = {
//fills in all the names
#include "names.h"
};

//save up name list size to build function list
#define ENTRIES sizeof(names)/sizeof(names[0])
const int passcount = ENTRIES;
#undef PASS_MACRO

//table that functions below jump to
fptr funcs[ENTRIES];
//create passthrough functions that get exported
#define PASS_MACRO(funcname) \
__declspec(dllexport) __attribute__((naked)) void funcname() { \
    __asm volatile("jmp *%0" : : "m"(funcs[__COUNTER__])); \
}
//fills in all the functions
#include "names.h"

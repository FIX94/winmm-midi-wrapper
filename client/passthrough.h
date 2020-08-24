
#ifndef _passthrough_h_
#define _passthrough_h_

//tables to function pointers and names
typedef int(*fptr)(void);
extern fptr funcs[];
extern char *names[];

//number of passthrough functions
extern const int passcount;

#endif

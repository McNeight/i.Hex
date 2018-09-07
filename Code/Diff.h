#ifndef _DIFF_H_
#define _DIFF_H_

#include "GArray.h"

struct ctrl_info
{
	NativeInt a[3];
};

struct diff_info
{
	GArray<ctrl_info> ctrl;
	GArray<uint8> db;
	GArray<uint8> eb;
};

extern bool binary_diff(diff_info &di, uint8 *old, int oldsize, uint8 *New, int newsize);

#endif
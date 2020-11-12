#pragma once

#define _CRTDBG_MAP_ALLOC
#include "stdlib.h"
#include <crtdbg.h>

void sleepcore(long milliseconds)
{
#ifdef _WIN32
	_sleep(milliseconds);
#else
	usleep(milliseconds * 1000);
#endif
}
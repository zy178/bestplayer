#pragma once


#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"
static bool binit = false;
static pthread_mutex_t mutex;

static class baserender
{
public:
	baserender();
	virtual int init();
	virtual int uninit() = 0;
};


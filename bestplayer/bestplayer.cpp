#include "bestplayer.h"
#include "playercore.h"
#include "pqueue.h"

#define _CRTDBG_MAP_ALLOC
#include "stdlib.h"
#include <crtdbg.h>

#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif

int main()
{
	//_CrtSetDbgFlag(0x01 | 0x20);

	playercore* player = new playercore();
	char moviePath[255] = "e:\\2.mp4";

	player->open(moviePath);
	return 0;
}

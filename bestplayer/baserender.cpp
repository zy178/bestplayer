#include "baserender.h"


baserender::baserender()
{
	pthread_mutex_init(&mutex, nullptr);
}

int baserender::init()
{
	pthread_mutex_lock(&mutex);
	if (!binit)
	{
		
		binit = true;
	}
	pthread_mutex_unlock(&mutex);

	return 0;
}

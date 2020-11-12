#pragma once
#include "pqueue.h"
#define HAVE_STRUCT_TIMESPEC
#include "pthread.h"
#define _CRTDBG_MAP_ALLOC
#include "stdlib.h"
#include <crtdbg.h>

enum state
{
	NONE = 0,
	FLUSHED
};
class pqueue
{
public:
	pqueue();
	~pqueue();
private:
	struct node
	{
		node* next = nullptr;
		void* obj = nullptr;
	};

	pthread_mutex_t mutex;
	pthread_cond_t full;
	pthread_cond_t empty;

	int maxsize = 0;
	int minsize = 0;
	int nodecount = 0;
	state enum_state = NONE;
	node *first_node, *last_node;
	void lock();
	void unlock();

public:
	void wait_product();
	void wait_consume();
	void notify_product();
	void notify_consume();
	bool isfull();
	bool isempty();
	int push(void* obj);
	void* dequeue(); 
	void* get(int index);
	void flush();
	int getmaxsize();
	void setmaxsize(int size);
	void setminsize(int size);
	int getlength();
	state getstate();
};


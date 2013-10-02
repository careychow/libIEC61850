/*
 *  linked_list.h
 *
 *  Copyright 2013 Michael Zillgith
 *
 *	This file is part of libIEC61850.
 *
 *	libIEC61850 is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	libIEC61850 is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	See COPYING file for the complete license text.
 */

#ifndef LINKED_LIST_H_
#define LINKED_LIST_H_

#include "libiec61850_platform_includes.h"

struct sLinkedList {
	void* data;
	struct sLinkedList* next;
};

typedef struct sLinkedList* LinkedList;

LinkedList
LinkedList_create();

void
LinkedList_destroy(LinkedList list);


typedef void (*LinkedListValueDeleteFunction) (void*);

void
LinkedList_destroyDeep(LinkedList list, LinkedListValueDeleteFunction valueDeleteFunction);

//void
//LinkedList_destroyDeep(LinkedList list, void (*valueDeleteFunction) (void*));

/**
 * Destroy list (free) without freeing the element data
 */
void
LinkedList_destroyStatic(LinkedList list);

void
LinkedList_add(LinkedList list, void* data);

bool
LinkedList_remove(LinkedList list, void* data);

//void LinkedList_removeAtIndex(LinkedList list, int index);

LinkedList
LinkedList_getNext(LinkedList list);

LinkedList
LinkedList_getLastElement(LinkedList list);

LinkedList
LinkedList_insertAfter(LinkedList list, void* data);

int
LinkedList_size(LinkedList list);

void
LinkedList_printStringList(LinkedList list);

#endif /* LINKED_LIST_H_ */

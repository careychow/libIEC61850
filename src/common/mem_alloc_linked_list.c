/*
 *  mem_alloc_linked_list.c
 *
 *  Copyright 2014 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */


#include "mem_alloc_linked_list.h"

struct sMemAllocLinkedList {
    void* data;
    struct sLinkedList* next;
    MemoryAllocator* ma;
};

MemAllocLinkedList
MemAllocLinkedList_create(MemoryAllocator* ma)
{
    MemAllocLinkedList self =
            (MemAllocLinkedList) MemoryAllocator_allocate(ma, sizeof(struct sMemAllocLinkedList));

    if (self == NULL)
        return NULL;

    self->ma = ma;
    self->data = NULL;
    self->next = NULL;

    return self;
}

LinkedList
MemAllocLinkedList_add(MemAllocLinkedList self, void* data)
{
    LinkedList newElement = (LinkedList)
            MemoryAllocator_allocate(self->ma, sizeof(struct sLinkedList));

    if (newElement == NULL)
        return NULL;

    newElement->data = data;
    newElement->next = NULL;

    LinkedList listEnd = LinkedList_getLastElement((LinkedList) self);

    listEnd->next = newElement;

    return newElement;
}


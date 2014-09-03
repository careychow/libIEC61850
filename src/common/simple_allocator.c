/*
 *  simple_allocator.c
 *
 *  Copyright 2013 Michael Zillgith
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

#include <stdlib.h>

#include "simple_allocator.h"

void
MemoryAllocator_init(MemoryAllocator* self, char* memoryBlock, int size)
{
    self->memoryBlock = memoryBlock;
    self->currentPtr = memoryBlock;
    self->size = size;
}

char*
MemoryAllocator_allocate(MemoryAllocator* self, int size)
{
    if (((self->currentPtr - self->memoryBlock) + size) <= self->size) {
        char* ptr = self->currentPtr;
        self->currentPtr += size;
        return ptr;
    }
    else
        return NULL;
}

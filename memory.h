#ifndef __CLOX_MEMORY_H
#define __CLOX_MEMORY_H

#include "common.h"
#include "object.h"

#define ALLOCATE(type, count) \
(type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
(type*)reallocate(pointer, sizeof(type) * (oldCount), \
sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
reallocate(pointer, sizeof(type) * (oldCount), 0)

void *reallocate (void *, size_t, size_t);
void freeObjects ();

#endif /* __CLOX_MEMORY_H */

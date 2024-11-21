#ifndef __CLOX_DEBUG_H
#define __CLOX_DEBUG_H

#include "chunk.h"

void disassembleChunk(Chunk*, const char*);
int disassembleInstruction(Chunk *, int);

#endif /* __CLOX_DEBUG_H */

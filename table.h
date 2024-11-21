#ifndef __CLOX_TABLE_H
#define __CLOX_TABLE_H

#include "common.h"
#include "value.h"

typedef struct {
  ObjString *key;
  Value value;
} Entry ;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table ;

void initTable (Table *);
void freeTable (Table *);
bool tableSet (Table *, ObjString *, Value);
bool tableGet (Table *, ObjString *, Value *);
bool tableDelete (Table *, ObjString *);
void tableAddAll (Table *, Table *);
ObjString *tableFindString (Table *, const char *, int, uint32_t);

#endif /*  __CLOX_TABLE_H */

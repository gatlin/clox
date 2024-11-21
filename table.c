#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD      0.75

void
initTable (Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void
freeTable (Table *table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable (table);
}

static Entry *
findEntry (Entry *entries, int capacity, ObjString *key) {
  Entry *entry, *tombstone = NULL;
  uint32_t index = key->hash % capacity;
  for (;;) {
    entry = &entries[index];
    if (NULL == entry->key) {
      if (IS_NIL(entry->value)) {
        return NULL != tombstone ? tombstone : entry;
      }
      else {
        if (NULL == tombstone) { tombstone = entry; }
      }
    }
    else if (key == entry->key) {
      return entry;
    }
    index = (index + 1) % capacity;
  }
}

bool
tableGet (Table *table, ObjString *key, Value *value) {
  Entry *entry;
  if (0 == table->count) { return false; }
  entry = findEntry (table->entries, table->capacity, key);
  if (NULL == entry->key) { return false; }
  *value = entry->value;
  return true;
}

static void
adjustCapacity (Table *table, int capacity) {
  Entry *entries = ALLOCATE(Entry, capacity), *entry, *dest;
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    entry = &table->entries[i];
    if (NULL == entry->key) { continue; }
    dest = findEntry (entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool
tableSet (Table *table, ObjString *key, Value value) {
  Entry *entry;
  bool isNewKey;
  int capacity;
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity (table, capacity);
  }
  entry = findEntry (table->entries, table->capacity, key);
  isNewKey = NULL == entry->key;
  if (isNewKey && IS_NIL(entry->value)) { table->count++; }
  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool
tableDelete (Table *table, ObjString *key) {
  Entry *entry;
  if (0 == table->count) { return false; }
  entry = findEntry (table->entries, table->capacity, key);
  if (NULL == entry->key) { return false; }
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  return true;
}

void
tableAddAll (Table *from, Table *to) {
  Entry *entry;
  for (int i = 0; i < from->capacity; i++) {
    entry = &from->entries[i];
    if (NULL != entry->key) {
      tableSet (to, entry->key, entry->value);
    }
  }
}

ObjString *
tableFindString (Table *table, const char *chars, int length, uint32_t hash) {
  Entry *entry;
  uint32_t index;
  if (0 == table->count) { return NULL; }
  index = hash % table->capacity;
  for (;;) {
    entry = &table->entries[index];
    if (NULL == entry->key) {
      if (IS_NIL(entry->value)) { return NULL; }
    }
    else if (length == entry->key->length
          && hash == entry->key->hash
          && 0 == memcmp (entry->key->chars, chars, length)) {
      return entry->key;
    }
    index = (index + 1) % table->capacity;
  }
}

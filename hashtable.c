// Home-grown hashtable implementation
// Uses open addressing and linear probing.
//
// My choice of hash function (summing up bytes) and linear probing
// offset (1) is quite shitty, but it works for simple use cases.
#include "common.h"

typedef struct {
  bool occupied;
  char *key;
  void *value;
} hashtable_entry;

typedef struct {
  size_t capacity;
  size_t items;
  size_t keytype;   // The key is terminated by a sequence of `keytype` zero bytes.
                    // This is to prevent ambiguity when the elements
                    // of the key are more than one byte long, since
                    // they may contain zero bytes without being NULL
                    // itself.
  hashtable_entry *entries;
} hashtable;

static size_t _hthash(char *key, size_t keytype, size_t capacity) {
  int sum;
  int i;
  bool allnull;
  char *p;

  sum = 0;
  p = key;

nextelem:
  i = 0;

  allnull = true;
  for (i = 0; i < keytype; i++)
    if (p[i]) { allnull = false; break; }
  if (allnull) return (size_t)sum % capacity;

  for (i = 0; i < keytype; p++, i++) sum += (int)*p;
  goto nextelem;
}

void htinit(hashtable *ht, size_t keytype) {
  size_t init = 8;
  ht->items = 0;
  ht->capacity = init;
  ht->keytype = keytype;
  ht->entries = calloc(init, sizeof(hashtable_entry));
}

static void _insert(hashtable_entry *entries, char *key, void *value, size_t keytype, size_t capacity) {
  size_t h;
  hashtable_entry *e;
  h = _hthash(key, keytype, capacity);
try:
  e = entries + h;
  if (e->occupied) {
    // Linear probe
    h++;
    if (h >= capacity) h -= capacity;
    goto try;
  }
  e->occupied = true;
  e->key = key;
  e->value = value;
}

void htinsert(hashtable *ht, char *key, void *value) {
  hashtable_entry *e, *e1;

  // Grow hashtable if necessary
  if ((ht->items << 1) >= ht->capacity) {
    e1 = calloc(ht->capacity << 1, sizeof(hashtable_entry));

    // Rehash everything
    for (e = ht->entries; e < ht->entries + ht->capacity; e++) {
      if (!e->occupied) continue;
      _insert(e1, e->key, e->value, ht->keytype, ht->capacity << 1);
    }

    e = ht->entries;
    ht->entries = e1;
    ht->capacity <<= 1;
    free(e);
  }

  _insert(ht->entries, key, value, ht->keytype, ht->capacity);
  ht->items++;
}

// Return 0 if key1 = key2 as arrays of `keytype`-byte sized elements,
//        1 otherwise.

static int comparekey(char *key1, char *key2, size_t keytype) {
  bool allnull1, allnull2;
  int i;
  char *p1, *p2;

  p1 = key1;
  p2 = key2;
nextelem:
  i = 0;

  allnull1 = true;
  allnull2 = true;
  for (i = 0; i < keytype; i++) {
    if (p1[i]) allnull1 = false;
    if (p2[i]) allnull2 = false;
  }
  if ((allnull1 && !allnull2) || (!allnull1 && allnull2)) return 1;
  if (allnull1 && allnull2) return 0;

  for (i = 0; i < keytype; p1++, p2++, i++)
    if (*p1 != *p2) return 1;
  goto nextelem;
}

void *htfind(hashtable *ht, char *key) {
  size_t h, h1;
  hashtable_entry *e;

  h = _hthash(key, ht->keytype, ht->capacity);
try:
  e = ht->entries + h;
  if (!e->occupied) return NULL;
  if (comparekey(e->key, key, ht->keytype) == 0) return e->value;
  h++;
  if (h >= ht->capacity) h -= ht->capacity;
  goto try;
}

void htdump(hashtable *ht) {
  hashtable_entry *e;
  size_t i;
  for (i = 0, e = ht->entries; i < ht->capacity; i++, e++)
    if (e->occupied)
      printf("%zu: %s (hash=%zu)\n", i, e->key, _hthash(e->key, ht->keytype, ht->capacity));
}
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "hashtable.h"

hashtable ht;

// For TEST 1
int a = 111;
int b = 222;
int c = 333;

// For TEST 2
char *s;
int *ints;

int main() {
  // TEST 1: a few inserts
  htinit(&ht, 1);

  htinsert(&ht, "test", &a);
  htinsert(&ht, "sfst", &b);
  htinsert(&ht, "testy2", &c);

  void *aa = htfind(&ht, "test");
  void *bb = htfind(&ht, "sfst");
  void *cc = htfind(&ht, "testy2");
  assert(*((int *)aa) == 111);
  assert(*((int *)bb) == 222);
  assert(*((int *)cc) == 333);
  htdump(&ht);

  // TEST 2: many inserts
  htinit(&ht, 1);  // Re-initialise
  s = malloc(3 * 26 * 26);
  ints = malloc(3 * 26 * 26 * sizeof(int *));

  int i = 0;
  for (char c = 'a'; c <= 'z'; c++) {
    for (char d = 'a'; d <= 'z'; d++) {
      ints[i] = i;
      s[3*i] = c;
      s[3*i+1] = d;
      s[3*i+2] = '\0';
      htinsert(&ht, s+3*i, ints+i);
      i++;
    }
  }
  htdump(&ht);
}
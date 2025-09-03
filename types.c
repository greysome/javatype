// The backend implementation of Java types
//
// Small note: "signature" and "sig" refer to the ordered list of
// parameter types of a method. In reality, a signature also includes
// the method name and the caller's type, but I choose to overload
// (haha) the terminology because I don't know how else to name it. I
// hope I don't cause confusion!
#include "common.h"
#include "hashtable.c"
#include "err.c"

struct _type {
  struct _type *super;
  char *name;
};
typedef struct _type type;

typedef struct {
  type *calltype;
  type *rettype;
} method;

typedef struct {
  type *ctt;
  type *rtt;
  char *name;
} object;

hashtable TYPES;            // char * -> type *
hashtable OBJECTS;          // char * -> object *
                            // Three-layer hashtable of methods:
hashtable VTABLES;          // char *  (type name)   -> vtable
typedef hashtable vtable;   // char *  (method name) -> sigtable
typedef hashtable sigtable; // type ** (signature)   -> method

type ROOTTYPE = {.super = NULL, .name = "_Root"};

bool issubtype(type *s, type *t) {
  if (!t) return false;
  for (; s; s = s->super)
    if (s->name == t->name) return true;
  return false;
}

type *gettype(char *name) {
  return htfind(&TYPES, name);
}

bool creattype(char *name, char *supername) {
  type *t, *t1;
  char *s;

  t = htfind(&TYPES, supername);
  if (!t) { errmsg = "undefined type"; return false; }

  t1 = htfind(&TYPES, name);
  if (htfind(&TYPES, name)) { errmsg = "type is already defined"; return false; }
  t1 = malloc(sizeof(type));
  t1->super = t;
  s = malloc(strlen(name) + 1);
  strcpy(s, name);
  t1->name = s;
  htinsert(&TYPES, s, t1);
  return true;
}

object *getobject(char *name) {
  return htfind(&OBJECTS, name);
}

bool creatobject(char *name, type *ctt, type *rtt) {
  object *o;
  char *s;
  if (htfind(&OBJECTS, name)) { errmsg = "object already exists"; return false; };
  o = malloc(sizeof(object));
  o->ctt = ctt;
  o->rtt = rtt;
  s = malloc(strlen(name) + 1);
  strcpy(s, name);
  o->name = s;
  htinsert(&OBJECTS, s, o);
}

bool creatmethod(char *name, type *calltype, type **sig, type *rettype) {
  char *s;
  vtable *vt;
  sigtable *st, *st1;
  method *meth, *meth1;
  method *overriding;

  vt = htfind(&VTABLES, calltype->name);
  if (!vt) {     // entry in VTABLES doesn't exist
    vt = malloc(sizeof(vtable));
    htinit(vt, 1);
    htinsert(&VTABLES, calltype->name, vt);
  }

  st = htfind(vt, name);
  if (!st) {     // entry in vtable doesn't exist
    st = malloc(sizeof(sigtable));
    htinit(st, sizeof(type *));
    s = malloc(strlen(name) + 1);
    strcpy(s, name);
    htinsert(vt, s, st);
  }

  meth = htfind(st, (char *)sig);
  if (meth) { errmsg = "method with same signature already exists"; return false; }
  meth = malloc(sizeof(method));
  meth->calltype = calltype;
  meth->rettype = rettype;

  // Find most recent parent that this method is overriding, or NULL
try:
  calltype = calltype->super;
  if (!calltype) goto ret;
  vt = htfind(&VTABLES, calltype->name);
  if (!vt) goto try;
  st1 = htfind(vt, name);
  if (!st1) goto try;
  meth1 = htfind(st1, (char *)sig);
  if (!meth1) goto try;
  // Found it! The overriding method's return type must be a subtype
  if (!issubtype(rettype, meth1->rettype) && (rettype || meth1->rettype))
    { errmsg = "overriding method's return type is not a subtype"; return false; }

ret:
  // Finally add the method
  htinsert(st, (char *)sig, meth);
  return true;
}

// sig1 is more specific than sig2 iff they have the same length, and
// each type in sig1 is a subtype of the corresponding type in sig2.

bool morespecific(type **sig1, type **sig2) {
  type **t, **t1;

  t = sig1, t1 = sig2;
  while (true) {
    if (!*t && !*t1) return true;
    if ((*t && !*t1) || (!*t && *t1)) return false;
    if (!issubtype(*t, *t1)) return false;
    t++; t1++;
  }
}

// Do a compile-time resolution of method call; calltype should be the
// ctt of the calling object.
//
// Returns:
// - The most specific type defining that method, via besttype
// - The most specific matching signature, via bestsig

bool cttresolve(char *name, type *calltype, type **sig, type **besttype, type ***bestsig) {
  vtable *vt;
  sigtable *st;
  int i;
  hashtable_entry *e;
  type **cursig;
  type **_bestsig;

  _bestsig = NULL;
try:
  vt = htfind(&VTABLES, calltype->name);
  if (!vt) goto again;  // calltype has not defined any methods
  st = htfind(vt, name);
  if (!st) goto again;  // calltype does not have a method of that name

  // Search for most specific matching signature
  for (i = 0, e = st->entries; i < st->capacity; i++, e++) {
    if (!e->occupied) continue;
    cursig = (type **)e->key;

    if (morespecific(sig, cursig)) {
      if (!_bestsig) _bestsig = cursig;
      else if (morespecific(cursig, _bestsig)) _bestsig = cursig;
    }
  }

  if (!_bestsig) {      // No matching signature found
    // Caller type does not have a method with the given name and a
    // signature that is equally or less specific; check parent types.
again:
    calltype = calltype->super;
    if (!calltype) { errmsg = "no matching signature"; return false; }
    goto try;
  }

  // Check that bestsig is the unique "most specific matching signature"
  for (i = 0, e = st->entries; i < st->capacity; i++, e++) {
    if (!e->occupied) continue;
    cursig = (type **)e->key;
    if (morespecific(sig, cursig))
      if (!morespecific(_bestsig, cursig)) { errmsg = "multiple matching signatures"; return false; }
  }

  *besttype = calltype;
  *bestsig = _bestsig;
  return true;
}

// Do a run-time resolution of method call; calltype should be the rtt
// of the calling object, and besttype and bestsig should come from
// cttcompile().
//
// Returns:
// - The method, via meth.
// - The type (a subtype of besttype) defining the most specific
//   override for bestsig, via bestbesttype.

bool rttresolve(char *name, type *calltype, type *besttype, type **bestsig, type **bestbesttype, method **meth) {
  vtable *vt;
  sigtable *st;
  method *_meth;

try:
  vt = htfind(&VTABLES, calltype->name);
  if (!vt) goto again;     // calltype has not defined any methods
  st = htfind(vt, name);
  if (!st) goto again;     // calltype does not have a method of that name
  _meth = htfind(st, (char *)bestsig); // calltype does not define an override for method with bestsig
  if (!_meth) goto again;
  *bestbesttype = calltype;
  *meth = _meth;
  return true;

again:
  calltype = calltype->super;
  if (!issubtype(calltype, besttype)) { errmsg = "could not find runtime overload"; return false; }
  goto try;
}

void dumptypes() {
  hashtable_entry *e;
  size_t i;
  type *t;

  for (i = 0; i < TYPES.capacity; i++) {
    e = TYPES.entries + i;
    if (e->occupied) {
      t = e->value;
      if (t->super)
	printf("%s <: %s\n", t->name, t->super->name);
      else
	printf("%s\n", t->name);
    }
  }
}

void dumpobjects() {
  hashtable_entry *e;
  size_t i;
  object *o;

  for (i = 0; i < OBJECTS.capacity; i++) {
    e = OBJECTS.entries + i;
    if (e->occupied) {
      o = e->value;
      printf("%s : %s (rtt=%s)\n", o->name, o->ctt->name, o->rtt->name);
    }
  }
}

void dumpsig(type **sig) {
  type **t;
  for (t = sig; *t; t++) printf("%s,", (*t)->name);
  printf("\b");
}

void dumpparams(object **params, int n) {
  int i;
  object *o;
  for (i = 0; i < n; i++) {
    o = params[i];
    assert(!o->rtt || issubtype(o->rtt, o->ctt));
    if (o->rtt != o->ctt) printf("(%s)", o->ctt->name);
    printf("%s,", o->name);
  }
  printf("\b");
}

void dumpvtables() {
  hashtable_entry *e, *e1, *e2;
  vtable *vt;
  sigtable *st;

  char *methodname;
  type **sig;
  method *meth;
  type *t;

  size_t i, j, k;

  for (i = 0; i < VTABLES.capacity; i++) {
    e = VTABLES.entries + i;
    if (!e->occupied) continue;
    vt = e->value;

    for (j = 0; j < vt->capacity; j++) {
      e1 = vt->entries + j;
      if (!e1->occupied) continue;
      methodname = e1->key;
      st = e1->value;

      for (k = 0; k < st->capacity; k++) {
	e2 = st->entries + k;
	if (!e2->occupied) continue;

	sig = (type **)e2->key;
	meth = e2->value;

	printf("%s.%s(", meth->calltype->name, methodname);
	dumpsig(sig);
	if (meth->rettype) printf(") -> %s\n", meth->rettype->name);
	else printf(")\n");
      }
    }
  }
}

void setuptypes() {
  htinit(&TYPES, 1);
  htinit(&OBJECTS, 1);
  htinit(&VTABLES, 1);
  htinsert(&TYPES, "_Root", &ROOTTYPE);
  creattype("Object", "_Root");
  creattype("int", "_Root");
  creattype("char", "_Root");
  creattype("float", "_Root");
  creattype("double", "_Root");
  creattype("boolean", "_Root");
}
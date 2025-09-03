// The frontend
//
// Refer to types.c for a note about the term "signature/sig"
#include "common.h"
#include "err.c"
#include "types.c"

#define ARENAMAX 1024
#define LINEMAX  128
#define TOKMAX   64
#define SIGMAX   16

// Disgusting hack in order to get quoted macros
#define STRR(X) #X
#define STR(X)  STRR(X)

char line[LINEMAX+2];
char *lineptr;
char tok[TOKMAX];
char *newstr;

#define SPECIALCHAR(c) ((c)==':' || (c)=='=' || (c)=='<' || (c)==',' || (c)=='.' || (c)=='(' || (c)==')' || !(c))
#define ERROR(s,...)   printf("\033[31merror:\033[37m " s "\n" __VA_OPT__(,) __VA_ARGS__)

void nexttok() {
  char *s;

  for (; *lineptr == ' ' || *lineptr == '\t'; lineptr++);
  if (SPECIALCHAR(*lineptr)) {
    tok[0] = *lineptr;
    tok[1] = '\0';
    lineptr++;
    return;
  }

  s = tok;
  while (1) {
    if (*lineptr == ' ' || *lineptr == '\t') break;
    if (SPECIALCHAR(*lineptr)) break;
    *s = *lineptr;
    s++; lineptr++;
  };

  *s++ = '\0';
}

#define NONSPECIAL     '*'
bool expect(char c) {
  char *s;
  s = lineptr;
  nexttok();
  if ((*tok == c) || (c == NONSPECIAL && !SPECIALCHAR(*tok))) return true;
  lineptr = s;
  return false;
}

bool expectstr(char *s) {
  char *s1;
  s1 = lineptr;
  nexttok();
  if (strcmp(tok, s) == 0) return true;
  lineptr = s1;
  return false;
}

bool parse_typedecl() {
  // tok1 is the previous type parsed,
  // tok  is the current type
  char tok1[TOKMAX];
  type *t, *t1;

start:  // Start parsing a new chain of types
  *tok1 = '\0';
  goto first;

loop:
  if (expect(',')) {
    printf("- %s <: Object\n", tok1);
    goto start;
  }

  else if (expect('<')) {
first:
    if (!expect(NONSPECIAL)) return false;

    if (gettype(tok)) {
      printf("info: type %s already exists\n", tok);
      goto skip;
    }

    if (!creattype(tok, "Object")) return false;

skip:
    if (*tok1) {               // Update previous type's parent
      t = gettype(tok);
      t1 = gettype(tok1);
      assert(t);
      assert(t1);
      t1->super = t;
      printf("- %s <: %s\n", tok1, tok);
    }

    strcpy(tok1, tok);
    goto loop;
  }

  else if (expect('\0')) {
    if (*tok1) printf("- %s <: Object\n", tok1);
  }

  else return false;

  return true;
}

// A method declaration is a statement of the form
// Type::method(Type1, Type2, ...)

bool parse_methoddecl() {
  char tok1[TOKMAX];
  type *calltype;
  type *rettype;
  type *t;
  type **sig;
  int i;

  if (!expect(NONSPECIAL)) return false;    // Calling type
  calltype = gettype(tok);
  if (!calltype) { errmsg = "undefined calling type"; return false; }

  if (!expect(':')) return false;
  if (!expect(':')) return false;

  if (!expect(NONSPECIAL)) return false;    // Method name
  strcpy(tok1, tok);

  if (!expect('(')) return false;

  sig = malloc((SIGMAX+1) * sizeof(type *));
  if (expect(')')) goto skip;

  i = 0;
  while (1) {
    if (i >= SIGMAX) { free(sig); errmsg = "too many parameters; maximum allowed is " STR(SIGMAX); return false; }

    if (!expect(NONSPECIAL)) { free(sig); return false; }  // Parameter type
    t = gettype(tok);
    if (!t) { free(sig); errmsg = "undefined parameter type"; return false; }
    sig[i++] = t;

    if (expect(')')) break;
    else if (expect(',')) continue;
    else { free(sig); return false; }
  }
  sig[i] = NULL;

skip:
  if (expect('\0')) rettype = NULL;         // void return type

  else if (expectstr("return")) {           // Nonempty return type
    if (!expect(NONSPECIAL)) { free(sig); return false; }
    rettype = gettype(tok);
    if (!rettype) { free(sig); errmsg = "undefined return type"; return false; }
  }

  else return false;

  if (!creatmethod(tok1, calltype, sig, rettype)) return false;

  //printf("- %s::%s(", calltype->name, tok1);
  //dumpsig(sig);
  //printf(")");
  //if (rettype) printf(" return %s\n", rettype->name);
  //else         printf("\n");
  return true;
}

// An object expression is an expression of the form
// obj or (Type)obj,
// where Type is a supertype of obj's rtt and a subtype of obj's ctt.
//
// Returns the appropriate type in resulttype.

bool parse_object(type **resulttype) {
  char tok1[TOKMAX];
  type *t;
  object *o;

  if (expect('(')) {
    if (!expect(NONSPECIAL)) return false;
    t = gettype(tok);
    if (!t) { errmsg = "undefined cast type"; return false; }

    if (!expect(')')) return false;

    if (!expect(NONSPECIAL)) return false;
    o = getobject(tok);
    if (!o) { errmsg = "undefined object"; return false; }
    if (!issubtype(o->rtt, t)) { errmsg = "object's rtt not a subtype of cast type"; return false; }
    if (!issubtype(t, o->ctt)) { errmsg = "cast type not a subtype of object's ctt"; return false; }
    *resulttype = t;
  }

  else if (expect(NONSPECIAL)) {
    o = getobject(tok);
    if (!o) { errmsg = "undefined object"; return false; }
    *resulttype = o->ctt;
  }

  else return false;

  return true;
}

// A method call is an expression of the form
// obj.method(param1, param2, ...)
//
// Performs dynamic dispatching and returns the appropriate return
// type in resulttype.

bool parse_methodcall(type **resulttype) {
  char tok1[TOKMAX];
  object *caller;
  object *o;
  int i;

  type **sig;
  type **bestsig;
  type *besttype;
  type *bestbesttype;
  method *meth;

  if (!expect(NONSPECIAL)) return false;             // Calling object
  caller = getobject(tok);
  if (!caller) { errmsg = "undefined caller"; return false; }

  if (!expect('.')) return false;
  if (!expect(NONSPECIAL)) return false;             // Method name
  strcpy(tok1, tok);

  if (!expect('(')) return false;
  i = 0;

  sig = malloc((SIGMAX+1) * sizeof(type *));
  if (expect(')')) goto skip;

  while (1) {
    if (i >= SIGMAX) { free(sig); errmsg = "too many parameters; maximum is " STR(SIGMAX); return false; }

    if (!parse_object(sig+(i++))) { free(sig); return false; }  // Parameter

    if (expect(')')) break;
    else if (expect(',')) continue;
    else { free(sig); return false; }
  }
  sig[i] = NULL;

skip:
  if (!caller->rtt) { free(sig); errmsg = "uninitialised caller"; return false; }
  if (!cttresolve(tok1, caller->ctt, sig, &besttype, &bestsig)) { free(sig); return false; }
  if (!rttresolve(tok1, caller->rtt, besttype, bestsig, &bestbesttype, &meth)) { free(sig); return false; }

  printf("- %s.%s(", caller->name, tok1);
  dumpsig(sig);
  printf(") -> %s::%s(", besttype->name, tok1);
  dumpsig(bestsig);
  printf(") (ctt) -> %s::%s(", bestbesttype->name, tok1);
  dumpsig(bestsig);
  printf(") (rtt)\n");
  free(sig);
  return true;
}

// A 'rhs' is an expression of the form
// Case 1: obj                             or
// Case 2: (Type)obj                       or
// Case 3: Type()                          or
// Case 4: obj.method(param1, param2, ...)
//
// Returns the type that the expression evaluates to in rtt.

bool parse_rhs(type **rtt) {
  char tok1[TOKMAX];
  type *t;
  object *o;
  type *sig;
  char *s;

  s = lineptr;

  if (expect('(')) {           // CASE 2, typecast
    if (!expect(NONSPECIAL)) return false;
    t = gettype(tok);
    if (!t) { errmsg = "undefined cast type"; return false; }

    if (!expect(')')) return false;

    if (!expect(NONSPECIAL)) return false;
    o = getobject(tok);
    if (!o) { errmsg = "undefined object"; return false; }
    if (!issubtype(o->rtt, t)) { errmsg = "object's rtt not a subtype of cast type"; return false; }
    *rtt = t;
  }

  else if (expect(NONSPECIAL)) {
    strcpy(tok1, tok);

    if (expect('\0')) {        // CASE 1, object, tok1 = object name
      o = getobject(tok1);
      if (!o) { errmsg = "undefined object"; return false; }
      *rtt = o->rtt;
    }

    else if (expect('(')) {    // CASE 3, constructor, tok1 = type name
      t = gettype(tok1);
      if (!t) { errmsg = "undefined type"; return false; }
      if (!expect(')')) return false;
      *rtt = t;
    }

    else if (expect('.')) {    // CASE 4, method call, tok1 = object name
      lineptr = s;
      if (!parse_methodcall(rtt)) return false;
    }

    else return false;
  }

  else return false;

  return true;
}

// An object assignment is a statement of the form
// obj = <rhs>

bool parse_objectasgn() {
  object *o;
  type *resulttype;

  if (!expect(NONSPECIAL)) return false;
  o = getobject(tok);
  if (!o) { errmsg = "undefined object"; return false; }

  if (!expect('=')) return false;

  if (!parse_rhs(&resulttype)) return false;
  if (!issubtype(resulttype, o->ctt)) { errmsg = "rhs is not a subtype of object's ctt"; return false; }
  o->rtt = resulttype;
  printf("- %s : %s (rtt=%s)\n", o->name, o->ctt->name, o->rtt->name);
  return true;
}

// An object declaration is a statement of the form
// Case 1: Type obj          or
// Case 2: Type obj = <rhs>

bool parse_objectdecl() {
  char tok1[64];
  type *ctt, *rtt;

  if (!expect(NONSPECIAL)) return false;  // Type name
  ctt = gettype(tok);
  if (!ctt) { errmsg = "undefined type"; return false; }

  if (!expect(NONSPECIAL)) return false;  // Object name
  strcpy(tok1, tok);

  if (expect('\0')) rtt = NULL;           // CASE 1
  else if (expect('=')) {                 // CASE 2
    if (!parse_rhs(&rtt)) return false;
  }
  else return false;

  if (rtt)
    if (!issubtype(rtt, ctt)) { errmsg = "rhs not a subtype of lhs"; return false; }
  if (!creatobject(tok1, ctt, rtt)) return false;
  if (rtt) printf("- %s : %s (rtt=%s)\n", tok1, ctt->name, rtt->name);
  else     printf("- %s : %s (rtt=nil)\n", tok1, ctt->name);
  return true;
}

void help() {
  printf("? to print this help message\n");
  printf("q to quit\n");
  printf("?t to dump types\n");
  printf("?o to dump objects\n");
  printf("?v to dump all methods (v for vtable)\n");
  printf("To learn the basic syntax, view test.txt\n");
}

int main(int argc, char **argv) {
  FILE *fp;
  char tok1[TOKMAX];
  int i;
  char *s;

  setuptypes();

  printf("\n     \033[33mjavatype\033[37m, by wyan\n");
  printf("     ? for help\n\n");
  if (argc > 1) {
    fp = fopen(argv[1], "r");
    if (!fp) {
      ERROR("could not read file '%s': %s", argv[1], strerror(errno));
      return 1;
    }
    printf("\033[32mReading from file\033[37m %s\033[32m...\033[37m\n", argv[1]);
  }
  else fp = stdin;

nextline:
  errmsg = NULL;
  if (fp == stdin) {
    printf("> ");
    fflush(stdout);
    if (!fgets(line, LINEMAX+2, fp)) { fclose(fp); return 0; }
  }
  else {
    if (!fgets(line, LINEMAX+2, fp)) { fclose(fp); return 0; }
    printf("> %s", line);
  }

  s = strchr(line, '\n');
  // Either the user pressed Ctrl-D with a nonempty line, or the line
  // was too long
  if (!s) goto nextline;
  // Make line null-terminated rather than \n-terminated.
  *s = '\0';

  lineptr = line;

  if (*line == '#') goto nextline;           // Comment

  if (line[0] == '?') {                      // Help
    if (!line[1]) help();
    else if (line[1] == 't') dumptypes();
    else if (line[1] == 'o') dumpobjects();
    else if (line[1] == 'v') dumpvtables();
    else printf("I don't know this help option\n");
    goto nextline;
  }

  if (line[0] == 'q' && !line[1]) return 0;  // Quit

  // First two tokens tells us what kind of statement we are dealing with

  if (expectstr("types")) {            // First token
    if (expect('\0')) { errmsg = "empty type declaration"; goto err; }
    if (!parse_typedecl()) goto err;
    goto nextline;
  }

  else if (expect(NONSPECIAL)) {
    if (expect('.')) {                 // Second token
      type *useless;
      lineptr = line;
      if (!parse_methodcall(&useless)) goto err;
    }

    else if (expect('=')) {
      lineptr = line;
      if (!parse_objectasgn()) goto err;
    }

    else if (expect(':')) {
      lineptr = line;
      if (!parse_methoddecl()) goto err;
    }

    else if (expect(NONSPECIAL)) {
      lineptr = line;
      if (!parse_objectdecl()) goto err;
    }

    else goto err;
  }

  else goto err;

  goto nextline;

err:
  printf("  ");
  for (i = 0; i < lineptr-line; i++) putchar(' ');
  printf("^\n");
  if (errmsg) ERROR("%s", errmsg);
  else        ERROR("parsing");
  //if (fp != stdin) { fclose(fp); return 1; }
  goto nextline;
}
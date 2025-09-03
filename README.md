# javatype

To build, just `gcc javatype.c -o javatype`

To run, either `./javatype` for interactive prompt or `./javatype <filename>` to load statements from the file line-by-line. Note that files must end with a newline.

To learn the syntax, look at `test.txt`. Also, `err.txt` shows all the possible errors that can occur.

# Demo

```
     javatype, by wyan
     ? for help

> types B<A
- B <: A
- A <: Object
> ?t
boolean <: _Root
A <: Object
B <: A
_Root
int <: _Root
float <: _Root
Object <: _Root
double <: _Root
char <: _Root
> A::f(Object)
> B::f(Object)
> B::f(A)
> ?v
A.f(Object)
B.f(Object)
B.f(A)
> A a = A()
- a : A (rtt=A)
> A b1 = B()
- b1 : A (rtt=B)
> B b2 = B()
- b2 : B (rtt=B)
> ?o
a : A (rtt=A)
b1 : A (rtt=B)
b2 : B (rtt=B)
> a.f(a)
- a.f(A) -> A::f(Object) (ctt) -> A::f(Object) (rtt)
> b1.f(a)
- b1.f(A) -> A::f(Object) (ctt) -> B::f(Object) (rtt)
> b2.f(a)
- b2.f(A) -> B::f(A) (ctt) -> B::f(A) (rtt)
```

# Limitations

- No interfaces (hence no multiple inheritance)
- No generics and array types, and other kinds of fancy Java features
- Parameters in a method call are limited to expressions of the form `obj` and `(Type)obj`, no complex expressions like `obj.method(...)`
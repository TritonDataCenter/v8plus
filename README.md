v8+: Node addon C++ to C boundary layer

## Usage

For full docs, read the source code.

### Overview

This layer offers a way to write at least simple Node addons in C without
all the horrible C++ goop you'd otherwise be expected to use.  That goop
still exists, but you don't have to write it.  More importantly, you can
write your module in a sane programming environment, avoiding the confusing
and error-prone C++ semantics.

Your module is an object factory.  It returns native objects.  When the
JavaScript function named by v8plus_js_factory_name is called, the C
function pointed to by v8plus_ctor will be invoked.  Its first argument is
a pointer to the JavaScript argument list (see Argument Handling below)
and its second argument points to space for storing a pointer to your C
object representation.  Allocate one and stuff it in there.  If something
goes wrong, set the object pointer to NULL and see Exceptions below.
Otherwise, return v8plus_void().

The JavaScript object thus created will have a set of methods you list in
v8plus_methods[].  Each entry in this array is of type v8plus_method_descr_t
and has two members: md_name, the JavaScript name of the method; and
md_c_func, a pointer to the C function that will be invoked when the
JavaScript method is called.  Its first argument will be the pointer to
whatever C object you created in the constructor, and the second will be
the JavaScript arguments to the method (see Argument Handling below).  You
may return v8plus_void() if your function does not return anything, or
an nvlist containing a 'res' member as described in 'Returning Values'
below.  If you need to throw a JavaScript exception, see Exceptions below.

If your constructor (or any method's implementation) allocates memory, free
it in your destructor, which will be passed the pointer set by your
constructor when the object is garbage collected.  Set v8plus_dtor to an
appropriate destructor, which may be an empty function.

### Argument Handling

When JavaScript objects cross the boundary from C++ to C, they are
converted from v8 C++ objects into C nvlists.  The arguments to a
JavaScript function are treated as an array and marshalled into a single
nvlist whose members are named "0", "1", and so on.  Each member is
encoded as follows:

- numbers and Number objects (regardless of size): double
- strings and String objects: UTF-8 encoded C string
- booleans and Boolean objects: boolean_value
- undefined: boolean
- null: byte, value 0
- Objects, including Arrays: nvlist with own properties as members and the
member ".__v8plus_type" set to the object's JavaScript type name.  Note
that the member name itself begins with a . to reduce the likelihood of a
collision with an actual JavaScript member name.

Because JavaScript arrays may be sparse, we cannot use the libnvpair array
types.  Consider them reserved for internal use.  JavaScript Arrays are
represented as they really are in JavaScript: objects with properties whose
names happen to be integers.

Other data types cannot be represented and will result in a TypeError
being thrown.  If your object has methods that need other argument types,
you cannot use v8+.  XXX Callbacks need to be represented as pointers
somehow so that they can be called.

Side effects within the VM, including modification of the arguments, are
not supported.  If you need them, you cannot use v8+.

### Returning Values

Similarly, when returning data across the boundary from C to C++, a
pointer to an nvlist must be returned.  This object will be decoded in
the same manner as described above and returned to the JavaScript caller
of your method.  Note that booleans, strings, and numbers will be encoded
as their primitive types, not objects.  If you need to return something
containing these object types, you cannot use v8+.  Other data types
cannot be represented.  If you need to return them, you cannot use v8+.

The nvlist being returned must have one of two members: "res", an nvlist
containing the result of the call to be returned, or "err", an nvlist
containing members to be added to an exception. 

For convenience, you may return v8plus_void() instead of an nvlist,
which indicates successful execution of a function that returns nothing.

### Exceptions

If you are unable to create an nvlist to hold exception data, or you want a
generic exception to be thrown, return the value returned by v8plus_error().
In this case, the error code will be translated to an exception type and the
message string will be used as the message member of the exception.  Other
members will not be present in the exception unless you also return an
nvlist containing an 'err' member (or, from a constructor, any nvlist) Only
basic v8-provided exception types can be thrown; if your addon needs to
throw some other kind of exception, you will need to either use v8 directly
or catch and re-throw from a JavaScript wrapper.

## Errors

The v8plus_errno_t enumerated type and a family of utility functions are
automatically generated by generrno.js from a simple JSON file.  The schema
of this file is as follows:

	{
		"error_base": <string>,
		"errors": [
		{
			"code": <string>,
			"msg": <string>,
			"exception": <string>
		},
		...  ]
	}

For each entry in the errors array, an identifier V8PLUSERR_<code> will be
added to v8plus_errno_t.  By convention, code should be all upper case.  The
default error message (present in JavaScript exceptions if a more specific
error message is not provided to v8plus_error()) is given by the msg
property.  The exception property must be one of "Error", "TypeError",
"ReferenceError", "RangeError", or "SyntaxError"; i.e., the standard
exception types available in v8.  This is the type of exception that will be
generated and thrown when a C function returns NULL with this error code
set.  In addition, the built-in error codes V8PLUSERR_NOMEM,
V8PLUSERR_YOUSUCK, and V8PLUSERR_UNKNOWN are available for your use,
indicating an out of memory condition, programmer error (e.g., failure of
something you would assert in JavaScript), and an error code that cannot be
translated, respectively.

Set the make variable ERRNO_JSON to the name of this file.

## Installation

The expectation is that you will use v8+ as a git submodule located under
the src/ subdirectory of your Node module.  That is,

	my-cool-module
	|
	+ lib
	|
	+ src
	  |
	  + my_source.c
	  |
	  + v8plus

Because v8+ doesn't do anything itself, there's no installation procedure and
no npm support.  All you need to do to use v8+ is:

0. Write the C code that does whatever your module does.  Be sure to
#include "v8plus/v8plus_glue.h".  Do not include any other v8+ headers.

1. Add posix-getopt to your module's dependency list in package.json and
make sure it's installed.

2. Create a JSON file defining the error codes your module will use.  See
Errors above.

3. Create a Makefile for your native module that includes the v8+ makefiles.
See the example for help.

4. Run make.

It's not necessary for v8+ sources to be located immediately beneath the build
directory or your sources.  Set V8PLUS to the appropriate location in your
makefile if needed.  You should not need to modify either of the delivered
makefiles; override the definitions in Makefile.v8plus.defs in your makefile
as appropriate.

## FAQ

- Why?

Because C++ is garbage.  Writing good software is challenging enough without
trying to understand a bunch of implicit side effects or typing templated
identifiers that can't fit in 80 columns without falling afoul of the
language's ambiguous grammar.  Don't get me started.

- Why not use [FFI](https://github.com/rbranson/node-ffi)?

FFI is really cool; it offers us the ability to use C libraries without
writing bindings at all.  However, it also exposes a lot of C nastiness to
JavaScript code, essentially placing the interface boundary in consuming
code itself.  This pretty much breaks the JavaScript interface model --
for example, you can't really have a function that inspects the types of its
arguments -- and requires you to write an additional C library anyway if you
want or need to do something natively that's not quite what the C library
already does.  Of course, one could use it to write "bindings" in JavaScript
that actually look like a JavaScript interface, which may end up being the
best answer, especially if those are autogenerated from CTF!  In short, v8+
and FFI are different approaches to the problem.  Use whichever fits your
need, and note that they're not mutually exclusive, either.

- What systems can I use this on?

[illumos](http://illumos.org) distributions, or possibly other platforms with
a working libnvpair.  I'm sorry if your system doesn't have it; it's open
source and pretty easy to port.

- What about node-waf?

Fuck python, fuck WAF, and fuck all the hipster douchebags for whom make is
too hard, too old, or "too Unixy".  Make is simple, easy to use, and
extremely reliable.  It was building big, important pieces of software when
your parents were young, and it Just Works.  If you don't like using make
here, you probably don't want to use v8+ either, so just go away.  Write
your CoffeeScript VM in something else, and gyp-scons-waf-rake your way to
an Instagram retirement in Bali with all your hipster douchebag friends.
Just don't bother me about it, because I don't care.

- Why is Node failing in dlopen()?

Most likely, your module has a typo or needs to be linked with a library.
Normally, shared objects like Node addons should be linked with -zdefs so that
these problems are found at build time, but Node doesn't deliver a mapfile
specifying its API so you're left with a bunch of undefined symbols you just
have to hope are defined somewhere in your node process's address space.  If
they aren't, you're boned.  LD_DEBUG=all will help you find the missing
symbol(s).

- Why use the old init() instead of NODE_MODULE()?

Because NODE_MODULE() is a macro that can't be passed another macro as the
name of your addon.  Using it would therefore require the source to v8+ to
be generated at build time to match your module's name, which is
inconvenient.  There may be a way to work around this.

- Why can't I see my exception's decorative properties in JavaScript?

Be careful when decorating exceptions.  There are several built-in hidden
properties; if you decorate the exception with a property with the same
name, you will change the hidden property's value but it will still be
hidden.  This almost certainly is not what you want, so you should prefix
the decorative property names with something unique to your module to avoid
stepping on v8's (or JavaScript's) property namespace.

- What if the factory model doesn't work for me?

See "License" below.

- Is there any way to pass JavaScript functions into these methods and call
them from C?

Yes, but I haven't gotten around to that yet.  It's obviously needed.

## License

MIT.

## Bugs

See <https://github.com/wesolows/v8plus/issues>.

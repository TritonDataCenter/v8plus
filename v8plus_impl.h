/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#ifndef	_V8PLUS_IMPL_H
#define	_V8PLUS_IMPL_H

#include <sys/ccompile.h>
#include <stdarg.h>
#include <libnvpair.h>
#include <v8.h>
#include "v8plus_glue.h"

/*
 * STOP!
 *
 * Do not #include this header in code that consumes v8+.  This is a private
 * implementation header for use by v8+ internal C++ code.  It cannot be
 * included from C code and contains nothing usable by consumers.
 */

#define	V8PLUS_THROW(_t, _e, _f, _args...) \
    v8::ThrowException(v8plus::exception((_t), (_e), (_f), ## _args))
#define	V8PLUS_THROW_DEFAULT()		V8PLUS_THROW(NULL, NULL, NULL)
#define	V8PLUS_THROW_DECORATED(_e)	V8PLUS_THROW(NULL, (_e), NULL)

namespace v8plus {

class ObjectWrap : public node::ObjectWrap {
public:
	static void init();
	static v8::Handle<v8::Value> cons(const v8::Arguments &);

private:
	static v8::Persistent<v8::Function> _constructor;
	static v8plus_method_descr_t *_mtbl;
	void *_c_impl;

	ObjectWrap() : _c_impl(NULL) {};
	~ObjectWrap() { v8plus_dtor(_c_impl); };

	static v8::Handle<v8::Value> _new(const v8::Arguments &);
	static v8::Handle<v8::Value> _entry(const v8::Arguments &);
};

extern nvlist_t *v8_Arguments_to_nvlist(const v8::Arguments &);
extern v8::Handle<v8::Value> nvpair_to_v8_Value(const nvpair_t *);
extern v8::Local<v8::Value> exception(const char *, const nvlist_t *,
    const char *, ...) __PRINTFLIKE(3);
extern void panic(const char *, ...) __PRINTFLIKE(1) __NORETURN;

}; /* namespace v8plus */

#endif	/* _V8PLUS_IMPL_H */

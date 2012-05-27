/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <string.h>
#include <new>
#include <stdlib.h>
#include <node.h>
#include "v8plus_impl.h"
#include "v8plus_glue.h"

#define	METHOD_NAME_FMT	"__v8plus_%s_%s"

v8::Persistent<v8::Function> v8plus::ObjectWrap::_constructor;
v8plus_method_descr_t *v8plus::ObjectWrap::_mtbl;

static char *
function_name(const char *lambda)
{
	char *fn;
	size_t len;

	len = snprintf(NULL, 0, METHOD_NAME_FMT,
	    v8plus_js_class_name, lambda);
	if ((fn = (char *)malloc(len + 1)) == NULL)
		v8plus::panic("out of memory for function name for %s", lambda);

	(void) snprintf(fn, len + 1, METHOD_NAME_FMT,
		    v8plus_js_class_name, lambda);

	return (fn);
}

void
v8plus::ObjectWrap::init()
{
	v8::Local<v8::FunctionTemplate> tpl = v8::FunctionTemplate::New(_new);
	const v8plus_method_descr_t *mdp;
	uint_t i;

	_mtbl = new (std::nothrow) v8plus_method_descr_t[v8plus_method_count];
	if (_mtbl == NULL)
		v8plus::panic("out of memory for method table");

	tpl->SetClassName(v8::String::NewSymbol(v8plus_js_class_name));
	tpl->InstanceTemplate()->SetInternalFieldCount(v8plus_method_count);

	for (i = 0; i < v8plus_method_count; i++) {
		v8::Local<v8::FunctionTemplate> fth =
		    v8::FunctionTemplate::New(_entry);
		v8::Local<v8::Function> fh = fth->GetFunction();
		mdp = &v8plus_methods[i];

		_mtbl[i].md_name = function_name(mdp->md_name);
		_mtbl[i].md_c_func = mdp->md_c_func;

		fh->SetName(v8::String::New(_mtbl[i].md_name));

		tpl->PrototypeTemplate()->Set(
		    v8::String::NewSymbol(mdp->md_name), fh);
	}

	_constructor = v8::Persistent<v8::Function>::New(tpl->GetFunction());
}

v8::Handle<v8::Value>
v8plus::ObjectWrap::_new(const v8::Arguments& args)
{
	v8::HandleScope scope;
	v8plus::ObjectWrap *op = new v8plus::ObjectWrap();
	nvlist_t *c_excp;
	nvlist_t *c_args;

	if ((c_args = v8plus::v8_Arguments_to_nvlist(args)) == NULL)
		return (V8PLUS_THROW_DEFAULT());

	c_excp = v8plus_ctor(c_args, &op->_c_impl);
	nvlist_free(c_args);
	if (op->_c_impl == NULL) {
		if (c_excp == NULL) {
			return (V8PLUS_THROW_DEFAULT());
		} else {
			return (V8PLUS_THROW_DECORATED(c_excp));
		}
	}

	op->Wrap(args.This());

	return (args.This());
}

v8::Handle<v8::Value>
v8plus::ObjectWrap::cons(const v8::Arguments &args)
{
	v8::HandleScope scope;
	const unsigned argc = 1;
	v8::Handle<v8::Value> argv[argc] = { args[0] };
	v8::Local<v8::Object> instance = _constructor->NewInstance(argc, argv);

	return (scope.Close(instance));
}

/*
 * This is the entry point for all methods.  We will start by demultiplexing
 * out the C method from the function name by which we were called.  There is
 * probably some mechanism by which overly clever JavaScript code could make
 * this not match the actual name; this will kill your Node process, so don't
 * get cute.
 */
v8::Handle<v8::Value>
v8plus::ObjectWrap::_entry(const v8::Arguments &args)
{
	v8::HandleScope scope;
	v8plus::ObjectWrap *op =
	    node::ObjectWrap::Unwrap<v8plus::ObjectWrap>(args.This());
	nvlist_t *c_args;
	nvlist_t *c_out;
	nvlist_t *res, *excp;
	v8::Local<v8::String> self = args.Callee()->GetName()->ToString();
	v8::String::Utf8Value selfsv(self);
	const char *fn = *selfsv;
	const v8plus_method_descr_t *mdp;
	v8plus_c_method_f c_method = NULL;
	uint_t i;

	for (i = 0; i < v8plus_method_count; i++) {
		mdp = &_mtbl[i];
		if (strcmp(mdp->md_name, fn) == 0) {
			c_method = mdp->md_c_func;
			break;
		}
	}

	if (c_method == NULL)
		v8plus::panic("impossible method name %s\n", fn);

	if ((c_args = v8plus::v8_Arguments_to_nvlist(args)) == NULL)
		return (V8PLUS_THROW_DEFAULT());

	c_out = c_method(op->_c_impl, c_args);
	nvlist_free(c_args);

	if (c_out == NULL) {
		if (_v8plus_errno == V8PLUSERR_NOERROR)
			return (scope.Close(v8::Undefined()));
		else
			return (V8PLUS_THROW_DEFAULT());
	} else {
		if (nvlist_lookup_nvlist(c_out, "err", &excp) == 0)
			return (V8PLUS_THROW_DECORATED(excp));
		else if (nvlist_lookup_nvlist(c_out, "res", &res) == 0)
			return (scope.Close(v8plus::nvlist_to_v8_Object(res)));
		else
			v8plus::panic("bad encoded object in return");
	}

	/*NOTREACHED*/
	return (v8::Undefined());
}

extern "C" void
init(v8::Handle<v8::Object> target)
{
	v8plus::ObjectWrap::init();
	target->Set(v8::String::NewSymbol(v8plus_js_factory_name),
	    v8::FunctionTemplate::New(
	    v8plus::ObjectWrap::cons)->GetFunction());
}

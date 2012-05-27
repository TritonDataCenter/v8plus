/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <dlfcn.h>
#include <libnvpair.h>
#include <node.h>
#include <v8.h>
#include "v8plus_impl.h"

#define	V8PLUS_OBJ_TYPE_MEMBER	".__v8plus_type"
#define	V8_EXCEPTION_CTOR_FMT \
    "_ZN2v89Exception%u%sENS_6HandleINS_6StringEEE"

static const char *
cstr(const v8::String::Utf8Value &v)
{
	return (*v);
}

/*
 * Suicide.  It's always an option.  Try to avoid using this as it's not
 * very nice to kill the entire node process; if at all possible we need
 * to throw a JavaScript exception instead.
 */
void
v8plus::panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) vfprintf(stderr, fmt, ap);
	va_end(ap);

	abort();
}

/*
 * Convenience macros for adding stuff to an nvlist and returning on failure.
 */
#define	LA_U(_l, _n, _e) \
	if (((_e) = nvlist_add_boolean((_l), (_n))) != 0) return (err)

#define	LA_N(_l, _n, _e) \
	if (((_e) = nvlist_add_byte((_l), (_n), 0)) != 0) return (err)

#define	LA_V(_l, _t, _n, _v, _e) \
	if (((_e) = nvlist_add_##_t((_l), (_n), (_v))) != 0) return (err)

/*
 * Add an element named <name> to list <lp> with a transcoded value
 * corresponding to <vh> if possible.  Only primitive types, objects that are
 * thin wrappers for primitive types, and objects containing members whose
 * types are all any of the above can be transcoded.
 *
 * Booleans and their Object type are encoded as boolean_value.
 * Numbers and their Object type are encoded as double.
 * Strings and their Object type are encoded as C strings (and assumed UTF-8).
 * Any Object (including an Array) is encoded as an nvlist whose elements
 * are the Object's own properties.
 * Null is encoded as a byte with value 0.
 * Undefined is encoded as the valueless boolean.
 *
 * Returns EINVAL if any argument fails these tests, or any other error code
 * that may be returned by nvlist_add_XXX(3nvpair).
 */
static int
nvlist_add_v8_Value(nvlist_t *lp, const char *name,
    const v8::Handle<v8::Value> &vh)
{
	int err = 0;

	if (vh->IsBoolean()) {
		boolean_t vv = vh->BooleanValue() ? _B_TRUE : _B_FALSE;
		LA_V(lp, boolean_value, name, vv, err);
	} else if (vh->IsNumber()) {
		double vv = vh->NumberValue();
		LA_V(lp, double, name, vv, err);
	} else if (vh->IsString()) {
		v8::String::Utf8Value s(vh);
		const char *vv = cstr(s);
		LA_V(lp, string, name, vv, err);
	} else if (vh->IsUndefined()) {
		LA_U(lp, name, err);
	} else if (vh->IsNull()) {
		LA_N(lp, name, err);
	} else if (vh->IsNumberObject()) {
		double vv = vh->NumberValue();
		LA_V(lp, double, name, vv, err);
	} else if (vh->IsStringObject()) {
		v8::String::Utf8Value s(vh);
		const char *vv = cstr(s);
		LA_V(lp, string, name, vv, err);
	} else if (vh->IsBooleanObject()) {
		boolean_t vv = vh->BooleanValue() ? _B_TRUE : _B_FALSE;
		LA_V(lp, boolean_value, name, vv, err);
	} else if (vh->IsObject()) {
		v8::Local<v8::Object> oh = vh->ToObject();
		v8::Local<v8::Array> keys = oh->GetOwnPropertyNames();
		v8::Local<v8::String> th = oh->GetConstructorName();
		v8::String::Utf8Value tv(th);
		const char *type = cstr(tv);
		nvlist_t *vlp;
		uint_t i;

		if ((err = nvlist_alloc(&vlp, NV_UNIQUE_NAME, 0)) != 0)
			return (err);

		/* XXX this is vile; can we handle this generally? */
		if (strcmp(type, "Object") != 0) {
			if (strcmp(type, "Array") == 0) {
				if ((err = nvlist_add_string(vlp,
				    V8PLUS_OBJ_TYPE_MEMBER, type)) != 0) {
					nvlist_free(vlp);
					return (err);
				}
			} else {
				/* XXX exception or something, fuck */
				(void) fprintf(stderr, "can't handle %s", type);
				abort();
			}
		}

		for (i = 0; i < keys->Length(); i++) {
			char knname[16];
			v8::Local<v8::Value> mk;
			v8::Local<v8::Value> mv;
			const char *k;

			(void) snprintf(knname, sizeof (knname), "%u", i);
			mk = keys->Get(v8::String::New(knname));
			mv = oh->Get(mk);
			v8::String::Utf8Value mks(mk);
			k = cstr(mks);

			if ((err = nvlist_add_v8_Value(vlp, k, mv)) != 0) {
				nvlist_free(vlp);
				return (err);
			}
		}

		LA_V(lp, nvlist, name, vlp, err);
	} else {
		return (EINVAL);
	}

	return (0);
}

#undef	LA_U
#undef	LA_N
#undef	LA_V

nvlist_t *
v8plus::v8_Arguments_to_nvlist(const v8::Arguments &args)
{
	char name[16];
	nvlist_t *lp;
	int err;
	uint_t i;

	if ((err = nvlist_alloc(&lp, NV_UNIQUE_NAME, 0)) != 0)
		return (v8plus_nverr(err, NULL));

	for (i = 0; i < (uint_t)args.Length(); i++) {
		(void) snprintf(name, sizeof (name), "%u", i);
		if ((err = nvlist_add_v8_Value(lp, name, args[i])) != 0) {
			nvlist_free(lp);
			return (v8plus_nverr(err, name));
		}
	}

	return (lp);
}

#define	SET(_o, _p, _jt, _ct, _xt, _pt) \
	do { \
		_ct _v; \
		(void) nvpair_value_##_pt((_p), &_v); \
		(_o)->Set(v8::String::New(nvpair_name((_p))), \
		    v8::_jt::New((_xt)_v)); \
	} while (0)

#define	SET_JS(_o, _p, _c) \
	(_o)->Set(v8::String::New(nvpair_name((_p))), (_c))

/* XXX pass Handle/Local by value or reference? */
static void
decorate_object(v8::Local<v8::Object> &obj, const nvlist_t *lp)
{
	nvpair_t *pp = NULL;

	while ((pp =
	    nvlist_next_nvpair(const_cast<nvlist_t *>(lp), pp)) != NULL) {
		switch (nvpair_type(pp)) {
		case DATA_TYPE_BOOLEAN:
			SET_JS(obj, pp, v8::Undefined());
			break;
		case DATA_TYPE_BOOLEAN_VALUE:
			SET(obj, pp, Boolean, boolean_t, bool, boolean_value);
			break;
		case DATA_TYPE_BYTE:
		{
			uint8_t _v = (uint8_t)-1;

			if (nvpair_value_byte(pp, &_v) != 0 || _v != 0)
				v8plus::panic("bad byte value %02x\n", _v);

			SET_JS(obj, pp, v8::Null());
		}
			break;
		case DATA_TYPE_INT8:
			SET(obj, pp, Number, int8_t, double, int8);
			break;
		case DATA_TYPE_UINT8:
			SET(obj, pp, Number, uint8_t, double, uint8);
			break;
		case DATA_TYPE_INT16:
			SET(obj, pp, Number, int16_t, double, int16);
			break;
		case DATA_TYPE_UINT16:
			SET(obj, pp, Number, uint16_t, double, uint16);
			break;
		case DATA_TYPE_INT32:
			SET(obj, pp, Number, int32_t, double, int32);
			break;
		case DATA_TYPE_UINT32:
			SET(obj, pp, Number, uint32_t, double, uint32);
			break;
		case DATA_TYPE_INT64:
			SET(obj, pp, Number, int64_t, double, int64);
			break;
		case DATA_TYPE_UINT64:
			SET(obj, pp, Number, uint64_t, double, uint64);
			break;
		case DATA_TYPE_STRING:
			SET(obj, pp, String,
			    char *, const char *, string);
			break;
		case DATA_TYPE_NVLIST:
		{
			nvlist_t *clp = NULL;
			(void) nvpair_value_nvlist(pp, &clp);
			SET_JS(obj, pp, v8plus::nvlist_to_v8_Object(clp));
		}
			break;
		default:
			v8plus::panic("bad data type %d\n", nvpair_type(pp));
		}
	}
}

#undef SET
#undef SET_JS

v8::Local<v8::Object>
v8plus::nvlist_to_v8_Object(const nvlist_t *lp)
{
	v8::Local<v8::Object> obj;
	const char *type;

	if (nvlist_lookup_string(const_cast<nvlist_t *>(lp),
	    V8PLUS_OBJ_TYPE_MEMBER, const_cast<char **>(&type)) != 0)
		type = "Object";

	if (strcmp(type, "Array") == 0)
		obj = v8::Array::New()->ToObject();
	else if (strcmp(type, "Object") != 0)
		v8plus::panic("bad object type %s\n", type);
	else
		obj = v8::Object::New();

	decorate_object(obj, lp);

	return (obj);
}

static v8::Local<v8::Value>
sexception(const char *type, const nvlist_t *lp, const char *msg)
{
	char *ctor_name;
	v8::Local<v8::Value> (*excp_ctor)(v8::Handle<v8::String>);
	void *obj_hdl;
	size_t len;
	v8::Local<v8::Value> excp;
	v8::Local<v8::Object> obj;
	v8::Local<v8::String> jsmsg = v8::String::New(msg);

	if (type == NULL) {
		type = v8plus_excptype(_v8plus_errno);
		if (type == NULL)
			type = "Error";
	}

	len = snprintf(NULL, 0, V8_EXCEPTION_CTOR_FMT,
	    (uint_t)strlen(type), type);
	ctor_name = reinterpret_cast<char *>(alloca(len + 1));
	(void) snprintf(ctor_name, len + 1, V8_EXCEPTION_CTOR_FMT,
	    (uint_t)strlen(type), type);

	obj_hdl = dlopen(NULL, RTLD_NOLOAD);
	if (obj_hdl == NULL)
		v8plus::panic("%s\n", dlerror());

	excp_ctor =
	    reinterpret_cast<v8::Local<v8::Value>(*)(v8::Handle<v8::String>)>(
	    dlsym(obj_hdl, ctor_name));

	if (excp_ctor == NULL) {
		(void) dlclose(obj_hdl);
		if (strcmp(type, "Error") == 0) {
			v8plus::panic("Unable to find %s, aborting\n",
			    ctor_name);
		} else {
			excp = v8::Exception::Error(v8::String::New(
			    "Nested exception: illegal exception type"));
			return (excp);
		}
	}

	excp = excp_ctor(jsmsg);
	(void) dlclose(obj_hdl);

	if (lp == NULL)
		return (excp);

	obj = excp->ToObject();
	decorate_object(obj, lp);

	return (excp);
}

v8::Local<v8::Value>
v8plus::exception(const char *type, const nvlist_t *lp, const char *fmt, ...)
{
	v8::Local<v8::Value> exception;
	char *msg;
	size_t len;
	va_list ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		len = vsnprintf(NULL, 0, fmt, ap);
		va_end(ap);
		msg = reinterpret_cast<char *>(alloca(len + 1));
	
		va_start(ap, fmt);
		(void) vsnprintf(msg, len + 1, fmt, ap);
		va_end(ap);
	} else {
		msg = _v8plus_errmsg;
	}

	exception = sexception(type, lp, msg);

	return (exception);
}

/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#include <sys/ccompile.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <uv.h>
#include "v8plus_glue.h"

__thread v8plus_errno_t _v8plus_errno;
__thread char _v8plus_errmsg[V8PLUS_ERRMSG_LEN];

typedef struct v8plus_uv_ctx {
	void *vuc_obj;
	void *vuc_ctx;
	void *vuc_result;
	v8plus_worker_f vuc_worker;
	v8plus_completion_f vuc_completion;
} v8plus_uv_ctx_t;

void *
v8plus_verror(v8plus_errno_t e, const char *fmt, va_list ap)
{
	if (fmt == NULL) {
		if (e == V8PLUSERR_NOERROR) {
			*_v8plus_errmsg = '\0';
		} else {
			(void) snprintf(_v8plus_errmsg, V8PLUS_ERRMSG_LEN,
			    "%s", v8plus_strerror(e));
		}
	} else {
		(void) vsnprintf(_v8plus_errmsg, V8PLUS_ERRMSG_LEN, fmt, ap);
	}
	_v8plus_errno = e;

	return (NULL);
}

void *
v8plus_error(v8plus_errno_t e, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void) v8plus_verror(e, fmt, ap);
	va_end(ap);

	return (NULL);
}

static void __NORETURN
v8plus_vpanic(const char *fmt, va_list ap)
{
	(void) vfprintf(stderr, fmt, ap);
	(void) fflush(stderr);
	abort();
}

void
v8plus_panic(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	v8plus_vpanic(fmt, ap);
	va_end(ap);
}

nvlist_t *
v8plus_nverr(int nverr, const char *member)
{
	(void) snprintf(_v8plus_errmsg, V8PLUS_ERRMSG_LEN,
	    "nvlist manipulation error on member %s: %s",
	    member == NULL ? "<none>" : member, strerror(nverr));

	switch (nverr) {
	case ENOMEM:
		_v8plus_errno = V8PLUSERR_NOMEM;
		break;
	case EINVAL:
		_v8plus_errno = V8PLUSERR_YOUSUCK;
		break;
	default:
		_v8plus_errno = V8PLUSERR_UNKNOWN;
		break;
	}

	return (NULL);
}

nvlist_t *
v8plus_syserr(int syserr, const char *fmt, ...)
{
	v8plus_errno_t e;
	va_list ap;

	switch (syserr) {
	case ENOMEM:
		e = V8PLUSERR_NOMEM;
		break;
	case EBADF:
		e = V8PLUSERR_BADF;
		break;
	default:
		e = V8PLUSERR_UNKNOWN;
		break;
	}

	va_start(ap, fmt);
	(void) v8plus_verror(e, fmt, ap);
	va_end(ap);

	return (NULL);
}

/*
 * The NULL nvlist with V8PLUSERR_NOERROR means we are returning void.
 */
nvlist_t *
v8plus_void(void)
{
	return (v8plus_error(V8PLUSERR_NOERROR, NULL));
}

v8plus_type_t
v8plus_typeof(const nvpair_t *pp)
{
	data_type_t t = nvpair_type((nvpair_t *)pp);

	switch (t) {
	case DATA_TYPE_DOUBLE:
		return (V8PLUS_TYPE_NUMBER);
	case DATA_TYPE_STRING:
		return (V8PLUS_TYPE_STRING);
	case DATA_TYPE_NVLIST:
		return (V8PLUS_TYPE_OBJECT);
	case DATA_TYPE_BOOLEAN_VALUE:
		return (V8PLUS_TYPE_BOOLEAN);
	case DATA_TYPE_BOOLEAN:
		return (V8PLUS_TYPE_UNDEFINED);
	case DATA_TYPE_BYTE:
	{
		uchar_t v;
		if (nvpair_value_byte((nvpair_t *)pp, &v) != 0 || v != 0)
			return (V8PLUS_TYPE_INVALID);
		return (V8PLUS_TYPE_NULL);
	}
	case DATA_TYPE_UINT64_ARRAY:
	{
		uint64_t *vp;
		uint_t nv;
		if (nvpair_value_uint64_array((nvpair_t *)pp, &vp, &nv) != 0 ||
		    nv != 1) {
			return (V8PLUS_TYPE_INVALID);
		}
		return (V8PLUS_TYPE_JSFUNC);
	}
	default:
		return (V8PLUS_TYPE_INVALID);
	}
}

static int
v8plus_arg_value(v8plus_type_t t, nvpair_t *pp, void *vp)
{
	data_type_t dt = nvpair_type(pp);

	switch (t) {
	case V8PLUS_TYPE_NONE:
		return (-1);
	case V8PLUS_TYPE_STRING:
		if (dt == DATA_TYPE_STRING) {
			if (vp != NULL)
				(void) nvpair_value_string(pp, (char **)vp);
			return (0);
		}
		return (-1);
	case V8PLUS_TYPE_NUMBER:
		if (dt == DATA_TYPE_DOUBLE) {
			if (vp != NULL)
				(void) nvpair_value_double(pp, (double *)vp);
			return (0);
		}
		return (-1);
	case V8PLUS_TYPE_BOOLEAN:
		if (dt == DATA_TYPE_BOOLEAN_VALUE) {
			if (vp != NULL) {
				(void) nvpair_value_boolean_value(pp,
				    (boolean_t *)vp);
			}
			return (0);
		}
		return (-1);
	case V8PLUS_TYPE_JSFUNC:
		if (dt == DATA_TYPE_UINT64_ARRAY) {
			uint_t nv;
			uint64_t *vpp;

			if (nvpair_value_uint64_array(pp, &vpp, &nv) == 0 &&
			    nv == 1) {
				if (vp != NULL)
					*(v8plus_jsfunc_t *)vp = vpp[0];
				return (0);
			}
		}
		return (-1);
	case V8PLUS_TYPE_OBJECT:
		if (dt == DATA_TYPE_NVLIST) {
			if (vp != NULL)
				(void) nvpair_value_nvlist(pp, (nvlist_t **)vp);
			return (0);
		}
		return (-1);
	case V8PLUS_TYPE_NULL:
		if (dt == DATA_TYPE_BYTE) {
			uchar_t v;

			if (nvpair_value_byte((nvpair_t *)pp, &v) == 0 &&
			    v == 0)
				return (0);
		}
		return (-1);
	case V8PLUS_TYPE_UNDEFINED:
		return (dt == DATA_TYPE_BOOLEAN ? 0 : -1);
	case V8PLUS_TYPE_ANY:
		if (vp != NULL)
			*(nvpair_t **)vp = pp;
		return (0);
	case V8PLUS_TYPE_INVALID:
		if (vp != NULL)
			*(data_type_t *)vp = dt;
		return (0);
	default:
		return (-1);
	}
}

int
v8plus_args(const nvlist_t *lp, uint_t flags, v8plus_type_t t, ...)
{
	v8plus_type_t nt = t;
	nvpair_t *pp;
	void *vp;
	va_list ap;
	uint_t i;
	char buf[32];

	va_start(ap, t);

	for (i = 0; ; i++) {
		if (nt == V8PLUS_TYPE_NONE)
			break;

		(void) va_arg(ap, void *);

		(void) snprintf(buf, sizeof (buf), "%u", i);
		if (nvlist_lookup_nvpair((nvlist_t *)lp, buf, &pp) != 0)
			return (-1);

		if (v8plus_arg_value(nt, pp, NULL) != 0)
			return (-1);

		nt = va_arg(ap, data_type_t);
	}

	va_end(ap);

	if (flags & V8PLUS_ARG_F_NOEXTRA) {
		(void) snprintf(buf, sizeof (buf), "%u", i);
		if (nvlist_lookup_nvpair((nvlist_t *)lp, buf, &pp) == 0)
			return (-1);
	}

	nt = t;
	va_start(ap, t);

	for (i = 0; ; i++) {
		if (nt == V8PLUS_TYPE_NONE)
			break;

		vp = va_arg(ap, void *);

		(void) snprintf(buf, sizeof (buf), "%u", i);
		if (nvlist_lookup_nvpair((nvlist_t *)lp, buf, &pp) != 0)
			return (-1);

		if (v8plus_arg_value(nt, pp, vp) != 0)
			return (-1);

		nt = va_arg(ap, data_type_t);
	}

	va_end(ap);

	return (0);
}

static nvlist_t *
v8plus_vobj(uint_t level, v8plus_type_t t, va_list *ap)
{
	v8plus_type_t nt = t;
	char *name;
	int err;
	nvlist_t *rp;

	/*
 	 * Do not call va_start() or va_end() in this function!  We are limited
 	 * to a single traversal of the arguments so that we can recurse to
 	 * handle embedded object definitions.
 	 */

	if ((err = nvlist_alloc(&rp, NV_UNIQUE_NAME, 0)) != 0)
		return (v8plus_nverr(err, NULL));

	while (nt != V8PLUS_TYPE_NONE) {
		(void) fprintf(stderr, "type %d\n", nt);
		name = va_arg(*ap, char *);
		(void) fprintf(stderr, "name %s\n", name);
		switch (nt) {
		case V8PLUS_TYPE_STRING:
		{
			char *s = va_arg(*ap, char *);
			if ((err = nvlist_add_string(rp, name, s)) != 0)
				return (v8plus_nverr(err, name));
			break;
		}
		case V8PLUS_TYPE_NUMBER:
		{
			double d = va_arg(*ap, double);
			if ((err = nvlist_add_double(rp, name, d)) != 0)
				return (v8plus_nverr(err, name));
			break;
		}
		case V8PLUS_TYPE_BOOLEAN:
		{
			boolean_t b = va_arg(*ap, boolean_t);
			if ((err = nvlist_add_boolean_value(rp,
			    name, b)) != 0)
				return (v8plus_nverr(err, name));
			break;
		}
		case V8PLUS_TYPE_JSFUNC:
		{
			v8plus_jsfunc_t j = va_arg(*ap, v8plus_jsfunc_t);
			if ((err = nvlist_add_uint64_array(rp,
			    name, &j, 1)) != 0)
				return (v8plus_nverr(err, name));
			if ((err = nvlist_add_string_array(rp,
			    V8PLUS_JSF_COOKIE, NULL, 0)) != 0)
				return (v8plus_nverr(err, V8PLUS_JSF_COOKIE));
			v8plus_jsfunc_hold(j);
			break;
		}
		case V8PLUS_TYPE_OBJECT:
		{
			nt = va_arg(*ap, v8plus_type_t);
			nvlist_t *lp = v8plus_vobj(level + 1, nt, ap);
			if (lp == NULL)
				return (NULL);
			if ((err = nvlist_add_nvlist(rp, name, lp)) != 0)
				return (v8plus_nverr(err, name));
			break;
		}
		case V8PLUS_TYPE_NULL:
			if ((err = nvlist_add_byte(rp, name, 0)) != 0)
				return (v8plus_nverr(err, name));
			break;
		case V8PLUS_TYPE_UNDEFINED:
			if ((err = nvlist_add_boolean(rp, name)) != 0)
				return (v8plus_nverr(err, name));
			break;
		case V8PLUS_TYPE_ANY:
		{
			nvpair_t *pp = va_arg(*ap, nvpair_t *);
			if ((err = nvlist_add_nvpair(rp, pp)) != 0)
				return (v8plus_nverr(err, name));
			break;
		}
		case V8PLUS_TYPE_INVALID:
		default:
			return (v8plus_error(V8PLUSERR_YOUSUCK,
			    "invalid property type %d", nt));
		}

		nt = va_arg(*ap, v8plus_type_t);
	}

	return (rp);
}

nvlist_t *
v8plus_obj(v8plus_type_t t, ...)
{
	nvlist_t *rp;
	va_list ap;

	va_start(ap, t);
	rp = v8plus_vobj(0, t, &ap);
	va_end(ap);

	return (rp);
}

static void
v8plus_uv_worker(uv_work_t *wp)
{
	v8plus_uv_ctx_t *cp = wp->data;

	cp->vuc_result = cp->vuc_worker(cp->vuc_obj, cp->vuc_ctx);
}

static void
v8plus_uv_completion(uv_work_t *wp)
{
	v8plus_uv_ctx_t *cp = wp->data;

	cp->vuc_completion(cp->vuc_obj, cp->vuc_ctx, cp->vuc_result);
	v8plus_obj_rele(cp->vuc_obj);
	free(cp);
	free(wp);
}

void
v8plus_defer(void *cop, void *ctxp, v8plus_worker_f worker,
    v8plus_completion_f completion)
{
	uv_work_t *wp = malloc(sizeof (uv_work_t));
	v8plus_uv_ctx_t *cp = malloc(sizeof (v8plus_uv_ctx_t));

	bzero(wp, sizeof (uv_work_t));
	bzero(cp, sizeof (v8plus_uv_ctx_t));

	v8plus_obj_hold(cop);
	cp->vuc_obj = cop;
	cp->vuc_ctx = ctxp;
	cp->vuc_worker = worker;
	cp->vuc_completion = completion;
	wp->data = cp;

	uv_queue_work(uv_default_loop(), wp, v8plus_uv_worker,
	    v8plus_uv_completion);
}

/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#include <sys/ccompile.h>
#include <stdlib.h>
#include <float.h>
#include <errno.h>
#include <libnvpair.h>
#include "example.h"

static nvlist_t *
example_set_impl(example_t *ep, nvpair_t *pp)
{
	double dv;
	const char *sv;
	const char *ev;
	uint64_t v;

	switch (nvpair_type(pp)) {
	case DATA_TYPE_DOUBLE:
		(void) nvpair_value_double(pp, &dv);
		if (dv > (1ULL << DBL_MANT_DIG) - 1) {
			return (v8plus_error(V8PLUSERR_IMPRECISE,
			    "large number lacks integer precision"));
		}
		ep->e_val = (uint64_t)dv;
		break;
	case DATA_TYPE_STRING:
		(void) nvpair_value_string(pp, (char **)&sv);
		errno = 0;
		v = (uint64_t)strtoull(sv, (char **)&ev, 0);
		if (errno == ERANGE) {
			return (v8plus_error(V8PLUSERR_RANGE,
			    "value '%s' is out of range", sv));
		}
		if (ev != NULL && *ev != '\0') {
			return (v8plus_error(V8PLUSERR_MALFORMED,
			    "value '%s' is malformed", sv));
		}
		ep->e_val = v;
		break;
	case DATA_TYPE_BOOLEAN:	/* undefined */
		ep->e_val = 0;
		break;
	default:
		return (v8plus_error(V8PLUSERR_BADARG,
		    "argument 0 is of incorrect type %d", nvpair_type(pp)));
	}

	return (v8plus_void());
}

static nvlist_t *
example_ctor(const nvlist_t *ap, void **epp)
{
	nvpair_t *pp;
	example_t *ep = malloc(sizeof (example_t));

	if (ep == NULL)
		return (v8plus_error(V8PLUSERR_NOMEM, NULL));

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) == 0) {
		(void)example_set_impl(ep, pp);
		if (_v8plus_errno != V8PLUSERR_NOERROR) {
			free(ep);
			return (NULL);
		}
	} else {
		ep->e_val = 0;
	}

	*epp = ep;

	return (v8plus_void());
}

static void
example_dtor(void *op)
{
	free(op);
}

static nvlist_t *
example_set(void *op, const nvlist_t *ap)
{
	nvpair_t *pp;
	example_t *ep = op;

	if (v8plus_args(ap, 0,
	    V8PLUS_TYPE_ANY, &pp,
	    V8PLUS_TYPE_NONE) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	(void) example_set_impl(ep, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	return (v8plus_void());
}

static nvlist_t *
example_add(void *op, const nvlist_t *ap)
{
	example_t *ep = op;
	example_t ae;
	nvpair_t *pp;
	nvlist_t *eap;
	nvlist_t *erp;

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	(void) example_set_impl(&ae, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	ep->e_val += ae.e_val;

	(void) nvlist_alloc(&eap, NV_UNIQUE_NAME, 0);
	(void) nvlist_add_string(eap, "0", "add");
	erp = v8plus_method_call(op, "__emit", eap);
	nvlist_free(eap);
	nvlist_free(erp);

	return (v8plus_void());
}

static nvlist_t *
example_static_add(const nvlist_t *ap)
{
	char buf[32];
	example_t ae0, ae1;
	nvlist_t *rp;
	nvpair_t *pp;
	uint64_t rv;
	int err;

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	(void) example_set_impl(&ae0, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "1", &pp) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	(void) example_set_impl(&ae1, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	rv = ae0.e_val + ae1.e_val;

	if ((err = nvlist_alloc(&rp, NV_UNIQUE_NAME, 0)) != 0)
		return (v8plus_nverr(err, NULL));

	(void) snprintf(buf, sizeof (buf), "%llu", (unsigned long long)rv);
	if ((err = nvlist_add_string(rp, "res", buf)) != 0) {
		nvlist_free(rp);
		return (v8plus_nverr(err, "res"));
	}

	return (rp);
}

static nvlist_t *
example_multiply(void *op, const nvlist_t *ap)
{
	example_t *ep = op;
	example_t ae;
	nvpair_t *pp;

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	(void) example_set_impl(&ae, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	ep->e_val *= ae.e_val;

	return (v8plus_void());
}

typedef struct async_multiply_ctx {
	example_t amc_operand;
	v8plus_jsfunc_t amc_cb;
} async_multiply_ctx_t;

static void *
async_multiply_worker(void *op, void *ctx)
{
	example_t *ep = op;
	async_multiply_ctx_t *cp = ctx;
	example_t *ap = &cp->amc_operand;

	ep->e_val *= ap->e_val;	/* XXX lock */

	return (v8plus_void());
}

static void
async_multiply_done(void *op, void *ctx, void *res)
{
	async_multiply_ctx_t *cp = ctx;
	nvlist_t *rp;
	nvlist_t *ap;

	(void) res;
	(void) op;

	(void) nvlist_alloc(&ap, NV_UNIQUE_NAME, 0);
	rp = v8plus_call(cp->amc_cb, ap);

	nvlist_free(ap);
	nvlist_free(rp);
	v8plus_jsfunc_rele(cp->amc_cb);
	free(cp);
}

static nvlist_t *
example_multiplyAsync(void *op, const nvlist_t *ap)
{
	nvpair_t *pp;
	v8plus_jsfunc_t cb;
	async_multiply_ctx_t *cp;

	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 0 is required"));
	}

	if (nvlist_lookup_v8plus_jsfunc((nvlist_t *)ap, "1", &cb) != 0) {
		return (v8plus_error(V8PLUSERR_MISSINGARG,
		    "argument 1 is required"));
	}

	if ((cp = malloc(sizeof (async_multiply_ctx_t))) == NULL)
		return (v8plus_error(V8PLUSERR_NOMEM, "no memory for context"));

	(void) example_set_impl(&cp->amc_operand, pp);
	if (_v8plus_errno != V8PLUSERR_NOERROR)
		return (NULL);

	v8plus_jsfunc_hold(cb);
	cp->amc_cb = cb;

	v8plus_defer(op, cp, async_multiply_worker, async_multiply_done);

	return (v8plus_void());
}

static nvlist_t *
example_toString(void *op, const nvlist_t *ap)
{
	example_t *ep = op;
	nvpair_t *pp;
	nvlist_t *rp;
	int err;
	char vbuf[32];

	if ((err = nvlist_alloc(&rp, NV_UNIQUE_NAME, 0)) != 0)
		return (v8plus_nverr(err, NULL));

	/*
 	 * Example of decorated exceptions.  Not strictly needed.  And yeah,
 	 * this interface kind of sucks.
 	 */
	if (nvlist_lookup_nvpair((nvlist_t *)ap, "0", &pp) == 0) {
		nvlist_t *xp;

		if ((err = nvlist_alloc(&xp, NV_UNIQUE_NAME, 0)) != 0) {
			nvlist_free(rp);
			return (v8plus_nverr(err, NULL));
		}

		if ((err = nvlist_add_double(xp, "example_argument", 0)) != 0) {
			nvlist_free(xp);
			nvlist_free(rp);
			return (v8plus_nverr(err, "example_argument"));
		}

		if ((err = nvlist_add_double(xp,
		    "example_type", nvpair_type(pp))) != 0) {
			nvlist_free(xp);
			nvlist_free(rp);
			return (v8plus_nverr(err, "example_type"));
		}

		if ((err = nvlist_add_nvlist(rp, "err", xp)) != 0) {
			nvlist_free(xp);
			nvlist_free(rp);
			return (v8plus_nverr(err, "err"));
		}

		(void) v8plus_error(V8PLUSERR_EXTRAARG,
		    "unsupported superfluous argument(s) found");
		return (rp);
	}

	(void)snprintf(vbuf, sizeof (vbuf), "%llu",
	    (unsigned long long)ep->e_val);

	if ((err = nvlist_add_string(rp, "res", vbuf)) != 0) {
		nvlist_free(rp);
		return (v8plus_nverr(err, "res"));
	}

	return (rp);
}

/*
 * v8+ boilerplate
 */
const v8plus_c_ctor_f v8plus_ctor = example_ctor;
const v8plus_c_dtor_f v8plus_dtor = example_dtor;
const char *v8plus_js_factory_name = "create";
const char *v8plus_js_class_name = "Example";
const v8plus_method_descr_t v8plus_methods[] = {
	{
		md_name: "set",
		md_c_func: example_set
	},
	{
		md_name: "add",
		md_c_func: example_add
	},
	{
		md_name: "multiply",
		md_c_func: example_multiply
	},
	{
		md_name: "toString",
		md_c_func: example_toString
	},
	{
		md_name: "multiplyAsync",
		md_c_func: example_multiplyAsync
	}
};
const uint_t v8plus_method_count =
    sizeof (v8plus_methods) / sizeof (v8plus_methods[0]);

const v8plus_static_descr_t v8plus_static_methods[] = {
	{
		sd_name: "static_add",
		sd_c_func: example_static_add
	}
};
const uint_t v8plus_static_method_count =
    sizeof (v8plus_static_methods) / sizeof (v8plus_static_methods[0]);

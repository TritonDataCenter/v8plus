/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#include <stdarg.h>
#include <string.h>
#include "v8plus_glue.h"

__thread v8plus_errno_t _v8plus_errno;
__thread char _v8plus_errmsg[V8PLUS_ERRMSG_LEN];

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

/*
 * The NULL nvlist with V8PLUSERR_NOERROR means we are returning void.
 */
nvlist_t *
v8plus_void(void)
{
	return (v8plus_error(V8PLUSERR_NOERROR, NULL));
}

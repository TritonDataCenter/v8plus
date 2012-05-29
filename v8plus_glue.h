/*
 * Copyright (c) 2012 Joyent, Inc.  All rights reserved.
 */

#ifndef	_V8PLUS_GLUE_H
#define	_V8PLUS_GLUE_H

#include <stdarg.h>
#include <libnvpair.h>
#include "v8plus_errno.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

#define	V8PLUS_ERRMSG_LEN	512

/*
 * C constructor, destructor, and method prototypes.  See README.md.
 */
typedef nvlist_t *(*v8plus_c_ctor_f)(const nvlist_t *, void **);
typedef nvlist_t *(*v8plus_c_method_f)(void *, const nvlist_t *);
typedef void (*v8plus_c_dtor_f)(void *);

typedef struct v8plus_method_descr {
	const char *md_name;
	v8plus_c_method_f md_c_func;
} v8plus_method_descr_t;

extern __thread v8plus_errno_t _v8plus_errno;
extern __thread char _v8plus_errmsg[V8PLUS_ERRMSG_LEN];

/*
 * Set the errno and message, indicating an error.  The code and
 * printf-formatted message, if one is given, will be used in constructing
 * an exception to be thrown in JavaScript if your method later returns NULL
 * or an nvlist with an "err" member.
 */
extern void *v8plus_verror(v8plus_errno_t, const char *, va_list);
extern void *v8plus_error(v8plus_errno_t, const char *, ...);

/*
 * As above, this convenience function sets the error code and message based
 * on the nvlist-generated error code in its first argument.  The second
 * argument, which may be NULL, should contain the name of the member on
 * which the error occurred.
 */
extern nvlist_t *v8plus_nverr(int, const char *);

/*
 * Clear the errno and message.  This is needed only when one wishes to return
 * NULL from a C method whose return type is effectively void.  The idiom is
 *
 * return (v8plus_void());
 */
extern nvlist_t *v8plus_void(void);

/*
 * These methods are analogous to strerror(3c) and similar functions; they
 * translate among error names, codes, and default messages.  There is
 * normally little need for these functions in C methods, as everything
 * necessary to construct a JavaScript exception is done by v8+, but these
 * may be useful in the construction of supplementary exception decorations
 * for debugging purposes.
 */
extern const char *v8plus_strerror(v8plus_errno_t);
extern const char *v8plus_errname(v8plus_errno_t);
extern const char *v8plus_excptype(v8plus_errno_t);

/*
 * Provided by C code.  See README.md.
 */
extern const v8plus_c_ctor_f v8plus_ctor;
extern const v8plus_c_dtor_f v8plus_dtor;
extern const char *v8plus_js_factory_name;
extern const char *v8plus_js_class_name;
extern const v8plus_method_descr_t v8plus_methods[];
extern const uint_t v8plus_method_count;

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* _V8PLUS_GLUE_H */

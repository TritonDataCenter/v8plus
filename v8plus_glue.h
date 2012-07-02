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

#define	DATA_TYPE_JSFUNC	DATA_TYPE_UINT64_ARRAY

typedef uint64_t v8plus_jsfunc_t;

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
 * Find the named V8 function in the nvlist.  Analogous to other lookup
 * routines; see libnvpair(3lib), with an important exception: the
 * nvlist_lookup_v8plus_jsfunc() and nvpair_value_v8plus_jsfunc() functions
 * place a hold on the underlying function object, which must be released by C
 * code when it is no longer needed.  See the documentation to understand how
 * this works.  The add routine is of very limited utility because there is no
 * mechanism for creating a JS function from C.  It can however be used to
 * return a function (or object containing one, etc.) from a deferred
 * completion routine in which a JS function has been invoked that returned
 * such a thing to us.
 */
extern int nvlist_lookup_v8plus_jsfunc(const nvlist_t *, const char *,
    v8plus_jsfunc_t *);
extern int nvpair_value_v8plus_jsfunc(const nvpair_t *, v8plus_jsfunc_t *);
extern void v8plus_jsfunc_hold(v8plus_jsfunc_t);
extern void v8plus_jsfunc_rele(v8plus_jsfunc_t);

/*
 * Perform a background, possibly blocking and/or expensive, task.  First,
 * the worker function will be enqueued for execution on another thread; its
 * first argument is a pointer to the C object on which to operate, and the
 * second is arbitrary per-call context, arguments, etc. defined by the caller.
 * When that worker function has completed execution, the completion function
 * will be invoked in the main thread.  Its arguments are the C object, the
 * original context pointer, and the return value from the worker function.
 * See the documentation for a typical use case.
 */
typedef void *(*v8plus_worker_f)(void *, void *);
typedef void (*v8plus_completion_f)(void *, void *, void *);

extern void v8plus_defer(void *, void *, v8plus_worker_f, v8plus_completion_f);

/*
 * Call an opaque JavaScript function from C.  The caller is responsible for
 * freeing the returned list.  The first argument is not const because it is
 * possible for the JS code to modify the function represented by the cookie.
 */
extern nvlist_t *v8plus_call(v8plus_jsfunc_t, const nvlist_t *);

/*
 * Call the named JavaScript function in the context of the JS object
 * represented by the native object.  Calling and return conventions are the
 * same as for the C interfaces; i.e., the nvlist will be converted into JS
 * objects and the return value or exception will be in the "res" or "err"
 * members of the nvlist that is returned, respectively.  If an internal
 * error occurs, NULL is returned and _v8plus_errno set accordingly.  The
 * results of calling a method implemented in C via this interface are
 * undefined.
 *
 * This can be used in concert with JS code to emit events asynchronously;
 * see the documentation.
 */
extern nvlist_t *v8plus_method_call(void *, const char *, const nvlist_t *);

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

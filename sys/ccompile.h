/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2015 EveryCity Ltd. All rights reserved.
 */

#ifndef	_SYS_CCOMPILE_H
#define	_SYS_CCOMPILE_H

/*
 * This file contains definitions designed to enable different compilers
 * to be used harmoniously on Solaris systems.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Allow for version tests for compiler bugs and features.
 */
#if defined(__GNUC__)
#define	__GNUC_VERSION	\
	(__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
#define	__GNUC_VERSION	0
#endif

#if defined(__ATTRIBUTE_IMPLEMENTED) || defined(__GNUC__)

/*
 * analogous to lint's PRINTFLIKEn
 */
#define	__sun_attr___PRINTFLIKE__(__n)	\
		__attribute__((__format__(printf, __n, (__n)+1)))
#define	__sun_attr___VPRINTFLIKE__(__n)	\
		__attribute__((__format__(printf, __n, 0)))

/*
 * Handle the kernel printf routines that can take '%b' too
 */
#if __GNUC_VERSION < 30402
/*
 * XX64 at least this doesn't work correctly yet with 3.4.1 anyway!
 */
#define	__sun_attr___KPRINTFLIKE__	__sun_attr___PRINTFLIKE__
#define	__sun_attr___KVPRINTFLIKE__	__sun_attr___VPRINTFLIKE__
#else
#define	__sun_attr___KPRINTFLIKE__(__n)	\
		__attribute__((__format__(cmn_err, __n, (__n)+1)))
#define	__sun_attr___KVPRINTFLIKE__(__n) \
		__attribute__((__format__(cmn_err, __n, 0)))
#endif

/*
 * This one's pretty obvious -- the function never returns
 */
#define	__sun_attr___noreturn__ __attribute__((__noreturn__))

/*
 * The function is 'extern inline' and expects GNU C89 behaviour, not C99
 * behaviour.
 *
 * Should only be used on 'extern inline' definitions for GCC.
 */
#if __GNUC_VERSION >= 40200
#define	__sun_attr___gnu_inline__	__attribute__((__gnu_inline__))
#else
#define	__sun_attr___gnu_inline__
#endif

/*
 * The function has control flow such that it may return multiple times (in
 * the manner of setjmp or vfork)
 */
#if __GNUC_VERSION >= 40100
#define	__sun_attr___returns_twice__	__attribute__((__returns_twice__))
#else
#define	__sun_attr___returns_twice__
#endif

/*
 * This is an appropriate label for functions that do not
 * modify their arguments, e.g. strlen()
 */
#define	__sun_attr___pure__	__attribute__((__pure__))

/*
 * This is a stronger form of __pure__. Can be used for functions
 * that do not modify their arguments and don't depend on global
 * memory.
 */
#define	__sun_attr___const__	__attribute__((__const__))

#if __GNUC_VERSION >= 20700
#define	__aligned(x)		__attribute__((__aligned__(x)))
/*
 * This attribute, attached to a variable, means that the variable is meant to
 * be possibly unused. GCC will not produce a warning for this variable.
 */
#define	__sun_attr___unused__	__attribute__((__unused__))
#endif

#define	___sun_attr_inner(__a)	__sun_attr_##__a
#define	__sun_attr__(__a)	___sun_attr_inner __a

#else	/* __ATTRIBUTE_IMPLEMENTED || __GNUC__ */

#define	__aligned(x)
#define	__sun_attr__(__a)
#define	__sun_attr___unused__

#endif	/* __ATTRIBUTE_IMPLEMENTED || __GNUC__ */

/*
 * Shorthand versions for readability
 */

#define	__PRINTFLIKE(__n)	__sun_attr__((__PRINTFLIKE__(__n)))
#define	__VPRINTFLIKE(__n)	__sun_attr__((__VPRINTFLIKE__(__n)))
#define	__KPRINTFLIKE(__n)	__sun_attr__((__KPRINTFLIKE__(__n)))
#define	__KVPRINTFLIKE(__n)	__sun_attr__((__KVPRINTFLIKE__(__n)))
#define	__NORETURN		__sun_attr__((__noreturn__))
#define	__GNU_INLINE		__inline__ __sun_attr__((__gnu_inline__))
#define	__RETURNS_TWICE		__sun_attr__((__returns_twice__))
#define	__CONST			__sun_attr__((__const__))
#define	__PURE			__sun_attr__((__pure__))
#define	__packed		__attribute__((__packed__))
#define	__unused		__sun_attr__((__unused__))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CCOMPILE_H */

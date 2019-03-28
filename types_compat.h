/*
 * Copyright 2019 Joyent, inc.
 */


#include <stdint.h>

#ifndef _COMPAT_TYPES_H
#define _COMPAT_TYPES_H

typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef unsigned long ulong_t;
typedef long long longlong_t;
typedef unsigned long long u_longlong_t;
typedef enum {B_FALSE, B_TRUE} boolean_t;
typedef long long unsigned int hrtime_t;

#define	_B_FALSE B_FALSE
#define	_B_TRUE B_TRUE
#endif

#
# Copyright (c) 2016 Joyent, Inc.  All rights reserved.
#

V8PLUS =	..

include $(V8PLUS)/Makefile.v8plus.defs

MODULE =	example
MODULE_DIR =	.

SRCS =	\
		example.c

ERRNO_JSON =	errno.json

PREFIX_NODE =	$(shell dirname $$(bash -c 'hash node; hash -t node'))/..
CC =		/opt/local/bin/gcc
CXX =		/opt/local/bin/g++

CTFCONVERT =	/bin/true
CTFMERGE =	/bin/true

include $(V8PLUS)/Makefile.v8plus.targ

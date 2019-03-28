# v8plus Change History

## 1.0.4

Add headers from illumos to allow use on systems not derived from Solaris.

## 1.0.3

Fixes a use-after-free bug, which could, in the right circumstances, cause
the program to crash.

## 1.0.2

Fixes a bug whereby C constructors were silently forced into having one
(and exactly one) argument -- resulting in incorrect behavior when the
number of arguments was zero or more than one.

## 1.0.1

A minor bug fix that allows C constructors to indicate an exception without
inducing a core dump.

## 1.0.0

This is the first major release of *v8plus*.  Throughout its development
history, *v8plus* has sought to remain backwards compatible.  In particular, if
you are already using a 0.3.X version, the upgrade to 1.0.0 should be smooth.

This release is the first to support Node 0.12.X and Node 4.X.  This, and
subsequent versions will follow the usual semver rules with respect to
interface changes and bug fixes.


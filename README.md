## What is xlc-wrapper

This is intended to fix compiler parameters position incompatibility that 
happens due to use of configure scripts and makefiles written for unix-like 
systems where gcc is default compiler. So this tool used to minimize changes
to be done to configuration scripts and makefiles.

xlc-wrapper provides a miniature compatibility layer, intended to resolve
recurrent problems that arise due to inconvenient standard library-level
functionality. At the moment it only contains the following:
  - A fix for malloc and co. that translates calls to malloc(0) to calls to
    malloc(1). See compatmalloc.c for more details.

These are not intended to be permanant fixes. The proper way to fix these
problems would be to modify the programs exhibiting them to be more portable.
However, there are many packages, and we have only so much time that we can
spend applying the same fix over and over again.

## Usage
```sh
make
make test
# installs the library, config file, and stdlib.h
# It installs to $CONDA_PREFIX, xlc-wrapper will expect it to be there
make install
```
The config file takes care of everything except the library search path, since
apparently `-L` is not valid in a configuration file. The easiest way to fix
this is to set `_C89_LIBDIRS` to something like
`"$CONDA_PREFIX/unixcompat /lib /usr/lib"`. You would also need to set
`_CXX_LIBDIRS` for C++.

The dynamic version does not work with the current setup, so unixcompat
will always be linked statically. Fortunately it is quite small.

`make uninstall` will remove everything that was installed.


## Permanent Solutions 
There are a few ways to make non-compliant programs "correct". The best
long-term solutions all involve changes at potentially every call site in the
program. An ongoing list of potential approaches follow.
  1. Change each subsequent `result != NULL` check to also check for whether
  `size == 0`. These are the areas that have actually caused problems.
  Basically, change this:
  ```c
  ptr = malloc(size);
  if (!ptr) {
    /* error stuff */
  ```
  to this:
  ```c
  ptr = malloc(size);
  if (!ptr && size != 0) {
    /* error stuff */
  ```
  - This should not break the target programs if they actually work on other
    platforms. Dereferencing malloc(0) pointers will crash on most platforms
    (I think?), so presumably, if the program works, whenever it gets a pointer
    from malloc(0), it doesn't use it.

## Authors

Rick Harris - xlc-wrapper base

Giancarlo Frix - malloc compatibility layer

Mikhail Sviridov - support and refactoring

© 2016-2021 RocketSoftware, Inc. or its affiliates. All Rights Reserved.

#ifndef LIBCOMPATMALLOC_STDLIB_H
#  define LIBCOMPATMALLOC_STDLIB_H 1
#  pragma map(malloc, "zos_compat_malloc_wrapper")
#  pragma map(calloc, "zos_compat_calloc_wrapper")
#  pragma map(realloc, "zos_compat_realloc_wrapper")
#  pragma map(free, "zos_compat_free_wrapper")
#  include_next <stdlib.h>
#endif /* !LIBCOMPATMALLOC_STDLIB_H */

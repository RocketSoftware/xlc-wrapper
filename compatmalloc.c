#pragma comment(copyright, " © Copyright Rocket Software, Inc. 2017, 2020 ")

#include <stdlib.h>

/* TODO: figure out how to wrap new, new[], delete, and delete[],
 * if necessary.
 */


/* To somewhat convince ourselves that, this isn't introducing
 * memory leaks, maybe wrap free as well, then keep track of
 * the 0->1 allocations and frees of the corresponding addresses.
 * Might not cover all possible cases, but I think it would
 * work for the most common use cases.
 */

/* On Linux, Windows, and OS X, malloc(0) returns a
 * non-NULL, freeable pointer.
 */
void *zos_compat_malloc_wrapper(size_t size) {
    if (size == 0) {
        size = 1;
    }
    return malloc(size);
}

/* calloc(0, _) and calloc(_, 0) behaves in line
 * with malloc(0) on linux, probably other platforms too.
 */
void *zos_compat_calloc_wrapper(size_t num, size_t size) {
    if (num == 0) {
        num = 1;
    }
    if (size == 0) {
        size = 1;
    }
    return calloc(num, size);
}

/* On linux, with sole regard to return values, realloc acts as if
 * it were defined as such:
 *  realloc(ptr, size)
 *   | ptr == NULL   = malloc(val)
 *   | size == 0     = free(val)
 *   | otherwise     = malloc(val)
 */
void *zos_compat_realloc_wrapper(void *ptr, size_t size) {
    if (ptr == NULL && size == 0) {
        size = 1;
    }
    return realloc(ptr, size);
}

/* While at the moment the following wrapper does nothing, it's
 * included to provide a stable forward compatible ABI, in case
 * we ever want to do something involving tracing allocations.
 * I mean, we've already wrapped most of the allocation functions,
 * might as well properly take advantage of that fact by doing the
 * rest.
 */
void zos_compat_free_wrapper(void *ptr) {
    free(ptr);
}

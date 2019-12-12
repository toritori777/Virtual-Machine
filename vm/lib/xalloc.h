#include <stdio.h>

#ifndef _C0LIB_H
#define _C0LIB_H

/* xcalloc(nobj, size) returns a non-NULL pointer to
 * array of nobj objects, each of size size and
 * exits if the allocation fails.  Like calloc, the
 * array is initialized with zeroes.
 */
void* xcalloc(size_t nobj, size_t size);
/* rename to xcalloc, xmalloc */

/* xmalloc(size) returns a non-NULL pointer to
 * an object of size size and exits if the allocation
 * fails.  Like malloc, no initialization is guaranteed.
 */
void* xmalloc(size_t size);

#endif

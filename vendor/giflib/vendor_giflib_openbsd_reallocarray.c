// Minimal replacement for openbsd_reallocarray for Windows platform.
// Place this file as vendor/giflib/openbsd_reallocarray.c

#include <stdlib.h>
#include <errno.h>
#include <limits.h>

void *openbsd_reallocarray(void *ptr, size_t nmemb, size_t size) {
    // Check for overflow
    if (size && nmemb > SIZE_MAX / size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(ptr, nmemb * size);
}
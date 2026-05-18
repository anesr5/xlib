#ifndef XLIB_MEMORY_H
#define XLIB_MEMORY_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum x_memory_protection {
    X_MEMORY_PROTECT_NONE = 0,
    X_MEMORY_PROTECT_READ = 1,
    X_MEMORY_PROTECT_WRITE = 2,
    X_MEMORY_PROTECT_EXECUTE = 4
} x_memory_protection_t;

typedef struct x_mapped_file x_mapped_file_t;

XLIB_API int x_memory_page_size(size_t *page_size);

/*
 * Allocates page-backed virtual memory with the requested protection flags.
 * Free successful allocations with x_virtual_memory_free.
 */
XLIB_API int x_virtual_memory_alloc(void **address, size_t size, int protection);
XLIB_API int x_virtual_memory_protect(void *address, size_t size, int protection);
XLIB_API void x_virtual_memory_free(void *address, size_t size);

/*
 * Creates or truncates path to size bytes and maps it read-write.
 */
XLIB_API int x_mapped_file_create(x_mapped_file_t **mapping, const char *path, size_t size);

/*
 * Opens an existing file and maps it with the given protection flags.
 * Only X_MEMORY_PROTECT_READ and X_MEMORY_PROTECT_READ | X_MEMORY_PROTECT_WRITE
 * are supported; execute-only mappings are rejected.
 */
XLIB_API int x_mapped_file_open(x_mapped_file_t **mapping, const char *path, int protection);
XLIB_API void *x_mapped_file_data(x_mapped_file_t *mapping);
XLIB_API size_t x_mapped_file_size(const x_mapped_file_t *mapping);
XLIB_API int x_mapped_file_flush(x_mapped_file_t *mapping);
XLIB_API void x_mapped_file_destroy(x_mapped_file_t *mapping);

#ifdef __cplusplus
}
#endif

#endif

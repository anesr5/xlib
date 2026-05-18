#ifndef XLIB_DYNAMIC_LIBRARY_H
#define XLIB_DYNAMIC_LIBRARY_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_dynamic_library x_dynamic_library_t;

XLIB_API int x_dynamic_library_open(x_dynamic_library_t **library, const char *path);
XLIB_API int x_dynamic_library_symbol(x_dynamic_library_t *library, const char *name, void **symbol);
XLIB_API void x_dynamic_library_close(x_dynamic_library_t *library);

XLIB_API int x_dynamic_library_last_error(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif

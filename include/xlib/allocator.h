#ifndef XLIB_ALLOCATOR_H
#define XLIB_ALLOCATOR_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pluggable allocator vtable.  All three function pointers must be non-NULL.
 * user_data is forwarded to every call unchanged.
 *
 * alloc   must behave like malloc:   return NULL on failure, never on size 0.
 * realloc must behave like realloc:  ptr may be NULL (acts as alloc).
 * free    must behave like free:     ptr may be NULL (no-op).
 */
typedef struct x_allocator {
    void *(*alloc)(size_t size, void *user_data);
    void *(*realloc)(void *ptr, size_t size, void *user_data);
    void  (*free)(void *ptr, void *user_data);
    void  *user_data;
} x_allocator_t;

/*
 * Replaces the global allocator used by xlib.  The pointed-to struct is
 * copied; the caller does not need to keep it alive.
 * Passing NULL resets to the default malloc/realloc/free allocator.
 */
XLIB_API void x_allocator_set(const x_allocator_t *allocator);

/*
 * Resets the global allocator to the default malloc/realloc/free behaviour.
 */
XLIB_API void x_allocator_reset(void);

/*
 * Returns a pointer to the currently active allocator.
 */
XLIB_API const x_allocator_t *x_allocator_get(void);

/*
 * Convenience wrappers that dispatch through the active global allocator.
 * Use these inside xlib modules instead of calling malloc/realloc/free
 * directly so that custom allocators are honoured.
 */
XLIB_API void *x_alloc(size_t size);
XLIB_API void *x_realloc(void *ptr, size_t size);
XLIB_API void  x_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif

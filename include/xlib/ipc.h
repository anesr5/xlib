#ifndef XLIB_IPC_H
#define XLIB_IPC_H

#include <stddef.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_shared_memory x_shared_memory_t;
typedef struct x_named_semaphore x_named_semaphore_t;

/*
 * Creates a named shared memory region of size bytes, replacing any existing
 * region with the same name.  Map it into the process address space.
 * Close successful creations with x_shared_memory_close.
 * Remove the name from the system with x_shared_memory_unlink.
 */
XLIB_API int x_shared_memory_create(x_shared_memory_t **shm, const char *name, size_t size);

/*
 * Opens an existing named shared memory region for read-write access.
 * Close successful opens with x_shared_memory_close.
 */
XLIB_API int x_shared_memory_open(x_shared_memory_t **shm, const char *name);

XLIB_API void *x_shared_memory_data(x_shared_memory_t *shm);
XLIB_API size_t x_shared_memory_size(const x_shared_memory_t *shm);

/*
 * Unmaps the region and releases the handle.  Does not remove the name from
 * the system; call x_shared_memory_unlink for that.
 */
XLIB_API void x_shared_memory_close(x_shared_memory_t *shm);

/*
 * Removes the named shared memory object from the system.  Regions that are
 * already mapped continue to be accessible until closed.
 */
XLIB_API int x_shared_memory_unlink(const char *name);

/*
 * Creates or opens a named counting semaphore with initial_count permits.
 * Close successful creations with x_named_semaphore_close.
 * Remove the name from the system with x_named_semaphore_unlink.
 */
XLIB_API int x_named_semaphore_create(
    x_named_semaphore_t **semaphore,
    const char *name,
    unsigned int initial_count);

/*
 * Opens an existing named semaphore.
 * Close successful opens with x_named_semaphore_close.
 */
XLIB_API int x_named_semaphore_open(x_named_semaphore_t **semaphore, const char *name);

XLIB_API int x_named_semaphore_wait(x_named_semaphore_t *semaphore);
XLIB_API int x_named_semaphore_post(x_named_semaphore_t *semaphore);

/*
 * Closes the handle to the semaphore.  Does not remove the name from the
 * system; call x_named_semaphore_unlink for that.
 */
XLIB_API void x_named_semaphore_close(x_named_semaphore_t *semaphore);

/*
 * Removes the named semaphore from the system.  Handles that are already open
 * continue to function until closed.
 */
XLIB_API int x_named_semaphore_unlink(const char *name);

#ifdef __cplusplus
}
#endif

#endif

#ifndef XLIB_IPC_H
#define XLIB_IPC_H

#include <stddef.h>

#include <xlib/memory.h>
#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct x_shared_memory x_shared_memory_t;
typedef struct x_named_semaphore x_named_semaphore_t;
typedef struct x_named_mutex x_named_mutex_t;
typedef struct x_message_queue x_message_queue_t;
typedef struct x_named_pipe x_named_pipe_t;

/*
 * Creates a named shared memory region of size bytes, replacing any existing
 * region with the same name.  Map it into the process address space.
 * Close successful creations with x_shared_memory_close.
 * Remove the name from the system with x_shared_memory_unlink.
 */
XLIB_API int x_shared_memory_create(x_shared_memory_t **shm, const char *name, size_t size);
XLIB_API int x_shared_memory_create_ex(x_shared_memory_t **shm, const char *name, size_t size, int protection);

/*
 * Opens an existing named shared memory region for read-write access.
 * Close successful opens with x_shared_memory_close.
 */
XLIB_API int x_shared_memory_open(x_shared_memory_t **shm, const char *name);
XLIB_API int x_shared_memory_open_ex(x_shared_memory_t **shm, const char *name, int protection);

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

/*
 * Creates or opens a named cross-process mutex.  Close successful creations
 * with x_named_mutex_close.  Remove the name from the system with
 * x_named_mutex_unlink where supported/required by the platform.
 */
XLIB_API int x_named_mutex_create(x_named_mutex_t **mutex, const char *name);
XLIB_API int x_named_mutex_open(x_named_mutex_t **mutex, const char *name);
XLIB_API int x_named_mutex_lock(x_named_mutex_t *mutex);
XLIB_API int x_named_mutex_unlock(x_named_mutex_t *mutex);
XLIB_API void x_named_mutex_close(x_named_mutex_t *mutex);
XLIB_API int x_named_mutex_unlink(const char *name);

/*
 * Fixed-size inter-process message queue built on xlib shared memory and
 * named synchronization primitives. Messages larger than message_size are
 * rejected with EMSGSIZE. Receives report the copied byte count.
 */
XLIB_API int x_message_queue_create(
    x_message_queue_t **queue,
    const char *name,
    size_t capacity,
    size_t message_size);
XLIB_API int x_message_queue_open(x_message_queue_t **queue, const char *name);
XLIB_API int x_message_queue_send(x_message_queue_t *queue, const void *data, size_t size);
XLIB_API int x_message_queue_receive(x_message_queue_t *queue, void *buffer, size_t buffer_size, size_t *size);
XLIB_API void x_message_queue_close(x_message_queue_t *queue);
XLIB_API int x_message_queue_unlink(const char *name);

/* Cross-platform named byte-stream pipe (Windows named pipe / POSIX FIFO). */
XLIB_API int x_named_pipe_create(x_named_pipe_t **pipe, const char *name);
XLIB_API int x_named_pipe_open(x_named_pipe_t **pipe, const char *name);
XLIB_API int x_named_pipe_read(x_named_pipe_t *pipe, void *buffer, size_t size, size_t *bytes_read);
XLIB_API int x_named_pipe_write(x_named_pipe_t *pipe, const void *buffer, size_t size, size_t *bytes_written);
XLIB_API void x_named_pipe_close(x_named_pipe_t *pipe);
XLIB_API int x_named_pipe_unlink(const char *name);

#ifdef __cplusplus
}
#endif

#endif

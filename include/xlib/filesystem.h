#ifndef XLIB_FILESYSTEM_H
#define XLIB_FILESYSTEM_H

#include <stddef.h>
#include <stdint.h>

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

#define XLIB_PATH_MAX 4096
#define XLIB_NAME_MAX 1024

typedef struct x_file x_file_t;
typedef struct x_directory x_directory_t;
typedef struct x_file_watcher x_file_watcher_t;
typedef struct x_async_file_read x_async_file_read_t;

typedef enum x_file_mode {
    X_FILE_READ = 1,
    X_FILE_WRITE = 2,
    X_FILE_CREATE = 4,
    X_FILE_TRUNCATE = 8,
    X_FILE_APPEND = 16
} x_file_mode_t;

typedef enum x_file_seek_origin {
    X_FILE_SEEK_SET = 0,
    X_FILE_SEEK_CURRENT = 1,
    X_FILE_SEEK_END = 2
} x_file_seek_origin_t;

typedef struct x_directory_entry {
    char name[XLIB_NAME_MAX];
    int is_directory;
    uint64_t size;
} x_directory_entry_t;

typedef enum x_file_watch_flags {
    X_FILE_WATCH_CREATED = 1,
    X_FILE_WATCH_MODIFIED = 2,
    X_FILE_WATCH_DELETED = 4
} x_file_watch_flags_t;

typedef void (*x_file_watch_callback)(const char *path, int events, void *user_data);
typedef void (*x_async_file_read_callback)(int error, const void *data, size_t size, void *user_data);

XLIB_API int x_file_open(x_file_t **file, const char *path, int mode);
XLIB_API int x_file_read(x_file_t *file, void *buffer, size_t size, size_t *bytes_read);
XLIB_API int x_file_write(x_file_t *file, const void *buffer, size_t size, size_t *bytes_written);
XLIB_API int x_file_seek(x_file_t *file, int64_t offset, int origin);
XLIB_API int x_file_size(x_file_t *file, uint64_t *size);
XLIB_API void x_file_close(x_file_t *file);
XLIB_API int x_file_remove(const char *path);

XLIB_API int x_directory_create(const char *path);
XLIB_API int x_directory_remove(const char *path);
XLIB_API int x_directory_open(x_directory_t **directory, const char *path);
XLIB_API int x_directory_next(x_directory_t *directory, x_directory_entry_t *entry, int *has_entry);
XLIB_API void x_directory_close(x_directory_t *directory);

XLIB_API int x_path_join(char *buffer, size_t buffer_size, const char *left, const char *right);
XLIB_API int x_path_basename(const char *path, const char **basename);
XLIB_API int x_path_is_absolute(const char *path);

XLIB_API int x_file_watcher_create(x_file_watcher_t **watcher, const char *path);
XLIB_API int x_file_watcher_poll(x_file_watcher_t *watcher, x_file_watch_callback callback, void *user_data);
XLIB_API const char *x_file_watcher_path(x_file_watcher_t *watcher);
XLIB_API void x_file_watcher_destroy(x_file_watcher_t *watcher);

XLIB_API int x_async_file_read_all(
    x_async_file_read_t **operation,
    const char *path,
    x_async_file_read_callback callback,
    void *user_data);
XLIB_API int x_async_file_read_wait(x_async_file_read_t *operation);
XLIB_API void x_async_file_read_destroy(x_async_file_read_t *operation);

#ifdef __cplusplus
}
#endif

#endif

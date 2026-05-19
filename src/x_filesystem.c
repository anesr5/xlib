#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include <xlib/filesystem.h>
#include <xlib/thread.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define XLIB_DWORD_MAX_VALUE ((size_t)0xffffffffUL)
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

struct x_file {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};

struct x_directory {
#ifdef _WIN32
    HANDLE handle;
    WIN32_FIND_DATAW data;
    int has_pending_entry;
    int pending_error;
#else
    DIR *handle;
    char path[XLIB_PATH_MAX];
#endif
};

struct x_file_watcher_state {
    int exists;
    int is_directory;
    uint64_t size;
    uint64_t modified;
};

struct x_file_watcher {
    char path[XLIB_PATH_MAX];
    struct x_file_watcher_state state;
};

struct x_async_file_read {
    x_thread_t *thread;
    char *path;
    unsigned char *data;
    size_t size;
    int error;
    int joined;
    x_async_file_read_callback callback;
    void *user_data;
};

#ifdef _WIN32
static int x_filesystem_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return EEXIST;
    case ERROR_ACCESS_DENIED:
        return EACCES;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
        return EINVAL;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    default:
        return EIO;
    }
}

static int x_filesystem_utf8_to_wide(const char *value, wchar_t **wide)
{
    int count;
    wchar_t *converted;

    if (value == NULL || wide == NULL) {
        return EINVAL;
    }

    *wide = NULL;

    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (count == 0) {
        return x_filesystem_error_from_windows(GetLastError());
    }

    converted = (wchar_t *)calloc((size_t)count, sizeof(*converted));
    if (converted == NULL) {
        return ENOMEM;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, converted, count) == 0) {
        int error = x_filesystem_error_from_windows(GetLastError());
        free(converted);
        return error;
    }

    *wide = converted;
    return 0;
}

static int x_filesystem_wide_to_utf8(const wchar_t *value, char *buffer, size_t buffer_size)
{
    int count;

    if (value == NULL || buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    if (buffer_size > (size_t)INT_MAX) {
        return EOVERFLOW;
    }

    count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value,
        -1,
        buffer,
        (int)buffer_size,
        NULL,
        NULL);

    if (count == 0) {
        DWORD error = GetLastError();
        return error == ERROR_INSUFFICIENT_BUFFER
            ? ENAMETOOLONG
            : x_filesystem_error_from_windows(error);
    }

    return 0;
}

static int x_filesystem_make_search_pattern(const char *path, wchar_t **pattern)
{
    char joined[XLIB_PATH_MAX];
    int error;

    error = x_path_join(joined, sizeof(joined), path, "*");
    if (error != 0) {
        return error;
    }

    return x_filesystem_utf8_to_wide(joined, pattern);
}
#endif

static int x_filesystem_is_dot_entry(const char *name)
{
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

static int x_filesystem_has_separator(const char *path)
{
    size_t length;

    if (path == NULL) {
        return 0;
    }

    length = strlen(path);
    return length > 0U
        && (path[length - 1U] == '/'
#ifdef _WIN32
            || path[length - 1U] == '\\'
#endif
        );
}

static char *x_filesystem_strdup(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value);
    copy = (char *)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length + 1U);
    return copy;
}

static int x_file_watcher_snapshot(const char *path, struct x_file_watcher_state *state)
{
    if (path == NULL || state == NULL) {
        return EINVAL;
    }

    memset(state, 0, sizeof(*state));

#ifdef _WIN32
    {
        wchar_t *wide_path;
        WIN32_FILE_ATTRIBUTE_DATA data;
        ULARGE_INTEGER modified;
        int error = x_filesystem_utf8_to_wide(path, &wide_path);

        if (error != 0) {
            return error;
        }

        if (!GetFileAttributesExW(wide_path, GetFileExInfoStandard, &data)) {
            DWORD win_error = GetLastError();
            free(wide_path);
            if (win_error == ERROR_FILE_NOT_FOUND || win_error == ERROR_PATH_NOT_FOUND) {
                return 0;
            }
            return x_filesystem_error_from_windows(win_error);
        }

        free(wide_path);
        modified.LowPart = data.ftLastWriteTime.dwLowDateTime;
        modified.HighPart = data.ftLastWriteTime.dwHighDateTime;

        state->exists = 1;
        state->is_directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        state->size = state->is_directory ? 0U : (((uint64_t)data.nFileSizeHigh << 32) | (uint64_t)data.nFileSizeLow);
        state->modified = modified.QuadPart;
        return 0;
    }
#else
    {
        struct stat status;

        if (stat(path, &status) != 0) {
            return errno == ENOENT ? 0 : errno;
        }

        state->exists = 1;
        state->is_directory = S_ISDIR(status.st_mode) ? 1 : 0;
        state->size = state->is_directory ? 0U : (uint64_t)status.st_size;
        state->modified = (uint64_t)status.st_mtime;
        return 0;
    }
#endif
}

static int x_async_file_read_worker(void *data)
{
    x_async_file_read_t *operation = (x_async_file_read_t *)data;
    x_file_t *file = NULL;
    uint64_t file_size = 0U;
    size_t total = 0U;
    int error;

    error = x_file_open(&file, operation->path, X_FILE_READ);
    if (error == 0) {
        error = x_file_size(file, &file_size);
    }

    if (error == 0 && file_size > (uint64_t)((size_t)-1 - 1U)) {
        error = EOVERFLOW;
    }

    if (error == 0) {
        operation->data = (unsigned char *)malloc((size_t)file_size + 1U);
        if (operation->data == NULL) {
            error = ENOMEM;
        }
    }

    while (error == 0 && total < (size_t)file_size) {
        size_t bytes_read = 0U;
        error = x_file_read(file, operation->data + total, (size_t)file_size - total, &bytes_read);
        if (error != 0) {
            break;
        }
        if (bytes_read == 0U) {
            break;
        }
        total += bytes_read;
    }

    if (file != NULL) {
        x_file_close(file);
    }

    if (operation->data != NULL) {
        operation->data[total] = '\0';
    }

    operation->size = total;
    operation->error = error;

    if (operation->callback != NULL) {
        operation->callback(operation->error, operation->data, operation->size, operation->user_data);
    }

    return error;
}

int x_file_open(x_file_t **file, const char *path, int mode)
{
    x_file_t *created;

    if (file == NULL || path == NULL
        || (mode & (X_FILE_READ | X_FILE_WRITE | X_FILE_APPEND)) == 0) {
        return EINVAL;
    }

    *file = NULL;

    created = (x_file_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    {
        wchar_t *wide_path;
        DWORD access = 0;
        DWORD disposition;
        int error = x_filesystem_utf8_to_wide(path, &wide_path);

        if (error != 0) {
            free(created);
            return error;
        }

        if ((mode & X_FILE_READ) != 0) {
            access |= GENERIC_READ;
        }

        if ((mode & (X_FILE_WRITE | X_FILE_APPEND)) != 0) {
            access |= GENERIC_WRITE;
        }

        if ((mode & X_FILE_TRUNCATE) != 0) {
            disposition = (mode & X_FILE_CREATE) != 0 ? CREATE_ALWAYS : TRUNCATE_EXISTING;
        } else if ((mode & X_FILE_CREATE) != 0) {
            disposition = OPEN_ALWAYS;
        } else {
            disposition = OPEN_EXISTING;
        }

        created->handle = CreateFileW(
            wide_path,
            access,
            FILE_SHARE_READ,
            NULL,
            disposition,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        free(wide_path);

        if (created->handle == INVALID_HANDLE_VALUE) {
            error = x_filesystem_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        if ((mode & X_FILE_APPEND) != 0) {
            LARGE_INTEGER offset;
            offset.QuadPart = 0;
            if (!SetFilePointerEx(created->handle, offset, NULL, FILE_END)) {
                error = x_filesystem_error_from_windows(GetLastError());
                CloseHandle(created->handle);
                free(created);
                return error;
            }
        }
    }
#else
    {
        int flags;

        if ((mode & X_FILE_READ) != 0 && (mode & (X_FILE_WRITE | X_FILE_APPEND)) != 0) {
            flags = O_RDWR;
        } else if ((mode & (X_FILE_WRITE | X_FILE_APPEND)) != 0) {
            flags = O_WRONLY;
        } else {
            flags = O_RDONLY;
        }

        if ((mode & X_FILE_CREATE) != 0) {
            flags |= O_CREAT;
        }

        if ((mode & X_FILE_TRUNCATE) != 0) {
            flags |= O_TRUNC;
        }

        if ((mode & X_FILE_APPEND) != 0) {
            flags |= O_APPEND;
        }

        created->fd = open(path, flags | O_CLOEXEC, 0666);
        if (created->fd < 0) {
            int error = errno;
            free(created);
            return error;
        }
    }
#endif

    *file = created;
    return 0;
}

int x_file_read(x_file_t *file, void *buffer, size_t size, size_t *bytes_read)
{
    if (file == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }

#ifdef _WIN32
    {
        DWORD chunk;
        DWORD actual = 0;

        if (size > XLIB_DWORD_MAX_VALUE) {
            chunk = (DWORD)XLIB_DWORD_MAX_VALUE;
        } else {
            chunk = (DWORD)size;
        }

        if (!ReadFile(file->handle, buffer, chunk, &actual, NULL)) {
            return x_filesystem_error_from_windows(GetLastError());
        }

        if (bytes_read != NULL) {
            *bytes_read = (size_t)actual;
        }
    }
#else
    {
        ssize_t result;
        size_t chunk = size;

        if (chunk > (size_t)SSIZE_MAX) {
            chunk = (size_t)SSIZE_MAX;
        }

        result = read(file->fd, buffer, chunk);
        if (result < 0) {
            return errno;
        }

        if (bytes_read != NULL) {
            *bytes_read = (size_t)result;
        }
    }
#endif

    return 0;
}

int x_file_write(x_file_t *file, const void *buffer, size_t size, size_t *bytes_written)
{
    if (file == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

#ifdef _WIN32
    {
        DWORD chunk;
        DWORD actual = 0;

        if (size > XLIB_DWORD_MAX_VALUE) {
            chunk = (DWORD)XLIB_DWORD_MAX_VALUE;
        } else {
            chunk = (DWORD)size;
        }

        if (!WriteFile(file->handle, buffer, chunk, &actual, NULL)) {
            return x_filesystem_error_from_windows(GetLastError());
        }

        if (bytes_written != NULL) {
            *bytes_written = (size_t)actual;
        }
    }
#else
    {
        ssize_t result;
        size_t chunk = size;

        if (chunk > (size_t)SSIZE_MAX) {
            chunk = (size_t)SSIZE_MAX;
        }

        result = write(file->fd, buffer, chunk);
        if (result < 0) {
            return errno;
        }

        if (bytes_written != NULL) {
            *bytes_written = (size_t)result;
        }
    }
#endif

    return 0;
}

int x_file_seek(x_file_t *file, int64_t offset, int origin)
{
    if (file == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        LARGE_INTEGER distance;
        DWORD method;

        if (origin == X_FILE_SEEK_SET) {
            method = FILE_BEGIN;
        } else if (origin == X_FILE_SEEK_CURRENT) {
            method = FILE_CURRENT;
        } else if (origin == X_FILE_SEEK_END) {
            method = FILE_END;
        } else {
            return EINVAL;
        }

        distance.QuadPart = (LONGLONG)offset;
        if (!SetFilePointerEx(file->handle, distance, NULL, method)) {
            return x_filesystem_error_from_windows(GetLastError());
        }
    }
#else
    {
        int whence;

        if (origin == X_FILE_SEEK_SET) {
            whence = SEEK_SET;
        } else if (origin == X_FILE_SEEK_CURRENT) {
            whence = SEEK_CUR;
        } else if (origin == X_FILE_SEEK_END) {
            whence = SEEK_END;
        } else {
            return EINVAL;
        }

        if (lseek(file->fd, (off_t)offset, whence) == (off_t)-1) {
            return errno;
        }
    }
#endif

    return 0;
}

int x_file_size(x_file_t *file, uint64_t *size)
{
    if (file == NULL || size == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file->handle, &file_size)) {
            return x_filesystem_error_from_windows(GetLastError());
        }

        *size = (uint64_t)file_size.QuadPart;
    }
#else
    {
        struct stat status;

        if (fstat(file->fd, &status) != 0) {
            return errno;
        }

        *size = (uint64_t)status.st_size;
    }
#endif

    return 0;
}

void x_file_close(x_file_t *file)
{
    if (file == NULL) {
        return;
    }

#ifdef _WIN32
    CloseHandle(file->handle);
#else
    close(file->fd);
#endif

    free(file);
}

int x_file_remove(const char *path)
{
    if (path == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_path;
        int error = x_filesystem_utf8_to_wide(path, &wide_path);
        if (error != 0) {
            return error;
        }

        if (!DeleteFileW(wide_path)) {
            error = x_filesystem_error_from_windows(GetLastError());
            free(wide_path);
            return error;
        }

        free(wide_path);
        return 0;
    }
#else
    if (unlink(path) != 0) {
        return errno;
    }

    return 0;
#endif
}

int x_directory_create(const char *path)
{
    if (path == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_path;
        int error = x_filesystem_utf8_to_wide(path, &wide_path);
        if (error != 0) {
            return error;
        }

        if (!CreateDirectoryW(wide_path, NULL)) {
            DWORD win_error = GetLastError();
            free(wide_path);
            return win_error == ERROR_ALREADY_EXISTS
                ? 0
                : x_filesystem_error_from_windows(win_error);
        }

        free(wide_path);
        return 0;
    }
#else
    if (mkdir(path, 0777) != 0 && errno != EEXIST) {
        return errno;
    }

    return 0;
#endif
}

int x_directory_remove(const char *path)
{
    if (path == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_path;
        int error = x_filesystem_utf8_to_wide(path, &wide_path);
        if (error != 0) {
            return error;
        }

        if (!RemoveDirectoryW(wide_path)) {
            error = x_filesystem_error_from_windows(GetLastError());
            free(wide_path);
            return error;
        }

        free(wide_path);
        return 0;
    }
#else
    if (rmdir(path) != 0) {
        return errno;
    }

    return 0;
#endif
}

int x_directory_open(x_directory_t **directory, const char *path)
{
    x_directory_t *created;

    if (directory == NULL || path == NULL) {
        return EINVAL;
    }

    *directory = NULL;

    created = (x_directory_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    {
        wchar_t *pattern;
        int error = x_filesystem_make_search_pattern(path, &pattern);
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = FindFirstFileW(pattern, &created->data);
        free(pattern);

        if (created->handle == INVALID_HANDLE_VALUE) {
            error = x_filesystem_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        created->has_pending_entry = 1;
    }
#else
    created->handle = opendir(path);
    if (created->handle == NULL) {
        int error = errno;
        free(created);
        return error;
    }

    if (strlen(path) >= sizeof(created->path)) {
        closedir(created->handle);
        free(created);
        return ENAMETOOLONG;
    }

    strcpy(created->path, path);
#endif

    *directory = created;
    return 0;
}

int x_directory_next(x_directory_t *directory, x_directory_entry_t *entry, int *has_entry)
{
    if (directory == NULL || entry == NULL || has_entry == NULL) {
        return EINVAL;
    }

    *has_entry = 0;
    memset(entry, 0, sizeof(*entry));

#ifdef _WIN32
    if (!directory->has_pending_entry && directory->pending_error != 0) {
        int error = directory->pending_error;
        directory->pending_error = 0;
        return error;
    }

    while (directory->has_pending_entry) {
        DWORD attributes = directory->data.dwFileAttributes;
        DWORD file_size_high = directory->data.nFileSizeHigh;
        DWORD file_size_low = directory->data.nFileSizeLow;
        int error = x_filesystem_wide_to_utf8(
            directory->data.cFileName,
            entry->name,
            sizeof(entry->name));

        if (FindNextFileW(directory->handle, &directory->data)) {
            directory->has_pending_entry = 1;
        } else {
            DWORD find_error = GetLastError();
            directory->has_pending_entry = 0;
            if (find_error != ERROR_NO_MORE_FILES) {
                directory->pending_error = x_filesystem_error_from_windows(find_error);
            }
        }

        if (error != 0) {
            return error;
        }

        if (x_filesystem_is_dot_entry(entry->name)) {
            continue;
        }

        entry->is_directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry->size = ((uint64_t)file_size_high << 32) | (uint64_t)file_size_low;
        *has_entry = 1;
        return 0;
    }

    return 0;
#else
    for (;;) {
        struct dirent *dir_entry;
        char full_path[XLIB_PATH_MAX];
        struct stat status;
        int error;

        errno = 0;
        dir_entry = readdir(directory->handle);
        if (dir_entry == NULL) {
            return errno == 0 ? 0 : errno;
        }

        if (x_filesystem_is_dot_entry(dir_entry->d_name)) {
            continue;
        }

        if (strlen(dir_entry->d_name) >= sizeof(entry->name)) {
            return ENAMETOOLONG;
        }

        strcpy(entry->name, dir_entry->d_name);

        error = x_path_join(full_path, sizeof(full_path), directory->path, dir_entry->d_name);
        if (error != 0) {
            return error;
        }

        if (stat(full_path, &status) != 0) {
            return errno;
        }

        entry->is_directory = S_ISDIR(status.st_mode) ? 1 : 0;
        entry->size = entry->is_directory ? 0U : (uint64_t)status.st_size;
        *has_entry = 1;
        return 0;
    }
#endif
}

void x_directory_close(x_directory_t *directory)
{
    if (directory == NULL) {
        return;
    }

#ifdef _WIN32
    if (directory->handle != NULL && directory->handle != INVALID_HANDLE_VALUE) {
        FindClose(directory->handle);
    }
#else
    if (directory->handle != NULL) {
        closedir(directory->handle);
    }
#endif

    free(directory);
}

int x_path_join(char *buffer, size_t buffer_size, const char *left, const char *right)
{
    const char separator =
#ifdef _WIN32
        '\\';
#else
        '/';
#endif
    size_t left_length;
    size_t right_length;
    size_t needs_separator;
    size_t total_length;

    if (buffer == NULL || buffer_size == 0U || left == NULL || right == NULL) {
        return EINVAL;
    }

    if (x_path_is_absolute(right) || left[0] == '\0') {
        right_length = strlen(right);
        if (right_length + 1U > buffer_size) {
            return ENAMETOOLONG;
        }

        memcpy(buffer, right, right_length + 1U);
        return 0;
    }

    left_length = strlen(left);
    right_length = strlen(right);
    needs_separator = x_filesystem_has_separator(left) || right[0] == '\0' ? 0U : 1U;
    total_length = left_length + needs_separator + right_length;

    if (total_length + 1U > buffer_size) {
        return ENAMETOOLONG;
    }

    memcpy(buffer, left, left_length);
    if (needs_separator) {
        buffer[left_length] = separator;
    }
    memcpy(buffer + left_length + needs_separator, right, right_length + 1U);

    return 0;
}

int x_path_basename(const char *path, const char **basename)
{
    const char *cursor;
    const char *last;

    if (path == NULL || basename == NULL) {
        return EINVAL;
    }

    last = path;
    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/'
#ifdef _WIN32
            || *cursor == '\\'
#endif
        ) {
            last = cursor + 1;
        }
    }

    *basename = last;
    return 0;
}

int x_path_is_absolute(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }

    if (path[0] == '/') {
        return 1;
    }

#ifdef _WIN32
    if (path[0] == '\\' && path[1] == '\\') {
        return 1;
    }

    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))
        && path[1] == ':'
        && (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
#endif

    return 0;
}

int x_file_watcher_create(x_file_watcher_t **watcher, const char *path)
{
    x_file_watcher_t *created;
    int error;

    if (watcher == NULL || path == NULL) {
        return EINVAL;
    }

    if (strlen(path) >= XLIB_PATH_MAX) {
        return ENAMETOOLONG;
    }

    *watcher = NULL;

    created = (x_file_watcher_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    strcpy(created->path, path);
    error = x_file_watcher_snapshot(path, &created->state);
    if (error != 0) {
        free(created);
        return error;
    }

    *watcher = created;
    return 0;
}

int x_file_watcher_poll(x_file_watcher_t *watcher, x_file_watch_callback callback, void *user_data)
{
    struct x_file_watcher_state current;
    int events = 0;
    int error;

    if (watcher == NULL) {
        return EINVAL;
    }

    error = x_file_watcher_snapshot(watcher->path, &current);
    if (error != 0) {
        return error;
    }

    if (!watcher->state.exists && current.exists) {
        events |= X_FILE_WATCH_CREATED;
    } else if (watcher->state.exists && !current.exists) {
        events |= X_FILE_WATCH_DELETED;
    } else if (watcher->state.exists && current.exists
        && (watcher->state.is_directory != current.is_directory
            || watcher->state.size != current.size
            || watcher->state.modified != current.modified)) {
        events |= X_FILE_WATCH_MODIFIED;
    }

    watcher->state = current;

    if (events != 0 && callback != NULL) {
        callback(watcher->path, events, user_data);
    }

    return 0;
}

const char *x_file_watcher_path(x_file_watcher_t *watcher)
{
    if (watcher == NULL) {
        return NULL;
    }

    return watcher->path;
}

void x_file_watcher_destroy(x_file_watcher_t *watcher)
{
    free(watcher);
}

int x_async_file_read_all(
    x_async_file_read_t **operation,
    const char *path,
    x_async_file_read_callback callback,
    void *user_data)
{
    x_async_file_read_t *created;
    int error;

    if (operation == NULL || path == NULL) {
        return EINVAL;
    }

    *operation = NULL;

    created = (x_async_file_read_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->path = x_filesystem_strdup(path);
    if (created->path == NULL) {
        free(created);
        return ENOMEM;
    }

    created->callback = callback;
    created->user_data = user_data;

    error = x_thread_create(&created->thread, x_async_file_read_worker, created);
    if (error != 0) {
        free(created->path);
        free(created);
        return error;
    }

    *operation = created;
    return 0;
}

int x_async_file_read_wait(x_async_file_read_t *operation)
{
    int thread_result = 0;

    if (operation == NULL) {
        return EINVAL;
    }

    if (!operation->joined) {
        int error = x_thread_join(operation->thread, &thread_result);
        if (error != 0) {
            return error;
        }
        operation->joined = 1;
    }

    return operation->error;
}

void x_async_file_read_destroy(x_async_file_read_t *operation)
{
    if (operation == NULL) {
        return;
    }

    if (!operation->joined) {
        (void)x_async_file_read_wait(operation);
    }

    if (operation->thread != NULL) {
        x_thread_destroy(operation->thread);
    }

    free(operation->data);
    free(operation->path);
    free(operation);
}

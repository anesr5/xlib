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

#include <xlib/memory.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct x_mapped_file {
    void *data;
    size_t size;
    int protection;

#ifdef _WIN32
    HANDLE file;
    HANDLE mapping;
#else
    int fd;
#endif
};

#ifdef _WIN32
static int x_memory_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    case ERROR_ACCESS_DENIED:
        return EACCES;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
    case ERROR_INVALID_PARAMETER:
        return EINVAL;
    default:
        return EIO;
    }
}

static DWORD x_memory_virtual_protection(int protection)
{
    int readable = (protection & X_MEMORY_PROTECT_READ) != 0;
    int writable = (protection & X_MEMORY_PROTECT_WRITE) != 0;
    int executable = (protection & X_MEMORY_PROTECT_EXECUTE) != 0;

    if (protection == X_MEMORY_PROTECT_NONE) {
        return PAGE_NOACCESS;
    }

    if (executable && writable) {
        return PAGE_EXECUTE_READWRITE;
    }

    if (executable && readable) {
        return PAGE_EXECUTE_READ;
    }

    if (executable) {
        return PAGE_EXECUTE;
    }

    if (writable) {
        return PAGE_READWRITE;
    }

    if (readable) {
        return PAGE_READONLY;
    }

    return PAGE_NOACCESS;
}
#else
static int x_memory_posix_protection(int protection)
{
    int result = 0;

    if (protection & X_MEMORY_PROTECT_READ) {
        result |= PROT_READ;
    }

    if (protection & X_MEMORY_PROTECT_WRITE) {
        result |= PROT_WRITE;
    }

    if (protection & X_MEMORY_PROTECT_EXECUTE) {
        result |= PROT_EXEC;
    }

    return result;
}
#endif

static int x_memory_validate_protection(int protection)
{
    int known_bits = X_MEMORY_PROTECT_READ
        | X_MEMORY_PROTECT_WRITE
        | X_MEMORY_PROTECT_EXECUTE;

    return (protection & ~known_bits) == 0 ? 0 : EINVAL;
}

int x_memory_page_size(size_t *page_size)
{
    if (page_size == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        *page_size = (size_t)info.dwPageSize;
        return 0;
    }
#else
    {
        long result = sysconf(_SC_PAGESIZE);
        if (result <= 0) {
            return EINVAL;
        }

        *page_size = (size_t)result;
        return 0;
    }
#endif
}

int x_virtual_memory_alloc(void **address, size_t size, int protection)
{
    int error;

    if (address == NULL || size == 0U) {
        return EINVAL;
    }

    *address = NULL;

    error = x_memory_validate_protection(protection);
    if (error != 0) {
        return error;
    }

#ifdef _WIN32
    {
        void *allocated = VirtualAlloc(
            NULL,
            size,
            MEM_RESERVE | MEM_COMMIT,
            x_memory_virtual_protection(protection));

        if (allocated == NULL) {
            return x_memory_error_from_windows(GetLastError());
        }

        *address = allocated;
        return 0;
    }
#else
    {
        void *allocated = mmap(
            NULL,
            size,
            x_memory_posix_protection(protection),
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);

        if (allocated == MAP_FAILED) {
            return errno;
        }

        *address = allocated;
        return 0;
    }
#endif
}

int x_virtual_memory_protect(void *address, size_t size, int protection)
{
    int error;

    if (address == NULL || size == 0U) {
        return EINVAL;
    }

    error = x_memory_validate_protection(protection);
    if (error != 0) {
        return error;
    }

#ifdef _WIN32
    {
        DWORD old_protection;
        if (!VirtualProtect(address, size, x_memory_virtual_protection(protection), &old_protection)) {
            return x_memory_error_from_windows(GetLastError());
        }

        return 0;
    }
#else
    if (mprotect(address, size, x_memory_posix_protection(protection)) != 0) {
        return errno;
    }

    return 0;
#endif
}

void x_virtual_memory_free(void *address, size_t size)
{
    if (address == NULL) {
        return;
    }

#ifdef _WIN32
    (void)size;
    VirtualFree(address, 0, MEM_RELEASE);
#else
    munmap(address, size);
#endif
}

int x_mapped_file_create(x_mapped_file_t **mapping, const char *path, size_t size)
{
    x_mapped_file_t *created;

    if (mapping == NULL || path == NULL || size == 0U) {
        return EINVAL;
    }

    *mapping = NULL;

    created = (x_mapped_file_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    created->size = size;
    created->protection = X_MEMORY_PROTECT_READ | X_MEMORY_PROTECT_WRITE;

#ifdef _WIN32
    {
        LARGE_INTEGER file_size;

        if (size > (size_t)INT64_MAX) {
            free(created);
            return EOVERFLOW;
        }

        file_size.QuadPart = (LONGLONG)size;
        created->file = CreateFileA(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (created->file == INVALID_HANDLE_VALUE) {
            int error = x_memory_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        if (!SetFilePointerEx(created->file, file_size, NULL, FILE_BEGIN)
            || !SetEndOfFile(created->file)) {
            int error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->file);
            free(created);
            return error;
        }

        created->mapping = CreateFileMappingA(
            created->file,
            NULL,
            PAGE_READWRITE,
            0,
            0,
            NULL);

        if (created->mapping == NULL) {
            int error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->file);
            free(created);
            return error;
        }

        created->data = MapViewOfFile(created->mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (created->data == NULL) {
            int error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->mapping);
            CloseHandle(created->file);
            free(created);
            return error;
        }
    }
#else
    created->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (created->fd < 0) {
        int error = errno;
        free(created);
        return error;
    }

    if (ftruncate(created->fd, (off_t)size) != 0) {
        int error = errno;
        close(created->fd);
        free(created);
        return error;
    }

    created->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, created->fd, 0);
    if (created->data == MAP_FAILED) {
        int error = errno;
        close(created->fd);
        free(created);
        return error;
    }
#endif

    *mapping = created;
    return 0;
}

int x_mapped_file_open(x_mapped_file_t **mapping, const char *path, int protection)
{
    x_mapped_file_t *created;
    int readable;
    int writable;
    int error;

    if (mapping == NULL || path == NULL) {
        return EINVAL;
    }

    readable = (protection & X_MEMORY_PROTECT_READ) != 0;
    writable = (protection & X_MEMORY_PROTECT_WRITE) != 0;

    if (!readable) {
        return EINVAL;
    }

    error = x_memory_validate_protection(protection);
    if (error != 0) {
        return error;
    }

    *mapping = NULL;

    created = (x_mapped_file_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    {
        LARGE_INTEGER file_size;
        DWORD access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
        DWORD share  = FILE_SHARE_READ | (writable ? FILE_SHARE_WRITE : 0);
        DWORD page   = writable ? PAGE_READWRITE : PAGE_READONLY;
        DWORD view   = writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        created->file = CreateFileA(
            path,
            access,
            share,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        if (created->file == INVALID_HANDLE_VALUE) {
            error = x_memory_error_from_windows(GetLastError());
            free(created);
            return error;
        }

        if (!GetFileSizeEx(created->file, &file_size)) {
            error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->file);
            free(created);
            return error;
        }

        if (file_size.QuadPart == 0) {
            CloseHandle(created->file);
            free(created);
            return EINVAL;
        }

        created->size = (size_t)file_size.QuadPart;
        created->protection = protection;

        created->mapping = CreateFileMappingA(created->file, NULL, page, 0, 0, NULL);
        if (created->mapping == NULL) {
            error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->file);
            free(created);
            return error;
        }

        created->data = MapViewOfFile(created->mapping, view, 0, 0, 0);
        if (created->data == NULL) {
            error = x_memory_error_from_windows(GetLastError());
            CloseHandle(created->mapping);
            CloseHandle(created->file);
            free(created);
            return error;
        }
    }
#else
    {
        struct stat status;
        int flags = writable ? O_RDWR : O_RDONLY;
        int prot  = x_memory_posix_protection(protection);
        int map_flags = writable ? MAP_SHARED : MAP_PRIVATE;

        created->fd = open(path, flags | O_CLOEXEC);
        if (created->fd < 0) {
            error = errno;
            free(created);
            return error;
        }

        if (fstat(created->fd, &status) != 0) {
            error = errno;
            close(created->fd);
            free(created);
            return error;
        }

        if (status.st_size == 0) {
            close(created->fd);
            free(created);
            return EINVAL;
        }

        created->size = (size_t)status.st_size;
        created->protection = protection;
        created->data = mmap(NULL, created->size, prot, map_flags, created->fd, 0);
        if (created->data == MAP_FAILED) {
            error = errno;
            close(created->fd);
            free(created);
            return error;
        }
    }
#endif

    *mapping = created;
    return 0;
}

int x_mapped_file_resize(x_mapped_file_t *mapping, size_t size)
{
    if (mapping == NULL || size == 0U) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        LARGE_INTEGER file_size;
        DWORD page = (mapping->protection & X_MEMORY_PROTECT_WRITE) != 0 ? PAGE_READWRITE : PAGE_READONLY;
        DWORD view = (mapping->protection & X_MEMORY_PROTECT_WRITE) != 0 ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;

        if (size > (size_t)INT64_MAX) {
            return EOVERFLOW;
        }

        if (mapping->data != NULL) {
            UnmapViewOfFile(mapping->data);
            mapping->data = NULL;
        }
        if (mapping->mapping != NULL) {
            CloseHandle(mapping->mapping);
            mapping->mapping = NULL;
        }

        file_size.QuadPart = (LONGLONG)size;
        if (!SetFilePointerEx(mapping->file, file_size, NULL, FILE_BEGIN) || !SetEndOfFile(mapping->file)) {
            return x_memory_error_from_windows(GetLastError());
        }

        mapping->mapping = CreateFileMappingA(mapping->file, NULL, page, 0, 0, NULL);
        if (mapping->mapping == NULL) {
            return x_memory_error_from_windows(GetLastError());
        }

        mapping->data = MapViewOfFile(mapping->mapping, view, 0, 0, size);
        if (mapping->data == NULL) {
            return x_memory_error_from_windows(GetLastError());
        }
    }
#else
    {
        int prot = x_memory_posix_protection(mapping->protection);
        int flags = (mapping->protection & X_MEMORY_PROTECT_WRITE) != 0 ? MAP_SHARED : MAP_PRIVATE;

        if (mapping->data != NULL && mapping->data != MAP_FAILED) {
            munmap(mapping->data, mapping->size);
            mapping->data = NULL;
        }

        if (ftruncate(mapping->fd, (off_t)size) != 0) {
            return errno;
        }

        mapping->data = mmap(NULL, size, prot, flags, mapping->fd, 0);
        if (mapping->data == MAP_FAILED) {
            return errno;
        }
    }
#endif

    mapping->size = size;
    return 0;
}

void *x_mapped_file_data(x_mapped_file_t *mapping)
{
    return mapping == NULL ? NULL : mapping->data;
}

size_t x_mapped_file_size(const x_mapped_file_t *mapping)
{
    return mapping == NULL ? 0U : mapping->size;
}

int x_mapped_file_flush(x_mapped_file_t *mapping)
{
    if (mapping == NULL || mapping->data == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (!FlushViewOfFile(mapping->data, mapping->size) || !FlushFileBuffers(mapping->file)) {
        return x_memory_error_from_windows(GetLastError());
    }

    return 0;
#else
    if (msync(mapping->data, mapping->size, MS_SYNC) != 0) {
        return errno;
    }

    return 0;
#endif
}

void x_mapped_file_destroy(x_mapped_file_t *mapping)
{
    if (mapping == NULL) {
        return;
    }

#ifdef _WIN32
    if (mapping->data != NULL) {
        UnmapViewOfFile(mapping->data);
    }

    if (mapping->mapping != NULL) {
        CloseHandle(mapping->mapping);
    }

    if (mapping->file != NULL && mapping->file != INVALID_HANDLE_VALUE) {
        CloseHandle(mapping->file);
    }
#else
    if (mapping->data != NULL && mapping->data != MAP_FAILED) {
        munmap(mapping->data, mapping->size);
    }

    if (mapping->fd >= 0) {
        close(mapping->fd);
    }
#endif

    free(mapping);
}

#include <xlib/dynamic_library.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

struct x_dynamic_library {
#ifdef _WIN32
    HMODULE handle;
#else
    void *handle;
#endif
};

/* Each thread keeps its own error string to avoid data races when the
 * library is used from multiple threads simultaneously. */
#ifdef _WIN32
#if defined(_MSC_VER)
static __declspec(thread) char x_dynamic_library_error[512];
#else
static __thread char x_dynamic_library_error[512];
#endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
static _Thread_local char x_dynamic_library_error[512];
#else
static __thread char x_dynamic_library_error[512];
#endif

static void x_dynamic_library_set_error_text(const char *message)
{
    if (message == NULL) {
        x_dynamic_library_error[0] = '\0';
        return;
    }

    strncpy(x_dynamic_library_error, message, sizeof(x_dynamic_library_error) - 1U);
    x_dynamic_library_error[sizeof(x_dynamic_library_error) - 1U] = '\0';
}

static void x_dynamic_library_set_error_code(int error)
{
    const char *message = strerror(error);
    x_dynamic_library_set_error_text(message);
}

#ifdef _WIN32
static int x_dynamic_library_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_MOD_NOT_FOUND:
        return ENOENT;
    case ERROR_PROC_NOT_FOUND:
        return ENOENT;
    case ERROR_ACCESS_DENIED:
        return EACCES;
    case ERROR_INVALID_PARAMETER:
    case ERROR_BAD_EXE_FORMAT:
        return EINVAL;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return ENOMEM;
    default:
        return EIO;
    }
}

static void x_dynamic_library_set_windows_error(DWORD error)
{
    char message[sizeof(x_dynamic_library_error)];
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS
        | FORMAT_MESSAGE_MAX_WIDTH_MASK;

    if (FormatMessageA(
            flags,
            NULL,
            error,
            0,
            message,
            (DWORD)sizeof(message),
            NULL) == 0) {
        x_dynamic_library_set_error_code(x_dynamic_library_error_from_windows(error));
        return;
    }

    x_dynamic_library_set_error_text(message);
}

static int x_dynamic_library_utf8_to_wide(const char *value, wchar_t **wide)
{
    int count;
    wchar_t *converted;

    if (value == NULL || wide == NULL) {
        return EINVAL;
    }

    *wide = NULL;

    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (count == 0) {
        DWORD error = GetLastError();
        x_dynamic_library_set_windows_error(error);
        return x_dynamic_library_error_from_windows(error);
    }

    converted = (wchar_t *)calloc((size_t)count, sizeof(*converted));
    if (converted == NULL) {
        x_dynamic_library_set_error_code(ENOMEM);
        return ENOMEM;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, converted, count) == 0) {
        DWORD error = GetLastError();
        free(converted);
        x_dynamic_library_set_windows_error(error);
        return x_dynamic_library_error_from_windows(error);
    }

    *wide = converted;
    return 0;
}
#endif

int x_dynamic_library_open(x_dynamic_library_t **library, const char *path)
{
    x_dynamic_library_t *created;

    if (library == NULL || path == NULL) {
        x_dynamic_library_set_error_code(EINVAL);
        return EINVAL;
    }

    *library = NULL;

    created = (x_dynamic_library_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        x_dynamic_library_set_error_code(ENOMEM);
        return ENOMEM;
    }

#ifdef _WIN32
    {
        wchar_t *wide_path;
        int error = x_dynamic_library_utf8_to_wide(path, &wide_path);
        if (error != 0) {
            free(created);
            return error;
        }

        created->handle = LoadLibraryW(wide_path);
        free(wide_path);

        if (created->handle == NULL) {
            DWORD win_error = GetLastError();
            free(created);
            x_dynamic_library_set_windows_error(win_error);
            return x_dynamic_library_error_from_windows(win_error);
        }
    }
#else
    dlerror();
    created->handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (created->handle == NULL) {
        const char *message = dlerror();
        free(created);
        x_dynamic_library_set_error_text(message);
        return ENOENT;
    }
#endif

    x_dynamic_library_error[0] = '\0';
    *library = created;
    return 0;
}

int x_dynamic_library_symbol(x_dynamic_library_t *library, const char *name, void **symbol)
{
    if (library == NULL || name == NULL || symbol == NULL) {
        x_dynamic_library_set_error_code(EINVAL);
        return EINVAL;
    }

    *symbol = NULL;

#ifdef _WIN32
    {
        FARPROC address = GetProcAddress(library->handle, name);
        union {
            FARPROC function;
            void *object;
        } converter;

        if (address == NULL) {
            DWORD error = GetLastError();
            x_dynamic_library_set_windows_error(error);
            return x_dynamic_library_error_from_windows(error);
        }

        converter.function = address;
        *symbol = converter.object;
    }
#else
    {
        const char *message;

        dlerror();
        *symbol = dlsym(library->handle, name);
        message = dlerror();
        if (message != NULL) {
            *symbol = NULL;
            x_dynamic_library_set_error_text(message);
            return ENOENT;
        }
    }
#endif

    x_dynamic_library_error[0] = '\0';
    return 0;
}

void x_dynamic_library_close(x_dynamic_library_t *library)
{
    if (library == NULL) {
        return;
    }

#ifdef _WIN32
    FreeLibrary(library->handle);
#else
    dlclose(library->handle);
#endif

    free(library);
}

int x_dynamic_library_last_error(char *buffer, size_t buffer_size)
{
    size_t length;

    if (buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

    length = strlen(x_dynamic_library_error);
    if (length + 1U > buffer_size) {
        return ENAMETOOLONG;
    }

    memcpy(buffer, x_dynamic_library_error, length + 1U);
    return 0;
}

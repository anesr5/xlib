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

#include <xlib/process.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

struct x_process {
    int waited;
    int exit_code;

#ifdef _WIN32
    PROCESS_INFORMATION information;
#else
    pid_t pid;
#endif
};

struct x_pipe {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};

#ifdef _WIN32
static int x_process_error_from_windows(DWORD error)
{
    switch (error) {
    case ERROR_SUCCESS:
        return 0;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return ENOENT;
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

static int x_process_utf8_to_wide(const char *value, wchar_t **wide)
{
    int count;
    wchar_t *converted;

    if (value == NULL || wide == NULL) {
        return EINVAL;
    }

    *wide = NULL;

    count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, NULL, 0);
    if (count == 0) {
        return x_process_error_from_windows(GetLastError());
    }

    converted = (wchar_t *)calloc((size_t)count, sizeof(*converted));
    if (converted == NULL) {
        return ENOMEM;
    }

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, converted, count) == 0) {
        int error = x_process_error_from_windows(GetLastError());
        free(converted);
        return error;
    }

    *wide = converted;
    return 0;
}

static size_t x_process_quoted_length(const char *value)
{
    size_t length = 2U;
    size_t backslashes = 0U;

    while (*value != '\0') {
        if (*value == '\\') {
            ++backslashes;
            ++length;
        } else if (*value == '"') {
            length += backslashes + 2U;
            backslashes = 0U;
        } else {
            backslashes = 0U;
            ++length;
        }

        ++value;
    }

    return length + backslashes + 1U;
}

static void x_process_append_quoted(char *target, size_t *offset, const char *value)
{
    size_t backslashes = 0U;

    target[(*offset)++] = '"';
    while (*value != '\0') {
        if (*value == '\\') {
            target[(*offset)++] = *value;
            ++backslashes;
        } else if (*value == '"') {
            while (backslashes-- > 0U) {
                target[(*offset)++] = '\\';
            }
            target[(*offset)++] = '\\';
            target[(*offset)++] = '"';
            backslashes = 0U;
        } else {
            target[(*offset)++] = *value;
            backslashes = 0U;
        }

        ++value;
    }

    while (backslashes-- > 0U) {
        target[(*offset)++] = '\\';
    }
    target[(*offset)++] = '"';
}

static int x_process_build_command_line(
    const char *path,
    char *const arguments[],
    wchar_t **command_line)
{
    char *utf8_command;
    size_t length;
    size_t offset = 0U;
    size_t i;
    int error;

    if (path == NULL || command_line == NULL) {
        return EINVAL;
    }

    *command_line = NULL;

    length = x_process_quoted_length(path);
    if (arguments != NULL) {
        for (i = 0U; arguments[i] != NULL; ++i) {
            length += 1U + x_process_quoted_length(arguments[i]);
        }
    }

    utf8_command = (char *)calloc(length + 1U, sizeof(*utf8_command));
    if (utf8_command == NULL) {
        return ENOMEM;
    }

    x_process_append_quoted(utf8_command, &offset, path);
    if (arguments != NULL) {
        for (i = 0U; arguments[i] != NULL; ++i) {
            utf8_command[offset++] = ' ';
            x_process_append_quoted(utf8_command, &offset, arguments[i]);
        }
    }
    utf8_command[offset] = '\0';

    error = x_process_utf8_to_wide(utf8_command, command_line);
    free(utf8_command);
    return error;
}

static int x_process_open_redirect(
    const char *path,
    DWORD access,
    DWORD disposition,
    HANDLE *handle)
{
    SECURITY_ATTRIBUTES attributes;
    wchar_t *wide_path;
    int error;

    if (path == NULL || handle == NULL) {
        return EINVAL;
    }

    *handle = NULL;

    error = x_process_utf8_to_wide(path, &wide_path);
    if (error != 0) {
        return error;
    }

    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = NULL;
    attributes.bInheritHandle = TRUE;

    *handle = CreateFileW(
        wide_path,
        access,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &attributes,
        disposition,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    free(wide_path);

    if (*handle == INVALID_HANDLE_VALUE) {
        *handle = NULL;
        return x_process_error_from_windows(GetLastError());
    }

    return 0;
}

static int x_process_duplicate_pipe_for_child(x_pipe_t *pipe, HANDLE *handle)
{
    HANDLE current;

    if (pipe == NULL || handle == NULL) {
        return EINVAL;
    }

    *handle = NULL;
    current = GetCurrentProcess();

    if (!DuplicateHandle(
            current,
            pipe->handle,
            current,
            handle,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS)) {
        return x_process_error_from_windows(GetLastError());
    }

    return 0;
}

static int x_process_build_environment_block(char *const environment[], wchar_t **block)
{
    size_t total = 1U;
    size_t i;
    wchar_t *created;
    wchar_t *cursor;

    if (block == NULL) {
        return EINVAL;
    }

    *block = NULL;

    if (environment == NULL) {
        return 0;
    }

    for (i = 0U; environment[i] != NULL; ++i) {
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, environment[i], -1, NULL, 0);
        if (count == 0) {
            return x_process_error_from_windows(GetLastError());
        }
        total += (size_t)count;
    }

    created = (wchar_t *)calloc(total, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

    cursor = created;
    for (i = 0U; environment[i] != NULL; ++i) {
        int count = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            environment[i],
            -1,
            cursor,
            (int)(total - (size_t)(cursor - created)));
        if (count == 0) {
            int error = x_process_error_from_windows(GetLastError());
            free(created);
            return error;
        }
        cursor += count;
    }
    *cursor = L'\0';

    *block = created;
    return 0;
}
#endif

#ifndef _WIN32
extern char **environ;

static size_t x_process_argument_count(char *const arguments[])
{
    size_t count = 0U;

    if (arguments == NULL) {
        return 0U;
    }

    while (arguments[count] != NULL) {
        ++count;
    }

    return count;
}

static int x_process_redirect_fd(const char *path, int target_fd, int flags)
{
    int fd;

    if (path == NULL) {
        return 0;
    }

    fd = open(path, flags | O_CLOEXEC, 0666);
    if (fd < 0) {
        return errno;
    }

    if (dup2(fd, target_fd) < 0) {
        int error = errno;
        close(fd);
        return error;
    }

    close(fd);
    return 0;
}

static int x_process_redirect_pipe(x_pipe_t *pipe, int target_fd)
{
    if (pipe == NULL) {
        return 0;
    }

    if (dup2(pipe->fd, target_fd) < 0) {
        return errno;
    }

    return 0;
}

static const char *x_process_environment_value(char *const environment[], const char *name)
{
    size_t name_length = strlen(name);
    size_t i;

    if (environment == NULL) {
        return getenv(name);
    }

    for (i = 0U; environment[i] != NULL; ++i) {
        if (strncmp(environment[i], name, name_length) == 0 && environment[i][name_length] == '=') {
            return environment[i] + name_length + 1U;
        }
    }

    return NULL;
}

static void x_process_exec_environment(const char *path, char *const arguments[], char *const environment[])
{
    const char *path_value;
    const char *start;

    if (environment == NULL) {
        if (strchr(path, '/') != NULL) {
            execv(path, arguments);
        } else {
            execvp(path, arguments);
        }
        return;
    }

    if (strchr(path, '/') != NULL) {
        execve(path, arguments, environment);
        return;
    }

    path_value = x_process_environment_value(environment, "PATH");
    if (path_value == NULL || path_value[0] == '\0') {
        path_value = "/bin:/usr/bin";
    }

    start = path_value;
    for (;;) {
        const char *end = strchr(start, ':');
        size_t directory_length = end == NULL ? strlen(start) : (size_t)(end - start);
        size_t path_length = strlen(path);
        size_t needs_separator = directory_length > 0U ? 1U : 0U;
        char *candidate = (char *)malloc(directory_length + needs_separator + path_length + 1U);

        if (candidate != NULL) {
            memcpy(candidate, start, directory_length);
            if (needs_separator) {
                candidate[directory_length] = '/';
            }
            memcpy(candidate + directory_length + needs_separator, path, path_length + 1U);
            execve(candidate, arguments, environment);
            free(candidate);
        }

        if (end == NULL) {
            break;
        }
        start = end + 1;
    }
}

static void x_process_set_exit_status(x_process_t *process, int status)
{
    if (WIFEXITED(status)) {
        process->exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        process->exit_code = 128 + WTERMSIG(status);
    } else {
        process->exit_code = -1;
    }
}
#endif

int x_process_start(
    x_process_t **process,
    const char *path,
    char *const arguments[],
    const x_process_options_t *options)
{
    x_process_t *created;

    if (process == NULL || path == NULL) {
        return EINVAL;
    }

    if (options != NULL
        && ((options->stdin_path != NULL && options->stdin_pipe != NULL)
            || (options->stdout_path != NULL && options->stdout_pipe != NULL)
            || (options->stderr_path != NULL && options->stderr_pipe != NULL))) {
        return EINVAL;
    }

    *process = NULL;

    created = (x_process_t *)calloc(1, sizeof(*created));
    if (created == NULL) {
        return ENOMEM;
    }

#ifdef _WIN32
    {
        STARTUPINFOW startup;
        wchar_t *command_line;
        wchar_t *working_directory = NULL;
        wchar_t *environment_block = NULL;
        HANDLE stdin_handle = NULL;
        HANDLE stdout_handle = NULL;
        HANDLE stderr_handle = NULL;
        BOOL inherit_handles = FALSE;
        int error;

        memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        memset(&created->information, 0, sizeof(created->information));

        error = x_process_build_command_line(path, arguments, &command_line);
        if (error != 0) {
            free(created);
            return error;
        }

        if (options != NULL && options->working_directory != NULL) {
            error = x_process_utf8_to_wide(options->working_directory, &working_directory);
            if (error != 0) {
                free(command_line);
                free(created);
                return error;
            }
        }

        if (options != NULL && options->environment != NULL) {
            error = x_process_build_environment_block(options->environment, &environment_block);
            if (error != 0) {
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
        }

        if (options != NULL && options->stdin_path != NULL) {
            error = x_process_open_redirect(options->stdin_path, GENERIC_READ, OPEN_EXISTING, &stdin_handle);
            if (error != 0) {
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        } else if (options != NULL && options->stdin_pipe != NULL) {
            error = x_process_duplicate_pipe_for_child(options->stdin_pipe, &stdin_handle);
            if (error != 0) {
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        }

        if (options != NULL && options->stdout_path != NULL) {
            error = x_process_open_redirect(options->stdout_path, GENERIC_WRITE, CREATE_ALWAYS, &stdout_handle);
            if (error != 0) {
                if (stdin_handle != NULL) {
                    CloseHandle(stdin_handle);
                }
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        } else if (options != NULL && options->stdout_pipe != NULL) {
            error = x_process_duplicate_pipe_for_child(options->stdout_pipe, &stdout_handle);
            if (error != 0) {
                if (stdin_handle != NULL) {
                    CloseHandle(stdin_handle);
                }
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        }

        if (options != NULL && options->stderr_path != NULL) {
            error = x_process_open_redirect(options->stderr_path, GENERIC_WRITE, CREATE_ALWAYS, &stderr_handle);
            if (error != 0) {
                if (stdin_handle != NULL) {
                    CloseHandle(stdin_handle);
                }
                if (stdout_handle != NULL) {
                    CloseHandle(stdout_handle);
                }
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        } else if (options != NULL && options->stderr_pipe != NULL) {
            error = x_process_duplicate_pipe_for_child(options->stderr_pipe, &stderr_handle);
            if (error != 0) {
                if (stdin_handle != NULL) {
                    CloseHandle(stdin_handle);
                }
                if (stdout_handle != NULL) {
                    CloseHandle(stdout_handle);
                }
                free(environment_block);
                free(working_directory);
                free(command_line);
                free(created);
                return error;
            }
            inherit_handles = TRUE;
        }

        if (inherit_handles) {
            startup.dwFlags |= STARTF_USESTDHANDLES;
            startup.hStdInput = stdin_handle != NULL ? stdin_handle : GetStdHandle(STD_INPUT_HANDLE);
            startup.hStdOutput = stdout_handle != NULL ? stdout_handle : GetStdHandle(STD_OUTPUT_HANDLE);
            startup.hStdError = stderr_handle != NULL ? stderr_handle : GetStdHandle(STD_ERROR_HANDLE);
        }

        if (!CreateProcessW(
                NULL,
                command_line,
                NULL,
                NULL,
                inherit_handles,
                environment_block != NULL ? CREATE_UNICODE_ENVIRONMENT : 0,
                environment_block,
                working_directory,
                &startup,
                &created->information)) {
            error = x_process_error_from_windows(GetLastError());
            if (stdin_handle != NULL) {
                CloseHandle(stdin_handle);
            }
            if (stdout_handle != NULL) {
                CloseHandle(stdout_handle);
            }
            if (stderr_handle != NULL) {
                CloseHandle(stderr_handle);
            }
            free(environment_block);
            free(working_directory);
            free(command_line);
            free(created);
            return error;
        }

        if (stdin_handle != NULL) {
            CloseHandle(stdin_handle);
        }
        if (stdout_handle != NULL) {
            CloseHandle(stdout_handle);
        }
        if (stderr_handle != NULL) {
            CloseHandle(stderr_handle);
        }
        free(environment_block);
        free(working_directory);
        free(command_line);
    }
#else
    {
        size_t argument_count = x_process_argument_count(arguments);
        char **child_arguments = (char **)calloc(argument_count + 2U, sizeof(*child_arguments));
        pid_t pid;
        size_t i;

        if (child_arguments == NULL) {
            free(created);
            return ENOMEM;
        }

        child_arguments[0] = (char *)path;
        for (i = 0U; i < argument_count; ++i) {
            child_arguments[i + 1U] = arguments[i];
        }
        child_arguments[argument_count + 1U] = NULL;

        pid = fork();
        if (pid < 0) {
            int error = errno;
            free(child_arguments);
            free(created);
            return error;
        }

        if (pid == 0) {
            if (options != NULL && options->working_directory != NULL) {
                if (chdir(options->working_directory) != 0) {
                    _exit(127);
                }
            }

            if (options != NULL) {
                if (x_process_redirect_fd(options->stdin_path, STDIN_FILENO, O_RDONLY) != 0
                    || x_process_redirect_fd(options->stdout_path, STDOUT_FILENO, O_WRONLY | O_CREAT | O_TRUNC) != 0
                    || x_process_redirect_fd(options->stderr_path, STDERR_FILENO, O_WRONLY | O_CREAT | O_TRUNC) != 0
                    || x_process_redirect_pipe(options->stdin_pipe, STDIN_FILENO) != 0
                    || x_process_redirect_pipe(options->stdout_pipe, STDOUT_FILENO) != 0
                    || x_process_redirect_pipe(options->stderr_pipe, STDERR_FILENO) != 0) {
                    _exit(127);
                }
            }

            x_process_exec_environment(path, child_arguments, options != NULL ? options->environment : environ);
            _exit(127);
        }

        created->pid = pid;
        free(child_arguments);
    }
#endif

    *process = created;
    return 0;
}

int x_process_wait(x_process_t *process, int *exit_code)
{
    if (process == NULL) {
        return EINVAL;
    }

    if (!process->waited) {
#ifdef _WIN32
        DWORD code;

        if (WaitForSingleObject(process->information.hProcess, INFINITE) != WAIT_OBJECT_0) {
            return x_process_error_from_windows(GetLastError());
        }

        if (!GetExitCodeProcess(process->information.hProcess, &code)) {
            return x_process_error_from_windows(GetLastError());
        }

        process->exit_code = (int)code;
#else
        int status;
        pid_t result;

        do {
            result = waitpid(process->pid, &status, 0);
        } while (result < 0 && errno == EINTR);

        if (result < 0) {
            return errno;
        }

        x_process_set_exit_status(process, status);
#endif
        process->waited = 1;
    }

    if (exit_code != NULL) {
        *exit_code = process->exit_code;
    }

    return 0;
}

int x_process_poll(x_process_t *process, int *completed, int *exit_code)
{
    if (process == NULL || completed == NULL) {
        return EINVAL;
    }

    if (process->waited) {
        *completed = 1;
        if (exit_code != NULL) {
            *exit_code = process->exit_code;
        }
        return 0;
    }

#ifdef _WIN32
    {
        DWORD code;
        DWORD result = WaitForSingleObject(process->information.hProcess, 0);

        if (result == WAIT_TIMEOUT) {
            *completed = 0;
            return 0;
        }

        if (result != WAIT_OBJECT_0) {
            return x_process_error_from_windows(GetLastError());
        }

        if (!GetExitCodeProcess(process->information.hProcess, &code)) {
            return x_process_error_from_windows(GetLastError());
        }

        process->exit_code = (int)code;
    }
#else
    {
        int status;
        pid_t result;

        do {
            result = waitpid(process->pid, &status, WNOHANG);
        } while (result < 0 && errno == EINTR);

        if (result < 0) {
            return errno;
        }

        if (result == 0) {
            *completed = 0;
            return 0;
        }

        x_process_set_exit_status(process, status);
    }
#endif

    process->waited = 1;
    *completed = 1;
    if (exit_code != NULL) {
        *exit_code = process->exit_code;
    }
    return 0;
}

int x_process_terminate(x_process_t *process)
{
    if (process == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    if (!TerminateProcess(process->information.hProcess, 1U)) {
        return x_process_error_from_windows(GetLastError());
    }
#else
    if (kill(process->pid, SIGTERM) != 0) {
        return errno;
    }
#endif

    return 0;
}

void x_process_destroy(x_process_t *process)
{
    if (process == NULL) {
        return;
    }

#ifdef _WIN32
    if (process->information.hThread != NULL) {
        CloseHandle(process->information.hThread);
    }

    if (process->information.hProcess != NULL) {
        CloseHandle(process->information.hProcess);
    }
#endif

    free(process);
}

int x_pipe_create(x_pipe_t **read_pipe, x_pipe_t **write_pipe)
{
    x_pipe_t *created_read;
    x_pipe_t *created_write;

    if (read_pipe == NULL || write_pipe == NULL) {
        return EINVAL;
    }

    *read_pipe = NULL;
    *write_pipe = NULL;

    created_read = (x_pipe_t *)calloc(1, sizeof(*created_read));
    created_write = (x_pipe_t *)calloc(1, sizeof(*created_write));
    if (created_read == NULL || created_write == NULL) {
        free(created_read);
        free(created_write);
        return ENOMEM;
    }

#ifdef _WIN32
    {
        HANDLE read_handle;
        HANDLE write_handle;
        SECURITY_ATTRIBUTES attributes;

        attributes.nLength = sizeof(attributes);
        attributes.lpSecurityDescriptor = NULL;
        attributes.bInheritHandle = FALSE;

        if (!CreatePipe(&read_handle, &write_handle, &attributes, 0)) {
            int error = x_process_error_from_windows(GetLastError());
            free(created_read);
            free(created_write);
            return error;
        }

        created_read->handle = read_handle;
        created_write->handle = write_handle;
    }
#else
    {
        int fds[2];

        if (pipe(fds) != 0) {
            int error = errno;
            free(created_read);
            free(created_write);
            return error;
        }
        (void)fcntl(fds[0], F_SETFD, FD_CLOEXEC);
        (void)fcntl(fds[1], F_SETFD, FD_CLOEXEC);

        created_read->fd = fds[0];
        created_write->fd = fds[1];
    }
#endif

    *read_pipe = created_read;
    *write_pipe = created_write;
    return 0;
}

int x_pipe_read(x_pipe_t *pipe, void *buffer, size_t size, size_t *bytes_read)
{
    if (pipe == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_read != NULL) {
        *bytes_read = 0U;
    }

#ifdef _WIN32
    {
        DWORD chunk = size > (size_t)0xffffffffUL ? 0xffffffffUL : (DWORD)size;
        DWORD actual = 0;

        if (!ReadFile(pipe->handle, buffer, chunk, &actual, NULL)) {
            DWORD error = GetLastError();
            return error == ERROR_BROKEN_PIPE ? 0 : x_process_error_from_windows(error);
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

        result = read(pipe->fd, buffer, chunk);
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

int x_pipe_write(x_pipe_t *pipe, const void *buffer, size_t size, size_t *bytes_written)
{
    if (pipe == NULL || buffer == NULL) {
        return EINVAL;
    }

    if (bytes_written != NULL) {
        *bytes_written = 0U;
    }

#ifdef _WIN32
    {
        DWORD chunk = size > (size_t)0xffffffffUL ? 0xffffffffUL : (DWORD)size;
        DWORD actual = 0;

        if (!WriteFile(pipe->handle, buffer, chunk, &actual, NULL)) {
            return x_process_error_from_windows(GetLastError());
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

        result = write(pipe->fd, buffer, chunk);
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

void x_pipe_close(x_pipe_t *pipe)
{
    if (pipe == NULL) {
        return;
    }

#ifdef _WIN32
    if (pipe->handle != NULL) {
        CloseHandle(pipe->handle);
    }
#else
    if (pipe->fd >= 0) {
        close(pipe->fd);
    }
#endif

    free(pipe);
}

int x_process_run_pipeline(
    const char *first_path,
    char *const first_arguments[],
    const char *second_path,
    char *const second_arguments[],
    const x_process_options_t *options,
    int *exit_code)
{
    x_pipe_t *pipe_read = NULL;
    x_pipe_t *pipe_write = NULL;
    x_process_t *first = NULL;
    x_process_t *second = NULL;
    x_process_options_t first_options;
    x_process_options_t second_options;
    int first_exit = 0;
    int second_exit = 0;
    int error;

    if (first_path == NULL || second_path == NULL) {
        return EINVAL;
    }

    error = x_pipe_create(&pipe_read, &pipe_write);
    if (error != 0) {
        return error;
    }

    memset(&first_options, 0, sizeof(first_options));
    memset(&second_options, 0, sizeof(second_options));

    if (options != NULL) {
        first_options.working_directory = options->working_directory;
        first_options.stdin_path = options->stdin_path;
        first_options.stderr_path = options->stderr_path;
        first_options.environment = options->environment;
        first_options.stdin_pipe = options->stdin_pipe;
        first_options.stderr_pipe = options->stderr_pipe;

        second_options.working_directory = options->working_directory;
        second_options.stdout_path = options->stdout_path;
        second_options.stderr_path = options->stderr_path;
        second_options.environment = options->environment;
        second_options.stdout_pipe = options->stdout_pipe;
        second_options.stderr_pipe = options->stderr_pipe;
    }

    first_options.stdout_pipe = pipe_write;
    second_options.stdin_pipe = pipe_read;

    error = x_process_start(&first, first_path, first_arguments, &first_options);
    if (error == 0) {
        error = x_process_start(&second, second_path, second_arguments, &second_options);
    }

    x_pipe_close(pipe_write);
    x_pipe_close(pipe_read);

    if (error != 0) {
        if (first != NULL) {
            (void)x_process_terminate(first);
            (void)x_process_wait(first, NULL);
            x_process_destroy(first);
        }
        return error;
    }

    error = x_process_wait(first, &first_exit);
    if (error == 0) {
        error = x_process_wait(second, &second_exit);
    } else {
        (void)x_process_wait(second, &second_exit);
    }

    x_process_destroy(first);
    x_process_destroy(second);

    if (error != 0) {
        return error;
    }

    if (exit_code != NULL) {
        *exit_code = second_exit != 0 ? second_exit : first_exit;
    }

    return 0;
}

int x_environment_get(const char *name, char *buffer, size_t buffer_size)
{
    if (name == NULL || buffer == NULL || buffer_size == 0U) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_name;
        wchar_t *wide_value;
        DWORD required;
        int error = x_process_utf8_to_wide(name, &wide_name);
        if (error != 0) {
            return error;
        }

        required = GetEnvironmentVariableW(wide_name, NULL, 0);
        if (required == 0) {
            error = GetLastError() == ERROR_ENVVAR_NOT_FOUND ? ENOENT : x_process_error_from_windows(GetLastError());
            free(wide_name);
            return error;
        }

        wide_value = (wchar_t *)calloc((size_t)required, sizeof(*wide_value));
        if (wide_value == NULL) {
            free(wide_name);
            return ENOMEM;
        }

        if (GetEnvironmentVariableW(wide_name, wide_value, required) == 0) {
            error = x_process_error_from_windows(GetLastError());
            free(wide_value);
            free(wide_name);
            return error;
        }

        required = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            wide_value,
            -1,
            buffer,
            (int)buffer_size,
            NULL,
            NULL);
        free(wide_value);
        free(wide_name);

        if (required == 0) {
            return GetLastError() == ERROR_INSUFFICIENT_BUFFER
                ? ENAMETOOLONG
                : x_process_error_from_windows(GetLastError());
        }

        return 0;
    }
#else
    const char *value;
    size_t length;

    value = getenv(name);
    if (value == NULL) {
        return ENOENT;
    }

    length = strlen(value);
    if (length + 1U > buffer_size) {
        return ENAMETOOLONG;
    }

    memcpy(buffer, value, length + 1U);
    return 0;
#endif
}

int x_environment_set(const char *name, const char *value)
{
    if (name == NULL || value == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_name;
        wchar_t *wide_value;
        int error = x_process_utf8_to_wide(name, &wide_name);
        if (error != 0) {
            return error;
        }

        error = x_process_utf8_to_wide(value, &wide_value);
        if (error != 0) {
            free(wide_name);
            return error;
        }

        if (!SetEnvironmentVariableW(wide_name, wide_value)) {
            error = x_process_error_from_windows(GetLastError());
            free(wide_value);
            free(wide_name);
            return error;
        }

        free(wide_value);
        free(wide_name);
        return 0;
    }
#else
    if (setenv(name, value, 1) != 0) {
        return errno;
    }

    return 0;
#endif
}

int x_environment_unset(const char *name)
{
    if (name == NULL) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        wchar_t *wide_name;
        int error = x_process_utf8_to_wide(name, &wide_name);
        if (error != 0) {
            return error;
        }

        if (!SetEnvironmentVariableW(wide_name, NULL)) {
            error = GetLastError() == ERROR_ENVVAR_NOT_FOUND ? 0 : x_process_error_from_windows(GetLastError());
            free(wide_name);
            return error;
        }

        free(wide_name);
        return 0;
    }
#else
    if (unsetenv(name) != 0) {
        return errno;
    }

    return 0;
#endif
}

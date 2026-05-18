#include <xlib/terminal.h>

#include <errno.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

static int x_terminal_fd(int stream)
{
    if (stream == 0) {
#ifdef _WIN32
        return _fileno(stdin);
#else
        return STDIN_FILENO;
#endif
    }

    if (stream == 1) {
#ifdef _WIN32
        return _fileno(stdout);
#else
        return STDOUT_FILENO;
#endif
    }

    if (stream == 2) {
#ifdef _WIN32
        return _fileno(stderr);
#else
        return STDERR_FILENO;
#endif
    }

    return -1;
}

int x_terminal_is_tty(int stream)
{
    int fd = x_terminal_fd(stream);

    if (fd < 0) {
        return 0;
    }

#ifdef _WIN32
    return _isatty(fd) ? 1 : 0;
#else
    return isatty(fd) ? 1 : 0;
#endif
}

int x_terminal_size(int stream, int *columns, int *rows)
{
    int fd;

    if (columns == NULL || rows == NULL) {
        return EINVAL;
    }

    *columns = 0;
    *rows = 0;
    fd = x_terminal_fd(stream);
    if (fd < 0) {
        return EINVAL;
    }

#ifdef _WIN32
    {
        HANDLE handle;
        CONSOLE_SCREEN_BUFFER_INFO info;

        if (stream == 0) {
            handle = GetStdHandle(STD_INPUT_HANDLE);
        } else if (stream == 1) {
            handle = GetStdHandle(STD_OUTPUT_HANDLE);
        } else {
            handle = GetStdHandle(STD_ERROR_HANDLE);
        }

        if (handle == NULL || handle == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(handle, &info)) {
            return ENOTTY;
        }

        *columns = info.srWindow.Right - info.srWindow.Left + 1;
        *rows = info.srWindow.Bottom - info.srWindow.Top + 1;
        return 0;
    }
#else
    {
        struct winsize size;

        if (ioctl(fd, TIOCGWINSZ, &size) != 0) {
            return errno == 0 ? ENOTTY : errno;
        }

        *columns = (int)size.ws_col;
        *rows = (int)size.ws_row;
        return 0;
    }
#endif
}

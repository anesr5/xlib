#ifndef XLIB_TERMINAL_H
#define XLIB_TERMINAL_H

#include <xlib/xlib_export.h>

#ifdef __cplusplus
extern "C" {
#endif

XLIB_API int x_terminal_is_tty(int stream);
XLIB_API int x_terminal_size(int stream, int *columns, int *rows);

#ifdef __cplusplus
}
#endif

#endif

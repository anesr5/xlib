#ifndef XLIB_EXPORT_H
#define XLIB_EXPORT_H

/*
 * XLIB_API — symbol visibility macro.
 *
 * When xlib is built as a static library (the default), XLIB_API expands to
 * nothing.  When built as a shared library, it marks public symbols for
 * export so that they survive -fvisibility=hidden (GCC/Clang) or the default
 * hidden visibility on Windows DLLs.
 *
 * Build system defines:
 *   XLIB_STATIC    — defined when building or consuming xlib as a static lib.
 *   XLIB_BUILDING  — defined only when compiling the xlib library itself as
 *                     a shared library (triggers dllexport on Windows).
 */

#ifdef XLIB_STATIC
#define XLIB_API
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef XLIB_BUILDING
#ifdef __GNUC__
#define XLIB_API __attribute__((dllexport))
#else
#define XLIB_API __declspec(dllexport)
#endif
#else
#ifdef __GNUC__
#define XLIB_API __attribute__((dllimport))
#else
#define XLIB_API __declspec(dllimport)
#endif
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define XLIB_API __attribute__((visibility("default")))
#else
#define XLIB_API
#endif
#endif

#endif

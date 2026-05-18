#include <xlib/dynamic_library.h>

#include <stdio.h>
#include <string.h>

typedef int (*plugin_add_fn)(int left, int right);
typedef const char *(*plugin_name_fn)(void);

int main(int argc, char **argv)
{
    x_dynamic_library_t *library;
    plugin_add_fn add;
    plugin_name_fn name;
    void *symbol;

    if (argc != 2) {
        return 1;
    }

    if (x_dynamic_library_open(&library, argv[1]) != 0) {
        char error[256];
        x_dynamic_library_last_error(error, sizeof(error));
        puts(error);
        return 1;
    }

    if (x_dynamic_library_symbol(library, "xlib_dynamic_plugin_add", &symbol) != 0) {
        x_dynamic_library_close(library);
        return 1;
    }
    add = (plugin_add_fn)symbol;

    if (x_dynamic_library_symbol(library, "xlib_dynamic_plugin_name", &symbol) != 0) {
        x_dynamic_library_close(library);
        return 1;
    }
    name = (plugin_name_fn)symbol;

    if (add(20, 22) != 42 || strcmp(name(), "xlib dynamic plugin") != 0) {
        x_dynamic_library_close(library);
        return 1;
    }

    if (x_dynamic_library_symbol(library, "xlib_dynamic_plugin_missing", &symbol) == 0) {
        x_dynamic_library_close(library);
        return 1;
    }

    x_dynamic_library_close(library);

    printf("Dynamic library example passed.\n");
    return 0;
}

#include <xlib/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "env") == 0) {
        const char *value = getenv("XLIB_PROCESS_VALUE");
        int exit_code = argc > 2 ? atoi(argv[2]) : 0;
        printf("%s\n", value != NULL ? value : "");
        return exit_code;
    }

    if (argc > 1 && strcmp(argv[1], "cat") == 0) {
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            return 1;
        }

        printf("%s", buffer);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "sleep") == 0) {
        x_time_sleep_ms(5000U);
        return 0;
    }

    return 2;
}

#include <xlib/filesystem.h>
#include <xlib/process.h>
#include <xlib/time.h>

#include <stdio.h>
#include <string.h>

static int read_file_text(const char *path, char *buffer, size_t buffer_size)
{
    x_file_t *file;
    size_t actual;

    if (x_file_open(&file, path, X_FILE_READ) != 0) {
        return 1;
    }

    memset(buffer, 0, buffer_size);
    if (x_file_read(file, buffer, buffer_size - 1U, &actual) != 0) {
        x_file_close(file);
        return 1;
    }

    (void)actual;
    x_file_close(file);
    return 0;
}

static int write_file_text(const char *path, const char *text)
{
    x_file_t *file;
    size_t actual;

    if (x_file_open(&file, path, X_FILE_WRITE | X_FILE_CREATE | X_FILE_TRUNCATE) != 0) {
        return 1;
    }

    if (x_file_write(file, text, strlen(text), &actual) != 0 || actual != strlen(text)) {
        x_file_close(file);
        return 1;
    }

    x_file_close(file);
    return 0;
}

static int line_equals(const char *actual, const char *expected_without_newline)
{
    size_t expected_length = strlen(expected_without_newline);

    if (strncmp(actual, expected_without_newline, expected_length) != 0) {
        return 0;
    }

    actual += expected_length;
    if (actual[0] == '\r') {
        ++actual;
    }

    return actual[0] == '\n' && actual[1] == '\0';
}

static int check_environment_and_exit(const char *helper_path)
{
    const char *output_path = "xlib_process_env.txt";
    x_process_options_t options;
    x_process_t *process;
    char *arguments[] = { "env", "23", NULL };
    char buffer[128];
    int exit_code = -1;

    memset(&options, 0, sizeof(options));
    options.stdout_path = output_path;

    x_file_remove(output_path);

    if (x_environment_set("XLIB_PROCESS_VALUE", "process-ok") != 0) {
        return 1;
    }

    if (x_environment_get("XLIB_PROCESS_VALUE", buffer, sizeof(buffer)) != 0
        || strcmp(buffer, "process-ok") != 0) {
        return 1;
    }

    if (x_process_start(&process, helper_path, arguments, &options) != 0) {
        return 1;
    }

    if (x_process_wait(process, &exit_code) != 0 || exit_code != 23) {
        x_process_destroy(process);
        return 1;
    }

    x_process_destroy(process);

    if (read_file_text(output_path, buffer, sizeof(buffer)) != 0
        || !line_equals(buffer, "process-ok")) {
        x_file_remove(output_path);
        return 1;
    }

    x_file_remove(output_path);
    return 0;
}

static int check_stdio_redirection(const char *helper_path)
{
    const char *input_path = "xlib_process_input.txt";
    const char *output_path = "xlib_process_cat.txt";
    x_process_options_t options;
    x_process_t *process;
    char *arguments[] = { "cat", NULL };
    char buffer[128];
    int exit_code = -1;

    memset(&options, 0, sizeof(options));
    options.stdin_path = input_path;
    options.stdout_path = output_path;

    x_file_remove(input_path);
    x_file_remove(output_path);

    if (write_file_text(input_path, "hello stdin\n") != 0) {
        return 1;
    }

    if (x_process_start(&process, helper_path, arguments, &options) != 0) {
        x_file_remove(input_path);
        return 1;
    }

    if (x_process_wait(process, &exit_code) != 0 || exit_code != 0) {
        x_process_destroy(process);
        x_file_remove(input_path);
        x_file_remove(output_path);
        return 1;
    }

    x_process_destroy(process);

    if (read_file_text(output_path, buffer, sizeof(buffer)) != 0
        || !line_equals(buffer, "hello stdin")) {
        x_file_remove(input_path);
        x_file_remove(output_path);
        return 1;
    }

    x_file_remove(input_path);
    x_file_remove(output_path);
    return 0;
}

static int check_termination(const char *helper_path)
{
    x_process_t *process;
    char *arguments[] = { "sleep", NULL };
    int exit_code;

    if (x_process_start(&process, helper_path, arguments, NULL) != 0) {
        return 1;
    }

    x_time_sleep_ms(100U);

    if (x_process_terminate(process) != 0) {
        x_process_destroy(process);
        return 1;
    }

    if (x_process_wait(process, &exit_code) != 0) {
        x_process_destroy(process);
        return 1;
    }

    x_process_destroy(process);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 1;
    }

    if (check_environment_and_exit(argv[1]) != 0
        || check_stdio_redirection(argv[1]) != 0
        || check_termination(argv[1]) != 0
        || x_environment_unset("XLIB_PROCESS_VALUE") != 0) {
        return 1;
    }

    printf("Process example passed.\n");
    return 0;
}

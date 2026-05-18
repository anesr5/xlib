#include <xlib/filesystem.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *directory_path = "xlib_fs_example";
    const char *file_name = "notes_" "\xC3" "\xA9" ".txt";
    const char *base_name;
    const char *message = "hello filesystem";
    char file_path[XLIB_PATH_MAX];
    char read_buffer[64];
    x_directory_t *directory;
    x_directory_entry_t entry;
    x_file_t *file;
    uint64_t file_size;
    size_t actual;
    int found_file = 0;
    int has_entry;

    if (x_directory_create(directory_path) != 0) {
        return 1;
    }

    if (x_path_join(file_path, sizeof(file_path), directory_path, file_name) != 0) {
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_path_basename(file_path, &base_name) != 0 || strcmp(base_name, file_name) != 0) {
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_path_is_absolute(file_path)) {
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_file_open(
            &file,
            file_path,
            X_FILE_READ | X_FILE_WRITE | X_FILE_CREATE | X_FILE_TRUNCATE) != 0) {
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_file_write(file, message, strlen(message), &actual) != 0
        || actual != strlen(message)) {
        x_file_close(file);
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_file_size(file, &file_size) != 0 || file_size != strlen(message)) {
        x_file_close(file);
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_file_seek(file, 0, X_FILE_SEEK_SET) != 0) {
        x_file_close(file);
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    memset(read_buffer, 0, sizeof(read_buffer));
    if (x_file_read(file, read_buffer, strlen(message), &actual) != 0
        || actual != strlen(message)
        || strcmp(read_buffer, message) != 0) {
        x_file_close(file);
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    x_file_close(file);

    if (x_directory_open(&directory, directory_path) != 0) {
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    for (;;) {
        if (x_directory_next(directory, &entry, &has_entry) != 0) {
            x_directory_close(directory);
            x_file_remove(file_path);
            x_directory_remove(directory_path);
            return 1;
        }

        if (!has_entry) {
            break;
        }

        if (!entry.is_directory && strcmp(entry.name, file_name) == 0) {
            found_file = 1;
            if (entry.size != strlen(message)) {
                x_directory_close(directory);
                x_file_remove(file_path);
                x_directory_remove(directory_path);
                return 1;
            }
        }
    }

    x_directory_close(directory);

    if (!found_file) {
        x_file_remove(file_path);
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_file_remove(file_path) != 0) {
        x_directory_remove(directory_path);
        return 1;
    }

    if (x_directory_remove(directory_path) != 0) {
        return 1;
    }

    printf("Filesystem example passed.\n");
    return 0;
}

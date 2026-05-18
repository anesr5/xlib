#include <xlib/memory.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *path = "xlib_memory_example.tmp";
    x_mapped_file_t *mapping;
    unsigned char *mapped_data;
    unsigned char *memory;
    size_t page_size;

    if (x_memory_page_size(&page_size) != 0 || page_size == 0U) {
        return 1;
    }

    if (x_virtual_memory_alloc(
            (void **)&memory,
            page_size,
            X_MEMORY_PROTECT_READ | X_MEMORY_PROTECT_WRITE) != 0) {
        return 1;
    }

    memory[0] = 123U;
    memory[page_size - 1U] = 45U;

    if (x_virtual_memory_protect(memory, page_size, X_MEMORY_PROTECT_READ) != 0) {
        x_virtual_memory_free(memory, page_size);
        return 1;
    }

    if (memory[0] != 123U || memory[page_size - 1U] != 45U) {
        x_virtual_memory_free(memory, page_size);
        return 1;
    }

    if (x_virtual_memory_protect(
            memory,
            page_size,
            X_MEMORY_PROTECT_READ | X_MEMORY_PROTECT_WRITE) != 0) {
        x_virtual_memory_free(memory, page_size);
        return 1;
    }

    memory[0] = 9U;
    x_virtual_memory_free(memory, page_size);

    if (x_mapped_file_create(&mapping, path, page_size) != 0) {
        return 1;
    }

    if (x_mapped_file_size(mapping) != page_size) {
        x_mapped_file_destroy(mapping);
        remove(path);
        return 1;
    }

    mapped_data = (unsigned char *)x_mapped_file_data(mapping);
    if (mapped_data == NULL) {
        x_mapped_file_destroy(mapping);
        remove(path);
        return 1;
    }

    memcpy(mapped_data, "xlib", 4U);

    if (x_mapped_file_flush(mapping) != 0) {
        x_mapped_file_destroy(mapping);
        remove(path);
        return 1;
    }

    x_mapped_file_destroy(mapping);
    remove(path);

    printf("Memory example passed.\n");
    return 0;
}

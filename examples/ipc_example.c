#include <xlib/ipc.h>
#include <xlib/allocator.h>
#include <xlib/memory.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Shared memory example ------------------------------------------------ */

static int shared_memory_example(void)
{
    x_shared_memory_t *creator = NULL;
    x_shared_memory_t *opener  = NULL;
    const char *name = "xlib_ipc_example_shm";
    const char *message = "hello from xlib shared memory";
    int error;

    printf("Creating shared memory region '%s'...\n", name);

    error = x_shared_memory_create(&creator, name, 256);
    if (error != 0) {
        fprintf(stderr, "x_shared_memory_create failed: %d\n", error);
        return 1;
    }

    memcpy(x_shared_memory_data(creator), message, strlen(message) + 1U);
    printf("  wrote: %s\n", (const char *)x_shared_memory_data(creator));

    error = x_shared_memory_open(&opener, name);
    if (error != 0) {
        fprintf(stderr, "x_shared_memory_open failed: %d\n", error);
        x_shared_memory_close(creator);
        x_shared_memory_unlink(name);
        return 1;
    }

    printf("  read:  %s\n", (const char *)x_shared_memory_data(opener));

    x_shared_memory_close(opener);
    x_shared_memory_close(creator);
    x_shared_memory_unlink(name);

    printf("  shared memory OK\n\n");
    return 0;
}

/* --- Named semaphore example ---------------------------------------------- */

static int named_semaphore_example(void)
{
    x_named_semaphore_t *producer = NULL;
    x_named_semaphore_t *consumer = NULL;
    const char *name = "xlib_ipc_example_sem";
    int error;

    printf("Creating named semaphore '%s' with count 0...\n", name);

    error = x_named_semaphore_create(&producer, name, 0);
    if (error != 0) {
        /* A leftover semaphore from a previous run is harmless; try to unlink
         * it and recreate. */
        x_named_semaphore_unlink(name);
        error = x_named_semaphore_create(&producer, name, 0);
        if (error != 0) {
            fprintf(stderr, "x_named_semaphore_create failed: %d\n", error);
            return 1;
        }
    }

    error = x_named_semaphore_open(&consumer, name);
    if (error != 0) {
        fprintf(stderr, "x_named_semaphore_open failed: %d\n", error);
        x_named_semaphore_close(producer);
        x_named_semaphore_unlink(name);
        return 1;
    }

    printf("  posting from producer...\n");
    error = x_named_semaphore_post(producer);
    if (error != 0) {
        fprintf(stderr, "x_named_semaphore_post failed: %d\n", error);
        x_named_semaphore_close(consumer);
        x_named_semaphore_close(producer);
        x_named_semaphore_unlink(name);
        return 1;
    }

    printf("  waiting on consumer...\n");
    error = x_named_semaphore_wait(consumer);
    if (error != 0) {
        fprintf(stderr, "x_named_semaphore_wait failed: %d\n", error);
        x_named_semaphore_close(consumer);
        x_named_semaphore_close(producer);
        x_named_semaphore_unlink(name);
        return 1;
    }

    printf("  consumer unblocked\n");

    x_named_semaphore_close(consumer);
    x_named_semaphore_close(producer);
    x_named_semaphore_unlink(name);

    printf("  named semaphore OK\n\n");
    return 0;
}

/* --- Pluggable allocator example ------------------------------------------ */

static unsigned int custom_alloc_calls;
static unsigned int custom_free_calls;

static void *counting_alloc(size_t size, void *user_data)
{
    (void)user_data;
    ++custom_alloc_calls;
    return malloc(size);
}

static void *counting_realloc(void *ptr, size_t size, void *user_data)
{
    (void)user_data;
    return realloc(ptr, size);
}

static void counting_free(void *ptr, void *user_data)
{
    (void)user_data;
    if (ptr != NULL) {
        ++custom_free_calls;
    }
    free(ptr);
}

static int allocator_example(void)
{
    x_allocator_t allocator;
    void *block;

    printf("Installing custom counting allocator...\n");

    allocator.alloc     = counting_alloc;
    allocator.realloc   = counting_realloc;
    allocator.free      = counting_free;
    allocator.user_data = NULL;

    x_allocator_set(&allocator);

    block = x_alloc(128);
    if (block == NULL) {
        fprintf(stderr, "x_alloc failed\n");
        x_allocator_reset();
        return 1;
    }

    x_free(block);

    x_allocator_reset();

    printf("  alloc calls: %u, free calls: %u\n", custom_alloc_calls, custom_free_calls);
    printf("  allocator OK\n\n");
    return 0;
}

/* --- Mapped file open example --------------------------------------------- */

static int mapped_file_open_example(void)
{
    x_mapped_file_t *writer = NULL;
    x_mapped_file_t *reader = NULL;
    const char *path = "xlib_ipc_example_mapped.bin";
    const char *payload = "xlib mapped file open test";
    int error;

    printf("Creating mapped file '%s'...\n", path);

    error = x_mapped_file_create(&writer, path, 128);
    if (error != 0) {
        fprintf(stderr, "x_mapped_file_create failed: %d\n", error);
        return 1;
    }

    memcpy(x_mapped_file_data(writer), payload, strlen(payload) + 1U);
    x_mapped_file_flush(writer);
    x_mapped_file_destroy(writer);

    printf("  opening read-only with x_mapped_file_open...\n");

    error = x_mapped_file_open(&reader, path, X_MEMORY_PROTECT_READ);
    if (error != 0) {
        fprintf(stderr, "x_mapped_file_open failed: %d\n", error);
        return 1;
    }

    printf("  read: %s\n", (const char *)x_mapped_file_data(reader));
    x_mapped_file_destroy(reader);

    printf("  mapped file open OK\n\n");
    return 0;
}

/* -------------------------------------------------------------------------- */

int main(void)
{
    int result = 0;

    result |= shared_memory_example();
    result |= named_semaphore_example();
    result |= allocator_example();
    result |= mapped_file_open_example();

    if (result == 0) {
        printf("All IPC and memory extension examples passed.\n");
    }

    return result;
}

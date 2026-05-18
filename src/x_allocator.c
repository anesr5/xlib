#include <xlib/allocator.h>

#include <stdlib.h>

static void *x_allocator_default_alloc(size_t size, void *user_data)
{
    (void)user_data;
    return malloc(size);
}

static void *x_allocator_default_realloc(void *ptr, size_t size, void *user_data)
{
    (void)user_data;
    return realloc(ptr, size);
}

static void x_allocator_default_free(void *ptr, void *user_data)
{
    (void)user_data;
    free(ptr);
}

static x_allocator_t x_allocator_current = {
    x_allocator_default_alloc,
    x_allocator_default_realloc,
    x_allocator_default_free,
    NULL
};

void x_allocator_set(const x_allocator_t *allocator)
{
    if (allocator == NULL) {
        x_allocator_reset();
        return;
    }

    x_allocator_current = *allocator;
}

void x_allocator_reset(void)
{
    x_allocator_current.alloc     = x_allocator_default_alloc;
    x_allocator_current.realloc   = x_allocator_default_realloc;
    x_allocator_current.free      = x_allocator_default_free;
    x_allocator_current.user_data = NULL;
}

const x_allocator_t *x_allocator_get(void)
{
    return &x_allocator_current;
}

void *x_alloc(size_t size)
{
    return x_allocator_current.alloc(size, x_allocator_current.user_data);
}

void *x_realloc(void *ptr, size_t size)
{
    return x_allocator_current.realloc(ptr, size, x_allocator_current.user_data);
}

void x_free(void *ptr)
{
    x_allocator_current.free(ptr, x_allocator_current.user_data);
}

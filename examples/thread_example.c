#include <xlib/thread.h>

#include <stdio.h>

static int worker(void *data)
{
    const char *message = (const char *)data;
    printf("%s\n", message);
    return 42;
}

int main(void)
{
    x_thread_t *thread;
    int result;

    if (x_thread_create(&thread, worker, "Hello from xlib thread!") != 0) {
        return 1;
    }

    if (x_thread_join(thread, &result) != 0) {
        x_thread_destroy(thread);
        return 1;
    }

    x_thread_destroy(thread);

    return result == 42 ? 0 : 1;
}

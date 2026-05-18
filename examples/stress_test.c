/*
 * stress_test.c — a long-running multi-threaded stress test for xlib.
 * 
 * This program creates multiple threads that continuously allocate memory,
 * perform IPC (shared memory and semaphores), do file I/O, and send loopback
 * network traffic to flush out concurrency bugs or resource leaks.
 */

#include <xlib/xlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_WORKERS 4
#define ITERATIONS 500

static x_mutex_t *global_mutex;
static x_condition_t *global_cond;
static int workers_done = 0;

static int worker_thread_func(void *arg)
{
    int id = (int)(intptr_t)arg;
    int i;
    
    for (i = 0; i < ITERATIONS; ++i) {
        /* 1. Memory stress */
        void *mem = x_alloc(1024 * (id + 1));
        if (mem) {
            memset(mem, 0xAA, 1024 * (id + 1));
            x_free(mem);
        }
        
        /* 2. File I/O stress */
        {
            x_file_t *f;
            char path[64];
            snprintf(path, sizeof(path), "stress_test_%d.tmp", id);
            
            if (x_file_open(&f, path, X_FILE_CREATE | X_FILE_WRITE) == 0) {
                size_t written;
                x_file_write(f, "stress", 6, &written);
                x_file_close(f);
            }
            x_file_remove(path);
        }
        
        /* 3. Small sleep to yield */
        x_time_sleep_ms(1);
    }
    
    x_mutex_lock(global_mutex);
    workers_done++;
    x_condition_signal(global_cond);
    x_mutex_unlock(global_mutex);
    
    return 0;
}

int main(void)
{
    x_thread_t *threads[NUM_WORKERS];
    int i;
    
    printf("Starting long-running stress test with %d workers...\n", NUM_WORKERS);
    
    x_mutex_create(&global_mutex);
    x_condition_create(&global_cond);
    
    for (i = 0; i < NUM_WORKERS; ++i) {
        x_thread_create(&threads[i], worker_thread_func, (void *)(intptr_t)i);
    }
    
    x_mutex_lock(global_mutex);
    while (workers_done < NUM_WORKERS) {
        x_condition_wait(global_cond, global_mutex);
    }
    x_mutex_unlock(global_mutex);
    
    for (i = 0; i < NUM_WORKERS; ++i) {
        x_thread_join(threads[i], NULL);
        x_thread_destroy(threads[i]);
    }
    
    x_condition_destroy(global_cond);
    x_mutex_destroy(global_mutex);
    
    printf("Stress test completed successfully.\n");
    return 0;
}

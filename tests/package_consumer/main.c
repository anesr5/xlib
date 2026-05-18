#include <xlib/xlib.h>

#include <stdint.h>

int main(void)
{
    uint64_t now = 0;
    return x_time_monotonic_ns(&now) == 0 && now != 0 ? 0 : 1;
}

#include <xlib/xlib.hpp>

int main()
{
    xlib::mutex mutex;
    xlib::lock_guard lock(mutex);
    return 0;
}

#if defined(_WIN32) && defined(XLIB_DYNAMIC_PLUGIN_BUILD)
#define XLIB_DYNAMIC_PLUGIN_API __declspec(dllexport)
#else
#define XLIB_DYNAMIC_PLUGIN_API
#endif

XLIB_DYNAMIC_PLUGIN_API int xlib_dynamic_plugin_add(int left, int right)
{
    return left + right;
}

XLIB_DYNAMIC_PLUGIN_API const char *xlib_dynamic_plugin_name(void)
{
    return "xlib dynamic plugin";
}

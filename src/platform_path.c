#include "platform_path.h"

#include <stdio.h>
#include <string.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <limits.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>

static bool dir_has_resources(const char *base) {
    char path[PATH_MAX];
    if (!base || snprintf(path, sizeof(path), "%s/resources", base) >= (int)sizeof(path))
        return false;
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool try_chdir_base(const char *base) {
    if (dir_has_resources(base)) {
        (void)chdir(base);
        return true;
    }
    return false;
}

bool platformer_set_resource_root(void) {
    if (dir_has_resources("."))
        return true;

    char exe[PATH_MAX];
    uint32_t size = (uint32_t)sizeof(exe);
    if (_NSGetExecutablePath(exe, &size) != 0)
        return false;

    char *macos = strstr(exe, "/Contents/MacOS/");
    if (macos == NULL)
        return false;

    *macos = '\0';

    char resources_base[PATH_MAX];
    if (snprintf(resources_base, sizeof(resources_base), "%s/Contents/Resources", exe) < (int)sizeof(resources_base)
        && try_chdir_base(resources_base))
        return true;

    char macos_base[PATH_MAX];
    if (snprintf(macos_base, sizeof(macos_base), "%s/Contents/MacOS", exe) < (int)sizeof(macos_base)
        && try_chdir_base(macos_base))
        return true;

    return false;
}

#elif defined(_WIN32)
#include <windows.h>

static bool dir_has_resources_w(const char *base) {
    char path[MAX_PATH];
    if (!base || snprintf(path, sizeof(path), "%s\\resources", base) >= (int)sizeof(path))
        return false;
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool platformer_set_resource_root(void) {
    if (dir_has_resources_w("."))
        return true;

    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return false;

    char *slash = strrchr(path, '\\');
    if (slash == NULL)
        slash = strrchr(path, '/');
    if (slash == NULL)
        return false;
    *slash = '\0';
    SetCurrentDirectoryA(path);
    return dir_has_resources_w(".");
}

#else

bool platformer_set_resource_root(void) {
    return true;
}

#endif

#ifndef PLATFORM_PATH_H
#define PLATFORM_PATH_H

#include <stdbool.h>

/* Set cwd so bundled resources/ paths resolve (macOS .app, Windows exe dir). */
bool app_set_resource_root(void);

#endif

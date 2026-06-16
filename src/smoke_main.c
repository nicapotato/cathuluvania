#include "acts.h"
#include "platform_path.h"

#include <stdio.h>
#include <sys/stat.h>

static bool file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool smoke_check_act(const ActDef *act) {
    if (!act || !act->id || !act->background_png || !act->collision_png)
        return false;
    if (act->cols <= 0 || act->rows <= 0 || act->width <= 0 || act->height <= 0)
        return false;
    if (act->room_count <= 0)
        return false;

    if (!file_exists(act->background_png)) {
        fprintf(stderr, "smoke: missing background: %s\n", act->background_png);
        return false;
    }
    if (!file_exists(act->collision_png)) {
        fprintf(stderr, "smoke: missing collision: %s\n", act->collision_png);
        return false;
    }

    printf("smoke: act %s ok (%dx%d, %d rooms)\n", act->id, act->cols, act->rows, act->room_count);
    return true;
}

int main(void) {
    app_set_resource_root();

    for (int i = 0; i < ACT_COUNT; i++) {
        if (!smoke_check_act(&ACTS[i]))
            return 1;
    }

    printf("smoke: all %d acts validated\n", ACT_COUNT);
    return 0;
}

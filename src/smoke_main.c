#include "act_registry.h"
#include "platform_path.h"

#include <stdio.h>
#include <sys/stat.h>

static bool file_exists(const char *path) {
    struct stat st;
    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool smoke_check_act(const ActRegistry *reg, const ActDesc *act) {
    if (!act || act->id[0] == '\0' || act->background_png[0] == '\0' ||
        act->collision_png[0] == '\0' || act->gameplay_json[0] == '\0')
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

    printf("smoke: act %s ok (%dx%d, %d rooms, gameplay=%s)\n",
           act->id, act->cols, act->rows, act->room_count, act->gameplay_json);
    (void)reg;
    return true;
}

int main(void) {
    app_set_resource_root();

    if (!file_exists("resources/gameplay/tile-catalog.json")) {
        fprintf(stderr, "smoke: missing tile catalog: resources/gameplay/tile-catalog.json\n");
        return 1;
    }

    ActRegistry reg = { 0 };
    if (!act_registry_load(&reg, NULL)) {
        fprintf(stderr, "smoke: failed to load act registry\n");
        return 1;
    }

    for (int i = 0; i < act_registry_count(&reg); i++) {
        const ActDesc *act = act_registry_get(&reg, i);
        if (!smoke_check_act(&reg, act)) {
            act_registry_free(&reg);
            return 1;
        }
    }

    printf("smoke: all %d acts validated\n", act_registry_count(&reg));
    act_registry_free(&reg);
    return 0;
}

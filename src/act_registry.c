#include "act_registry.h"

#include "../../external/cjson/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ACT_MANIFEST_DEFAULT "resources/acts.manifest.json"
#define EXPORT_DIR "resources/visual/layers"
#define SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM 8

static bool file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static char *act_registry_strdup(ActDesc *act, const char *src) {
    if (!src)
        return NULL;

    char *copy = strdup(src);
    if (!copy)
        return NULL;

    char **next = (char **)realloc(act->owned_strings,
                                  (size_t)(act->owned_string_count + 1) * sizeof(char *));
    if (!next) {
        free(copy);
        return NULL;
    }

    act->owned_strings = next;
    act->owned_strings[act->owned_string_count++] = copy;
    return copy;
}

static void act_desc_free(ActDesc *act) {
    if (!act)
        return;

    free(act->saves);
    free(act->rooms);
    free(act->tunnels);
    free(act->teleports);

    if (act->owned_strings) {
        for (int i = 0; i < act->owned_string_count; i++)
            free(act->owned_strings[i]);
        free(act->owned_strings);
    }

    memset(act, 0, sizeof(*act));
}

static bool read_text_file(const char *path, char **out_text) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return false;
    }

    char *text = (char *)malloc((size_t)len + 1);
    if (!text) {
        fclose(f);
        return false;
    }

    if (fread(text, 1, (size_t)len, f) != (size_t)len) {
        free(text);
        fclose(f);
        return false;
    }

    text[len] = '\0';
    fclose(f);
    *out_text = text;
    return true;
}

static bool load_export_into_act(ActDesc *act, const char *export_path, const char *label_fallback) {
    char *text = NULL;
    if (!read_text_file(export_path, &text))
        return false;

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root)
        return false;

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "id");
    cJSON *width = cJSON_GetObjectItemCaseSensitive(root, "width");
    cJSON *height = cJSON_GetObjectItemCaseSensitive(root, "height");
    cJSON *bg = cJSON_GetObjectItemCaseSensitive(root, "background_png");
    cJSON *col = cJSON_GetObjectItemCaseSensitive(root, "collision_png");

    if (!cJSON_IsString(id) || !cJSON_IsNumber(width) || !cJSON_IsNumber(height)
        || !cJSON_IsString(bg) || !cJSON_IsString(col)) {
        cJSON_Delete(root);
        return false;
    }

    snprintf(act->id, sizeof(act->id), "%s", id->valuestring);
    snprintf(act->label, sizeof(act->label), "%s", label_fallback ? label_fallback : id->valuestring);
    act->width = width->valueint;
    act->height = height->valueint;
    act->cols = act->width / 16;
    act->rows = act->height / 16;
    snprintf(act->background_png, sizeof(act->background_png), "%s", bg->valuestring);
    snprintf(act->collision_png, sizeof(act->collision_png), "%s", col->valuestring);
    snprintf(act->gameplay_json, sizeof(act->gameplay_json), "%s/%s.gameplay.json", EXPORT_DIR,
             act->id);
    snprintf(act->aseprite_path, sizeof(act->aseprite_path), "resources/visual/%s.aseprite",
             act->id);

    cJSON *rooms_json = cJSON_GetObjectItemCaseSensitive(root, "rooms");
    if (cJSON_IsArray(rooms_json)) {
        int n = cJSON_GetArraySize(rooms_json);
        act->rooms = (RoomDef *)calloc((size_t)n, sizeof(RoomDef));
        act->room_count = n;
        for (int i = 0; i < n; i++) {
            cJSON *room = cJSON_GetArrayItem(rooms_json, i);
            cJSON *rid = cJSON_GetObjectItemCaseSensitive(room, "id");
            cJSON *name = cJSON_GetObjectItemCaseSensitive(room, "name");
            cJSON *isolated = cJSON_GetObjectItemCaseSensitive(room, "isolated");
            cJSON *x = cJSON_GetObjectItemCaseSensitive(room, "x");
            cJSON *y = cJSON_GetObjectItemCaseSensitive(room, "y");
            cJSON *w = cJSON_GetObjectItemCaseSensitive(room, "w");
            cJSON *h = cJSON_GetObjectItemCaseSensitive(room, "h");
            cJSON *view_y = cJSON_GetObjectItemCaseSensitive(room, "view_y");
            cJSON *view_h = cJSON_GetObjectItemCaseSensitive(room, "view_h");

            RoomDef *rd = &act->rooms[i];
            rd->id = act_registry_strdup(act, cJSON_IsString(rid) ? rid->valuestring : "r-?");
            rd->name = cJSON_IsString(name) && name->valuestring
                           ? act_registry_strdup(act, name->valuestring)
                           : NULL;
            rd->isolated = cJSON_IsTrue(isolated);
            rd->x = cJSON_IsNumber(x) ? (float)x->valuedouble : 0.0f;
            rd->y = cJSON_IsNumber(y) ? (float)y->valuedouble : 0.0f;
            rd->w = cJSON_IsNumber(w) ? (float)w->valuedouble : 0.0f;
            rd->h = cJSON_IsNumber(h) ? (float)h->valuedouble : 0.0f;
            rd->view_y = cJSON_IsNumber(view_y) ? (float)view_y->valuedouble : rd->y;
            rd->view_h = cJSON_IsNumber(view_h) ? (float)view_h->valuedouble : rd->h;
        }
    }

    cJSON *saves_json = cJSON_GetObjectItemCaseSensitive(root, "saves");
    if (cJSON_IsArray(saves_json)) {
        int n = cJSON_GetArraySize(saves_json);
        act->saves = (SavePointDef *)calloc((size_t)n, sizeof(SavePointDef));
        act->save_count = n;
        for (int i = 0; i < n; i++) {
            cJSON *save = cJSON_GetArrayItem(saves_json, i);
            cJSON *index = cJSON_GetObjectItemCaseSensitive(save, "index");
            cJSON *sx = cJSON_GetObjectItemCaseSensitive(save, "x");
            cJSON *sy = cJSON_GetObjectItemCaseSensitive(save, "y");
            cJSON *room = cJSON_GetObjectItemCaseSensitive(save, "room");
            act->saves[i].index = cJSON_IsNumber(index) ? index->valueint : 0;
            act->saves[i].x = cJSON_IsNumber(sx) ? (float)sx->valuedouble : 0.0f;
            act->saves[i].y = cJSON_IsNumber(sy) ? (float)sy->valuedouble : 0.0f;
            act->saves[i].room_id = act_registry_strdup(act, cJSON_IsString(room) ? room->valuestring : "r-1");
        }
    }

    cJSON *tunnels_json = cJSON_GetObjectItemCaseSensitive(root, "tunnels");
    if (!tunnels_json)
        tunnels_json = cJSON_GetObjectItemCaseSensitive(root, "doors");
    if (cJSON_IsArray(tunnels_json)) {
        int n = cJSON_GetArraySize(tunnels_json);
        act->tunnels = (TunnelDef *)calloc((size_t)n, sizeof(TunnelDef));
        act->tunnel_count = n;
        for (int i = 0; i < n; i++) {
            cJSON *tunnel = cJSON_GetArrayItem(tunnels_json, i);
            cJSON *tid = cJSON_GetObjectItemCaseSensitive(tunnel, "id");
            cJSON *room_a = cJSON_GetObjectItemCaseSensitive(tunnel, "room_a");
            cJSON *room_b = cJSON_GetObjectItemCaseSensitive(tunnel, "room_b");
            cJSON *x = cJSON_GetObjectItemCaseSensitive(tunnel, "x");
            cJSON *y = cJSON_GetObjectItemCaseSensitive(tunnel, "y");
            cJSON *w = cJSON_GetObjectItemCaseSensitive(tunnel, "w");
            cJSON *h = cJSON_GetObjectItemCaseSensitive(tunnel, "h");
            cJSON *spawn_a = cJSON_GetObjectItemCaseSensitive(tunnel, "spawn_a");
            cJSON *spawn_b = cJSON_GetObjectItemCaseSensitive(tunnel, "spawn_b");

            TunnelDef *td = &act->tunnels[i];
            td->id = act_registry_strdup(act, cJSON_IsString(tid) ? tid->valuestring : "door-?");
            td->room_a_id = act_registry_strdup(act, cJSON_IsString(room_a) ? room_a->valuestring : "");
            td->room_b_id = act_registry_strdup(act, cJSON_IsString(room_b) ? room_b->valuestring : "");
            td->x = cJSON_IsNumber(x) ? (float)x->valuedouble : 0.0f;
            td->y = cJSON_IsNumber(y) ? (float)y->valuedouble : 0.0f;
            td->w = cJSON_IsNumber(w) ? (float)w->valuedouble : 0.0f;
            td->h = cJSON_IsNumber(h) ? (float)h->valuedouble : 0.0f;

            if (cJSON_IsObject(spawn_a)) {
                cJSON *sax = cJSON_GetObjectItemCaseSensitive(spawn_a, "x");
                cJSON *say = cJSON_GetObjectItemCaseSensitive(spawn_a, "y");
                td->spawn_ax = cJSON_IsNumber(sax) ? (float)sax->valuedouble : td->x;
                td->spawn_ay = cJSON_IsNumber(say) ? (float)say->valuedouble : td->y;
            }
            if (cJSON_IsObject(spawn_b)) {
                cJSON *sbx = cJSON_GetObjectItemCaseSensitive(spawn_b, "x");
                cJSON *sby = cJSON_GetObjectItemCaseSensitive(spawn_b, "y");
                td->spawn_bx = cJSON_IsNumber(sbx) ? (float)sbx->valuedouble : td->x;
                td->spawn_by = cJSON_IsNumber(sby) ? (float)sby->valuedouble : td->y;
            }
        }
    }

    cJSON *teleports_json = cJSON_GetObjectItemCaseSensitive(root, "teleports");
    if (cJSON_IsArray(teleports_json)) {
        int n = cJSON_GetArraySize(teleports_json);
        act->teleports = (TeleportDef *)calloc((size_t)n, sizeof(TeleportDef));
        act->teleport_count = n;
        for (int i = 0; i < n; i++) {
            cJSON *tp = cJSON_GetArrayItem(teleports_json, i);
            cJSON *tid = cJSON_GetObjectItemCaseSensitive(tp, "id");
            cJSON *room = cJSON_GetObjectItemCaseSensitive(tp, "room");
            cJSON *link = cJSON_GetObjectItemCaseSensitive(tp, "link");
            cJSON *name = cJSON_GetObjectItemCaseSensitive(tp, "name");
            cJSON *x = cJSON_GetObjectItemCaseSensitive(tp, "x");
            cJSON *y = cJSON_GetObjectItemCaseSensitive(tp, "y");
            cJSON *w = cJSON_GetObjectItemCaseSensitive(tp, "w");
            cJSON *h = cJSON_GetObjectItemCaseSensitive(tp, "h");
            cJSON *spawn_x = cJSON_GetObjectItemCaseSensitive(tp, "spawn_x");
            cJSON *spawn_y = cJSON_GetObjectItemCaseSensitive(tp, "spawn_y");

            TeleportDef *td = &act->teleports[i];
            td->id = act_registry_strdup(act, cJSON_IsString(tid) ? tid->valuestring : "teleport-?");
            td->room_id = act_registry_strdup(act, cJSON_IsString(room) ? room->valuestring : "r-1");
            td->link_id = act_registry_strdup(act, cJSON_IsString(link) ? link->valuestring : "");
            td->name = cJSON_IsString(name) && name->valuestring
                           ? act_registry_strdup(act, name->valuestring)
                           : NULL;
            td->x = cJSON_IsNumber(x) ? (float)x->valuedouble : 0.0f;
            td->y = cJSON_IsNumber(y) ? (float)y->valuedouble : 0.0f;
            td->w = cJSON_IsNumber(w) ? (float)w->valuedouble : 16.0f;
            td->h = cJSON_IsNumber(h) ? (float)h->valuedouble : 16.0f;
            td->spawn_x = cJSON_IsNumber(spawn_x) ? (float)spawn_x->valuedouble : td->x + td->w * 0.5f;
            td->spawn_y = cJSON_IsNumber(spawn_y)
                              ? (float)spawn_y->valuedouble
                              : td->y + td->h - (float)SPAWN_CENTER_OFFSET_FROM_SLICE_BOTTOM;
        }
    }

    cJSON_Delete(root);
    return true;
}

void act_registry_free(ActRegistry *reg) {
    if (!reg)
        return;

    if (reg->acts) {
        for (int i = 0; i < reg->count; i++)
            act_desc_free(&reg->acts[i]);
        free(reg->acts);
    }

    memset(reg, 0, sizeof(*reg));
}

bool act_registry_load(ActRegistry *reg, const char *manifest_path) {
    if (!reg)
        return false;

    act_registry_free(reg);

    if (!manifest_path || !manifest_path[0])
        manifest_path = ACT_MANIFEST_DEFAULT;

    char *text = NULL;
    if (!read_text_file(manifest_path, &text)) {
        fprintf(stderr, "act_registry: cannot read manifest %s\n", manifest_path);
        return false;
    }

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        fprintf(stderr, "act_registry: manifest parse error\n");
        return false;
    }

    cJSON *acts_json = cJSON_GetObjectItemCaseSensitive(root, "acts");
    if (!cJSON_IsArray(acts_json)) {
        cJSON_Delete(root);
        fprintf(stderr, "act_registry: manifest missing acts array\n");
        return false;
    }

    int n = cJSON_GetArraySize(acts_json);
    reg->acts = (ActDesc *)calloc((size_t)n, sizeof(ActDesc));
    if (!reg->acts) {
        cJSON_Delete(root);
        return false;
    }

    reg->count = 0;
    for (int i = 0; i < n; i++) {
        cJSON *entry = cJSON_GetArrayItem(acts_json, i);
        cJSON *id = cJSON_GetObjectItemCaseSensitive(entry, "id");
        cJSON *label = cJSON_GetObjectItemCaseSensitive(entry, "label");
        if (!cJSON_IsString(id) || !id->valuestring)
            continue;

        char export_path[ACT_DESC_PATH_LEN];
        snprintf(export_path, sizeof(export_path), "%s/%s.export.json", EXPORT_DIR, id->valuestring);
        if (!file_exists(export_path)) {
            fprintf(stderr, "act_registry: missing export for %s (%s)\n", id->valuestring, export_path);
            continue;
        }

        ActDesc *act = &reg->acts[reg->count];
        const char *lbl = cJSON_IsString(label) && label->valuestring ? label->valuestring : id->valuestring;
        if (!load_export_into_act(act, export_path, lbl)) {
            fprintf(stderr, "act_registry: failed to load export %s\n", export_path);
            act_desc_free(act);
            continue;
        }

        reg->count++;
    }

    cJSON_Delete(root);

    if (reg->count == 0) {
        fprintf(stderr, "act_registry: no acts loaded\n");
        return false;
    }

    return true;
}

bool act_registry_reload(ActRegistry *reg) {
    return act_registry_load(reg, ACT_MANIFEST_DEFAULT);
}

int act_registry_count(const ActRegistry *reg) {
    return reg ? reg->count : 0;
}

const ActDesc *act_registry_get(const ActRegistry *reg, int index) {
    if (!reg || index < 0 || index >= reg->count)
        return NULL;
    return &reg->acts[index];
}

const ActDesc *act_registry_find(const ActRegistry *reg, const char *id) {
    if (!reg || !id)
        return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->acts[i].id, id) == 0)
            return &reg->acts[i];
    }
    return NULL;
}

int act_registry_index_of(const ActRegistry *reg, const char *id) {
    if (!reg || !id)
        return -1;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->acts[i].id, id) == 0)
            return i;
    }
    return -1;
}

bool act_registry_append_manifest_entry(const char *manifest_path, const char *id, const char *label) {
    if (!manifest_path || !id || !label)
        return false;

    cJSON *root = NULL;
    char *text = NULL;

    if (read_text_file(manifest_path, &text))
        root = cJSON_Parse(text);

    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "acts", cJSON_CreateArray());
    }

    cJSON *acts = cJSON_GetObjectItemCaseSensitive(root, "acts");
    if (!cJSON_IsArray(acts)) {
        cJSON_Delete(root);
        free(text);
        return false;
    }

    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "id", id);
    cJSON_AddStringToObject(entry, "label", label);
    cJSON_AddItemToArray(acts, entry);

    char *out = cJSON_Print(root);
    cJSON_Delete(root);
    free(text);
    if (!out)
        return false;

    FILE *f = fopen(manifest_path, "wb");
    if (!f) {
        free(out);
        return false;
    }

    fputs(out, f);
    fputc('\n', f);
    fclose(f);
    free(out);
    return true;
}

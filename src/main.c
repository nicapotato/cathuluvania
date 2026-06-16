#include "main.h"
#include "game.h"
#include "platform_path.h"
#include "resource_dir.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
#if defined(__APPLE__) && defined(__MACH__)
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
#else
    SetConfigFlags(FLAG_VSYNC_HINT);
#endif
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
    if (!IsWindowReady()) {
        fprintf(stderr, "Failed to initialize window\n");
        return 1;
    }
    SetTargetFPS(60);

    if (!SearchAndSetResourceParentDir("resources") && !platformer_set_resource_root()) {
        fprintf(stderr, "main: could not find resources/ for game data\n");
    }

    Game *game = NULL;
    if (!game_new(&game)) {
        fprintf(stderr, "Failed to initialize game\n");
        CloseWindow();
        return 1;
    }

    if (!game_run(game))
        fprintf(stderr, "Game loop failed\n");

    game_free(&game);
    CloseWindow();
    return 0;
}

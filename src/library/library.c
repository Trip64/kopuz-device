#include "library.h"
#include "hal/hal_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static bool is_audio_file(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    dot++;
    char ext[8] = {0};
    size_t i = 0;
    while (dot[i] && i < sizeof(ext) - 1) {
        ext[i] = (char)tolower((unsigned char)dot[i]);
        i++;
    }
    ext[i] = '\0';
    return (strcmp(ext, "mp3") == 0 || strcmp(ext, "flac") == 0 || strcmp(ext, "wav") == 0);
}

static void extract_tags_from_path(const char *full_path, char *title, char *artist, char *album) {
    strcpy(title, "?");
    strcpy(artist, "Unknown Artist");
    strcpy(album, "Unknown Album");

    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, full_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    char *fname = last_slash ? (last_slash + 1) : path_copy;
    char *dot = strrchr(fname, '.');
    if (dot) *dot = '\0';
    snprintf(title, MAX_TITLE_LEN, "%s", fname);

    if (!last_slash) return;

    *last_slash = '\0';
    char *parent_slash = strrchr(path_copy, '/');
    char *parent_name = parent_slash ? (parent_slash + 1) : path_copy;
    if (parent_name[0] != '\0' && strcmp(parent_name, "sdcard") != 0 && strcmp(parent_name, ".") != 0) {
        snprintf(album, MAX_NAME_LEN, "%s", parent_name);
    }

    if (!parent_slash) return;

    *parent_slash = '\0';
    char *grand_slash = strrchr(path_copy, '/');
    char *grand_name = grand_slash ? (grand_slash + 1) : path_copy;
    if (grand_name[0] != '\0' && strcmp(grand_name, "sdcard") != 0 && strcmp(grand_name, ".") != 0) {
        snprintf(artist, MAX_NAME_LEN, "%s", grand_name);
    }
}

static void scan_dir_recursive(const char *dir_path, app_state_t *app) {
    if (app->queue_len >= app->queue_cap) return;

    hal_dir_t *d = hal_opendir(dir_path);
    if (!d) return;

    hal_dir_entry_t entry;
    while (hal_readdir(d, &entry)) {
        if (app->queue_len >= app->queue_cap) break;
        if (entry.name[0] == '.') continue;

        char full_path[MAX_PATH_LEN + 64];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry.name);

        if (entry.is_dir) {
            scan_dir_recursive(full_path, app);
        } else if (is_audio_file(entry.name)) {
            track_t *t = &app->queue[app->queue_len];
            strncpy(t->path, full_path, sizeof(t->path) - 1);
            t->path[sizeof(t->path) - 1] = '\0';
            extract_tags_from_path(full_path, t->title, t->artist, t->album);
            t->duration_secs = 0;
            app->queue_len++;
        }
    }

    hal_closedir(d);
}

static int compare_groups(const void *a, const void *b) {
    const track_group_t *ga = (const track_group_t*)a;
    const track_group_t *gb = (const track_group_t*)b;
    return strcasecmp(ga->name, gb->name);
}

static void build_groups(const track_t *tracks, uint16_t track_count, bool is_album, track_group_t **out_groups, uint16_t *out_count) {
    uint16_t max_groups = 256;
    track_group_t *groups = (track_group_t*)calloc(max_groups, sizeof(track_group_t));
    if (!groups) {
        *out_groups = NULL;
        *out_count = 0;
        return;
    }

    uint16_t group_count = 0;

    for (uint16_t i = 0; i < track_count; i++) {
        const char *name = is_album ? tracks[i].album : tracks[i].artist;

        int found_idx = -1;
        for (uint16_t g = 0; g < group_count; g++) {
            if (strcmp(groups[g].name, name) == 0) {
                found_idx = (int)g;
                break;
            }
        }

        if (found_idx >= 0) {
            track_group_t *grp = &groups[found_idx];
            if (grp->count >= grp->capacity) {
                grp->capacity = grp->capacity ? (grp->capacity * 2) : 8;
                grp->track_indices = (uint16_t*)realloc(grp->track_indices, grp->capacity * sizeof(uint16_t));
            }
            grp->track_indices[grp->count++] = i;
        } else if (group_count < max_groups) {
            track_group_t *grp = &groups[group_count++];
            strncpy(grp->name, name, sizeof(grp->name) - 1);
            grp->capacity = 8;
            grp->count = 1;
            grp->track_indices = (uint16_t*)malloc(grp->capacity * sizeof(uint16_t));
            grp->track_indices[0] = i;
        }
    }

    qsort(groups, group_count, sizeof(track_group_t), compare_groups);

    *out_groups = groups;
    *out_count = group_count;
}

uint16_t library_scan(const char *root_path, app_state_t *app) {
    if (!root_path || !app) return 0;

    app->queue_len = 0;
    scan_dir_recursive(root_path, app);

    if (app->albums) {
        for (uint16_t i = 0; i < app->albums_len; i++) {
            free(app->albums[i].track_indices);
        }
        free(app->albums);
        app->albums = NULL;
        app->albums_len = 0;
    }
    if (app->artists) {
        for (uint16_t i = 0; i < app->artists_len; i++) {
            free(app->artists[i].track_indices);
        }
        free(app->artists);
        app->artists = NULL;
        app->artists_len = 0;
    }

    build_groups(app->queue, app->queue_len, true, &app->albums, &app->albums_len);
    build_groups(app->queue, app->queue_len, false, &app->artists, &app->artists_len);

    if (app->play_order) free(app->play_order);
    if (app->queue_len > 0) {
        app->play_order = (uint16_t*)malloc(app->queue_len * sizeof(uint16_t));
        if (!app->play_order) {
            app_trigger_bsod(app, "ERR_OUT_OF_MEMORY", "Play order alloc failed");
            return 0;
        }
        for (uint16_t i = 0; i < app->queue_len; i++) {
            app->play_order[i] = i;
        }
    } else {
        app->play_order = NULL;
    }
    app->play_order_len = app->queue_len;
    app->play_pos = 0;

    app->dirty = true;
    return app->queue_len;
}

#include "library.h"
#include "hal/hal_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LIBRARY_MAX_SCAN_DEPTH 16

static void copy_truncated(char *destination, size_t destination_size, const char *source) {
    if (!destination || destination_size == 0) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }

    size_t length = 0;
    while (length + 1 < destination_size && source[length] != '\0') length++;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

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
    copy_truncated(path_copy, sizeof(path_copy), full_path);

    char *last_slash = strrchr(path_copy, '/');
    char *fname = last_slash ? (last_slash + 1) : path_copy;
    char *dot = strrchr(fname, '.');
    if (dot) *dot = '\0';
    copy_truncated(title, MAX_TITLE_LEN, fname);

    if (!last_slash) return;

    *last_slash = '\0';
    char *parent_slash = strrchr(path_copy, '/');
    char *parent_name = parent_slash ? (parent_slash + 1) : path_copy;
    if (parent_name[0] != '\0' && strcmp(parent_name, "sdcard") != 0 && strcmp(parent_name, ".") != 0) {
        copy_truncated(album, MAX_NAME_LEN, parent_name);
    }

    if (!parent_slash) return;

    *parent_slash = '\0';
    char *grand_slash = strrchr(path_copy, '/');
    char *grand_name = grand_slash ? (grand_slash + 1) : path_copy;
    if (grand_name[0] != '\0' && strcmp(grand_name, "sdcard") != 0 && strcmp(grand_name, ".") != 0) {
        copy_truncated(artist, MAX_NAME_LEN, grand_name);
    }
}

static void scan_dir_recursive(const char *dir_path, app_state_t *app, uint8_t depth) {
    if (app->queue_len >= app->queue_cap) return;
    if (depth > LIBRARY_MAX_SCAN_DEPTH) return;

    hal_dir_t *d = hal_opendir(dir_path);
    if (!d) return;

    hal_dir_entry_t entry;
    while (hal_readdir(d, &entry)) {
        if (app->queue_len >= app->queue_cap) break;
        if (entry.name[0] == '.') continue;

        char full_path[MAX_PATH_LEN];
        int path_len = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry.name);
        if (path_len < 0 || (size_t)path_len >= sizeof(full_path)) continue;

        if (entry.is_dir) {
            scan_dir_recursive(full_path, app, (uint8_t)(depth + 1));
        } else if (is_audio_file(entry.name)) {
            track_t *t = &app->queue[app->queue_len];
            copy_truncated(t->path, sizeof(t->path), full_path);
            extract_tags_from_path(full_path, t->title, t->artist, t->album);
            t->duration_secs = 0;
            app->queue_len++;
        }
    }

    hal_closedir(d);
}

static int compare_ascii_case_insensitive(const char *a, const char *b) {
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static int compare_groups(const void *a, const void *b) {
    const track_group_t *ga = (const track_group_t*)a;
    const track_group_t *gb = (const track_group_t*)b;
    return compare_ascii_case_insensitive(ga->name, gb->name);
}

static int compare_tracks(const void *a, const void *b) {
    const track_t *ta = (const track_t*)a;
    const track_t *tb = (const track_t*)b;
    return compare_ascii_case_insensitive(ta->path, tb->path);
}

static void free_groups(track_group_t *groups, uint16_t count) {
    if (!groups) return;
    for (uint16_t i = 0; i < count; i++) {
        free(groups[i].track_indices);
    }
    free(groups);
}

static bool build_groups(const track_t *tracks, uint16_t track_count, bool is_album,
                         track_group_t **out_groups, uint16_t *out_count) {
    *out_groups = NULL;
    *out_count = 0;
    if (track_count == 0) return true;

    const uint16_t max_groups = MAX_GROUPS;
    track_group_t *groups = (track_group_t*)calloc(max_groups, sizeof(*groups));
    if (!groups) {
        return false;
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
                uint16_t new_capacity = (uint16_t)(grp->capacity ? (grp->capacity * 2) : 8);
                uint16_t *new_indices = (uint16_t*)realloc(
                    grp->track_indices, (size_t)new_capacity * sizeof(*new_indices));
                if (!new_indices) {
                    free_groups(groups, group_count);
                    return false;
                }
                grp->track_indices = new_indices;
                grp->capacity = new_capacity;
            }
            grp->track_indices[grp->count++] = i;
        } else if (group_count < max_groups) {
            track_group_t *grp = &groups[group_count++];
            copy_truncated(grp->name, sizeof(grp->name), name);
            grp->capacity = 8;
            grp->count = 1;
            grp->track_indices = (uint16_t*)malloc((size_t)grp->capacity * sizeof(*grp->track_indices));
            if (!grp->track_indices) {
                free_groups(groups, group_count);
                return false;
            }
            grp->track_indices[0] = i;
        }
    }

    qsort(groups, group_count, sizeof(track_group_t), compare_groups);

    *out_groups = groups;
    *out_count = group_count;
    return true;
}

uint16_t library_scan(const char *root_path, app_state_t *app) {
    if (!root_path || !app || !app->queue || app->queue_cap == 0) return 0;

    app->queue_len = 0;
    scan_dir_recursive(root_path, app, 0);
    qsort(app->queue, app->queue_len, sizeof(*app->queue), compare_tracks);

    free_groups(app->albums, app->albums_len);
    app->albums = NULL;
    app->albums_len = 0;
    free_groups(app->artists, app->artists_len);
    app->artists = NULL;
    app->artists_len = 0;

    if (!build_groups(app->queue, app->queue_len, true, &app->albums, &app->albums_len) ||
        !build_groups(app->queue, app->queue_len, false, &app->artists, &app->artists_len)) {
        free_groups(app->albums, app->albums_len);
        app->albums = NULL;
        app->albums_len = 0;
        free_groups(app->artists, app->artists_len);
        app->artists = NULL;
        app->artists_len = 0;
        app_trigger_bsod(app, "ERR_OUT_OF_MEMORY", "Library grouping failed");
    }

    free(app->play_order);
    app->play_order = NULL;
    app->play_order_len = 0;
    if (app->queue_len > 0) {
        app->play_order = (uint16_t*)malloc((size_t)app->queue_len * sizeof(*app->play_order));
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
    app->current_index = 0;
    app->position_ms = 0;
    app->state = PLAYBACK_STOPPED;

    app->dirty = true;
    return app->queue_len;
}

#ifndef __ESP876_TABLES_H__
#define __ESP876_TABLES_H__

#include "esp876_api.h"

enum {
    ESP876_MODE_1280x720,
    ESP876_MODE_2560x720,
    ESP876_MODE_1280x480,
    ESP876_MODE_1280x360,
};

static const int esp876_60fps[] = {
    60,
};

static const int esp876_30fps[] = {
    30,
};

static const int esp876_15fps[] = {
    15,
};

static const int esp876_10fps[] = {
    10,
};

static const struct camera_common_frmfmt esp876_frmfmt[] = {
    {{1280, 720}, esp876_60fps, 1, 0, ESP876_MODE_1280x720},
    {{1280, 720}, esp876_30fps, 1, 0, ESP876_MODE_1280x720},
    {{1280, 720}, esp876_15fps, 1, 0, ESP876_MODE_1280x720},
    {{1280, 720}, esp876_10fps, 1, 0, ESP876_MODE_1280x720},
    {{2560, 720}, esp876_30fps, 1, 0, ESP876_MODE_2560x720},
    {{1280, 480}, esp876_15fps, 1, 0, ESP876_MODE_1280x480},
    {{1280, 360}, esp876_60fps, 1, 0, ESP876_MODE_1280x360},
    {{1280, 360}, esp876_30fps, 1, 0, ESP876_MODE_1280x360},
};
#endif  /* __esp876_TABLES__ */

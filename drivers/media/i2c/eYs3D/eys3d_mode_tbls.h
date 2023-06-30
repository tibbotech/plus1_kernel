#ifndef __EYS3D_TABLES_H__
#define __EYS3D_TABLES_H__

#include "eys3d_api.h"

enum {
    EYS3D_MODE_1280x720,
    EYS3D_MODE_2560x720,
    EYS3D_MODE_1280x480,
    EYS3D_MODE_1280x360,
};

static const int eys3d_60fps[] = {
    60,
};

static const int eys3d_30fps[] = {
    30,
};

static const int eys3d_15fps[] = {
    15,
};

static const int eys3d_10fps[] = {
    10,
};

static const struct camera_common_frmfmt eys3d_frmfmt[] = {
    {{1280, 720}, eys3d_60fps, 1, 0, EYS3D_MODE_1280x720},
    {{1280, 720}, eys3d_30fps, 1, 0, EYS3D_MODE_1280x720},
    {{1280, 720}, eys3d_15fps, 1, 0, EYS3D_MODE_1280x720},
    {{1280, 720}, eys3d_10fps, 1, 0, EYS3D_MODE_1280x720},
    {{2560, 720}, eys3d_30fps, 1, 0, EYS3D_MODE_2560x720},
    {{1280, 480}, eys3d_15fps, 1, 0, EYS3D_MODE_1280x480},
    {{1280, 360}, eys3d_60fps, 1, 0, EYS3D_MODE_1280x360},
    {{1280, 360}, eys3d_30fps, 1, 0, EYS3D_MODE_1280x360},
};
#endif

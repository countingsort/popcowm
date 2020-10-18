/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef KIWMI_INPUT_CURSOR_H
#define KIWMI_INPUT_CURSOR_H

#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>

#include "desktop/view.h"

enum kiwmi_cursor_mode {
    KIWMI_CURSOR_PASSTHROUGH,
    KIWMI_CURSOR_MOVE,
    KIWMI_CURSOR_RESIZE,
};

struct kiwmi_cursor {
    struct kiwmi_server *server;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *xcursor_manager;

    bool visible;

    enum kiwmi_cursor_mode cursor_mode;

    struct {
        struct kiwmi_view *view;
        int orig_x;
        int orig_y;
        struct wlr_box orig_geom;
        uint32_t resize_edges;
    } grabbed;

    struct wl_listener cursor_motion;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_button;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;

    struct {
        struct wl_signal button_down;
        struct wl_signal button_up;
        struct wl_signal motion;
    } events;
};

struct kiwmi_cursor_button_event {
    struct wlr_event_pointer_button *wlr_event;
    bool handled;
};

struct kiwmi_cursor_motion_event {
    double oldx;
    double oldy;
    double newx;
    double newy;
};

struct kiwmi_cursor *cursor_create(
    struct kiwmi_server *server,
    struct wlr_output_layout *output_layout);
void cursor_hide(struct kiwmi_cursor *cursor);
void cursor_show(struct kiwmi_cursor *cursor);
void cursor_destroy(struct kiwmi_cursor *cursor);

#endif /* KIWMI_INPUT_CURSOR_H */

/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "input/cursor.h"

#include <stdlib.h>

#include <wayland-server.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/log.h>

#include "desktop/desktop.h"
#include "desktop/layer_shell.h"
#include "desktop/output.h"
#include "desktop/view.h"
#include "input/seat.h"
#include "server.h"

static void
process_cursor_motion(struct kiwmi_server *server, uint32_t time)
{
    struct kiwmi_desktop *desktop = &server->desktop;
    struct kiwmi_input *input     = &server->input;
    struct kiwmi_cursor *cursor   = input->cursor;
    struct wlr_seat *seat         = input->seat->seat;

    switch (cursor->cursor_mode) {
    case KIWMI_CURSOR_MOVE: {
        struct kiwmi_view *view = cursor->grabbed.view;
        view->x                 = cursor->cursor->x - cursor->grabbed.orig_x;
        view->y                 = cursor->cursor->y - cursor->grabbed.orig_y;
        return;
    }
    case KIWMI_CURSOR_RESIZE: {
        struct kiwmi_view *view = cursor->grabbed.view;
        int dx                  = cursor->cursor->x - cursor->grabbed.orig_x;
        int dy                  = cursor->cursor->y - cursor->grabbed.orig_y;

        struct wlr_box new_geom = {
            .x      = view->x,
            .y      = view->y,
            .width  = cursor->grabbed.orig_geom.width,
            .height = cursor->grabbed.orig_geom.height,
        };

        if (cursor->grabbed.resize_edges & WLR_EDGE_TOP) {
            new_geom.y = cursor->grabbed.orig_y + dy;
            new_geom.height -= dy;
            if (new_geom.height < 1) {
                new_geom.y += new_geom.height;
            }
        } else if (cursor->grabbed.resize_edges & WLR_EDGE_BOTTOM) {
            new_geom.height += dy;
        }

        if (cursor->grabbed.resize_edges & WLR_EDGE_LEFT) {
            new_geom.x = cursor->grabbed.orig_geom.x + dx;
            new_geom.width -= dx;
            if (new_geom.width < 1) {
                new_geom.x += new_geom.width;
            }
        } else if (cursor->grabbed.resize_edges & WLR_EDGE_RIGHT) {
            new_geom.width += dx;
        }

        view->x = new_geom.x;
        view->y = new_geom.y;
        view_set_size(view, new_geom.width, new_geom.height);

        return;
    }
    default:
        // EMPTY
        break;
    }

    double ox                     = 0;
    double oy                     = 0;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        desktop->output_layout, cursor->cursor->x, cursor->cursor->y);

    wlr_output_layout_output_coords(
        desktop->output_layout, wlr_output, &ox, &oy);

    struct kiwmi_output *output = wlr_output->data;

    struct wlr_surface *surface = NULL;
    double sx;
    double sy;

    struct kiwmi_layer *layer = layer_at(
        &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
        &surface,
        cursor->cursor->x,
        cursor->cursor->y,
        &sx,
        &sy);

    cursor->visible = true;

    if (!layer) {
        struct kiwmi_view *view = view_at(
            desktop, cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);

        if (!view) {
            wlr_xcursor_manager_set_cursor_image(
                cursor->xcursor_manager, "left_ptr", cursor->cursor);
        }
    }

    if (surface) {
        bool focus_changed = surface != seat->pointer_state.focused_surface;

        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);

        if (!focus_changed) {
            wlr_seat_pointer_notify_motion(seat, time, sx, sy);
        }
    } else {
        wlr_seat_pointer_clear_focus(seat);
    }
}

static void
cursor_motion_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_cursor *cursor =
        wl_container_of(listener, cursor, cursor_motion);
    struct kiwmi_server *server            = cursor->server;
    struct wlr_event_pointer_motion *event = data;

    struct kiwmi_cursor_motion_event new_event = {
        .oldx = cursor->cursor->x,
        .oldy = cursor->cursor->y,
    };

    wlr_cursor_move(
        cursor->cursor, event->device, event->delta_x, event->delta_y);

    new_event.newx = cursor->cursor->x;
    new_event.newy = cursor->cursor->y;

    wl_signal_emit(&cursor->events.motion, &new_event);

    process_cursor_motion(server, event->time_msec);
}

static void
cursor_motion_absolute_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_cursor *cursor =
        wl_container_of(listener, cursor, cursor_motion_absolute);
    struct kiwmi_server *server                     = cursor->server;
    struct wlr_event_pointer_motion_absolute *event = data;

    struct kiwmi_cursor_motion_event new_event = {
        .oldx = cursor->cursor->x,
        .oldy = cursor->cursor->y,
    };

    wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);

    new_event.newx = cursor->cursor->x;
    new_event.newy = cursor->cursor->y;

    wl_signal_emit(&cursor->events.motion, &new_event);

    process_cursor_motion(server, event->time_msec);
}

static void
cursor_button_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_cursor *cursor =
        wl_container_of(listener, cursor, cursor_button);
    struct kiwmi_server *server            = cursor->server;
    struct kiwmi_input *input              = &server->input;
    struct wlr_event_pointer_button *event = data;

    struct kiwmi_cursor_button_event new_event = {
        .wlr_event = event,
        .handled   = false,
    };

    if (event->state == WLR_BUTTON_PRESSED) {
        wl_signal_emit(&cursor->events.button_down, &new_event);
    } else {
        wl_signal_emit(&cursor->events.button_up, &new_event);
    }

    if (!new_event.handled) {
        wlr_seat_pointer_notify_button(
            input->seat->seat, event->time_msec, event->button, event->state);
    }
}

static void
cursor_axis_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_cursor *cursor =
        wl_container_of(listener, cursor, cursor_axis);
    struct kiwmi_server *server          = cursor->server;
    struct kiwmi_input *input            = &server->input;
    struct wlr_event_pointer_axis *event = data;

    wlr_seat_pointer_notify_axis(
        input->seat->seat,
        event->time_msec,
        event->orientation,
        event->delta,
        event->delta_discrete,
        event->source);
}

static void
cursor_frame_notify(struct wl_listener *listener, void *UNUSED(data))
{
    struct kiwmi_cursor *cursor =
        wl_container_of(listener, cursor, cursor_frame);
    struct kiwmi_server *server = cursor->server;
    struct kiwmi_input *input   = &server->input;

    wlr_seat_pointer_notify_frame(input->seat->seat);
}

void
cursor_hide(struct kiwmi_cursor *cursor)
{
    wlr_cursor_set_surface(cursor->cursor, NULL, 0, 0);
    cursor->visible = false;
    wlr_seat_pointer_notify_clear_focus(cursor->server->input.seat->seat);
}

void
cursor_show(struct kiwmi_cursor *cursor)
{
    struct kiwmi_server *server = cursor->server;
    struct kiwmi_desktop *desktop = &server->desktop;
    struct wlr_seat *seat = server->input.seat->seat;

    double ox                     = 0;
    double oy                     = 0;
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        desktop->output_layout, cursor->cursor->x, cursor->cursor->y);

    wlr_output_layout_output_coords(
        desktop->output_layout, wlr_output, &ox, &oy);

    struct kiwmi_output *output = wlr_output->data;

    struct wlr_surface *surface = NULL;
    double sx;
    double sy;

    struct kiwmi_layer *layer = layer_at(
        &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
        &surface,
        cursor->cursor->x,
        cursor->cursor->y,
        &sx,
        &sy);

    cursor->visible = true;

    if (!layer) {
        struct kiwmi_view *view = view_at(
            desktop, cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);

        if (!view) {
            wlr_xcursor_manager_set_cursor_image(
                cursor->xcursor_manager, "left_ptr", cursor->cursor);
        }
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    }
}

struct kiwmi_cursor *
cursor_create(
    struct kiwmi_server *server,
    struct wlr_output_layout *output_layout)
{
    wlr_log(WLR_DEBUG, "Creating cursor");

    struct kiwmi_cursor *cursor = malloc(sizeof(*cursor));
    if (!cursor) {
        wlr_log(WLR_ERROR, "Failed to allocate kiwmi_cursor");
        return NULL;
    }

    cursor->server      = server;
    cursor->cursor_mode = KIWMI_CURSOR_PASSTHROUGH;

    cursor->cursor = wlr_cursor_create();
    if (!cursor->cursor) {
        wlr_log(WLR_ERROR, "Failed to create cursor");
        free(cursor);
        return NULL;
    }

    wlr_cursor_attach_output_layout(cursor->cursor, output_layout);

    cursor->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);

    cursor->cursor_motion.notify = cursor_motion_notify;
    wl_signal_add(&cursor->cursor->events.motion, &cursor->cursor_motion);

    cursor->cursor_motion_absolute.notify = cursor_motion_absolute_notify;
    wl_signal_add(
        &cursor->cursor->events.motion_absolute,
        &cursor->cursor_motion_absolute);

    cursor->cursor_button.notify = cursor_button_notify;
    wl_signal_add(&cursor->cursor->events.button, &cursor->cursor_button);

    cursor->cursor_axis.notify = cursor_axis_notify;
    wl_signal_add(&cursor->cursor->events.axis, &cursor->cursor_axis);

    cursor->cursor_frame.notify = cursor_frame_notify;
    wl_signal_add(&cursor->cursor->events.frame, &cursor->cursor_frame);

    wl_signal_init(&cursor->events.button_down);
    wl_signal_init(&cursor->events.button_up);
    wl_signal_init(&cursor->events.motion);

    cursor->visible = true;

    return cursor;
}

void
cursor_destroy(struct kiwmi_cursor *cursor)
{
    wlr_cursor_destroy(cursor->cursor);
    wlr_xcursor_manager_destroy(cursor->xcursor_manager);

    wl_list_remove(&cursor->cursor_motion.link);
    wl_list_remove(&cursor->cursor_motion_absolute.link);
    wl_list_remove(&cursor->cursor_button.link);
    wl_list_remove(&cursor->cursor_axis.link);
    wl_list_remove(&cursor->cursor_frame.link);

    free(cursor);
}

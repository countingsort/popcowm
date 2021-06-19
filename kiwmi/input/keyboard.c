/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "input/keyboard.h"

#include <stdbool.h>
#include <stdlib.h>

#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/multi.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#include "input/seat.h"
#include "server.h"

static bool
switch_vt(const xkb_keysym_t *syms, int nsyms, struct wlr_backend *backend)
{
    for (int i = 0; i < nsyms; ++i) {
        const xkb_keysym_t sym = syms[i];

        if (sym >= XKB_KEY_XF86Switch_VT_1 && sym <= XKB_KEY_XF86Switch_VT_12) {
            if (wlr_backend_is_multi(backend)) {
                struct wlr_session *session = wlr_backend_get_session(backend);
                if (session) {
                    unsigned vt = sym - XKB_KEY_XF86Switch_VT_1 + 1;
                    wlr_session_change_vt(session, vt);
                }
            }
            return true;
        }
    }

    return false;
}

static void
keyboard_modifiers_notify(struct wl_listener *listener, void *UNUSED(data))
{
    struct kiwmi_keyboard *keyboard =
        wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->input.seat->seat, keyboard->device);
    wlr_seat_keyboard_notify_modifiers(
        keyboard->server->input.seat->seat,
        &keyboard->device->keyboard->modifiers);
}

static void
keyboard_key_notify(struct wl_listener *listener, void *data)
{
    struct kiwmi_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct kiwmi_server *server     = keyboard->server;
    struct wlr_event_keyboard_key *event = data;
    struct wlr_input_device *device      = keyboard->device;

    uint32_t keycode = event->keycode + 8;

    const xkb_keysym_t *raw_syms;
    xkb_layout_index_t layout_index =
        xkb_state_key_get_layout(device->keyboard->xkb_state, keycode);
    int raw_syms_len = xkb_keymap_key_get_syms_by_level(
        device->keyboard->keymap, keycode, layout_index, 0, &raw_syms);

    const xkb_keysym_t *translated_syms;
    int translated_syms_len = xkb_state_key_get_syms(
        keyboard->device->keyboard->xkb_state, keycode, &translated_syms);

    bool handled = false;

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        handled =
            switch_vt(translated_syms, translated_syms_len, server->backend);
    }

    if (!handled) {
        struct kiwmi_keyboard_key_event data = {
            .raw_syms            = raw_syms,
            .translated_syms     = translated_syms,
            .raw_syms_len        = raw_syms_len,
            .translated_syms_len = translated_syms_len,
            .keycode             = keycode,
            .keyboard            = keyboard,
            .handled             = false,
        };

        if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
            wl_signal_emit(&keyboard->events.key_down, &data);
        } else {
            wl_signal_emit(&keyboard->events.key_up, &data);
        }

        handled = data.handled;
    }

    if (!handled) {
        wlr_seat_set_keyboard(server->input.seat->seat, keyboard->device);
        wlr_seat_keyboard_notify_key(
            server->input.seat->seat,
            event->time_msec,
            event->keycode,
            event->state);
    }
}

static void
keyboard_destroy_notify(struct wl_listener *listener, void *UNUSED(data))
{
    struct kiwmi_keyboard *keyboard =
        wl_container_of(listener, keyboard, device_destroy);

    wl_list_remove(&keyboard->link);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->device_destroy.link);

    wl_signal_emit(&keyboard->events.destroy, keyboard);

    free(keyboard);
}

struct kiwmi_keyboard *
keyboard_create(struct kiwmi_server *server, struct wlr_input_device *device)
{
    wlr_log(WLR_DEBUG, "Creating keyboard");

    struct kiwmi_keyboard *keyboard = malloc(sizeof(*keyboard));
    if (!keyboard) {
        return NULL;
    }

    keyboard->server = server;
    keyboard->device = device;

    keyboard->modifiers.notify = keyboard_modifiers_notify;
    wl_signal_add(&device->keyboard->events.modifiers, &keyboard->modifiers);

    keyboard->key.notify = keyboard_key_notify;
    wl_signal_add(&device->keyboard->events.key, &keyboard->key);

    keyboard->device_destroy.notify = keyboard_destroy_notify;
    wl_signal_add(&device->events.destroy, &keyboard->device_destroy);

    struct xkb_rule_names rules = {0};
    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap =
        xkb_map_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(device->keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(device->keyboard, 25, 600);

    wlr_seat_set_keyboard(server->input.seat->seat, device);

    wl_signal_init(&keyboard->events.key_down);
    wl_signal_init(&keyboard->events.key_up);
    wl_signal_init(&keyboard->events.destroy);

    wl_signal_emit(&server->input.events.keyboard_new, keyboard);

    return keyboard;
}

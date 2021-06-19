/* Copyright (c), Niclas Meyer <niclas@countingsort.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "server.h"

#include <stdio.h>
#include <stdlib.h>

#include <limits.h>

#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/util/log.h>

#include "luak/luak.h"

bool
server_init(struct kiwmi_server *server, char *config_path)
{
    wlr_log(WLR_DEBUG, "Initializing Wayland server");

    server->wl_display = wl_display_create();
    if (!server->wl_display) {
        wlr_log(WLR_ERROR, "Failed to create display");
        return false;
    }

    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

    server->backend = wlr_backend_autocreate(server->wl_display);
    if (!server->backend) {
        wlr_log(WLR_ERROR, "Failed to create backend");
        wl_display_destroy(server->wl_display);
        return false;
    }

    struct wlr_renderer *renderer = wlr_backend_get_renderer(server->backend);
    wlr_renderer_init_wl_display(renderer, server->wl_display);

    if (!desktop_init(&server->desktop, renderer)) {
        wlr_log(WLR_ERROR, "Failed to initialize desktop");
        wl_display_destroy(server->wl_display);
        return false;
    }

    if (!input_init(&server->input)) {
        wlr_log(WLR_ERROR, "Failed to initialize input");
        wl_display_destroy(server->wl_display);
        return false;
    }

    server->socket = wl_display_add_socket_auto(server->wl_display);
    if (!server->socket) {
        wlr_log(WLR_ERROR, "Failed to open Wayland socket");
        wl_display_destroy(server->wl_display);
        return false;
    }

    if (!config_path) {
        // default config path
        config_path = realloc(config_path, PATH_MAX);
        if (!config_path) {
            wlr_log(WLR_ERROR, "Falied to allocate memory");
            wl_display_destroy(server->wl_display);
            return false;
        }

        const char *config_home = getenv("XDG_CONFIG_HOME");
        if (config_home) {
            snprintf(config_path, PATH_MAX, "%s/kiwmi/init.lua", config_home);
        } else {
            snprintf(
                config_path,
                PATH_MAX,
                "%s/.config/kiwmi/init.lua",
                getenv("HOME"));
        }
    }

    wlr_screencopy_manager_v1_create(server->wl_display);

    server->config_path = config_path;

    if (!(server->lua = luaK_create(server))) {
        wlr_log(WLR_ERROR, "Failed to initialize Lua");
        wl_display_destroy(server->wl_display);
        return false;
    }

    return true;
}

bool
server_run(struct kiwmi_server *server)
{
    wlr_log(
        WLR_DEBUG, "Running Wayland server on display '%s'", server->socket);

    setenv("WAYLAND_DISPLAY", server->socket, true);

    if (!luaK_dofile(server->lua, server->config_path)) {
        wl_display_destroy(server->wl_display);
        return false;
    }

    if (!wlr_backend_start(server->backend)) {
        wlr_log(WLR_ERROR, "Failed to start backend");
        wl_display_destroy(server->wl_display);
        return false;
    }

    wl_display_run(server->wl_display);

    return true;
}

void
server_fini(struct kiwmi_server *server)
{
    wlr_log(WLR_DEBUG, "Shutting down Wayland server");

    wl_display_destroy_clients(server->wl_display);
    wl_display_destroy(server->wl_display);

    desktop_fini(&server->desktop);
    input_fini(&server->input);

    luaK_destroy(server->lua);

    free(server->config_path);
}

# Lua API Documentation

kiwmi is configured completely in Lua.
All types kiwmi offers are actually reference types, pointing to the actual internal types.
This means Lua's garbage collection has no effect on the lifetime of the object.

kiwmi offers the following classes to work with:

## kiwmi_server

This is the type of the global `kiwmi` singleton, representing the compositor.
This is the entry point to the API.

### Methods

#### kiwmi:cursor()

Returns a reference to the cursor object.

#### kiwmi:focused_view()

Returns the currently focused view.

#### kiwmi:on(event, callback)

Used to register event listeners.

#### kiwmi:quit()

Quit kiwmi.

#### kiwmi:schedule(delay, callback)

Call `callback` after `delay` ms.
Callback get passed itself, so that it can easily reregister itself.

#### kiwmi:spawn(command)

Spawn a new process.
`command` is passed to `/bin/sh`.

#### kiwmi:stop_interactive()

Stops an interactive move or resize.

#### kiwmi:view_at(x, y)

Get the view at a specified position.

### Events

#### keyboard

A new keyboard got attached.
Callback receives a reference to the keyboard.

#### output

A new output got attached.
Callback receives a reference to the output.

#### view

A new view got created (actually mapped).
Callback receives a reference to the view.

## kiwmi_cursor

A reference to the cursor object.

### Methods

#### cursor:hide()

Hides the cursor.

#### cursor:on(event, callback)

Used to register event listeners.

#### cursor:pos()

Get the current position of the cursor.
Returns two parameters: `x` and `y`.

### cursor:show()

Shows the cursor.

#### cursor:view_at_pos()

Returns the view at the cursor position, or `nil` if there is none.

### Events

#### button_down

A mouse button got pressed.
Callback receives the ID of the button (i.e. LMB is 1, RMB is 2, ...).

The callback is supposed to return `true` if the event was handled.
The compositor will not forward it to the view under the cursor.

#### button_up

A mouse button got released.
Callback receives the ID of the button (i.e. LMB is 1, RMB is 2, ...).

The callback is supposed to return `true` if the event was handled.
The compositor will not forward it to the view under the cursor.

#### motion

The cursor got moved.
Callback receives a table containing `oldx`, `oldy`, `newx`, and `newy`.

## kiwmi_keyboard

A handle to a keyboard.

### Methods

#### keyboard:keymap(keymap)

The function takes a table as parameter.
The possible table indexes are "rules, model, layout, variant, options".
All the table parameters are optional and set to the system default if not set.
For the values to set have a look at the xkbcommon library.
<https://xkbcommon.org/doc/current/structxkb__rule__names.html>

#### keyboard:modifiers()

Returns a table with the state of all modifiers.
These are: `shift`, `caps`, `ctrl`, `alt`, `mod2`, `mod3`, `super`, and `mod5`.

#### keyboard:on(event, callback)

Used to register event listeners.

### Events

#### destroy

The keyboard is getting destroyed.
Callback receives the keyboard.

#### key_down

A key got pressed.

Callback receives a table containing the `key`, `keycode`, `raw`, and the `keyboard`.

This event gets triggered twice, once with mods applied (i.e. `Shift+3` is `#`) and `raw` set to `false`, and then again with no mods applied and `raw` set to `true`.

The callback is supposed to return `true` if the event was handled.
The compositor will not forward it to the focused view in that case.

#### key_up

A key got released.
Callback receives a table containing the `key`, `keycode`, `raw`, and the `keyboard`.

This event gets triggered twice, once with mods applied (i.e. `Shift+3` is `#`) and `raw` set to `false`, and then again with no mods applied and `raw` set to `true`.

The callback is supposed to return `true` if the event was handled.
The compositor will not forward it to the focused view in that case.

## kiwmi_lua_callback

A handle to a registered callback.

## kiwmi_output

Represents an output (most often a display).

### Methods

#### output:auto()

Tells the compositor to start automatically positioning the output (this is on per default).

#### output:move(x, y)

Moves the output to a specified position.
This is referring to the top-left corner.

#### output:name()

The name of the output.

#### output:on(event, callbacks)

Used to register event listeners.

#### output:pos()

Get the position of the output.
Returns two parameters: `x` and `y`.

#### output:size()

Get the size of the output.
Returns two parameters: `width` and `height`.

### Events

#### destroy

The output is getting destroyed.
Callback receives the output.

#### resize

The output is being resized.
Callback receives a table containing the `output`, the new `width`, and the new `height`.

## kiwmi_renderer

Represents a rendering context, to draw on the output.

### Methods

#### renderer:draw_rect(color, x, y, w, h)

Draws a rect at the given position.
Color is a string in the form #rrggbb or #rrggbbaa.

## kiwmi_view

Represents a view (a window in kiwmi terms).

### Methods

#### view:app_id()

Returns the app id of the view.
This is comparable to the window class of X windows.

#### view:close()

Closes the view.

#### view:csd(client_draws)

Set whether the client is supposed to draw their own client decoration.

#### view:focus()

Focuses the view.

#### view:hidden()

Returns `true` if the view is hidden, `false` otherwise.

#### view:hide()

Hides the view.

#### view:imove()

Starts an interactive move.

#### view:iresize(edges)

Starts an interactive resize.
Takes a table containing the edges, that the resize is happening on.

#### view:move(x, y)

Moves the view to the specified position.

#### view:on(event, callback)

Used to register event listeners.

#### view:pid()

Returns the process ID of the client associated with the view.

#### view:pos()

Returns the position of the view (top-left corner).

#### view:resize(width, height)

Resizes the view.

#### view:show()

Unhides the view.

#### view:size()

Returns the size of the view.

**NOTE**: Used directly after `view:resize()`, this still returns the old size.

#### view:tiled(edges)

Takes a table containing all edges that are tiled, or a bool to indicate all 4 edges.

#### view:title()

Returns the title of the view.

### Events

#### destroy

The view is being destroyed.
Callback receives the view.

#### post_render

The view finished being rendered.
Callback receives a table with the `view`, the `renderer` and the `output`.

This event occurs once per output the view might be drawn on.

#### pre_render

The view is about to be rendered.
Callback receives a table with the `view`, the `renderer` and the `output`.

This event occurs once per output the view might be drawn on.

#### request_move

The view wants to start an interactive move.
Callback receives the view.

#### request_resize

The view wants to start an interactive resize.
Callback receives a table containing the `view`, and `edges`, containing the edges.

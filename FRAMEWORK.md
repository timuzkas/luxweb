# Luxweb Framework Reference

Luxweb is a small C++ web framework for server-rendered apps, JSON APIs, static assets, and first-party browser reactivity.

## Core Types

- `lux::App` in [include/luxweb/app.hpp](./include/luxweb/app.hpp)
- `lux::Context` in [include/luxweb/context.hpp](./include/luxweb/context.hpp)
- `lux::Auth` in [include/luxweb/auth.hpp](./include/luxweb/auth.hpp)
- `lux::UserStore` in [include/luxweb/user_store.hpp](./include/luxweb/user_store.hpp)
- `lux::StyleTheme` and built-in asset generation in [include/luxweb/style.hpp](./include/luxweb/style.hpp)

## App Basics

Create an app, register templates, routes, static files, and run it:

```cpp
lux::App app;
app.host("127.0.0.1");
app.add_template_path("templates");
app.pages("templates/pages");
app.static_files("/assets", "public");
app.port(18080);
app.run();
```

`lux::App` also serves these built-in framework assets:

- `/lux/lux.css`
- `/lux/lux.js`

## Page Routing

`app.pages("templates/pages")` scans HTML files and maps them to routes:

- `templates/pages/index.html` -> `/`
- `templates/pages/about.html` -> `/about`
- `templates/pages/nested/index.html` -> `/nested`

## HTTP Handlers

Use `app.get(...)` and `app.post(...)` for explicit routes, or `app.api("/api")` for grouped API endpoints.

```cpp
app.get("/health", [](lux::Context& ctx) {
  return ctx.json({{"ok", true}});
});

auto api = app.api("/api");
api.get("/me", [](lux::Context& ctx) {
  return ctx.json({{"user", ctx.current_user() ? ctx.current_user()->email : nullptr}});
});
```

`lux::Context` gives access to:

- `request()`
- `render(template_name, data)`
- `json(data, code)`
- `redirect(location, code)`
- `current_user()`

## Auth

`lux::Auth` wraps session-based sign in/out flows around `UserStore`.

- `sessions()` returns middleware that loads the current user
- `require_user(handler)` protects a route
- `signup(ctx)`, `login(ctx)`, and `logout(ctx)` return responses
- `current_user_json(ctx)` exposes a safe API payload for the browser

## Browser Runtime

`/lux/lux.js` exposes a small first-party client runtime:

- `lux.signal(initial)`
- `lux.effect(fn)`
- `lux.store(name, initialState, options?)`
- `lux.action(name, fn)`
- `lux.mount(root = document)`

Supported markup bindings:

- `data-lux-text="expr"`
- `data-lux-html="expr"`
- `data-lux-show="expr"`
- `data-lux-class:active="expr"`
- `data-lux-on:click="actionName"`
- `data-lux-model="store.path"`

The expression language is intentionally small. Use store paths, booleans, strings, numbers, negation, and simple comparisons.

Example:

```html
<script defer src="/lux/lux.js"></script>
<script defer src="/assets/app.js"></script>

<button type="button" data-lux-on:click="counter.increment">+</button>
<strong data-lux-text="counter.count">0</strong>
<input data-lux-model="counter.label">
```

```js
const counter = lux.store("counter", { count: 0, label: "Clicks" }, { persist: true });

lux.action("counter.increment", () => {
  counter.set("count", counter.get("count") + 1);
});
```

## Starter App

The starter app in `examples/starter/` shows the recommended wiring:

- `examples/starter/templates/layouts/base.html` includes `/lux/lux.js`
- `examples/starter/public/lux-user.js` hydrates `lux.store("user", ...)` from `/api/me`
- `examples/starter/templates/auth/dashboard.html` demonstrates bound user state and local dashboard state

## CLI Commands

`luxweb new <name>` creates a new app with CMake, templates, public assets, `/lux/lux.js`, and a reactive counter example.

`luxweb dev` configures, builds, and runs the current app in Debug mode using `luxweb.toml`.

`luxweb build` configures and builds the current app in Release mode:

```sh
luxweb build
luxweb build --debug
luxweb build --release --build-dir dist-build
luxweb build --target my_app
```

The output executable is written to the selected build directory. For scaffolded apps, `luxweb build` generates `.luxweb/embedded_assets.cpp` and compiles templates plus `public/` assets into the executable. The resulting binary can serve the app without the source `templates/` or `public/` directories.

Older apps need the generated CMake hook before `luxweb build` can embed assets. New apps created by `luxweb new <name>` include it automatically.

`luxweb serve` runs an already-built app executable. It is useful for filesystem-backed builds, local process launch, or running a binary built by another command. Inside a Luxweb project, it reads `luxweb.toml` and runs `build/<name> --host <host> --port <port>`. It can also run an explicit binary:

```sh
luxweb serve
luxweb serve ./build/my_app 3000
```

Scaffolded apps bind to `127.0.0.1` by default.

Host configuration sources:

- `luxweb.toml` `host` is used by the CLI when launching via `luxweb dev`
- `LUXWEB_HOST`
- `HOST`
- `--host`, `--host=0.0.0.0`, or `-H 0.0.0.0`

Port configuration sources:

- `luxweb.toml` `port` is used by the CLI when launching via `luxweb dev` or `luxweb serve`
- `LUXWEB_PORT`
- `PORT`
- positional executable argument, for example `./build/my_app 3000`
- `--port`, `--port=3000`, or `-p 3000`

Route handlers can read arbitrary environment variables through `lux::Context`:

```cpp
app.api("/api").get("/config", [](lux::Context& ctx) {
  return ctx.json({{"api_key_configured", ctx.env("API_KEY").has_value()}});
});
```

## Notes

- Built-in CSS is generated in `src/luxweb_core/style.cpp`.
- Built-in JS is generated in `src/luxweb_core/style.cpp`.
- Scaffolded apps created by `luxweb new <name>` already include the framework script tag and a reactive example.

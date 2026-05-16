# Luxweb

Luxweb is a small C++ web framework for server-rendered apps, JSON APIs, static assets, and a bit of browser-side state.

If you want the full docs, go here: [lux.timuzkas.xyz](https://lux.timuzkas.xyz).

## Install

On Ubuntu, the usual setup is:

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config git \
  libsqlite3-dev libssl-dev libfmt-dev libspdlog-dev
```

That covers the native bits. The rest of the framework pulls in its own third-party source during CMake configure.

## Build

From the repo root:

```sh
cmake -S . -B build
cmake --build build
```

That builds the framework library, the `luxweb` CLI, and the starter example.

If you only want the CLI:

```sh
cmake --build build --target luxweb
```

If you only want the starter example:

```sh
cmake --build build --target luxweb_starter
```

## Bundle

`luxweb build` is the bundle step for generated apps. It embeds templates and public assets into the app binary so you can ship one executable instead of a source tree.

Typical flow in an app created with `luxweb new`:

```sh
luxweb build
```

If the app has been moved to another machine, make sure it can still find the Luxweb source tree. The app CMake can use one of these:

```sh
cmake -S . -B build -DLUXWEB_SOURCE_DIR=/path/to/luxweb
```

or:

```sh
export LUXWEB_SOURCE_DIR=/path/to/luxweb
```

Or put the framework checkout next to the app as `../luxweb`, which is the easiest setup if you keep both in one folder.

## Run

For the starter example:

```sh
./build/luxweb_starter
```

By default it binds to `127.0.0.1`. If you want it reachable from outside the box, use:

```sh
./build/luxweb_starter --host 0.0.0.0 --port 3010
```

## FAQ and FEE 

If you get missing library errors like `libfmt.so` or `libspdlog.so`, rebuild on the same machine you plan to run on, or make sure the runtime packages match the binary you copied over.

If `luxweb build` fails with a path error, it usually means the app was copied somewhere else and the framework source path got stale. Set `LUXWEB_SOURCE_DIR` or keep the repo checkout next to the app.

# stok Project Overview

## Current layout

- `server/`
  Bootstrap process. It remains the lifecycle entrypoint and is being adapted to start and supervise child services.
- `services/`
  Product-facing processes for `stok`. The first batch now contains:
  - `stok-market-data-service`: synthetic quote publisher.
  - `stok-desktop-shell`: Qt6 + QML desktop container.
  - `common/`: shared configuration, Fast-DDS SHM bus, and OpenTelemetry bootstrap code.
- `platform/`
  Vendored macchina.io / Poco platform code still used as the runtime foundation for the bootstrap side.
- `docs/`
  Project-level notes that describe the new `stok` architecture instead of the legacy copied template.

## What was stale

- The root `CMakeLists.txt` still referenced removed modules such as `Geo`, `Serial`, `WebTunnel`, and `webUI`.
- `platform/OSP/CMakeLists.txt` still referenced deleted submodules such as `SimpleAuth`, `Web`, `WebEvent`, and `WebServer`.
- The repository still behaved like a generic `MyIoT` template, not a stock application workspace.

## New runtime path

1. `server/macchina` acts as the launcher and lifecycle owner.
2. Child processes live under the build output `services/` directory.
3. `stok-market-data-service` publishes stock quote snapshots through Fast-DDS shared memory transport.
4. `stok-desktop-shell` subscribes to the same topic and renders the watchlist in a VS Code inspired QML shell.
5. Each process installs a local OpenTelemetry bridge for logs, traces, and metrics. OTLP export is off by default and can be enabled from the service property files.

## Next layer to implement

- `server` should read a `stok.services.*` block from `macchina.properties` and launch the new child processes in order.
- Real business services can be added beside the synthetic publisher and wired into the same DDS topic model.
- The desktop shell can be extended with order entry, portfolio panes, alerts, and analytics once product requirements settle.

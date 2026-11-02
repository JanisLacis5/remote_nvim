# Remote Nvim

Remote Nvim (`janisnvim`) is a remote-workspace workflow for Neovim that prioritizes **local editing responsiveness** without requiring the development toolchain or full source tree to live on the local machine.

A command invoked on a remote host opens the requested remote file in **local Neovim**. The file is copied into a local cache and edited there. Changes are synchronized back to the remote host, while project-dependent services, notably language servers, run on the remote host over SSH.

> **Status:** Architecture/design direction. This document describes intended behavior, not a claim that everything actually works :)

## Goals

- Make typing, Vim motions, operators, and macros respond at local-editor speed, without waiting for a network round trip.
- Keep the authoritative project, build environment, compilers, generated files, and language servers on the remote host.
- Avoid copying an entire project merely to open and edit a file.
- Integrate with an existing SSH workflow and local Neovim configuration.
- Preserve remote file identity and avoid silently overwriting concurrent remote changes.

## Architecture

```text
REMOTE HOST                                LOCAL MACHINE

shell / tmux                               janisnvim daemon / client
    |                                              |
    +-- janisnvim path/to/file.cpp --------------->|
                                                   |  fetch file over SSH
remote project                                     v
  /home/user/project/file.cpp             local cached copy
           ^                               /tmp/.../file.cpp
           |                                       |
           | periodic / save-triggered sync       v
           +-------------------------------- local Neovim
                                                   |
                                                   | LSP client messages
                                                   v
remote clangd <------------- SSH transport / path mapping
  | project sources, headers, compile_commands.json |
  +-------------------------------------------------+
```

The diagram is conceptual. Transport startup, authentication, and exact local cache paths are implementation details.

### Local editor and cache

Neovim runs **locally**, with local keymaps, rendering, buffer operations, and plugins that do not require remote project access. A remote file is fetched to a namespaced local cache before being opened. A cached file must retain an unambiguous association with its remote host, workspace, and path.

Local edits are immediately visible because they are applied by local Neovim. File synchronization is independent of ordinary keystroke processing.

### Synchronization

The remote project remains the durable source of truth. The local cached copy is a working copy that may temporarily be ahead of the remote file.

The design calls for synchronization from the local cache back to the remote host, potentially on an interval. Exact triggers (on save, periodic, on close), conflict detection, atomic replacement, retry behavior, and crash recovery are **not yet finalized**. An implementation must not assume that blindly overwriting the remote file is safe.

The system also needs a defined policy for modifications made remotely while a local copy is open.

### Remote LSP

Neovim's LSP client runs locally; the language server (for example, `clangd`) runs on the remote host, where it can access the real project, compile database, compiler, and headers. LSP messages are transported over SSH.

Local open buffers are communicated to the language server through standard LSP document notifications (`didOpen`, `didChange`, etc.), so language analysis does not have to wait for file synchronization. Local and remote document paths/URIs must map consistently in both directions.

A definition or reference may point to a remote file that is not cached yet. Path translation by itself does **not** fetch that file; navigation must account for cache misses.

### Other project-aware tooling

- **Search / Telescope:** Execute project searches (for example, `rg`) remotely and map result paths into the local workspace/cache.
- **Builds, tests, Git, debugging:** These naturally operate against the remote project but require explicit integration if exposed through local Neovim plugins.
- **Local-only plugins:** UI, editing, keymaps, Treesitter, and other locally self-contained functionality can run normally.

Not every Neovim plugin becomes remote-aware automatically. Plugins that read the filesystem, spawn subprocesses, or assume the local working directory is the full project need an integration strategy.

## Key invariants

1. Normal editing and rendering never synchronously wait for the remote host.
2. Every cached file has a stable, collision-resistant remote identity.
3. The LSP's document identity corresponds to the correct remote path, including in responses and diagnostics.
4. Unsaved editor state is not confused with the last synchronized on-disk state.
5. Network errors cannot silently discard local edits or overwrite unrelated remote changes.
6. Remote file writes must have deliberate semantics for permissions, replacement, and concurrency.

## Open design decisions

- How the remote command discovers and contacts the local service through the active SSH session.
- Cache root, cache lifetime, cleanup, and mapping between remote and local files.
- Sync scheduling, whether a sync includes only saved buffers, and how sync completion is acknowledged.
- Remote-change detection, conflict policy, atomic updates, and recovery after interruption.
- How to fetch uncached files reached via LSP navigation or search results.
- Whether LSP forwarding uses `clangd --path-mappings`, a generic proxy, or another mechanism.
- Which project-aware Neovim plugins receive first-class support.

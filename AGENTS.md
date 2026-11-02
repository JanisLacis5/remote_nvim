# Agent Instructions — Remote Nvim

## Project purpose

Remote Nvim (`janisnvim`) makes a **remote workspace editable with local Neovim latency**. A command executed on a remote machine requests a file; the file is fetched over SSH to a local cache, edited with local Neovim, and synchronized back to the remote machine. Development services that require the real project (notably LSP servers such as `clangd`) remain remote and communicate with local Neovim over SSH.

This is the **current intended architecture**. Do not introduce or refer to a remote Neovim UI/redraw-streaming architecture.

## Source of truth

- Read the repository, tests, build files, and `README.md` before making changes. This file specifies architectural intent, **not** proof that components already exist.
- Prefer existing naming, structure, error conventions, and platform abstractions over assumptions in this document.
- Do not invent commands, configuration keys, paths, transport details, or implementation status.
- If a design choice is unresolved, report the tradeoff and avoid silently setting project-wide policy.

## Architecture boundaries

**Local machine**
- Runs Neovim, UI, key handling, editing commands, macros, and local plugins.
- Holds cached working copies of files opened from remote hosts.
- Runs the LSP client and sends its messages to remote language servers.
- Editing responsiveness must not depend on network round trips.

**Remote machine**
- Retains the project files, toolchain, compilation database, headers, and build context.
- Executes project-aware services such as `clangd`, remote searches, builds, and Git commands.
- Receives synchronized edits under explicit consistency and conflict rules.

**Transport and mediation**
- SSH carries file transfers, requests, and remote service traffic as appropriate.
- Remote file identity must include sufficient host/workspace/path information to avoid cache collisions.
- Remote/local path and URI translation must be correct in both directions.
- LSP `didOpen` / `didChange` communicates editor buffer content independently of disk synchronization.
- An uncached file returned by LSP navigation must be fetched or handled deliberately; a URI rewrite alone is not sufficient.

## Correctness requirements

- **No silent data loss:** An interrupted transfer, remote conflict, or failed write must not lose locally edited data.
- **No blind overwrite:** Account for remote modifications since the cached version was obtained. Exact conflict resolution is a design decision, not permission to overwrite.
- **Explicit consistency:** Distinguish editor buffer content, cached on-disk content, and remote on-disk content. Do not assume they are equal.
- **Safe writes:** Consider temporary files, atomic rename where appropriate, permissions, metadata, and how partial failures are reported. Do not claim that rename is universally atomic across filesystems.
- **Robust transport:** Handle partial reads/writes, EOF, cancellation, reconnection, backpressure, and process cleanup. Do not assume read/write boundaries preserve message boundaries.
- **Path safety:** Treat remote paths as data, not shell snippets; avoid command injection and path traversal. Respect symlinks and canonicalization policy once defined.
- **Resource lifecycle:** Make ownership of subprocesses, sockets, file descriptors, cached files, and temporary files explicit.
- **LSP semantics:** Preserve framing and message content unless an intentional transformation is necessary. Translate URIs/paths consistently, including diagnostics, locations, workspace edits, and relevant requests.

## C++ engineering rules

- Use the C++ standard and compiler requirements actually configured by the repository; C++23 is the project's intended language level where supported.
- Prefer RAII, explicit ownership, narrow interfaces, value semantics where sensible, and minimal allocations in hot paths.
- Do not introduce shared ownership, threads, generic frameworks, or extra layers merely for extensibility.
- Handle short I/O, transient failures, process exits, and cleanup paths explicitly.
- Do not block the Neovim UI/event loop on SSH, remote filesystem operations, or remote language-server responses.
- Follow established formatting and naming conventions visible in the codebase.

## Scope and workflow

1. Inspect the relevant code and understand ownership, callers, invariants, and tests.
2. Make the smallest change that satisfies the request and fits the architecture.
3. Add focused tests for the behavior changed, especially failure modes and boundaries.
4. Run only available, relevant builds/tests; state what was actually executed.
5. Summarize changed files, notable decisions, and any unresolved limitations.

Do not perform unrelated refactors, fabricate implemented features, or add compatibility mechanisms for obsolete architectures.

## Important unresolved policies

Do not decide these implicitly while implementing an unrelated feature:

- Sync trigger/interval and whether unsaved buffers are included.
- Conflict detection and merge/reject behavior.
- Remote change monitoring and local cache invalidation.
- Cache naming, lifetime, and cleanup.
- SSH connection setup and local-service discovery.
- Generic LSP proxy versus language-server-specific path mapping.
- Behavior of project-aware plugins beyond explicitly implemented integrations.

## Testing priorities

Use targeted tests for remote identity/path mapping, LSP request/response and document URI translation, cached-file lifecycle, concurrent remote modifications, interrupted synchronization, error recovery, and responsiveness under network delay. Tests must validate precise expected behavior rather than only exercising happy-path integration.

## Engineering and Code Quality Requirements

### Simplicity and Architecture

- Prefer the simplest implementation that satisfies the requirements.
- Do not introduce abstractions, interfaces, inheritance hierarchies, factories, or design patterns without a concrete need.
- Prefer concrete types, free functions, and small cohesive classes.
- Avoid speculative extensibility, unnecessary configuration, and overengineering.
- Do not refactor unrelated code while implementing a feature.
- Prefer explicit ownership and deterministic resource management.
- Do not introduce dependencies without explaining why they are necessary.
- Do not silently change architectural assumptions.

### Formatting, Spacing, and Readability

**Vertical whitespace is a strict code quality requirement, not an optional stylistic preference.**

Generated code must be visually organized into distinct logical blocks. Dense, uninterrupted sequences of statements are unacceptable, even when the code is technically correct and passes formatting checks.

Follow these rules:

- **Separate logically distinct groups of statements with a blank line.** This includes setup, validation, resource acquisition, computation, state changes, error handling, and cleanup.
- **Use blank lines to communicate intent.** A reader should be able to identify the major stages of a function by scanning its structure.
- **Always separate function definitions with blank lines.**
- **Separate unrelated declarations and operations.** Do not pack initialization, conditionals, loops, and return statements into one uninterrupted block.
- **Keep tightly related statements together.** Do not insert blank lines mechanically after every statement.
- **Prefer short, clearly structured functions.** If a function contains many unrelated logical sections, consider whether decomposition would improve clarity. Do not introduce unnecessary helper functions solely to shorten code.
- **Prioritize human readability over compactness.** Minimizing the number of lines is not a goal.
- Follow the repository's `.clang-format` configuration, but recognize that passing `clang-format` does not guarantee acceptable vertical spacing.
- Match the existing codebase's naming conventions, indentation, brace style, and organizational patterns.

#### Example of unacceptable formatting

```cpp
void process() {
    auto data = read_data();
    if (data.empty()) return;
    auto result = transform(data);
    if (!result) return;
    write_data(*result);
    log_success();
}
```

#### Expected formatting

```cpp
void process() {
    auto data = read_data();
    if (data.empty())
        return;

    auto result = transform(data);
    if (!result)
        return;

    write_data(*result);

    log_success();
}
```

The second example is preferred because validation, transformation, output, and reporting are visually separated.

**Before finishing any implementation, review the code specifically for vertical whitespace and logical grouping.** Do not consider code complete simply because it compiles, passes tests, or conforms to `clang-format`.

### Scope and Maintainability

- Keep patches focused and reasonably small.
- Avoid modifying unrelated files.
- Do not add unnecessary comments explaining obvious code.
- Prefer clear control flow over clever or overly compact expressions.
- Do not sacrifice readability to reduce line count.
- Follow established patterns in the repository unless there is a concrete reason to change them.

### Before Implementing a Substantial Feature

Briefly explain:

1. The proposed implementation.
2. Any new abstractions or dependencies.
3. The important correctness and failure cases.

Wait for approval before introducing significant architectural changes.

### After Implementation

Report:

- What changed.
- What was tested.
- What remains unverified.
- Any important design decisions made during implementation.

Before presenting the final result, inspect the generated code for unnecessary abstractions, dense formatting, missing blank lines between logical blocks, and deviations from the existing code style.

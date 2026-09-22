# AGENTS.md

This workspace is used for operating-system coursework and RISC-V development.
Follow the rules below when reading, modifying, building, or testing projects
inside this workspace.

## 1. Scope and safety

- Work only inside the current project/workspace unless the user explicitly
  asks otherwise.
- Do not modify system files, shell configuration, global packages, WSL
  settings, compiler installations, QEMU installations, or files outside the
  project without explicit permission.
- Do not run destructive commands without explicit confirmation.
  This includes commands such as:
  - rm -rf
  - git reset --hard
  - git clean -fd / -fdx
  - forced checkout/restore that discards work
  - deleting or overwriting project files
- Never expose, print, store, or commit API keys, access tokens, passwords,
  credentials, or other secrets.

## 2. Git discipline

Before modifying code:

1. Run `git status`.
2. Inspect the current branch and existing uncommitted changes.
3. Never overwrite or revert pre-existing user changes unless explicitly asked.

During development:

- Keep changes focused on the current task.
- Avoid unrelated refactors, formatting changes, or large-scale rewrites.
- Use `git diff` to inspect modifications.
- Prefer small, logically isolated changes.
- Do not create commits, push branches, merge, rebase, or alter Git history
  unless the user explicitly asks for it.

Before using `git restore`, `git checkout`, `git reset`, `git clean`, or similar
commands, inspect the affected files and explain what would be discarded.

## 3. Understand before modifying

Before changing implementation code:

- Read the relevant source files and surrounding code.
- Identify the existing design and conventions.
- Check relevant headers, build scripts, Makefiles, tests, and documentation.
- Do not invent interfaces or assumptions when they can be verified from the
  repository.
- Prefer fixing the root cause rather than patching symptoms.

For operating-system code, pay particular attention to architecture-specific
behavior, memory management, page tables, traps/interrupts, privilege levels,
linker scripts, boot flow, and hardware/emulator assumptions.

## 4. Build and test

A code change is not considered complete merely because it looks correct.

After meaningful modifications:

- Build the project using its documented build command.
- Run the relevant tests.
- For uCore-style projects, use commands such as `make`, `make qemu`, and
  `make grade` when they are supported by the repository.
- Read compiler errors, warnings, runtime output, and test failures carefully.
- Fix problems based on evidence from the build/test output.

Do not claim that a change works unless it has actually been tested, or clearly
state when testing was not possible.

## 5. Preserve the environment

The current WSL environment may already contain working RISC-V toolchains,
QEMU, OpenSBI, Git, and other development tools.

- Do not reinstall, upgrade, downgrade, or replace toolchains automatically.
- Do not change PATH, LD_LIBRARY_PATH, .bashrc, proxy settings, or other
  environment configuration merely to solve a project-level problem.
- If a build failure appears to be caused by toolchain or environment
  compatibility, diagnose and explain it first before changing the environment.

## 6. Communication

When a task is non-trivial:

- Briefly explain what files or components are relevant before making broad
  changes.
- After modification, summarize:
  - what changed,
  - why it changed,
  - what commands/tests were run,
  - whether they passed,
  - any remaining uncertainty.

If the requested change conflicts with the repository's documented behavior or
would risk destroying existing work, stop and ask before proceeding.

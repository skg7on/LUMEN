# Worktree isolation

- Make design and implementation edits in an isolated worktree. Detect an existing
  linked worktree first and reuse it; do not nest worktrees or reset another task's
  checkout.
- Prefer a platform-managed worktree when available. Its location and lifecycle
  belong to the platform; do not move or remove its directory manually.
- When creating a worktree manually, place it under `.claude/worktrees/` relative
  to the LUMEN repository root. Verify that directory is ignored before creation.
  Resolve the root with Git rather than hardcoding a developer's absolute path.
- Use `codex/<short-description>` for new branches unless the task specifies
  another convention. Use lowercase kebab-case, for example
  `codex/project-rules`, `codex/kv-page-leases`, or `codex/backend-abi`.
- Preserve existing staged, unstaged, and untracked work. Copy only task-relevant
  inputs into a new checkout and stage explicit paths after reviewing the diff.
- Verify and commit in the isolated checkout. A request to create a commit does
  not require opening a PR; follow the requested publication scope.
- Keep unmerged or unpushed work recoverable. Clean up manually created worktrees
  only after integration or an explicit discard request, checking for local work
  first. Use the platform's archive/cleanup action for managed worktrees.

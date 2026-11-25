# Commit and PR naming

Use a conventional prefix for commit subjects and PR titles:

```text
<type>(<optional-scope>): <optional-task-tag> <imperative subject>
```

- Types: `feat`, `fix`, `docs`, `test`, `refactor`, `perf`, `build`, `ci`, or `chore`.
- Omit parentheses when no scope is useful. Use lowercase LUMEN subsystem scopes
  such as `engine`, `scheduler`, `memory`, `executor`, `backend`, `kernels`,
  `model`, `tokenizer`, `frontend`, `collective`, `mesh`, or `modelpack`.
- Choose the type from the primary change. Fixtures and assertions alone use
  `test`; behavior-preserving restructuring uses `refactor`.
- Start with the type prefix. Optional task tags follow the prefix in brackets;
  real issue references go at the end. Do not invent issue numbers or task IDs.
- Use a lowercase imperative subject with no trailing period. Aim for a subject
  within 72 characters. Put attribution, validation details, and explanation in
  the body.
- For one-task PRs, reuse the primary commit's type and scope. Describe the actual
  problem, resulting behavior, and validation in the PR body.

Examples:

```text
docs: add project development rules
feat(engine): add immutable step plans
fix(memory): defer page reuse until completion
test(backend): cover stale completion epochs
perf(kernels): reduce decode attention staging
```

Before creating or editing a PR, check its title against this convention. Create
commits, push branches, and open PRs only within the user's requested scope.

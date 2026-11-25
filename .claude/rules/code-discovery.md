# Code discovery

Prefer the codebase-memory-mcp knowledge graph for code discovery. Select the
project by its repository root using `list_projects`; if it is not indexed, call
`index_repository` for this checkout before searching. Refresh the index when it
does not reflect the code being inspected.

Use this order:

1. `search_graph` to locate functions, classes, routes, and other symbols.
2. `trace_path` to inspect callers, callees, and execution relationships.
3. `get_code_snippet` to read a symbol using its exact qualified name from search.
4. `query_graph` for multi-hop relationships and structural queries.
5. `get_architecture` for a high-level overview of modules and dependencies.

Use `rg` or direct file reads for Markdown, build configuration, shell scripts,
string literals, error messages, and config values. Fall back to these tools for
code when graph tools are unavailable or return insufficient results; state the
limitation when it affects a conclusion. An empty graph is not evidence that a
feature exists or that a repository has no files.

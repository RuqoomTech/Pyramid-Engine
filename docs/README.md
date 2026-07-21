# Pyramid Engine documentation

The documentation is intentionally compact and source-oriented. Public claims should describe code that exists in this repository; planned work belongs in the roadmap rather than the API documentation.

| Document | Purpose |
|---|---|
| [Building and testing](BUILDING.md) | Supported toolchain, presets, outputs, tests, smoke tests, and troubleshooting |
| [Architecture](ARCHITECTURE.md) | Runtime layers, data flow, ownership, rendering pipeline, and module status |
| [API overview](API.md) | Supported public interfaces with small, compilable usage patterns |
| [Examples](EXAMPLES.md) | The two applications that currently exist and how to run them |
| [Development guide](DEVELOPMENT.md) | Contribution workflow, coding conventions, tests, and documentation rules |
| [Roadmap and known issues](ROADMAP.md) | Source-backed limitations and prioritized implementation work |
| [Changelog](../CHANGELOG.md) | Historical project changes |

## Documentation maintenance rules

1. Verify names, namespaces, signatures, target names, and paths against source before merging documentation changes.
2. Label partial behavior explicitly. Do not document a declared method as supported when it has no definition or contains a placeholder path.
3. Keep tutorials tied to a checked-in example. Planned tutorials and systems belong in `ROADMAP.md`.
4. Update `README.md`, `ARCHITECTURE.md`, `API.md`, and `ROADMAP.md` when a change affects public behavior or project status.
5. Run the local Markdown-link check described in [Development guide](DEVELOPMENT.md) after moving documentation.

Last source audit: **July 21, 2026**.

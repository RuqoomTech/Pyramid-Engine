# Pyramid Engine documentation

This documentation describes the source tree as it exists at the latest audit. Planned behavior belongs in the roadmap, not in API documentation.

| Document | Purpose |
|---|---|
| [Building and testing](BUILDING.md) | Supported platform, presets, tests, CI, installation, and troubleshooting |
| [Architecture](ARCHITECTURE.md) | Runtime layers, rendering flow, ownership, packaging, and subsystem status |
| [API overview](API.md) | Supported public interfaces and explicit capability limits |
| [Examples](EXAMPLES.md) | Checked-in applications, output paths, and validation expectations |
| [Development guide](DEVELOPMENT.md) | Engineering workflow, conventions, tests, documentation, and release hygiene |
| [Roadmap and known issues](ROADMAP.md) | Completed stabilization work and remaining priorities |
| [Changelog](../CHANGELOG.md) | Concise implementation history |

## Maintenance rules

1. Verify paths, targets, namespaces, signatures, and behavior against source.
2. A public declaration must have a definition or fail explicitly and predictably.
3. Do not describe reserved APIs, placeholder algorithms, or planned systems as supported.
4. Keep setup instructions in `BUILDING.md`; do not add overlapping setup files.
5. Update the status table, API overview, roadmap, and changelog when public behavior changes.
6. Run the Markdown-link check and the public-API linkage test after reorganizing files or interfaces.

Last source audit: **July 21, 2026**.

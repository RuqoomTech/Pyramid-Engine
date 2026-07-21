# Development guide

## Workflow

1. Create a focused branch from the current main branch.
2. Configure and build with the checked-in presets.
3. Add or update tests for behavior changes.
4. Run CTest and the graphical smoke test when graphics/runtime code changes.
5. Update the compact documentation set when public behavior, status, or build commands change.
6. Submit a pull request with the exact commands and configurations tested.

## Coding conventions

The existing codebase targets C++17 and generally uses:

- four-space indentation;
- braces on a new line;
- `PascalCase` for types and most public methods;
- `camelCase` for locals and parameters;
- `m_` prefixes for fields;
- `Pyramid`, `Pyramid::Renderer`, `Pyramid::SceneManagement`, `Pyramid::Math`, and `Pyramid::Util` namespaces;
- RAII and smart pointers for ownership;
- `PYRAMID_LOG_*` and assertion macros for diagnostics.

Match the surrounding module when legacy naming is inconsistent. Avoid broad style-only rewrites in functional changes.

## Architecture constraints

- Do not advertise or select non-OpenGL `GraphicsAPI` values until a backend exists.
- Keep Win32-specific types out of generic public headers except the Win32 implementation header.
- Raw pointers passed into device/command APIs are non-owning; do not store them beyond the owning resource lifetime.
- New public interfaces should not use silent no-op defaults for required behavior.
- Keep render-pass ordering explicit and test state transitions across passes.
- Treat scene serialization and unfinished `SceneManager` methods as unavailable until implemented and linked by tests.

## Tests

Build the registered suite:

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests-clean
ctest --preset test-debug
```

Tests are standalone executables registered from `Engine/Utils/test/CMakeLists.txt`. New tests should:

- use a `Test<Feature>.cpp` name;
- return zero only when all checks pass;
- print actionable failure context;
- avoid depending on developer-specific absolute paths;
- register with `add_utils_test` or an equivalent module-local helper.

For renderer changes, run:

```powershell
./scripts/run-smoke.ps1 -BuildDir build -Config Debug -DurationSeconds 5
```

Then visually inspect both examples. A timed process check is not a pixel/rendering test.

## Documentation changes

The maintained set is:

```text
README.md
docs/README.md
docs/BUILDING.md
docs/ARCHITECTURE.md
docs/API.md
docs/EXAMPLES.md
docs/DEVELOPMENT.md
docs/ROADMAP.md
CHANGELOG.md
```

Prefer updating an existing document instead of adding another status report, best-practices file, or overlapping setup guide.

Check relative Markdown links from the repository root with:

```powershell
python -c "from pathlib import Path; import re; r=Path('.'); p=re.compile(r'\\[[^]]*\\]\\(([^)]+)\\)'); bad=[]; [(bad.extend([(str(f),x) for x in p.findall(f.read_text(encoding='utf-8')) if not x.startswith(('http://','https://','mailto:','#')) and not (f.parent/x.split('#')[0]).exists()])) for f in r.rglob('*.md')]; print(*bad, sep='\\n')"
```

Also review code samples against public headers. Documentation snippets are not compiled automatically in this snapshot.

## Commit and pull-request expectations

Use a concise imperative subject such as `Fix deferred pass target binding` or `Document Windows build requirements`.

A pull request should include:

- purpose and affected modules;
- noteworthy design or ownership decisions;
- build/test commands and results;
- screenshots or video for visible rendering changes;
- known limitations or follow-up work;
- documentation and changelog updates when applicable.

## Release hygiene

Before tagging a release:

1. synchronize the version in `CMakeLists.txt` with the changelog/tag;
2. build a clean Release configuration;
3. run all tests and examples on a supported Windows machine;
4. verify install rules into an empty prefix;
5. confirm public headers link for a minimal external consumer;
6. remove unsupported claims and unresolved placeholder APIs from release notes.

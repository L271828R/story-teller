# MDViewer — Claude guidance

## Tests

Always run the test suite **before and after** making any code changes.

```bash
cmake --build build --target test_mdviewer && ./build/test_mdviewer
```

All tests must pass before committing.

## Adding features — strict TDD cycle

Every piece of new functionality follows this exact order. No exceptions.

1. **Run the suite green.** Confirm all existing tests pass before touching anything.
2. **Write a failing test.** Add the test that exercises the new behaviour. Run the suite — it must fail on the new case and pass on all others.
3. **Implement.** Write the minimum code to make the new test pass.
4. **Run the suite green again.** All tests — old and new — must pass.
5. **Build clean.** `bash build.sh` — zero warnings expected.

Never write implementation code before a failing test exists for it. Never skip to step 5 to "just check it compiles."

## Code organisation and separation of concerns

Split by concern — a new concern gets a new file, not more lines in an existing one.

| File | Owns |
|---|---|
| `html_template.h/cpp` | `WrapWithTemplate`, CSS/JS generation |
| `markdown.h/cpp` | `RenderMarkdown`, `ProcessInline`, `EscapeHTML` |
| `mdviewer.h/cpp` | Frame class: constructor, event table, file I/O |
| `creator.h/cpp` | Prompt building, LLM backend dispatch, file saving |
| `project.h/cpp` | Project folder layout, per-project config, chapter/tidbit index |
| `editor.h/cpp` | Chapter and tidbit patch operations (re-generate, swap character) |

When a function doesn't clearly belong to an existing module, make a new one rather than stretching an existing file. If you are unsure, ask.

Headers are the public interface. A reader should understand a module by reading its `.h` file alone — keep them free of implementation detail.

## C++ style

- Prefer free functions over member functions when a function doesn't need `this`.
- No raw `new`/`delete` — use `wxWeakRef`, stack allocation, or smart pointers.
- Raw string literals (`R"HTML(...)HTML"`) must not be broken mid-tag. If you need to splice a runtime value in, end the literal at a clean boundary (end of attribute value, end of line).
- Keep functions under ~50 lines. If a function is growing, extract a named helper.

## Content creation features (in progress)

The app is being extended with a **Create** tab for LLM-assisted markdown generation. Key design decisions:

- **Project folders** — each project lives in its own directory containing `claude.md` (project-level prompt context), the generated chapter `.md` files, and an index file tracking numeric IDs.
- **Numeric IDs** — every chapter and every tidbit block carries a short numeric identifier (e.g. `ch:3`, `tb:7`) embedded as an HTML comment in the rendered source. These IDs are stable across edits.
- **Edit tab** — the user pastes an ID and a plain-English instruction ("make this friendlier", "swap character for Einstein"). The app looks up the block, builds a patch prompt, calls the LLM, and splices the result back into the source file.
- **LLM backends** — `claude -p` (shell out), Anthropic API (key in config), Ollama (local HTTP), or clipboard (build prompt and copy, user pastes manually).
- **Tidbit characters** — the Create form lets the user pick characters; personas are passed in the prompt alongside the `--llm` syntax reference so the LLM produces valid tidbit blocks.

New tests for these features live in `tests/test_creator.cpp`, `tests/test_project.cpp`, and `tests/test_editor.cpp`.

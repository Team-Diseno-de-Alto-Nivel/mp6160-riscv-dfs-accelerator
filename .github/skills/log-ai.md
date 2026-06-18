---
name: log-ai
description: Append a row to the AI-Assisted Development table in README.md to declare AI usage as required by course policy.
argument-hint: "<Model> | <Type of use> | <Prompt description>"
user-invocable: true
---

Append a new row to the AI-Assisted Development table in README.md.

Arguments: $ARGUMENTS

Parse the arguments as pipe-separated values: "Model | Type of use | Prompt"

If arguments are missing or incomplete, ask the user for:
1. **Model** — exact model name (e.g., `Claude Sonnet 4.6`, `GitHub Copilot`)
2. **Type of use** — one of: `concept lookup`, `code generation`, `documentation generation`, `diagram generation`, `code review`, `debugging`, `writing improvement`
3. **Prompt** — brief description of what was asked (if it was a base template, say so explicitly)

Then append a new row to the markdown table under `## AI-Assisted Development` in README.md:

```
| <Model> | <Type of use> | <Prompt> |
```

Match the existing row format exactly. Do not modify any other part of the file.

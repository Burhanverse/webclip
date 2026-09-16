# Agent Instructions & Coding Standards

## Core Principles

- **Code Cleanliness & Neatness**:
  - Keep code clean, modular, and organized.
  - Follow idiomatic formatting and modern conventions for the language and framework in use (C++, Qt, CMake).
  - Use clear, descriptive, and consistent naming conventions for classes, methods, variables, and files.
  - Keep functions focused and concise with a single responsibility.

- **Strictly No Comments**:
  - Do not add any comments to code files under any circumstances.
  - Avoid inline comments, block comments, documentation comments/docstrings, and header comments in source files.
  - Do not keep commented-out code; delete unused code entirely.
  - Write self-documenting code through expressive naming, proper encapsulation, and intuitive control flow.

- **Formatting & Consistency**:
  - Maintain consistent indentation, spacing, and bracket placement across all files.
  - Keep imports and includes sorted and organized logically.
  - Remove trailing whitespace and ensure files end with a newline.

- **Quality & Safety**:
  - Follow modern C++ best practices (RAII, smart pointers, const correctness).
  - Implement robust error handling without cluttering business logic.
  - Ensure any new code builds cleanly and integrates seamlessly with existing project structures.

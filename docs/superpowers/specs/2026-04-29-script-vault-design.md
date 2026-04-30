# Design Spec: Persistent Script Vault & TUI Manager for Gemini CLI

**Date:** 2026-04-29
**Topic:** Persistent Script Saving Feature
**Status:** Draft

## 1. Problem Statement
Generated code and scripts in Gemini CLI are currently ephemeral. Users often need to reuse a specific snippet or script later but have no built-in way to save it with context (category, name) and browse it easily within the CLI.

## 2. Proposed Solution
Introduce a "Vault" system that allows users to manually save generated outputs into a structured local directory and browse them using a Terminal UI (TUI).

## 3. Key Features
- **Manual Triggering:**
  - A new command `/save` or a flag `--save` to trigger the saving of the most recent output.
  - Interactive prompt for a category/folder name if not provided.
- **Structured Storage:**
  - Default storage location: `~/.gemini/vault/` (configurable).
  - Subdirectories based on user-defined categories.
  - Files saved with timestamp and optional descriptive name.
- **TUI Management Browser:**
  - A new command `/vault` or `/saved` to open the TUI.
  - Search/Filter by category or filename.
  - Syntax highlighting for saved code snippets.
  - Quick actions: `copy-to-clipboard`, `insert-to-terminal`, `delete`.

## 4. User Experience (UX)
1. User generates a complex script.
2. User types `/save utility`.
3. CLI confirms: `Saved to ~/.gemini/vault/utility/2026-04-29-script-name.sh`.
4. Later, user types `/vault`, browses to "utility", finds the script, and selects "Insert" to use it again.

## 5. Technical Considerations
- **TUI Library:** Recommend using `bubbletea` (Go) or `tview` depending on the core language of Gemini CLI (Node.js/Go). *Note: Gemini CLI is primarily Node.js based, so a library like `blessed` or `ink` might be appropriate.*
- **Storage Format:** Simple file system structure for transparency, with an optional `metadata.json` for enhanced search.

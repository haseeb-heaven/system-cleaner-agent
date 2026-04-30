# Persistent Script Vault Implementation Plan

**Goal:** Implement a script saving and management system (`/save` and `/vault` commands) in `gemini-cli`.

**Architecture:**
- Create `VaultService` in `packages/core/src/services/` for file system operations.
- Add `/save` and `/vault` commands to `packages/cli/src/ui/commands/`.
- Add `VaultView` in `packages/cli/src/ui/components/views/`.
- Register commands in `BuiltinCommandLoader`.

---

### Task 1: Create VaultService
- [ ] Create `packages/core/src/services/vaultService.ts` to manage `~/.gemini/vault/`.
- [ ] Implement `saveScript(content: string, category: string, name: string)` method.
- [ ] Implement `listVaultItems()` method.
- [ ] Add unit tests in `packages/core/src/services/vaultService.test.ts`.

### Task 2: Implement CLI Commands
- [ ] Create `packages/cli/src/ui/commands/saveCommand.ts` (for `/save`).
- [ ] Create `packages/cli/src/ui/commands/vaultCommand.ts` (for `/vault`).
- [ ] Update `packages/cli/src/services/BuiltinCommandLoader.ts` to register new commands.

### Task 3: Develop Vault UI
- [ ] Create `packages/cli/src/ui/components/views/VaultView.tsx` using `Ink` for TUI.
- [ ] Integrate with `VaultService` to list and manage saved scripts.

### Task 4: Integration & Verification
- [ ] Add E2E tests in `packages/cli/src/integration-tests/vault.test.ts`.
- [ ] Run `npm run preflight`.

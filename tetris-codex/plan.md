# Next Enhancement Plan: Gameplay and In-Game UX Polish

## Summary

The stability pass is complete: the project now has stronger UEFI startup checks,
a non-RWX linker layout, `make check`, and QEMU documentation. The next best
enhancement is to improve gameplay clarity and player feedback without changing
the UEFI target or adding external dependencies.

This pass should add a visible next-piece preview, a one-piece hold slot for each
player, clearer per-player status text, and safer input handling around the new
actions. Keep the existing two-player split-screen game and garbage rule intact.

## Implementation Changes

1. Add hold-piece state to each `Player`.
   - Track held piece type and whether hold was already used for the current
     falling piece.
   - Add a new `ActionHold` input action.
   - Player 1 uses `Enter` for hold.
   - Player 2 uses `G` for hold.
   - Holding swaps the current piece with the held piece; if no piece is held,
     store the current piece and spawn the next piece.
   - Reset rotation and spawn position after hold.
   - Allow only one hold per locked piece.

2. Render next and hold previews.
   - Draw compact 4x4 preview boxes beside each board using the existing GOP
     rectangle drawing helpers.
   - Show both `Next` and `Hold` for each player.
   - If no held piece exists, draw an empty hold box.
   - Keep layout responsive using the existing `cell_size()` approach and avoid
     drawing outside the current GOP resolution.

3. Improve status text.
   - Include hold controls in the existing text HUD.
   - Display each player's held/next state visually rather than only in text.
   - Preserve the existing pause, restart, quit, auto-play, score, and lines
     messages.

4. Keep behavior compatibility.
   - Do not change scoring, fall speed, garbage rows, auto-play toggle, or
     existing controls.
   - Auto-play may ignore hold in this pass; it should continue using the
     current and next-piece flow without crashing or corrupting hold state.
   - Restart must clear boards, score, line count, game-over state, next piece,
     and hold state.

## Important Files

- `src/main.c`: add hold state, input mapping, hold mechanics, preview rendering,
  and HUD text updates.
- `README.md`: document the new hold controls and preview behavior.
- `Makefile` / `uefi.lds`: no expected changes unless validation exposes a build
  issue.

## Test Plan

1. Run `make check`.
2. Verify the generated EFI remains a PE32+ EFI application with `.reloc`.
3. Run in QEMU/OVMF and smoke test:
   - Both boards render.
   - Next preview appears for both players.
   - Hold preview starts empty for both players.
   - Player 1 can hold with `Enter`.
   - Player 2 can hold with `G`.
   - A player cannot hold twice before locking a piece.
   - Hold resets after the piece locks.
   - Pause, restart, quit, auto-play toggle, hard drop, soft drop, movement, and
     rotation still work.
   - Restart clears held pieces and previews.

## Assumptions

- The next enhancement should prioritize user-visible gameplay polish over a
  large module split.
- The project remains freestanding C99 for UEFI Shell.
- No new third-party dependencies are introduced.
- The existing two-player local battle mode remains the primary mode.

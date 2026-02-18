# TUI Implementation Options - Difficulty Assessment

## Overview

User requested: Split-screen TUI with header bar, main game area, ASCII art sidebar, inventory, journey map, and footer input.

We evaluated 4 approaches ordered by difficulty (easiest first):

---

## Option 4: TUI Wrapper Around Existing Dumb Interface ⭐ RECOMMENDED
**Difficulty**: 2/10 (Easiest!)
**Time Estimate**: 4-6 hours
**Risk**: Low

### Why This Is Best

We ALREADY capture all game output via `output_capture_add()` in `src/llm/output_capture.c`. We just need to change where it displays!

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ TUI Layer (src/tui/split_screen.c - NEW, ~300 lines)       │
│                                                             │
│  ┌─────────────────────────────────┬──────────────────────┐│
│  │ Header: LLM Status              │                      ││
│  ├─────────────────────────────────┼──────────────────────┤│
│  │ Game Output Window              │ ASCII Art            ││
│  │ (scrolling, 70%)                │ Inventory            ││
│  │                                 │ Journey Map          ││
│  │ Receives text from:             │ (30%)                ││
│  │ • tui_display_text()            │                      ││
│  │ • Called by output_capture_add()│                      ││
│  ├─────────────────────────────────┴──────────────────────┤│
│  │ Footer: Input                                          ││
│  └────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
                          ↑
                          │ tui_display_text(text)
                          │
┌─────────────────────────┴───────────────────────────────────┐
│ Dumb Interface (UNCHANGED - proven, 100 lines)             │
│                                                             │
│  os_display_string() → output_capture_add() → tui_display()│
└─────────────────────────────────────────────────────────────┘
```

### Implementation Steps

1. **Create `src/tui/split_screen.c`** (~300 lines):
   ```c
   // Public API
   void tui_init(void);                    // Setup ncurses layout
   void tui_display_text(const char *s);   // Add text to game window
   void tui_update_header(void);           // Refresh LLM status
   void tui_update_sidebar(void);          // Refresh art/inv/journey
   char* tui_read_input(void);             // Get command from footer
   void tui_shutdown(void);                // Cleanup
   ```

2. **Modify `src/zmachine/frotz/src/dumb/doutput.c`** (10 lines):
   ```c
   void os_display_string(const zchar *s) {
       // ... existing code ...
       output_capture_add(converted);

       // OLD: fprintf(stdout, "%s", converted);
       // NEW: tui_display_text(converted);  // ← Only change!
   }
   ```

3. **Modify `src/zmachine/frotz/src/dumb/dinit.c`** (5 lines):
   ```c
   void os_init_screen(void) {
       // OLD: (no UI setup needed)
       // NEW: tui_init();  // ← Setup ncurses layout
   }
   ```

4. **Modify `src/journey/monitor.c`** (3 lines):
   ```c
   void on_location_changed(void) {
       // ... existing journey tracking ...
       tui_update_sidebar();  // ← Refresh sidebar
   }
   ```

### Benefits

✅ **Minimal changes**: Only ~20 lines of modifications to existing code
✅ **Proven core**: Keep dumb interface (100 lines) vs modifying curses (5000 lines)
✅ **Clean separation**: TUI is a separate module, easy to test
✅ **Easy rollback**: Can toggle TUI on/off with environment variable
✅ **Output already captured**: Leverage existing output_capture system
✅ **Fast to implement**: 4-6 hours vs 2-3 days

### Risks

⚠️ **Terminal compatibility**: ncurses should handle this automatically
⚠️ **Unicode/emoji**: Use ncursesw (wide char support)
⚠️ **Z-machine styles**: Dumb interface doesn't have bold/italic (acceptable tradeoff)

### Testing

```bash
# Build with TUI
./scripts/build_local.sh

# Run with TUI (default)
./zork-native game/zork1.z3

# Disable TUI for debugging
ZORK_TUI_DISABLE=1 ./zork-native game/zork1.z3
```

---

## Option 3: Hybrid - PTY Wrapper
**Difficulty**: 4/10
**Time Estimate**: 1-2 days
**Risk**: Medium

Run dumb interface in pseudo-terminal, capture output, display in custom TUI.

### Architecture
```
Custom TUI → PTY → zork-native (dumb)
   ↑                    ↓
   └─── parse output ───┘
```

### Benefits
✅ Zero changes to Frotz
✅ Use modern TUI library (notcurses, ratatui)

### Drawbacks
❌ Need PTY management (complex)
❌ Need to parse text output to detect events
❌ More code (~500 lines)

---

## Option 2: Standalone TUI Program
**Difficulty**: 5/10
**Time Estimate**: 2-3 days
**Risk**: Medium

Build separate TUI program that communicates with game via IPC.

### Drawbacks
❌ IPC complexity
❌ Two processes to manage
❌ More code (~800 lines)

---

## Option 1: Modify Frotz Curses Interface
**Difficulty**: 7/10
**Time Estimate**: 2-3 days
**Risk**: High

Modify Frotz's existing curses interface (5,324 lines).

### What's Involved
- Understand 5,324 lines of curses code
- Redirect 24 stdscr references to custom windows
- Modify ux_text.c, ux_init.c, ux_screen.c
- Risk breaking Z-machine features (colors, scrolling, styles)

### Why This Is Hard
❌ Curses interface uses implicit `stdscr` (no window parameter)
❌ All text output uses `addch()` which writes to stdscr
❌ Complex resize handling, color management
❌ Need to preserve Z-machine display semantics

### Benefits
✅ Full Z-machine feature support (colors, styles)

### Why Not Recommended
We don't need Z-machine v5/v6 features (graphics, colors, styles).
Zork I is v3 (basic text only).
Dumb interface handles v3 perfectly in 100 lines.

---

## Recommendation: Option 4

**Go with Option 4** - TUI wrapper around existing dumb interface.

**Rationale**:
1. We already capture output (leverage existing code)
2. Dumb interface is proven and simple (100 lines)
3. Minimal changes (~20 lines)
4. Fast to implement (4-6 hours)
5. Easy to test and rollback
6. Zork I doesn't need curses features

**Trade-off**:
- Lose Z-machine v5/v6 features (colors, bold, italic)
- **Acceptable**: Zork I is v3 (basic text only)
- **Gain**: Simplicity, maintainability, speed

**Next Steps**:
1. Create `src/tui/split_screen.c` - ncurses layout manager
2. Create `src/tui/split_screen.h` - public API
3. Modify doutput.c - redirect output to TUI (1 line change)
4. Modify dinit.c - initialize TUI (1 line change)
5. Modify monitor.c - update sidebar on events (1 line change)
6. Update build script - add TUI sources, link ncurses
7. Test with LLM features enabled

---

## FAQ

**Q: Why not use Frotz curses?**
A: It's 5,324 lines vs 100 lines for dumb. We don't need v5/v6 features. Simpler is better.

**Q: What about colors and styles?**
A: Zork I (v3) doesn't use them. If we later want colors for UI chrome (header, sidebar), we can add them in our TUI layer.

**Q: What if we want to play v5/v6 games later?**
A: Option 1 (modify Frotz curses) becomes worth it then. For Zork I, overkill.

**Q: Can we toggle TUI on/off?**
A: Yes! Use environment variable `ZORK_TUI_DISABLE=1` to fall back to plain output.

**Q: What about terminal compatibility?**
A: ncurses handles 99% of terminals automatically. We'll test on xterm, gnome-terminal, iTerm2.

---

*Assessment created: 2026-02-18*
*Recommended approach: Option 4 (TUI Wrapper)*

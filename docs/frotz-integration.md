# Frotz Integration Notes

## Overview

We're integrating Frotz 2.55 as the Z-machine interpreter core for this project.

## Frotz Architecture

### Core Components
- **src/common/**: Z-machine implementation (platform-independent)
  - `main.c` - Main interpreter loop
  - `process.c` - Instruction processing
  - `text.c` - Text handling
  - `object.c` - Object system
  - `files.c` - Save/restore
  - And many more...

- **src/dumb/**: Simple terminal interface (our base)
  - `dinit.c` - Initialization
  - `dinput.c` - Input handling
  - `doutput.c` - Output handling
  - `dfrotz.h` - Interface definitions

### Required OS Functions

Frotz requires platform-specific implementations of OS functions defined in `frotz.h`:

**Initialization**:
- `os_init_setup()` - Initialize platform
- `os_process_arguments()` - Handle command line args

**Display**:
- `os_display_char()` - Display a single character
- `os_display_string()` - Display a string
- `os_erase_area()` - Clear screen area
- `os_set_cursor()` - Position cursor
- `os_set_colour()` - Set colors
- `os_set_font()` - Set font
- `os_set_text_style()` - Set text style (bold, italic, etc.)

**Input**:
- `os_read_line()` - Read line of input
- `os_read_key()` - Read single key
- `os_read_mouse()` - Mouse input (can be stubbed)

**Files**:
- `os_read_file_name()` - File picker dialog
- `os_path_open()` - Open file

**Sound/Graphics** (can be stubbed for text-only):
- `os_init_pictures()` - Initialize graphics
- `os_picture_data()` - Load picture
- `os_draw_picture()` - Draw picture
- `os_start_sample()` - Play sound
- `os_stop_sample()` - Stop sound

## Integration Strategy

### Phase 1: Native Build (Current)

1. **Use dumb interface as reference**
   - Copy dumb interface files to `src/io/`
   - Modify for our I/O abstraction

2. **Build configuration**
   - Compile common/ core
   - Link with our I/O layer
   - Native stdio for testing

3. **Minimal feature set**
   - Text input/output only
   - No graphics or sound
   - Save/restore functionality
   - Focus on Z-machine version 3 (Zork I)

### Phase 2: TT-Metal Build

1. **TT-Metal I/O layer**
   - Implement same I/O interface
   - Use TT-Metal host APIs
   - RISC-V core execution

2. **Cross-compilation**
   - RISC-V toolchain
   - Link with TT-Metal SDK

## Build System Integration

### Directory Structure
```
src/
├── zmachine/
│   ├── frotz/          # Frotz source (submodule/clone)
│   └── main.c          # Our main() that calls Frotz
├── io/
│   ├── io.h            # I/O abstraction API
│   ├── io_native.c     # Native implementation
│   └── io_ttmetal.c    # TT-Metal implementation
└── parser/
    └── ...             # Future parser abstraction
```

### Compilation Strategy

**Native build** (`scripts/build_local.sh`):
```makefile
# Common core
COMMON_SRC = frotz/src/common/*.c
# Our I/O layer
IO_SRC = src/io/io_native.c
# Main
MAIN_SRC = src/zmachine/main.c

gcc -DBUILD_NATIVE \
    -I src/zmachine/frotz/src/common \
    -I src/io \
    $COMMON_SRC $IO_SRC $MAIN_SRC \
    -o zork-native
```

**RISC-V build** (`scripts/build_riscv.sh`):
```makefile
riscv64-gcc -DBUILD_TT_METAL \
    -I $TT_METAL_HOME/tt_metal \
    -I src/zmachine/frotz/src/common \
    -I src/io \
    $COMMON_SRC src/io/io_ttmetal.c $MAIN_SRC \
    -o zork-riscv
```

## Testing Strategy

1. **Verify Frotz core**
   - Build dumb Frotz standalone
   - Test with zork1.z3
   - Ensure it works correctly

2. **Integrate with our build**
   - Replace placeholder main.c
   - Compile Frotz common core
   - Test native build

3. **I/O abstraction**
   - Verify all OS functions work
   - Test save/restore
   - Test full gameplay

## Next Steps

1. Create `src/io/io.h` - I/O abstraction API
2. Create `src/io/io_native.c` - Native implementation
3. Update `src/zmachine/main.c` to call Frotz properly
4. Update `scripts/build_local.sh` to compile Frotz core
5. Test with Zork I gameplay

## References

- Frotz source: `src/zmachine/frotz/`
- Z-machine spec: https://www.ifwiki.org/Z-machine
- Frotz docs: `src/zmachine/frotz/doc/`

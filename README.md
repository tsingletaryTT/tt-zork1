# Zork on Tenstorrent

**A novel port of Zork I to run on Tenstorrent AI accelerators with LLM-based natural language parsing**

This project ports the classic interactive fiction game Zork I to run on Tenstorrent Blackhole/Wormhole hardware, using a hybrid architecture where:
- **RISC-V cores** run the Z-machine interpreter and game logic
- **Tensix cores** run LLM inference for natural language understanding
- **TT-Metal** provides the low-level programming framework

## Quick Start

### Native Build (Working!)
```bash
# Build for local testing (macOS, Linux)
./scripts/build_local.sh

# Play Zork!
./zork-native game/zork1.z3
```

### RISC-V Cross-Compilation (Build System Ready!)
```bash
# Install RISC-V toolchain first (see docs/RISCV_SETUP.md)
# macOS: brew tap riscv-software-src/riscv && brew install riscv-gnu-toolchain
# Ubuntu: sudo apt-get install gcc-riscv64-linux-gnu

# Build for Tenstorrent RISC-V cores
./scripts/build_riscv.sh

# Test on QEMU
qemu-riscv64 ./zork-riscv game/zork1.z3
```

### Hardware Deployment (Coming in Phase 1.4)
```bash
# Deploy to Wormhole/Blackhole (requires TT-Metal SDK)
./scripts/deploy.sh [wormhole|blackhole]
```

## Project Structure

```
tt-zork1/
├── src/
│   ├── zmachine/      # Frotz-based Z-machine interpreter
│   ├── parser/        # Parser abstraction & LLM integration
│   ├── io/            # I/O abstraction layer
│   └── kernels/       # TT-Metal kernels for inference
├── game/
│   └── zork1.z3       # Compiled Z-machine bytecode
├── scripts/
│   ├── build_local.sh # Native build for testing
│   ├── build_riscv.sh # Cross-compile for Tenstorrent
│   └── deploy.sh      # Deploy to hardware
├── docs/
│   └── architecture.md # Detailed architecture documentation
└── tests/
    └── integration/   # Test suite
```

## Documentation

- **[Architecture Documentation](docs/architecture.md)** - Detailed technical architecture
- **[Phase 1.2 Complete](docs/PHASE_1_2_COMPLETE.md)** - Frotz integration with UTF-8 support
- **[RISC-V Setup Guide](docs/RISCV_SETUP.md)** - Cross-compilation instructions
- **[Phase 1.3 Ready](docs/PHASE_1_3_READY.md)** - RISC-V build system documentation
- **[Implementation Plan](.claude/plans/)** - Original phased plan
- **[Original Zork Source](#zork-i-source-code-collection)** - Historical Zork source information

## Development Status

**Phase 1.2: Frotz Integration** ✅ COMPLETE
- ✅ Frotz 2.56pre dumb interface integration
- ✅ UTF-8 support enabled
- ✅ Native build working perfectly (macOS, Linux ready)
- ✅ Full Zork I gameplay functional
- ✅ ~220KB binary, 3-4 second build time

**Phase 1.3: RISC-V Cross-Compilation** ✅ BUILD SYSTEM READY
- ✅ build_riscv.sh script complete
- ✅ Auto-detection of RISC-V toolchains
- ✅ RV64IMAC architecture targeting
- ✅ UTF-8 support
- ✅ Static linking for bare-metal
- ✅ Comprehensive documentation
- ⏳ Awaiting toolchain installation for testing

**Phase 1.4: Hardware Deployment** (Next)
- ⏳ TT-Metal I/O layer implementation
- ⏳ RISC-V loader for Tenstorrent cores
- ⏳ Hardware testing on Wormhole/Blackhole

**Phase 2: Hybrid Architecture** (Future)
- ⏳ Parser abstraction layer
- ⏳ Message passing between RISC-V and Tensix
- ⏳ LLM inference integration points

**Phase 3: LLM Natural Language Parser** (Future)
- ⏳ Model selection and quantization
- ⏳ Training data generation
- ⏳ TT-Metal inference kernels
- ⏳ Hybrid classic/LLM parser

## Resources

- [Frotz Z-machine Interpreter](https://gitlab.com/DavidGriffith/frotz)
- [TT-Metal Documentation](https://github.com/tenstorrent/tt-metal)
- [Z-machine Specification](https://www.ifwiki.org/Z-machine)

---

# Zork I Source Code Collection

Zork I is a 1980 interactive fiction game written by Marc Blank, Dave Lebling, Bruce Daniels and Tim Anderson and published by Infocom.

Further information on Zork I:

* [Wikipedia](https://en.wikipedia.org/wiki/Zork_I)
* [The Digital Antiquarian](https://www.filfre.net/2012/01/selling-zork/)
* [The Interactive Fiction Database](https://ifdb.tads.org/viewgame?id=0dbnusxunq7fw5ro)
* [The Infocom Gallery](http://gallery.guetech.org/zork1/zork1.html)
* [IFWiki](http://www.ifwiki.org/index.php/Zork_I)

__What is this Repository?__

This repository is a directory of source code for the Infocom game "Zork I", including a variety of files both used and discarded in the production of the game. It is written in ZIL (Zork Implementation Language), a refactoring of MDL (Muddle), itself a dialect of LISP created by MIT students and staff.

The source code was contributed anonymously and represents a snapshot of the Infocom development system at time of shutdown - there is no remaining way to compare it against any official version as of this writing, and so it should be considered canonical, but not necessarily the exact source code arrangement for production.

__Basic Information on the Contents of This Repository__

It is mostly important to note that there is currently no known way to compile the source code in this repository into a final "Z-machine Interpreter Program" (ZIP) file using an official Infocom-built compiler. There is a user-maintained compiler named [ZILF](http://zilf.io) that has been shown to successfully compile these .ZIL files with minor issues. There are .ZIP files in some of the Infocom Source Code repositories but they were there as of final spin-down of the Infocom Drive and the means to create them is currently lost.

Throughout its history, Infocom used a TOPS20 mainframe with a compiler (ZILCH) to create and edit language files - this repository is a mirror of the source code directory archive of Infocom but could represent years of difference from what was originally released.

In general, Infocom games were created by taking previous Infocom source code, copying the directory, and making changes until the game worked the way the current Implementor needed. Structure, therefore, tended to follow from game to game and may or may not accurately reflect the actual function of the code.

There are also multiple versions of the "Z-Machine" and code did change notably between the first years of Infocom and a decade later. Addition of graphics, sound and memory expansion are all slowly implemented over time.

__What is the Purpose of this Repository__

This collection is meant for education, discussion, and historical work, allowing researchers and students to study how code was made for these interactive fiction games and how the system dealt with input and processing.

Researchers are encouraged to share their discoveries about the information in this source code and the history of Infocom and its many innovative employees.


### So it begins...

```bash
export TT_METAL_HOME=/home/ttuser/tt-metal && g++ -std=c++17 -O3 -I$TT_METAL_HOME/tt_metal/include -L$TT_METAL_HOME/build/lib -ltt_metal zork_on_blackhole.cpp -o zork_on_blackhole
```
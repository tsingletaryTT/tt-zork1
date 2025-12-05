# Zork on Tenstorrent

**A novel port of Zork I to run on Tenstorrent AI accelerators with LLM-based natural language parsing**

This project ports the classic interactive fiction game Zork I to run on Tenstorrent Blackhole/Wormhole hardware, using a hybrid architecture where:
- **RISC-V cores** run the Z-machine interpreter and game logic
- **Tensix cores** run LLM inference for natural language understanding
- **TT-Metal** provides the low-level programming framework

## Quick Start

### Local Development (Native Build)
```bash
# Build for local testing
./scripts/build_local.sh

# Run Zork
./zork-native game/zork1.z3
```

### Hardware Deployment
```bash
# On Wormhole/Blackhole system after git pull
./scripts/build_riscv.sh release
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
- **[Implementation Plan](.claude/plans/)** - Phased implementation plan
- **[Original Zork Source](#zork-i-source-code-collection)** - Historical Zork source information below

## Development Status

**Phase 1: Foundation** (In Progress)
- ✅ Repository structure and build system
- 🔄 Frotz interpreter integration
- ⏳ I/O abstraction layer
- ⏳ Hardware deployment

**Phase 2: Hybrid Architecture** (Not Started)
- ⏳ Parser abstraction
- ⏳ Inference integration points

**Phase 3: LLM Inference** (Not Started)
- ⏳ Model selection and training
- ⏳ TT-Metal inference kernels

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

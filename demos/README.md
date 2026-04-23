# Demo Scripts

Four asciinema recordings showing the Zork on Tenstorrent arc.

## Prerequisites

    pip install asciinema
    # For QB2 demos:
    source ~/code/tt-lang/build/env/activate
    # For remix demos:
    ollama pull qwen2.5:1.5b
    ollama pull qwen2.5:7b

## Recording

    asciinema rec -t "Zork — Stage 1" demos/stage1.cast -- bash demos/demo-stage1.sh
    asciinema rec -t "Zork — Stage 2" demos/stage2.cast -- bash demos/demo-stage2.sh
    asciinema rec -t "Zork — Stage 3" demos/stage3.cast -- bash demos/demo-stage3.sh
    asciinema rec -t "Zork — Stage 4" demos/stage4.cast -- bash demos/demo-stage4.sh

## Playback

    asciinema play demos/stage1.cast
    asciinema play demos/stage4.cast

## Upload (optional)

    asciinema upload demos/stage4.cast

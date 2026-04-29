# Demo Recordings

Eight asciinema cast files — two per demo: the game session and the hardware visualization. The hybrid and ai casts are generated when you run `./demos/record-all.sh` (the server starts automatically after the device demo).

## Cast files

| File | What it shows |
|------|---------------|
| `stage1.cast` | Pure Python Z-machine — no hardware |
| `stage1-hw.cast` | Chip idle — Tensix and RISC-V at rest |
| `stage2.cast` | Zork game file uploaded to QB2 DRAM, Python interpreter on host |
| `stage2-hw.cast` | DRAM traffic — 86 KB game file lives on-chip |
| `hybrid.cast` | LLM remix layer — Llama-3.3-70B rewrites every response |
| `hybrid-hw.cast` | Tensix cores hot — LLM inference load in arcade mode |
| `ai.cast` | AI auto-play — experimental persona, 30 turns, full TUI |
| `ai-hw.cast` | Tensix cores at full load for the entire AI session |

The `-hw.cast` files run `tt-toplike -m arcade` for the full duration of each demo.
They tell the hardware story and double as wall-clock timers.

## Recording all demos

```bash
./demos/record-all.sh              # all four demos in order (server auto-starts between stage2 and hybrid)
./demos/record-all.sh stage1       # re-record just Stage 1
./demos/record-all.sh hybrid ai    # re-record the two LLM demos (server must already be running)
```

### LLM demos (hybrid and ai)

When running `./demos/record-all.sh` in sequence, the inference server starts automatically after the stage2 demo resets the hardware. Allow ~3 minutes for the model to warm up — the script polls until it's ready.

To re-record only the LLM demos when the server is already running:

```bash
./demos/record-all.sh hybrid ai
```

### Recording a single demo manually

```bash
# In terminal A — hardware visualization:
asciinema rec --overwrite --cols 160 --rows 42 \
  -t "Zork — Stage 1 hardware" \
  -c "tt-toplike -m arcade --json -q" \
  demos/stage1-hw.cast

# In terminal B — game demo (start ~2 seconds after terminal A):
asciinema rec --overwrite -t "Zork — Stage 1" \
  -c "bash demos/demo-stage1.sh" demos/stage1.cast
# Ctrl-C terminal A when the demo finishes.

# For the AI demo (full TUI — wider terminal):
asciinema rec --overwrite --cols 160 --rows 40 \
  -t "Zork — AI Player" \
  -c "bash demos/demo-ai.sh" demos/ai.cast
```

## Playback

```bash
asciinema play demos/ai.cast
asciinema play demos/hybrid.cast

# Side-by-side in tmux:
tmux new-session \; \
  send-keys "asciinema play demos/ai.cast" Enter \; \
  split-window -h \; \
  send-keys "asciinema play demos/ai-hw.cast" Enter
```

## Upload (optional)

```bash
asciinema upload demos/ai.cast
asciinema upload demos/hybrid.cast
```

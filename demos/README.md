# Demo Recordings

Ten asciinema cast files — two per demo: the game session and the hardware visualization.

## Cast files

| File | What it shows |
|------|---------------|
| `stage1.cast` | Pure Python Z-machine — no hardware |
| `stage1-hw.cast` | Chip idle — Tensix and RISC-V at rest |
| `stage2.cast` | Zork game file uploaded to QB2 DRAM, Python interpreter on host |
| `stage2-hw.cast` | DRAM traffic — 86 KB game file lives on-chip |
| `stage3.cast` | Z-machine interpreter running on Blackhole RISC-V cores |
| `stage3-hw.cast` | RISC-V cores lit, firmware watchdog wall explained |
| `hybrid.cast` | LLM remix layer — Llama-3.3-70B rewrites every response |
| `hybrid-hw.cast` | Tensix cores hot — LLM inference load in arcade mode |
| `ai.cast` | AI auto-play — experimental persona, 30 turns, full TUI |
| `ai-hw.cast` | Tensix cores at full load for the entire AI session |

The `-hw.cast` files run `tt-toplike -m arcade` for the full duration of each demo.
They tell the hardware story and double as wall-clock timers.

## Recording all demos

```bash
./demos/record-all.sh              # all five demos in order
./demos/record-all.sh stage1       # re-record just Stage 1
./demos/record-all.sh hybrid ai    # the two LLM demos
```

### LLM demo prerequisites (hybrid and ai)

tt-inference-server must be running with Llama-3.3-70B-Instruct on port 8000:

```bash
# Start Llama-3.3-70B on Tensix cores
cd ~/code/tt-inference-server
python run.py \
  --model meta-llama/Llama-3.3-70B-Instruct \
  --workflow server \
  --tt-device p300x2 \
  --docker-server \
  --skip-prerequisites \
  --no-auth \
  --service-port 8000

# Verify it's ready (~3 min warmup):
curl -s http://localhost:8000/v1/models | python3 -m json.tool

# Then record:
./demos/record-all.sh hybrid ai
```

### Recording a single demo manually

```bash
# In terminal A — hardware visualization:
COLUMNS=160 LINES=42 \
  asciinema rec --overwrite -t "Zork — Stage 1 hardware" \
    -c "tt-toplike -m arcade --json -q" \
    demos/stage1-hw.cast

# In terminal B — game demo (start ~2 seconds after terminal A):
asciinema rec --overwrite -t "Zork — Stage 1" \
  -c "bash demos/demo-stage1.sh" demos/stage1.cast
# Ctrl-C terminal A when the demo finishes.

# For the AI demo (full TUI — wider terminal):
COLUMNS=160 LINES=40 \
  asciinema rec --overwrite -t "Zork — AI Player" \
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

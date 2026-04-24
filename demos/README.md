# Demo Recordings

Eight asciinema cast files — two per stage: the game session and the hardware visualization.

## Cast files

| File | What it shows |
|------|---------------|
| `stage1.cast` | Pure Python Z-machine, no hardware |
| `stage1-hw.cast` | Chip idle — Tensix and RISC-V at rest |
| `stage2.cast` | Zork game file uploaded to QB2 DRAM, Python interpreter on host |
| `stage2-hw.cast` | DRAM traffic — 86 KB game file lives on-chip |
| `stage3.cast` | Z-machine interpreter running on Blackhole RISC-V cores |
| `stage3-hw.cast` | RISC-V cores lit, firmware watchdog wall explained |
| `stage4.cast` | LLM remix layer — Llama-3.1-8B rewrites every response |
| `stage4-hw.cast` | Tensix cores hot — the LLM inference load in arcade mode |

The `-hw.cast` files run `tt-toplike -m arcade` for the full duration of each stage.
They tell the hardware story and double as wall-clock timers for each stage.

## Recording all stages

```bash
./demos/record-all.sh          # all four stages
./demos/record-all.sh 1        # re-record just Stage 1
./demos/record-all.sh 4        # re-record Stage 4 (LLM server must be running)
```

### Stage 4 prerequisite

tt-inference-server must be running on port 8001 before recording Stage 4:

```bash
# Start Llama-3.1-8B on Tensix cores (all 4 Blackhole chips)
cd ~/code/tt-inference-server
python run.py \
  --model meta-llama/Llama-3.1-8B-Instruct \
  --workflow server \
  --tt-device p300x2 \
  --docker-server \
  --skip-prerequisites \
  --no-auth \
  --service-port 8001 \
  --override-docker-image ghcr.io/tenstorrent/tt-inference-server/vllm-tt-metal-src-release-ubuntu-22.04-amd64:qb2_launch-555f240-22be241

# Verify it's ready (~3 min warmup):
curl -s http://localhost:8001/v1/models | python3 -m json.tool

# Then record:
./demos/record-all.sh 4
```

### Recording a single stage manually

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
```

## Playback

```bash
asciinema play demos/stage4.cast
asciinema play demos/stage4-hw.cast

# Side-by-side in tmux:
tmux new-session \; \
  send-keys "asciinema play demos/stage4.cast" Enter \; \
  split-window -h \; \
  send-keys "asciinema play demos/stage4-hw.cast" Enter
```

## Upload (optional)

```bash
asciinema upload demos/stage4.cast
asciinema upload demos/stage4-hw.cast
```

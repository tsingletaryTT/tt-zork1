# Demo Recordings

Four asciinema cast files — one per demo.

## Cast files

| File | What it shows |
|------|---------------|
| `stage1.cast` | Pure Python Z-machine — no hardware |
| `stage2.cast` | Zork game file uploaded to QB2 DRAM, Python interpreter on host |
| `hybrid.cast` | LLM remix layer — Llama-3.3-70B rewrites every response |
| `ai.cast` | AI auto-play — experimental persona, 30 turns, full TUI |

## Recording all demos

```bash
./demos/record-all.sh              # all four demos in order (server auto-starts between stage2 and hybrid)
./demos/record-all.sh stage1       # re-record just Stage 1
./demos/record-all.sh hybrid ai    # re-record the two LLM demos (server must already be running)
```

### LLM demos (hybrid and ai)

When running `./demos/record-all.sh` in sequence, the inference server starts automatically after the stage2 demo. Allow ~3–6 minutes for the model to warm up — the script watches the server log for the ready signal and proceeds automatically.

To re-record only the LLM demos when the server is already running:

```bash
./demos/record-all.sh hybrid ai
```

### Recording a single demo manually

```bash
asciinema rec --overwrite --cols 120 --rows 35 \
  -t "Zork — Stage 1" \
  -c "bash demos/demo-stage1.sh" demos/stage1.cast

# For the AI demo (full TUI — wider terminal):
asciinema rec --overwrite --cols 160 --rows 40 \
  -t "Zork — AI Player" \
  -c "bash demos/demo-ai.sh" demos/ai.cast
```

## Playback

```bash
asciinema play demos/ai.cast
asciinema play demos/hybrid.cast
```

## Upload (optional)

```bash
asciinema upload demos/ai.cast
asciinema upload demos/hybrid.cast
```

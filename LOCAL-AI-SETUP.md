# Local coding AI on your machine (32 GB RAM)

Written 2026-11-12 so you can follow it tomorrow without me. Honest version
first, because everything else depends on it.

## What 32 GB of RAM can actually run

No local model equals a frontier Claude or GPT class model - not close, and
anyone selling you otherwise is selling. What 32 GB buys is the *best open
coding model class that fits in memory*, which is genuinely useful for:
autocomplete, explaining code, small refactors, drafting functions, offline
work, and anything you don't want leaving the machine. For multi-file work
with real verification loops (what I do here), keep using a hosted agent.

The model to beat in your RAM budget, still, is **Qwen2.5-Coder-32B-Instruct**
quantized to 4-bit (~20 GB loaded). Below it, **Qwen2.5-Coder-14B** (~9 GB)
runs 2-3x faster and is the sweet spot if the 32B feels sluggish. Mistral's
**Codestral-22B** is the other name worth knowing (great at fill-in-the-middle
completion; licence is free for personal use).

Speed reality on CPU only: 32B at 4-bit gives roughly 3-6 tokens/second on a
modern 8-core; 14B gives 8-15; 7B gives 20+. If you have an NVIDIA GPU with
16 GB+ VRAM the 32B fits on the GPU and flies; 12 GB VRAM runs the 14B on GPU.
Ollama splits CPU/GPU automatically - you don't configure anything.

## Option A - Ollama (recommended, least fuss)

1. Install: https://ollama.com/download (Windows installer, one click).
2. In a terminal:
   ```
   ollama pull qwen2.5-coder:32b-instruct-q4_K_M
   ```
   (~20 GB download once; close the browser while it runs, RAM matters).
   If that's too slow in use: `ollama pull qwen2.5-coder:14b-instruct-q4_K_M`.
3. Chat in the terminal: `ollama run qwen2.5-coder:32b-instruct-q4_K_M`.
4. It also serves an OpenAI-compatible API at `http://localhost:11434/v1`,
   which is what the editor plugins below talk to.

## Option B - LM Studio (if you want a GUI)

Install https://lmstudio.ai, search "Qwen2.5-Coder 32B Instruct GGUF", pick a
Q4_K_M file, click Load, chat. Same models as Ollama, friendlier picker, and
it shows tokens/second live so you can feel the CPU/GPU difference.

## Wiring it into your editor (where it earns its keep)

VS Code + **Continue** extension:
1. Install the "Continue" extension.
2. Config -> add model: provider `Ollama`, model `qwen2.5-coder:32b-instruct-q4_K_M`.
3. For autocomplete use a small fast model: `qwen2.5-coder:7b` (or `:14b` if
   the 7b suggestions feel weak); big models are too slow for tab-completion.
4. Now: Ctrl+L chat with context, Ctrl+I inline edits, Tab completions - all
   local, all offline.

Alternatives: **Cline** (VS Code, agent-style, point it at the Ollama API) or
**Aider** in a terminal (`aider --model ollama/qwen2.5-coder:32b-instruct-q4_K_M`).
Aider + local model is surprisingly good at single-file diffs inside a git
repo, which is most of your Orca scripting work.

## Getting good answers out of it

- Paste the actual file (or the relevant 100 lines) into context; local models
  degrade fast when they have to guess your code.
- Ask for diffs or complete functions, not "improve my project".
- Keep conversations short; start a new chat per task. Context beyond ~8k
  tokens slows everything and confuses small models.
- For PowerShell/C# questions it is solid. For "why is my Windows HID lock
  shared" class diagnosis, it will confabulate - that's still hosted-agent
  territory.

## Tomorrow, together

If you want, we do this live: I'll tell you exactly which pull to run while
you watch the tokens/second, and we tune 32B-vs-14B against your real CPU/GPU.
The file choices above are the safe defaults if you'd rather just proceed.

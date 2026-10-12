# Chapter 27. The AI-Enabled Coding Round

<!-- STATUS: not started. Structure and sources come from PHYSICAL_AI_BOOK_OUTLINE.md. -->
<!-- ★ SAMPLE CHAPTER -->
<!-- Part VIII · target ~7,500 words · 5 figures · readiness: High -->
<!-- Private source notes (write fresh; never paste): AI_native_coding/chapter_ai_native_coding.md; AI_native_coding/playbook.md -->

> The format: unfamiliar codebase, AI assistant, time pressure. The thesis: when code is cheap, the scarce skills are specifying and verifying.

## Industry technology (2025–2026)

- **AI coding tools in interviews:** Claude Code (Anthropic, CLI agent), Cursor (AI-native IDE), GitHub Copilot (VS Code/JetBrains), Windsurf (Codeium), Aider (open-source CLI), Continue (open-source IDE plugin)
- **Companies using AI-enabled coding rounds:** Anthropic (canonically), Google (internal tooling rounds), Meta (certain teams), robotics startups (practical debugging with AI). The format is spreading because it tests what matters in 2025: not memorized syntax, but judgment about what to delegate and what to verify.
- **The method:** Orient → Hypothesize → Delegate → Verify. The interviewer watches for: reading code before writing it, forming a hypothesis before asking the AI, verifying the AI's output against the spec (not just "does it compile"), knowing when to override the AI.
- **Practice codebases:** The companion repo includes 7 Physical AI-themed projects with failing tests:
  1. Sensor fusion pipeline (Python, temporal alignment bug)
  2. SPSC ring buffer (C++, memory ordering bug)
  3. KV cache manager (Python, eviction policy bug)
  4. Robot state machine (C++, transition logic bug)
  5. Model quantization script (Python, calibration data bug)
  6. Inference pipeline profiler (Python, bottleneck misidentification)
  7. Fleet routing simulator (Python, confidence threshold bug)

## What the loops actually ask

- Here is a codebase you've never seen: a sensor fusion pipeline in Python (~800 lines). There are 3 failing tests. You have Claude Code / Cursor as your AI assistant. Fix the tests. You have 45 minutes.
- How do you verify the AI's suggestion is correct without reading every line? Show your process.
- The AI suggests refactoring 200 lines. You have 10 minutes left and 1 failing test. What do you do?
- When do you override the AI and when do you trust it? Give examples from the practice session.
- You ask the AI to fix a concurrency bug. It suggests adding a mutex. Is that the right fix? How do you evaluate it?
- The AI's first suggestion doesn't work. What is your next step? (Hint: refine the prompt, not the code)

## Companion code

- `ch27/sensor_fusion/` — Practice project: sensor fusion pipeline with 3 temporal alignment bugs and failing tests
- `ch27/spsc_bug/` — Practice project: SPSC ring buffer with memory ordering bug
- `ch27/kv_cache_bug/` — Practice project: KV cache manager with eviction policy bug
- `ch27/README.md` — The Orient → Hypothesize → Delegate → Verify method walkthrough

## The mental model

## Worked problems

## In the room

## Failure modes

## Exercises

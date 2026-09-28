# Chapter 21. Rate Limiting and Quota for LLMs

<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->
<!-- Part VI · target ~4,000 words · 4 figures · readiness: High — restructure existing material -->
<!-- Private source notes (write fresh; never paste): company/openai/llm-rate-limiter.md; company/openai/token_rate_limiter.py -->

> Why requests-per-minute alone fails for LLMs, and how dual RPM+TPM token buckets fix it: reserving tokens before generation, reconciling afterward, syncing counters across regions, and multi-tenant fairness for agents that fan out.

## Why interviewers ask this

## What the loops actually ask

- Design a rate limiter for an LLM API.
- How do you charge tokens you haven't generated yet?

## The mental model

## Worked problems

## Code you can run

<!-- token rate limiter (Python) -->

## In the room

## Failure modes

## Exercises

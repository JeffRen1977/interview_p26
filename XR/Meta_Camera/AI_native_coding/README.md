# AI-Native Coding

This folder is a working example of **AI-native coding**: you treat tests and the existing skeleton as the spec, drive the model in short stages, and only keep code that tests can prove.

The first example is a gateway **RateLimiter Engine** (multi-tenant, dynamic window, degrade on overload / store failure).

| Path | What it is |
|------|------------|
| [`ratelimiter_engine/README.md`](./ratelimiter_engine/README.md) | Task, algorithm, and how to implement it |
| [`ratelimiter_engine/prompts.md`](./ratelimiter_engine/prompts.md) | Copy-paste prompts for each stage |
| [`ratelimiter_engine/project/`](./ratelimiter_engine/project/) | Skeleton + implementation + tests |

```bash
cd AI_native_coding/ratelimiter_engine/project
python3 -m unittest tests.test_ratelimiter -v
```

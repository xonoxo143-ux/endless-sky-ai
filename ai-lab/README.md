# Endless Sky AI Lab

This folder tracks local notes and patch plans for the AI hook work.

Current milestone: read-only telemetry hooks.

## Telemetry v0.2 test

Telemetry can still be enabled with:

```bash
./Endless_Sky-continuous-x86_64.AppImage --ai-telemetry --ai-telemetry-every 600
```

To keep telemetry JSON separate from normal logger output, set `ES_AI_TELEMETRY_FILE`:

```bash
ES_AI_TELEMETRY_FILE=ai-telemetry.jsonl ./Endless_Sky-continuous-x86_64.AppImage --ai-telemetry --ai-telemetry-every 600
```

The output file should contain only JSONL telemetry records.

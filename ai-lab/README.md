# Endless Sky AI Lab

This folder tracks local notes and patch plans for the AI hook work.

Current milestone: experimental control bridge scaffolding.

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

## Control bridge v0.3

The control bridge is experimental and opt-in. It validates command files and
emits command-result telemetry, but it does not yet fly the ship.

Example command file:

```json
{"type":"ai_command","seq":1,"action":"noop","duration":1}
```

Example launch:

```bash
ES_AI_CONTROL=1 ES_AI_COMMAND_FILE=ai-command.json ES_AI_TELEMETRY_FILE=ai-telemetry.jsonl ./Endless_Sky-continuous-x86_64.AppImage --ai-telemetry
```

You can also enable the bridge with `--ai-control` instead of `ES_AI_CONTROL=1`.
For v0.3, only `noop` is accepted. Movement-like actions such as `thrust`,
`turn_left`, `turn_right`, `brake`, and `stop_control` are parsed but rejected
with `action_not_implemented`.

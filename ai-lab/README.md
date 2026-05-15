# Endless Sky AI Lab

This folder tracks local notes and patch plans for the AI hook work.

Current milestone: experimental movement control bridge.

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

## Control bridge v0.4

The control bridge is experimental and opt-in. It validates command files and
emits command-result telemetry. Movement commands are routed through the
existing player input command path, only run in flight, and expire after their
requested duration.

Example command file:

```json
{"type":"ai_command","seq":1,"action":"thrust","duration":60}
```

Example launch:

```bash
ES_AI_CONTROL=1 ES_AI_COMMAND_FILE=ai-command.json ES_AI_TELEMETRY_FILE=ai-telemetry.jsonl ./Endless_Sky-continuous-x86_64.AppImage --ai-telemetry
```

You can also enable the bridge with `--ai-control` instead of `ES_AI_CONTROL=1`.
For v0.4, accepted actions are `noop`, `stop_control`, `thrust`, `turn_left`,
`turn_right`, and `brake`. Movement actions require `duration` from 1 to 300
frames. `stop_control` clears the active AI movement command.

Set `--ai-telemetry-every 1` while testing if you want every frame to include
the `ai_control` telemetry object showing the active command, its sequence, and
the tick when it expires.

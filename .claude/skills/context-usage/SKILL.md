---
name: context-usage
description: Show the current Claude Code session's context window usage by reading the session's JSONL transcript. Use when the user asks about context usage, tokens remaining, or how full the context window is.
allowed-tools: Bash(python3 *)
---

## Session context window usage

!`MAX=1000000 python "${CLAUDE_SKILL_DIR}/check_usage.py" "${CLAUDE_SESSION_ID}"`

Above is the current session's context window usage parsed from the transcript file. Report it to the user as:

- Total context tokens (the sum that fills the window)
- Percentage of the max context used
- Breakdown: input / cache read / cache creation

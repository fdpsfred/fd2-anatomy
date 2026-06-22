#!/usr/bin/env python3
"""limit_wait.py -- seconds to wait until a usage-limit reset.

The workflow stops fast on the usage-limit signature (3 consecutive agent() nulls) but
cannot read the "resets HH:MMam" message (agent() returns null with no detail). The
orchestrator CAN see that message in the workflow's <failures>, so it passes it here to
compute how long to sleep before auto-resuming.

Parses the reset clock time (e.g. "...resets 6:50am (Asia/Taipei)") and prints the seconds
from NOW until the next occurrence of that LOCAL clock time, plus a safety buffer. (This
machine's local time is Asia/Taipei, matching the reset's stated zone.)

Usage : python tools/src_refine/limit_wait.py "<error text or 'H:MMam'>" [buffer_seconds]
Output: integer seconds (>= buffer) on stdout.
"""
import datetime
import re
import sys


def main():
    s = sys.argv[1] if len(sys.argv) > 1 else ""
    buf = int(sys.argv[2]) if len(sys.argv) > 2 else 180
    m = re.search(r'(\d{1,2}):(\d{2})\s*([ap]m)', s, re.I)
    if not m:
        print(3600 + buf)            # unparseable -> conservative 1h + buffer
        return
    hh, mm, ap = int(m.group(1)), int(m.group(2)), m.group(3).lower()
    if ap == 'pm' and hh != 12:
        hh += 12
    if ap == 'am' and hh == 12:
        hh = 0
    now = datetime.datetime.now()
    target = now.replace(hour=hh, minute=mm, second=0, microsecond=0)
    if target <= now:                # reset clock time already passed today -> next day
        target += datetime.timedelta(days=1)
    wait = int((target - now).total_seconds()) + buf
    print(max(buf, wait))


if __name__ == "__main__":
    main()

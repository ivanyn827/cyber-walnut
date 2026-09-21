"""Read only completion metadata from this Mac's Codex history (no messages).

The local SQLite schema is an internal integration, checked on each query.
No thread is resumed and no model is invoked. Fail closed on schema errors.
"""
import json
import os
from pathlib import Path
import sqlite3
from contextlib import closing
import re
import time


class Completions:
    def __init__(self, home, state):
        self.home, self.state = Path(home), Path(state)
        self.seen = None
        self.total = 0
        self.since = int(time.time())
        self.events = []
        if self.state.exists():
            saved = json.loads(self.state.read_text())
            self.seen = set(saved['seen'])
            self.total = int(saved['total'])
            self.since = int(saved.get('since',self.state.stat().st_mtime))
            self.events = saved.get('events', [])

    def poll(self):
        # Migrated tasks keep their visible ID but use the rollout suffix as the
        # history storage ID. Read each DB independently: never hold both locks.
        with closing(sqlite3.connect((self.home/'state_5.sqlite').as_uri()+'?mode=ro',uri=True,timeout=2)) as db:
            tasks=db.execute("SELECT id,rollout_path FROM threads WHERE source IN ('vscode','cli')").fetchall()
        ids=set()
        for task,path in tasks:
            ids.add(task)
            match=re.search(r'_([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$',Path(path).stem)
            if match:ids.add(match[1])
        with closing(sqlite3.connect((self.home/'thread_history_1.sqlite').as_uri()+'?mode=ro', uri=True, timeout=2)) as db:
            rows=db.execute("SELECT thread_id,turn_id,completed_at FROM thread_turns WHERE status='completed' AND completed_at IS NOT NULL").fetchall()
        current = {a+'/'+b for a,b,ended in rows if a in ids}
        recent = {a+'/'+b for a,b,ended in rows if a in ids and ended>=self.since}
        now = time.time()
        events = [event for event in self.events if event[1] > now]
        total = self.total
        if self.seen is not None:
            ended_by_id = {a+'/'+b: ended for a,b,ended in rows if a+'/'+b in recent-self.seen}
            for ident, ended in sorted(ended_by_id.items(), key=lambda item:(item[1],item[0])):
                total += 1
                if now < ended+60 and ended <= now:
                    events.append([total, ended+60])
        changed = self.seen is None or bool(current-self.seen) or events != self.events
        seen = current if self.seen is None else self.seen | current
        if changed:
            self.state.parent.mkdir(parents=True, exist_ok=True)
            temporary = self.state.with_suffix('.tmp')
            fd = os.open(temporary, os.O_WRONLY|os.O_CREAT|os.O_TRUNC, 0o600)
            with os.fdopen(fd,'w') as out:
                json.dump({'total':total,'seen':sorted(seen),'since':self.since,'events':events},out)
            os.replace(temporary,self.state)
        self.total, self.seen, self.events = total, seen, events
        return self.total

    def next_notice(self, delivered, now=None):
        now = time.time() if now is None else now
        for count, expires in self.events:
            if count > delivered and expires > now:
                return count, expires
        # Advance the device watermark without replaying expired history.
        return self.total, 0

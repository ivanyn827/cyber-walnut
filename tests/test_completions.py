import sys
import sqlite3
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from task_completions import Completions
from quota_companion import notice_packet

class CompletionTests(unittest.TestCase):
    def test_multiple_restart_and_filter(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            with sqlite3.connect(root/'state_5.sqlite') as db:
                db.execute('CREATE TABLE threads(id TEXT,source TEXT,rollout_path TEXT)')
                db.executemany('INSERT INTO threads VALUES(?,?,?)',[('a','vscode','a.jsonl'),('b','vscode','b.jsonl'),('sub','subagent','sub.jsonl'),('visible','vscode','rollout_visible_01234567-0123-0123-0123-0123456789ab.jsonl')])
            db=sqlite3.connect(root/'thread_history_1.sqlite')
            db.execute('CREATE TABLE thread_turns(thread_id TEXT,turn_id TEXT,status TEXT,completed_at INTEGER)')
            db.execute("INSERT INTO thread_turns VALUES('a','old','completed',1)");db.commit()
            c=Completions(root,root/'state.json');c.since=0;self.assertEqual(c.poll(),0)
            db.executemany('INSERT INTO thread_turns VALUES(?,?,?,?)',[
                ('a','new','completed',2),('b','new','completed',2),('b','failed','failed',2),('sub','new','completed',2),('a','running','inProgress',None)])
            db.commit();self.assertEqual(c.poll(),2);self.assertEqual(c.poll(),2)
            c=Completions(root,root/'state.json');self.assertEqual(c.poll(),2)
            db.execute("UPDATE thread_turns SET status='completed',completed_at=3 WHERE turn_id='running'");db.commit()
            self.assertEqual(c.poll(),3)
            db.execute("INSERT INTO thread_turns VALUES('01234567-0123-0123-0123-0123456789ab','migrated','completed',4)");db.commit()
            self.assertEqual(c.poll(),4);self.assertEqual(c.poll(),4)
            db.close()
    def test_packet(self):
        p=notice_packet({'key':'ab'*32},'0123456789abcdef',3,123)
        self.assertTrue(p.startswith('N2 0123456789abcdef 123 3 0 '));self.assertLess(len(p),255)
        self.assertNotEqual(p,notice_packet({'key':'ab'*32},'0123456789abcdef',4,123))

    def test_expiry_and_restart(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            with sqlite3.connect(root/'state_5.sqlite') as db:
                db.execute('CREATE TABLE threads(id TEXT,source TEXT,rollout_path TEXT)')
                db.execute("INSERT INTO threads VALUES('a','vscode','a.jsonl')")
            with sqlite3.connect(root/'thread_history_1.sqlite') as db:
                db.execute('CREATE TABLE thread_turns(thread_id TEXT,turn_id TEXT,status TEXT,completed_at INTEGER)')
            c=Completions(root,root/'state.json');c.since=0;c.poll()
            with sqlite3.connect(root/'thread_history_1.sqlite') as db:
                db.executemany('INSERT INTO thread_turns VALUES(?,?,?,?)', [('a','old','completed',900),('a','fresh1','completed',970),('a','fresh2','completed',980)])
            with patch('task_completions.time.time',return_value=1000):
                self.assertEqual(c.poll(),3)
                self.assertEqual(c.next_notice(0),(2,1030))
                c=Completions(root,root/'state.json');c.poll()
                self.assertEqual(c.next_notice(2),(3,1040))
            self.assertEqual(c.next_notice(0,1030),(3,1040))
            self.assertEqual(c.next_notice(0,1040),(3,0))
            with patch('task_completions.time.time',return_value=1100):
                c.poll();self.assertEqual(c.events,[])
                self.assertEqual(c.next_notice(0),(3,0))

if __name__=='__main__':unittest.main()

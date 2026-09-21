import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from quota_companion import select_sample,packet
class QuotaTests(unittest.TestCase):
    def test_weekly_only(self):
        s=select_sample({'rateLimitsByLimitId':{'codex':{'primary':{'usedPercent':20,'windowDurationMins':300},'secondary':{'usedPercent':43,'windowDurationMins':10080,'resetsAt':1789435927}},'codex_bengalfox':{'primary':{'usedPercent':0,'windowDurationMins':10080}}}},100)
        self.assertEqual(s,{'remaining':57,'minutes':10080,'reset':1789435927,'observed':100})
    def test_unknown(self):
        for result in ({},{'rateLimitsByLimitId':{}},{'rateLimits':{'limitId':'codex_bengalfox','primary':{'usedPercent':0,'windowDurationMins':10080}}},{'rateLimits':{'primary':{'usedPercent':None,'windowDurationMins':10080}}}):
            self.assertEqual(select_sample(result)['remaining'],-1)
    def test_clamp_and_no_fake_reset(self):
        for used,expected in ((101,0),(-1,100),(100,0),(float('nan'),-1)):
            self.assertEqual(select_sample({'rateLimits':{'primary':{'usedPercent':used,'windowDurationMins':10080,'resetsAt':1}}},200)['remaining'],expected)
    def test_signature(self):
        s={'observed':1788970000,'remaining':57,'minutes':10080,'reset':1789435927}
        p=packet({'key':'ab'*32},'1234567890123456',s,1)
        self.assertLess(len(p),255);self.assertNotEqual(p,packet({'key':'ac'*32},'1234567890123456',s,1))
if __name__=='__main__': unittest.main()

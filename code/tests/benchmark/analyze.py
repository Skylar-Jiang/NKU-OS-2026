#!/usr/bin/env python3
"""Parse real QEMU logs; never synthesize observations or fill missing runs."""
from pathlib import Path
import csv,json,re,statistics
CODE=Path(__file__).resolve().parents[2]
OUT=CODE/'tests/results'
POLICIES=['rr','stride','fifo','sjf','cfs']
NAMES={'rr':'RR_scheduler','stride':'stride_scheduler','fifo':'FIFO','sjf':'SJF','cfs':'CFS_scheduler'}
SCENARIOS=['cpu_equal','different_lengths','weighted','mixed_yield']
def fields(line):return {k:int(v) for k,v in re.findall(r'(\w+)=(-?\d+)',line)}
def jain(values):
    return sum(values)**2/(len(values)*sum(x*x for x in values)) if any(values) else 0
processes=[];runs=[]
for policy in POLICIES:
 for case,scenario in enumerate(SCENARIOS):
  for round in (1,2,3):
   p=OUT/f'qemu/{policy}_{case}_{round}.txt'
   s=p.read_text();assert 'sched class: '+NAMES[policy] in s,p
   assert 'schedbench pass.' in s and 'init check memory pass.' in s,p
   jobs={r['pid']:r for r in map(fields,(l for l in s.splitlines() if l.startswith('BJOB ')))}
   stats={r['pid']:r for r in map(fields,(l for l in s.splitlines() if l.startswith('KSTAT ')))}
   shares={r['pid']:r for r in map(fields,(l for l in s.splitlines() if l.startswith('KSHARE ')))}
   assert len(jobs)==4 and set(jobs)<=stats.keys() and set(jobs)==shares.keys(),p
   batch=[]
   total_share=sum(shares[pid]['run'] for pid in jobs)
   for pid,job in sorted(jobs.items()):
    r=stats[pid];assert r['arrival']<=r['first']<=r['end'] and r['run']>0,p
    row=dict(policy=policy,scenario=scenario,round=round,idx=job['idx'],pid=pid,weight=job['weight'])
    row.update(response_ms=(r['first']-r['arrival'])/10000,turnaround_ms=(r['end']-r['arrival'])/10000,
       waiting_ms=r['wait']/10000,cpu_interval_ms=r['run']/10000,dispatches=r['dispatches'],timer_ticks=r['ticks'],
       first_window_cpu_share=shares[pid]['run']/total_share)
    assert row['waiting_ms']<=row['turnaround_ms'],p
    batch.append(row);processes.append(row)
   parent=stats[2]
   row=dict(policy=policy,scenario=scenario,round=round)
   for key in ['response_ms','turnaround_ms','waiting_ms','cpu_interval_ms']:
    row[key]=statistics.mean(r[key] for r in batch)
   row.update(weighted_jain=jain([shares[pid]['run']/jobs[pid]['weight'] for pid in jobs]),
      decision_us=parent['decision_time']/parent['decisions']/10,
      context_switches=parent['switches'],dispatch_decisions=parent['decisions'])
   runs.append(row)
def write_csv(name,rows):
 with (OUT/name).open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
write_csv('per_process.csv',processes);write_csv('per_run.csv',runs)
summary=[]
for policy in POLICIES:
 for scenario in SCENARIOS:
  group=[r for r in runs if r['policy']==policy and r['scenario']==scenario]
  row={'policy':policy,'scenario':scenario,'repeats':len(group)}
  for key in ['response_ms','turnaround_ms','waiting_ms','cpu_interval_ms','weighted_jain','decision_us','context_switches']:
   vals=[r[key] for r in group];row[key]=statistics.mean(vals);row[key+'_sd']=statistics.stdev(vals)
  summary.append(row)
write_csv('summary.csv',summary)
priority=[]
for policy in POLICIES:
 s=(OUT/f'qemu/{policy}_priority.txt').read_text()
 accs=[int(x) for x in re.findall(r'main: pid \d+, acc (\d+),',s)]
 assert len(accs)==5 and all(accs),policy
 ratios=[x/accs[0] for x in accs]
 for i,(acc,ratio) in enumerate(zip(accs,ratios),1):priority.append(dict(policy=policy,weight=i,acc=acc,ratio_to_weight1=ratio))
 if policy in ['stride','cfs']:
  assert all(abs(r/i-1)<0.25 for i,r in enumerate(ratios,1)),(policy,ratios)
write_csv('priority.csv',priority)
lines=['# Real QEMU scheduler benchmark','',
 '60 boots: five policies, four scenarios, three repeats. QEMU 4.1.1 virt, single hart, -icount shift=3; simulated time, not host wall time.',
 'rdtime = 10 MHz; times below are milliseconds, decision cost is microseconds. CPU interval includes kernel services and dispatch-to-switch accounting.',
 'Fairness is weighted Jain of CPU intervals from creation through the first worker completion, before any worker has left the comparison.',
 'Decision cost includes lightweight observer bookkeeping; excludes switch.S, satp/TLB work and printed diagnostics.', '',
 '|Policy|Scenario|Wait ms|Response ms|Turnaround ms|Weighted Jain|Decision us|',
 '|---|---|---:|---:|---:|---:|---:|']
for r in summary:
 lines.append('|'+r['policy']+'|'+r['scenario']+'|'+ '|'.join(f"{r[k]:.3f}" for k in ['waiting_ms','response_ms','turnaround_ms','weighted_jain','decision_us'])+'|')
lines+=['','CSV files contain per-process observations and sample standard deviations; no missing run is imputed.',
 'priority.csv uses normal QEMU time (no -icount); the official program checks time only once per 4000 loop batches, so finite-run ratios have sampling/startup bias.',
 'mixed_yield is voluntary yielding between CPU bursts, not actual disk/network I/O. SJF learns bursts; it does not know future job lengths.']
(OUT/'summary.md').write_text('\n'.join(lines)+'\n')
(OUT/'summary.json').write_text(json.dumps({'boots':60,'workers':240,'priority_boots':5,'time_unit':'QEMU instruction-count virtual milliseconds','summary':summary},indent=2)+'\n')
print('Parsed 60 real boots, 240 workers and 5 official priority runs; all invariants passed.')

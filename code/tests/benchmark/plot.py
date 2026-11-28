"""Standalone publication figures from measured CSV; requires matplotlib."""
from pathlib import Path
import csv
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
code=Path(__file__).resolve().parents[2];root=code.parent
rows=list(csv.DictReader((code/'tests/results/summary.csv').open()))
out=root/'report/images';out.mkdir(parents=True,exist_ok=True)
policies=['rr','stride','fifo','sjf','cfs'];scenarios=['cpu_equal','different_lengths','weighted','mixed_yield']
fig,axs=plt.subplots(1,3,figsize=(15,4.5),layout='constrained')
for ax,key,title in zip(axs,['waiting_ms','response_ms','turnaround_ms'],['Ready waiting','First response','Turnaround']):
 for i,p in enumerate(policies):
  group=[next(r for r in rows if r['policy']==p and r['scenario']==s) for s in scenarios]
  ax.bar([x+(i-2)*.15 for x in range(4)],[float(r[key]) for r in group],width=.15,label=p.upper(),yerr=[float(r[key+'_sd']) for r in group],capsize=2)
 ax.set_xticks(range(4),['Equal CPU','Lengths','Weights','Yield mix']);ax.set_title(title);ax.set_ylabel('QEMU virtual ms');ax.grid(axis='y',alpha=.2)
axs[0].legend(fontsize=8);fig.suptitle('Five real kernel schedulers: 3 repeated boots per workload (mean + sample SD)')
fig.savefig(out/'lab6-latency.png',dpi=180);plt.close(fig)
fig,axs=plt.subplots(1,2,figsize=(11,4.3),layout='constrained')
for ax,key,title in zip(axs,['weighted_jain','decision_us'],['Weighted fairness: first completion window','Mean scheduling decision cost']):
 vals=[float(next(r for r in rows if r['policy']==p and r['scenario']=='weighted')[key]) for p in policies]
 ax.bar([p.upper() for p in policies],vals,color=['#4c78a8','#f58518','#54a24b','#e45756','#b279a2']);ax.set_title(title);ax.grid(axis='y',alpha=.2)
axs[0].set_ylim(0,1.05);axs[0].set_ylabel('Weighted Jain index');axs[1].set_ylabel('QEMU virtual microseconds (observer included)')
fig.savefig(out/'lab6-fairness-cost.png',dpi=180);plt.close(fig)

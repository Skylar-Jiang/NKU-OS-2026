# Real QEMU scheduler benchmark

60 boots: five policies, four scenarios, three repeats. QEMU 4.1.1 virt, single hart, -icount shift=3; simulated time, not host wall time.
rdtime = 10 MHz; times below are milliseconds, decision cost is microseconds. CPU interval includes kernel services and dispatch-to-switch accounting.
Fairness is weighted Jain of CPU intervals from creation through the first worker completion, before any worker has left the comparison.
Decision cost includes lightweight observer bookkeeping; excludes switch.S, satp/TLB work and printed diagnostics.

|Policy|Scenario|Wait ms|Response ms|Turnaround ms|Weighted Jain|Decision us|
|---|---|---:|---:|---:|---:|---:|
|rr|cpu_equal|459.682|71.546|614.560|1.000|14.140|
|rr|different_lengths|177.671|71.545|284.479|0.915|13.489|
|rr|weighted|1827.127|71.546|2443.386|0.654|15.291|
|rr|mixed_yield|429.544|53.909|622.929|0.901|14.876|
|stride|cpu_equal|459.972|71.547|614.591|1.000|14.843|
|stride|different_lengths|178.066|71.546|284.504|0.915|14.005|
|stride|weighted|1309.143|71.546|1924.891|0.991|15.917|
|stride|mixed_yield|430.082|53.910|622.971|0.901|15.557|
|fifo|cpu_equal|234.436|234.436|389.239|0.250|11.375|
|fifo|different_lengths|234.436|234.436|341.196|0.250|11.387|
|fifo|weighted|926.271|926.271|1542.303|0.250|11.375|
|fifo|mixed_yield|581.064|89.253|774.397|0.736|14.367|
|sjf|cpu_equal|234.437|234.437|389.241|0.250|11.475|
|sjf|different_lengths|234.437|234.437|341.198|0.250|11.475|
|sjf|weighted|926.276|926.276|1542.311|0.250|11.500|
|sjf|mixed_yield|312.026|118.520|505.101|0.500|14.338|
|cfs|cpu_equal|454.548|26.528|609.006|0.998|15.025|
|cfs|different_lengths|145.689|26.527|252.018|0.967|13.730|
|cfs|weighted|1250.197|44.035|1866.110|0.999|15.668|
|cfs|mixed_yield|339.256|23.898|532.175|0.995|15.102|

CSV files contain per-process observations and sample standard deviations; no missing run is imputed.
priority.csv uses normal QEMU time (no -icount); the official program checks time only once per 4000 loop batches, so finite-run ratios have sampling/startup bias.
mixed_yield is voluntary yielding between CPU bursts, not actual disk/network I/O. SJF learns bursts; it does not know future job lengths.

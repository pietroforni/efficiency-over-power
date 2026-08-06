# Results directory

Root-level benchmark, metadata, perf, and plotting outputs are temporary and
ignored by Git. Incoming device transfers are also ignored so device files can
be checked before publication.

After reviewing a Raspberry Pi run, archive its accepted artifacts under a
stable directory such as:

```text
results/pi3/reference-YYYY-MM-DD/
├── benchmark-TIMESTAMP.csv
├── metadata-TIMESTAMP.txt
├── perf-TIMESTAMP.csv
├── vectorization-auto-strict.txt
├── vectorization-auto-relaxed.txt
└── plots/
    ├── summary.csv
    ├── single-core.png
    └── multicore.png
```

Keep accepted benchmark CSV and metadata files unchanged. Retain one successful
PMU profile for each reported configuration. Compiler reports may be reduced to
focused excerpts when the command and original diagnostic lines are preserved;
the complete reports can be reproduced from the documented Make targets.

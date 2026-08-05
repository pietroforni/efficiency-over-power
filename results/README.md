# Results directory

Root-level benchmark, metadata, perf, and plotting outputs are temporary and
ignored by Git. Incoming device transfers are also ignored so raw files can be
checked before publication.

After reviewing a Raspberry Pi run, preserve its files unchanged under a
stable directory such as:

```text
results/pi3/reference-YYYY-MM-DD/
├── benchmark-TIMESTAMP.csv
├── metadata-TIMESTAMP.txt
├── perf-aos.csv
├── perf-auto.csv
├── perf-neon.csv
├── vectorization.txt
└── plots/
    ├── summary.csv
    ├── single-core.png
    └── multicore.png
```

Commit accepted raw and derived files together. Never replace raw samples with
an averaged or manually edited file.

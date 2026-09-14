# Measurement results & analysis (thesis chapter 7)

Storage and analysis of the evaluation measurement series. **Raw data in `raw/`
is never modified** – every result is reproducibly regenerated in `analysis/`.

```
results/
├── raw/                  # unmodified serial captures (CSV)
└── analysis/
    ├── analyze.py        # generates tables + plots from raw/*.csv
    ├── requirements.txt
    ├── figures/          # *.pdf  -> \includegraphics in the thesis
    └── tables/           # *.tex  -> \input in the thesis  (+ *.csv)
```

No manifest is needed: `analyze.py` processes every CSV in `raw/` and derives the
test type and conditions from the **file name**.

## 1. File-name convention

`<test>_<label>.csv`

| Token | Meaning |
|---|---|
| `test` | first token before the first `_`: `packetloss`, `interference`, `range` or `longterm` |
| `label` | scenario name shown in plots/tables; for `range` it must contain a distance like `5m` or `0.5m` |
| `hidden` | optional token **directly after** `range`: measurement **without line of sight** (`range_hidden_50m.csv`) |

All four test types are recorded with the **same firmware** (`test_reliability_*`).
Only the procedure differs: `interference` adds interferers, `range` is repeated at
several distances, and `longterm` is simply a long capture.

Examples:

```
packetloss_ideal.csv        packetloss_stage.csv
interference_reference.csv  interference_wifi-bt.csv
range_1m.csv  range_5m.csv  range_10m.csv
range_hidden_10m.csv  range_hidden_50m.csv
longterm_4h.csv
```

### Range with and without line of sight

The range test is recorded in two variants: `range_<distance>.csv` with the
receiver in line of sight, `range_hidden_<distance>.csv` with the receiver
**behind a massive obstacle** (building, wall) at the same distance, so the link
only survives via diffraction and reflections. The `hidden` token has to come
directly after `range` – anywhere else it is ignored (with a warning).

Both variants are evaluated **identically**, but as separate series – averaging
one distance curve over two propagation conditions would be meaningless. The
hidden series therefore gets its own outputs with the prefix `range_hidden`:

```
figures/range_distance.pdf          figures/range_hidden_distance.pdf
figures/range_timeseries.pdf        figures/range_hidden_timeseries.pdf
tables/range_summary.tex            tables/range_hidden_summary.tex
tables/range_metrics_full.csv       tables/range_hidden_metrics_full.csv
```

LaTeX labels: `tab:range-summary` and `tab:range-hidden-summary`.

> Encode the measurement conditions in the label (e.g. `interference_3xwifi-2xbt`).
> Note the firmware commit (`git rev-parse --short HEAD`) in your lab notes – it is
> not stored automatically.

## 2. Record a measurement

```bash
# flash the devices
pio run -e test_reliability_bridge -t upload
pio run -e test_reliability_tube   -t upload

# capture the receiver output as raw data (file name = condition)
pio device monitor -e test_reliability_tube | tee results/raw/packetloss_ideal.csv
```

Stop the monitor with `Ctrl-C` after the desired duration (e.g. 60 s; for a
long-term run let it capture for several hours into a `longterm_*.csv`).

## 3. Analyze

```bash
cd results/analysis
pip install -r requirements.txt      # once
python3 analyze.py
```

This produces plots (`figures/*.pdf`) and tables (`tables/*.tex` + `.csv`) per test
type. Include them in the thesis (paths are relative to `thesis/`, where LaTeX runs):

```latex
\input{../test_programs/results/analysis/tables/range_summary.tex}
\includegraphics[width=\linewidth]{../test_programs/results/analysis/figures/range_distance.pdf}
```

The `.tex` tables are German and use `booktabs`. Three details worth knowing:

* Rows recorded **before the first packet arrived** are dropped – the firmware reports
  a fixed 100 % loss for them, which would otherwise become the maximum of every
  window statistic.
* `tables/<test>_metrics_full.csv` contains **every** computed metric, also the ones
  that are not printed in the thesis table (e.g. `win_clean_pct`, `recv_per_seq_min`).
  Any number quoted in the thesis text should be traceable to one of these files.
* Readable scenario names are mapped in `SCENARIO_NAMES` in `analyze.py`; raw file
  names are never changed. Outputs of a test type whose raw data was removed are
  deleted automatically, so `tables/` can never disagree with `raw/`.

## 4. Receiver CSV columns

### Packet loss / interference (`test_packet_loss_tube`, 1 row/s)
| Column | Meaning |
|---|---|
| `t_ms` | receiver timestamp [ms] |
| `win_recv` / `win_exp` / `win_lost` | received / expected / lost in the 1 s window |
| `win_loss_pct` | loss rate in the window [%] |
| `win_max_gap` | longest loss burst in the window (consecutive packets) |
| `cum_*` | same quantities cumulative over the run |
| `cum_max_gap` | longest loss burst overall |
| `iat_mean_ms` | mean inter-arrival time [ms] |
| `jitter_ms` | standard deviation of the inter-arrival time |
| `rssi_avg` / `rssi_min` | signal strength [dBm] (empty if no RSSI samples) |

The **long-term test uses the very same firmware and CSV format** – it is simply a
long packet-loss capture. The analysis then checks whether the performance stays
constant over time (loss trend in %/hour).

> Note: the **burst** (`max_gap`) matters more for flicker assessment than the mean
> loss rate – a single loss holds the last value for only 25 ms (invisible), while a
> long burst becomes visible during motion.

## 5. Which metrics per section

| Thesis section | Primary metrics |
|---|---|
| Packet loss rate | `cum_loss_pct`, `win_loss_pct` (mean/p95), `burst_max_ms` |
| Interference | Δ loss / Δ jitter / Δ `recv_per_seq_mean` (peak vs. off-peak) |
| Range | `cum_loss_pct` and `recv_per_seq_mean` versus distance (per variant, LOS vs. `hidden`) |
| Long-term stability | loss trend over time (%/h ≈ 0 → constant), `loss_pct`, jitter |

> `recv_per_seq_mean` (copies received per sequence value, max. 5) is the metric that
> degrades monotonically with distance, while `cum_loss_pct` does not: the redundant
> transmission absorbs single losses. **Do not** use `rssi_avg` as a path-loss measure –
> the promiscuous sniffer records the last packet seen on the channel, not necessarily
> the payload packet (see thesis section 7.1.4).

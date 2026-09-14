#!/usr/bin/env python3
"""
Analysis of the evaluation measurement series (thesis chapter 7).

The test type and the measurement conditions are derived from the file name.

File-name convention:  ``<test>_<label>.csv``

    test   first token before the first underscore, one of:
             packetloss   -> packet loss test
             interference -> interference / robustness test
             range        -> range test (label must contain a distance like "5m")
             longterm     -> long-term stability test
    label  the remaining text; used as the scenario name in plots/tables.
           For range tests a token like "5m" or "0.5m" provides the distance.
           "hidden" directly after "range" marks a measurement without line of
           sight (receiver behind a massive obstacle): "range_hidden_<distance>".
           Those points are evaluated identically but as a separate series with
           the output prefix "range_hidden".
           A readable German scenario name can be mapped in SCENARIO_NAMES.

Examples:
    packetloss_ideal.csv        packetloss_stage.csv
    interference_reference.csv   interference_wifi-bt.csv
    range_1m.csv  range_5m.csv  range_10m.csv
    range_hidden_10m.csv  range_hidden_50m.csv
    longterm_4h.csv

Outputs (German, ready to \\input / \\includegraphics in the thesis):
    figures/*.pdf   vector plots for LaTeX
    tables/*.tex    includable tables (+ matching *.csv)

Usage:      python3 analyze.py
Deps:       pip install -r requirements.txt
"""

from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

try:
    import numpy as np
    import pandas as pd
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover
    sys.exit(f"Missing dependency: {exc}\nRun: pip install -r requirements.txt")


# ---------------------------------------------------------------------------
#  Paths
# ---------------------------------------------------------------------------
SCRIPT_DIR = Path(__file__).resolve().parent          # .../results/analysis
RESULTS_DIR = SCRIPT_DIR.parent                        # .../results
RAW_DIR = RESULTS_DIR / "raw"
FIG_DIR = SCRIPT_DIR / "figures"
TAB_DIR = SCRIPT_DIR / "tables"

VALID_TESTS = {"packetloss", "interference", "range", "longterm"}
HIDDEN_TOKEN = "hidden"
RANGE_VARIANTS = {
    False: ("range",        "mit Sichtverbindung"),
    True:  ("range_hidden", "ohne Sichtverbindung"),
}
OUTPUT_PREFIXES = (VALID_TESTS - {"range"}) | {prefix for prefix, _ in RANGE_VARIANTS.values()}

Measurement = tuple[str, float | None, "pd.DataFrame", float, bool]

SCENARIO_NAMES = {
    "interference_noisy": "Stoßzeit",
    "interference_calm":  "Nebenzeit",
}

DEFAULT_SEQ_HZ = 40.0

# A loss burst of this many sequence steps holds the last light state long enough to fall below the 24 Hz motion-perception threshold
VISIBLE_BURST_STEPS = 2


def read_seq_interval_ms(path: Path) -> float:
    """Sequence interval [ms] from the capture header ('# ... expect_hz=40')."""
    hz = DEFAULT_SEQ_HZ
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if not line.startswith("#"):
                break
            match = re.search(r"expect_hz=(\d+(?:\.\d+)?)", line)
            if match:
                hz = float(match.group(1))
    return 1000.0 / hz


def load_raw(path: Path) -> pd.DataFrame:
    """Read a raw CSV; '#' comment lines are skipped, empty RSSI fields -> NaN"""
    df = pd.read_csv(path, comment="#", skip_blank_lines=True)
    df.columns = [c.strip() for c in df.columns]
    df = df.apply(pd.to_numeric, errors="coerce")
    if "cum_recv_all" in df.columns:
        df = df[df["cum_recv_all"] > 0]
    return df.reset_index(drop=True)


def parse_filename(path: Path) -> tuple[str | None, str, float | None, bool]:
    """Derive (test_type, label, distance_m, hidden) from the file name."""
    stem = path.stem
    parts = stem.split("_")
    prefix = parts[0].lower()
    test = prefix if prefix in VALID_TESTS else None
    rest = parts[1:]
    hidden = bool(rest) and rest[0].lower() == HIDDEN_TOKEN
    if hidden:
        rest = rest[1:]
    elif HIDDEN_TOKEN in (p.lower() for p in rest):
        print(f"  WARN '{path.name}': '{HIDDEN_TOKEN}' must directly follow the test "
              f"name ({prefix}_{HIDDEN_TOKEN}_...) -- treated as line of sight")
    label = "_".join(rest) if rest else stem
    match = re.search(r"(\d+(?:\.\d+)?)\s*m(?![a-zA-Z0-9])", label)
    distance = float(match.group(1)) if match else None
    return test, SCENARIO_NAMES.get(stem, label), distance, hidden


def p95(series: pd.Series) -> float:
    return float(series.quantile(0.95)) if len(series) else float("nan")


def save_fig(fig, name: str) -> None:
    FIG_DIR.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(FIG_DIR / f"{name}.pdf")
    plt.close(fig)
    print(f"  -> figures/{name}.pdf")


def cell(value) -> str:
    """Format one LaTeX cell: 2 decimals for floats, escape special chars in text."""
    if isinstance(value, float):
        if pd.isna(value):
            return "--"
        text = f"{value:.0f}" if value == int(value) else f"{value:.2f}"
        text = text.replace(".", ",")            # German decimal separator
        return text.replace("-", "$-$", 1)       # typographic minus
    if isinstance(value, (int, np.integer)):
        return str(int(value))
    text = str(value)
    for a, b in (("_", r"\_"), ("%", r"\%"), ("&", r"\&"), ("#", r"\#")):
        text = text.replace(a, b)
    return text


def save_table(df: pd.DataFrame, name: str, caption: str, label: str,
               tex_header: list[tuple[str, str]] | None = None) -> None:
    """Write a table as .csv and as an includable .tex"""
    TAB_DIR.mkdir(parents=True, exist_ok=True)
    df.to_csv(TAB_DIR / f"{name}.csv", index=False)

    cols = list(df.columns)
    align = "l" + "r" * (len(cols) - 1)   # first column left, numbers right
    if tex_header:
        header = (" & ".join(cell(top) for top, _ in tex_header) + r" \\" + "\n    "
                  + " & ".join(cell(bot) for _, bot in tex_header) + r" \\")
    else:
        header = " & ".join(cell(c) for c in cols) + r" \\"
    rows = "".join(
        "    " + " & ".join(cell(v) for v in row) + r" \\" + "\n"
        for row in df.itertuples(index=False, name=None)
    )
    tex = (
        "% Requires \\usepackage{booktabs}; otherwise replace \\toprule etc. with \\hline.\n"
        "\\begin{table}[htbp]\n"
        "  \\centering\n"
        "  \\small\n"
        f"  \\begin{{tabular}}{{{align}}}\n"
        "    \\toprule\n"
        f"    {header}\n"
        "    \\midrule\n"
        f"{rows}"
        "    \\bottomrule\n"
        "  \\end{tabular}\n"
        f"  \\caption{{{caption}}}\n"
        f"  \\label{{{label}}}\n"
        "\\end{table}\n"
    )
    (TAB_DIR / f"{name}.tex").write_text(tex, encoding="utf-8")
    print(f"  -> tables/{name}.tex  (+ .csv)")


FULL_METRIC_ROWS = [
    ("duration_s",        "Messdauer",                      "s"),
    ("packets_expected",  "Erwartete Sequenzen",            ""),
    ("packets_recv",      "Empfangene Sequenzen",           ""),
    ("packets_lost",      "Verlorene Sequenzen",            ""),
    ("loss_pct",          "Sequenzverlust, kumuliert",      "\\%"),
    ("loss_win_mean_pct", "Fensterverlust, Mittel",         "\\%"),
    ("loss_win_p95_pct",  "Fensterverlust, p95",            "\\%"),
    ("loss_win_max_pct",  "Fensterverlust, Maximum",        "\\%"),
    ("win_clean_pct",     "Verlustfreie Fenster",           "\\%"),
    ("win_visible_pct",   "Fenster mit Ausfall ab 50\\,ms", "\\%"),
    ("burst_max_steps",   "Längste Verlustserie",           "Sequenzen"),
    ("burst_max_ms",      "Längste Verlustserie",           "ms"),
    ("jitter_mean_ms",    "Jitter, Mittel",                 "ms"),
    ("recv_per_seq_mean", "Kopien je Sequenz, Mittel",      ""),
    ("recv_per_seq_min",  "Kopien je Sequenz, Minimum",     ""),
    ("rssi_mean_dbm",     "RSSI, Mittel",                   "dBm"),
    ("rssi_min_dbm",      "RSSI, Minimum",                  "dBm"),
]


def save_full_metrics(rows: list[dict], name: str, first_col: str,
                      caption: str, label: str) -> None:
    """Write every computed metric as CSV and as an includable .tex."""
    TAB_DIR.mkdir(parents=True, exist_ok=True)
    pd.DataFrame(rows).to_csv(TAB_DIR / f"{name}_metrics_full.csv", index=False)
    print(f"  -> tables/{name}_metrics_full.csv")

    points = [row[first_col] for row in rows]
    header = " & ".join(["Kennzahl", "Einheit"] + [cell(p) for p in points]) + r" \\"
    body = "".join(
        "    " + " & ".join([cell(title), unit]
                            + [cell(row.get(key, float("nan"))) for row in rows])
        + r" \\" + "\n"
        for key, title, unit in FULL_METRIC_ROWS
    )
    tex = (
        "% Requires \\usepackage{booktabs}; otherwise replace \\toprule etc. with \\hline.\n"
        "\\begin{table}[htbp]\n"
        "  \\centering\n"
        "  \\small\n"
        f"  \\begin{{tabular}}{{ll{'r' * len(points)}}}\n"
        "    \\toprule\n"
        f"    {header}\n"
        "    \\midrule\n"
        f"{body}"
        "    \\bottomrule\n"
        "  \\end{tabular}\n"
        f"  \\caption{{{caption}}}\n"
        f"  \\label{{{label}}}\n"
        "\\end{table}\n"
    )
    (TAB_DIR / f"{name}_metrics_full.tex").write_text(tex, encoding="utf-8")
    print(f"  -> tables/{name}_metrics_full.tex")


def drop_stale_outputs(active_prefixes: set[str]) -> None:
    """Remove artefacts of measurement series that no longer have raw data."""
    for test in sorted(OUTPUT_PREFIXES - active_prefixes):
        for stale in (TAB_DIR / f"{test}_summary.tex", TAB_DIR / f"{test}_summary.csv",
                      TAB_DIR / f"{test}_metrics_full.csv",
                      TAB_DIR / f"{test}_metrics_full.tex",
                      FIG_DIR / f"{test}_timeseries.pdf", FIG_DIR / f"{test}_distance.pdf"):
            if stale.exists():
                stale.unlink()
                print(f"  -- removed stale {stale.parent.name}/{stale.name} (no raw data)")


def loss_metrics(df: pd.DataFrame, seq_ms: float) -> dict:
    last = df.iloc[-1]
    burst_steps = int(df["cum_max_gap"].max()) if "cum_max_gap" in df else -1
    return {
        "duration_s":        round((df["t_ms"].iloc[-1] - df["t_ms"].iloc[0]) / 1000.0, 1),
        "packets_recv":      int(last["cum_recv_unique"]),
        "packets_expected":  int(last["cum_exp"]),
        "packets_lost":      int(last["cum_lost"]),
        "loss_pct":          float(last["cum_loss_pct"]),
        "loss_win_mean_pct": float(df["win_loss_pct"].mean()),
        "loss_win_p95_pct":  p95(df["win_loss_pct"]),
        "loss_win_max_pct":  float(df["win_loss_pct"].max()),
        "burst_max_steps":   burst_steps,
        "burst_max_ms":      round(burst_steps * seq_ms) if burst_steps >= 0 else -1,
        "win_visible_pct":   float((df["win_max_gap"] >= VISIBLE_BURST_STEPS).mean() * 100.0),
        "win_clean_pct":     float((df["win_loss_pct"] == 0).mean() * 100.0),
        "jitter_mean_ms":    float(df["jitter_seq_ms"].mean()),
        "rssi_mean_dbm":     float(df["rssi_avg"].mean(skipna=True)),
        "rssi_min_dbm":      float(df["rssi_min"].min(skipna=True)),
        "recv_per_seq_mean": float(df["recv_per_seq_mean"].mean()),
        "recv_per_seq_min":  float(df["recv_per_seq_mean"].min()),
    }


TABLE_COLUMNS = [
    ("loss_pct",          "Verlust",      "[%]"),
    ("loss_win_p95_pct",  "Fenster p95",  "[%]"),
    ("recv_per_seq_mean", "Kopien",       "je Seq."),
    ("jitter_mean_ms",    "Jitter",       "[ms]"),
    ("burst_max_ms",      "max. Ausfall", "[ms]"),
    ("win_visible_pct",   "Fenster",      "ab 50 ms [%]"),
    ("rssi_mean_dbm",     "RSSI",         "[dBm]"),
]

RSSI_CAPTION_NOTE = (
    "Der RSSI-Wert stammt vom Promiscuous-Sniffer und erfasst das jeweils letzte "
    "Datenpaket auf dem Kanal, nicht zwingend das Nutzpaket; er ist daher als "
    "Indikator der Kanalbelegung und nicht als Pfadverlustmaß zu lesen "
    "(siehe Abschnitt~\\ref{sec:evaluierungsziele})."
)

BURST_CAPTION_NOTE = (
    "Die Spalte „Fenster ab 50 ms“ bezeichnet den Anteil der 1-s-Fenster, in denen "
    "mindestens zwei aufeinanderfolgende Sequenzschritte ausfielen und der letzte "
    "Lichtzustand damit länger als 41,7 ms gehalten wurde."
)


def summary_frame(rows: list[dict], first_col: str) -> tuple[pd.DataFrame,
                                                             list[tuple[str, str]]]:
    """Select and rename the reported metrics; `first_col` is the label column."""
    keys = [key for key, _, _ in TABLE_COLUMNS]
    df = pd.DataFrame(rows)[[first_col] + keys]
    names = {key: f"{top} {bot}" for key, top, bot in TABLE_COLUMNS}
    header = [(first_col, "")] + [(top, bot) for _, top, bot in TABLE_COLUMNS]
    return df.rename(columns=names), header


# ---------------------------------------------------------------------------
#  Test: packet loss / interference / longterm  (time series + comparison table)
# ---------------------------------------------------------------------------
_TIME_UNITS = {
    "s": ("Zeit [s]", 1_000.0),
    "h": ("Zeit [h]", 3_600_000.0),
}

_TEST_TITLES = {
    "packetloss":   "Paketverlust",
    "interference": "Störfestigkeit",
    "longterm":     "Langzeitstabilität",
}


def analyze_packet_loss(items: list[Measurement],
                        test: str, time_unit: str = "s") -> set[str]:
    print(f"[{test}] {len(items)} measurement(s)")
    xlabel, divisor = _TIME_UNITS[time_unit]
    title = _TEST_TITLES.get(test, test)
    summary = []
    fig, ax = plt.subplots(figsize=(7, 4))

    for label, _distance, df, seq_ms, _hidden in items:
        summary.append({"Szenario": label, **loss_metrics(df, seq_ms)})
        t = (df["t_ms"] - df["t_ms"].iloc[0]) / divisor
        ax.plot(t, df["win_recv_unique"], label=label, linewidth=1.0)

    ax.set_xlabel(xlabel)
    ax.set_ylabel("Empfangene Sequenzen je 1-s-Fenster")
    ax.set_ylim(bottom=0)
    ax.set_title(f"Empfangene Sequenzen über die Zeit ({title})")
    ax.legend()
    ax.grid(True, alpha=0.3)
    save_fig(fig, f"{test}_timeseries")

    save_full_metrics(summary, test, "Szenario",
                      caption=(f"Vollständige Kennzahlen der Messreihe {title}. Auswahl "
                               f"daraus in Tabelle~\\ref{{tab:{test}-summary}}."),
                      label=f"tab:{test}-full")
    frame, header = summary_frame(summary, "Szenario")
    save_table(frame, f"{test}_summary",
               caption=f"Messergebnisse {title}. {BURST_CAPTION_NOTE} {RSSI_CAPTION_NOTE}",
               label=f"tab:{test}-summary", tex_header=header)
    return {test}


# ---------------------------------------------------------------------------
#  Test: range  (loss + link margin versus distance)
# ---------------------------------------------------------------------------
#  Measured in two variants, evaluated identically: with line of sight and
#  "hidden", i.e. with the receiver behind a massive obstacle (see thesis
#  section 7.1). Both series are kept apart so that the distance curves are not
#  averaged over two fundamentally different propagation conditions.
HIDDEN_CAPTION_NOTE = (
    "Die Messreihe wurde ohne Sichtverbindung aufgenommen: Der Empfänger stand "
    "bei jedem Messpunkt hinter einem massiven Hindernis, sodass die Strecke "
    "ausschließlich über Beugung und Reflexionen überbrückt wurde."
)


def analyze_range(items: list[Measurement]) -> set[str]:
    """Evaluate the line-of-sight and the hidden (NLOS) series separately."""
    produced = set()
    for hidden, (prefix, variant) in RANGE_VARIANTS.items():
        subset = [item for item in items if item[4] == hidden]
        if subset and analyze_range_variant(subset, prefix, variant):
            produced.add(prefix)
    return produced


def analyze_range_variant(items: list[Measurement], prefix: str, variant: str) -> bool:
    """Run the range evaluation for one variant; False if no usable point remained."""
    print(f"[{prefix}] {len(items)} distance point(s) ({variant})")
    rows = []
    for label, distance, df, seq_ms, _hidden in items:
        if distance is None:
            print(f"  WARN no distance in name '{label}' -- skipped")
            continue
        name = label if label.endswith(")") or " " in label else f"{distance:g} m"
        rows.append({"Messpunkt": name, "_distance": distance,
                     "_df": df, **loss_metrics(df, seq_ms)})
    if not rows:
        return False
    rows.sort(key=lambda r: (r["_distance"], r["Messpunkt"]))

    # Loss and link margin versus distance. RSSI is deliberately not plotted here
    # (it does not measure path loss, see RSSI_CAPTION_NOTE).
    fig, ax1 = plt.subplots(figsize=(7, 4))
    ax1.plot([r["_distance"] for r in rows], [r["loss_pct"] for r in rows],
             "o-", color="tab:red", label="Sequenzverlust")
    ax1.set_xlabel("Distanz [m]")
    ax1.set_ylabel("Sequenzverlust [%]", color="tab:red")
    ax1.set_ylim(bottom=0)
    ax1.tick_params(axis="y", labelcolor="tab:red")
    ax1.grid(True, alpha=0.3)

    ax2 = ax1.twinx()
    ax2.plot([r["_distance"] for r in rows], [r["recv_per_seq_mean"] for r in rows],
             "s--", color="tab:blue", label="Kopien je Sequenz")
    ax2.set_ylabel("Empfangene Kopien je Sequenz", color="tab:blue")
    ax2.set_ylim(0, 5.5)
    ax2.tick_params(axis="y", labelcolor="tab:blue")
    ax1.set_title(f"Reichweite ({variant}): Sequenzverlust und Empfangsredundanz "
                  "über die Distanz")
    save_fig(fig, f"{prefix}_distance")

    # Loss over time per measurement point: makes time-variant disturbances visible.
    fig, ax = plt.subplots(figsize=(7, 4))
    for r in rows:
        df = r["_df"]
        ax.plot((df["t_ms"] - df["t_ms"].iloc[0]) / 1000.0, df["win_loss_pct"],
                label=r["Messpunkt"], linewidth=0.9)
    ax.set_xlabel("Zeit [s]")
    ax.set_ylabel("Sequenzverlust je 1-s-Fenster [%]")
    ax.set_ylim(bottom=0)
    ax.set_title(f"Reichweite ({variant}): zeitlicher Verlauf des Sequenzverlusts")
    ax.legend(ncol=2, fontsize="small")
    ax.grid(True, alpha=0.3)
    save_fig(fig, f"{prefix}_timeseries")

    for r in rows:
        del r["_distance"], r["_df"]
    hidden_note = f" {HIDDEN_CAPTION_NOTE}" if prefix.endswith(HIDDEN_TOKEN) else ""
    save_full_metrics(rows, prefix, "Messpunkt",
                      caption=(f"Vollständige Kennzahlen der Reichweitenmessung ({variant}). "
                               f"Auswahl daraus in "
                               f"Tabelle~\\ref{{tab:{prefix.replace('_', '-')}-summary}}."),
                      label=f"tab:{prefix.replace('_', '-')}-full")
    frame, header = summary_frame(rows, "Messpunkt")
    save_table(frame, f"{prefix}_summary",
               caption=(f"Reichweitenmessung ({variant}): Sequenzverlust, Empfangsredundanz "
                        f"und Ausfalldauer je Messpunkt.{hidden_note} "
                        f"{BURST_CAPTION_NOTE} {RSSI_CAPTION_NOTE}"),
               label=f"tab:{prefix.replace('_', '-')}-summary", tex_header=header)
    return True


HANDLERS = {
    "packetloss":   lambda items: analyze_packet_loss(items, "packetloss"),
    "interference": lambda items: analyze_packet_loss(items, "interference"),
    "range":        analyze_range,
    "longterm":     lambda items: analyze_packet_loss(items, "longterm", time_unit="h"),
}


def main() -> None:
    files = sorted(RAW_DIR.glob("*.csv"))
    if not files:
        print(f"No CSV files found in {RAW_DIR}. Record measurements first "
              "(see README.md for the file-name convention).")
        return

    groups: dict[str, list[Measurement]] = defaultdict(list)
    for path in files:
        test, label, distance, hidden = parse_filename(path)
        if test is None:
            print(f"  SKIP '{path.name}': unknown test type (see file-name convention)")
            continue
        groups[test].append((label, distance, load_raw(path),
                             read_seq_interval_ms(path), hidden))

    produced: set[str] = set()
    for test, items in groups.items():
        produced |= HANDLERS[test](items)

    drop_stale_outputs(produced)

    print("\nDone. Tables in tables/, plots in figures/.")


if __name__ == "__main__":
    main()

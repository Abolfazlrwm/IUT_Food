#!/usr/bin/env python3
"""Render docs/images/benchmark.webp as a before/after comparison.

Reads docs/benchmark-results.json (the current release) and
docs/benchmark-results-baseline.json (the same workload measured on
the previous release), and draws their latency percentiles side by
side so the effect of the v2.4 data-layer work is visible at a glance.
"""
import json
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from PIL import Image

DOCS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs")

with open(f"{DOCS}/benchmark-results.json", encoding="utf-8") as fh:
    new = json.load(fh)
with open(f"{DOCS}/benchmark-results-baseline.json", encoding="utf-8") as fh:
    old = json.load(fh)

keys = ["p50", "p90", "p95", "p99"]
labels = ["p50", "p90", "p95", "p99"]
old_vals = [old["latency_ms"][k] for k in keys]
new_vals = [new["latency_ms"][k] for k in keys]

fig, ax = plt.subplots(figsize=(7.2, 4.2), dpi=110, constrained_layout=True)
x = range(len(labels))
width = 0.38
b_old = ax.bar([i - width / 2 for i in x], old_vals, width,
               label=f"v2.3 baseline ({old['requests_per_second']:g} req/s)",
               color="#64748b", edgecolor="#1e293b", linewidth=0.6)
b_new = ax.bar([i + width / 2 for i in x], new_vals, width,
               label=f"v2.4 - WAL + statement reuse ({new['requests_per_second']:g} req/s)",
               color="#f97316", edgecolor="#1e293b", linewidth=0.6)

for bars in (b_old, b_new):
    for bar in bars:
        ax.annotate(f"{bar.get_height():g}",
                    (bar.get_x() + bar.get_width() / 2, bar.get_height()),
                    textcoords="offset points", xytext=(0, 4),
                    ha="center", fontsize=9)

ax.set_xticks(list(x), labels)
ax.set_ylabel("round-trip latency (ms)")
ax.set_title(
    f"IUT-Food server latency - {new['clients']} concurrent clients, "
    f"{new['total_requests']} requests", fontsize=11)
ax.grid(axis="y", alpha=0.3)
ax.legend(loc="upper left", bbox_to_anchor=(0.0, 1.0), framealpha=0.9)

fig.savefig("/tmp/benchmark.png")
Image.open("/tmp/benchmark.png").convert("RGB").save(
    f"{DOCS}/images/benchmark.webp", "WEBP", quality=88)
print("saved docs/images/benchmark.webp")

#!/usr/bin/env python3
"""Render docs/images/benchmark.webp from docs/benchmark-results.json."""
import json

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from PIL import Image

with open("/home/z/my-project/work/IUT-Food/docs/benchmark-results.json",
          encoding="utf-8") as fh:
    r = json.load(fh)
lat = r["latency_ms"]

labels = ["p50", "p90", "p95", "p99", "max (login stampede)"]
values = [lat["p50"], lat["p90"], lat["p95"], lat["p99"], lat["max"]]
colors = ["#f97316", "#fb923c", "#fdba74", "#fed7aa", "#e2e8f0"]

fig, ax = plt.subplots(figsize=(7.2, 4.2), dpi=110, constrained_layout=True)
bars = ax.bar(labels, values, color=colors, edgecolor="#1e293b", linewidth=0.6)
ax.set_yscale("log")
ax.set_ylabel("round-trip latency (ms, log scale)")
ax.set_title(
    f"IUT-Food server latency — {r['clients']} concurrent clients, "
    f"{r['total_requests']} requests, {r['requests_per_second']} req/s",
    fontsize=11)
ax.grid(axis="y", alpha=0.3)
for bar, v in zip(bars, values):
    ax.annotate(f"{v:g}", (bar.get_x() + bar.get_width() / 2, v),
                textcoords="offset points", xytext=(0, 4),
                ha="center", fontsize=9)
fig.savefig("/tmp/benchmark.png")
Image.open("/tmp/benchmark.png").convert("RGB").save(
    "/home/z/my-project/work/IUT-Food/docs/images/benchmark.webp", "WEBP", quality=88)
print("saved docs/images/benchmark.webp")

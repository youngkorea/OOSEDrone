import csv
import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

results_dir = os.path.expanduser("~/OOSEDrone/simulations/results")
csv_file = os.path.join(results_dir, "summary_avg.csv")

# Read CSV
data = {"AOOSE": {}, "COOSE": {}, "Baseline": {}}
with open(csv_file, "r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        scheme = row["scheme"]
        db = int(row["dbSize"])
        data[scheme][db] = {
            "offTotal": float(row["avg_offTotal_ms"]),
            "retrieval": float(row["avg_retrieval_us"]),
            "totalOnline": float(row["avg_totalOnline_ms"]),
            "numSent": float(row["avg_numSent"]),
            "rcvd": float(row["avg_rcvd"]),
            "pdr": float(row["avg_pdr_pct"]),
            "throughput": float(row["avg_throughput_pps"]),
        }

db_sizes = [10, 100, 200, 400, 600, 1000]
colors = {"AOOSE": "#05967E", "COOSE": "#4573D7", "Baseline": "#DC6626"}
markers = {"AOOSE": "o", "COOSE": "s", "Baseline": "^"}
labels = {"AOOSE": "AOOSE", "COOSE": "COOSE", "Baseline": "Baseline (Sun et al.)"}

def get_vals(scheme, key):
    return [data[scheme][db][key] for db in db_sizes]

def plot_line(ax, key, ylabel, title):
    for s in ["AOOSE", "COOSE", "Baseline"]:
        ax.plot(db_sizes, get_vals(s, key), f"-{markers[s]}", color=colors[s],
                label=labels[s], linewidth=2, markersize=7)
    ax.set_xlabel("DB Size (Number of OffSigncrypt Outputs)", fontsize=12)
    ax.set_ylabel(ylabel, fontsize=12)
    ax.set_title(title, fontsize=13, fontweight="bold")
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_xticks(db_sizes)

# ---- (a) OffSigncrypt Total Time ----
fig, ax = plt.subplots(figsize=(8, 5))
plot_line(ax, "offTotal", "Total OffSigncrypt Time (ms)", "(a) Pre-flight OffSigncrypt Latency")
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_a_offsigncrypt.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_a_offsigncrypt.png")

# ---- (b) Token Retrieval Time ----
fig, ax = plt.subplots(figsize=(8, 5))
plot_line(ax, "retrieval", "Retrieval Time (us)", "(b) OffSigncrypt Output Retrieval Time")
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_b_retrieval.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_b_retrieval.png")

# ---- (c) Total Online Phase Latency ----
fig, ax = plt.subplots(figsize=(8, 5))
plot_line(ax, "totalOnline", "Online Latency (ms)", "(c) Online Phase Latency (Retrieval + OnSigncrypt)")
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_c_online_latency.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_c_online_latency.png")

# ---- (d) Sender Throughput ----
fig, ax = plt.subplots(figsize=(8, 5))
plot_line(ax, "throughput", "Throughput (packets/s)", "(d) Sender Throughput under IEEE 802.11")
ax.axhline(y=10, color="gray", linestyle="--", linewidth=1, alpha=0.5, label="Max rate (10 pkt/s)")
ax.legend(fontsize=10)
ax.set_ylim([1, 11])
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_d_throughput.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_d_throughput.png")

# ---- (e) Packet Delivery Ratio ----
fig, ax = plt.subplots(figsize=(8, 5))
plot_line(ax, "pdr", "Packet Delivery Ratio (%)", "(e) Packet Delivery Ratio")
ax.set_ylim([10, 75])
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_e_pdr.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_e_pdr.png")

# ---- (f) Total Latency: OffSigncrypt + Online ----
fig, ax = plt.subplots(figsize=(8, 5))
for s in ["AOOSE", "COOSE", "Baseline"]:
    e2e = [data[s][db]["offTotal"] + data[s][db]["totalOnline"] for db in db_sizes]
    ax.plot(db_sizes, e2e, f"-{markers[s]}", color=colors[s],
            label=labels[s], linewidth=2, markersize=7)
ax.set_xlabel("DB Size (Number of OffSigncrypt Outputs)", fontsize=12)
ax.set_ylabel("Total Latency (ms)", fontsize=12)
ax.set_title("(f) Total Latency: OffSigncrypt + Online Phase", fontsize=13, fontweight="bold")
ax.legend(fontsize=10)
ax.grid(True, alpha=0.3)
ax.set_xticks(db_sizes)
plt.tight_layout()
plt.savefig(os.path.join(results_dir, "graph_f_total_Latency.png"), dpi=300, bbox_inches="tight")
plt.close()
print("Saved: graph_f_total_Latency.png")

print("\nAll graphs saved to:", results_dir)
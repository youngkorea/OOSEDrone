import csv
import os

results_dir = os.path.expanduser("~/OOSEDrone/simulations/results")
output_file = os.path.join(results_dir, "summary_avg.csv")

configs = [
    "AOOSE_N10", "AOOSE_N100", "AOOSE_N200", "AOOSE_N400", "AOOSE_N600", "AOOSE_N1000",
    "COOSE_N10", "COOSE_N100", "COOSE_N200", "COOSE_N400", "COOSE_N600", "COOSE_N1000",
    "Baseline_N10", "Baseline_N100", "Baseline_N200", "Baseline_N400", "Baseline_N600", "Baseline_N1000",
]

num_repeats = 2
rows = []

for config in configs:
    parts = config.split("_")
    scheme = parts[0]
    dbSize = int(parts[1][1:])

    # Collect values from each repeat
    all_numSent = []
    all_offTotal = []
    all_retrieval = []
    all_totalOnline = []
    all_rcvd = []

    for r in range(num_repeats):
        sca_file = os.path.join(results_dir, f"{config}-#{r}.sca")
        if not os.path.exists(sca_file):
            print(f"  WARNING: {sca_file} not found, skipping")
            continue

        numSent = 0
        offTotal = 0
        retrieval_mean = 0
        totalOnline_mean = 0
        rcvd = 0

        with open(sca_file, "r") as f:
            for line in f:
                if "numSent" in line and "scalar" in line and "app" in line:
                    numSent = int(float(line.strip().split()[-1]))
                elif "offSigncryptTotalTime" in line and "scalar" in line and "app" in line:
                    offTotal = float(line.strip().split()[-1]) * 1000  # to ms
                elif "totalOnlineTime:mean" in line and "app" in line:
                    totalOnline_mean = float(line.strip().split()[-1]) * 1000  # to ms
                elif "retrievalTime:mean" in line and "app" in line:
                    retrieval_mean = float(line.strip().split()[-1]) * 1e6  # to us
                elif "packetReceived:count" in line and "receiver" in line and "app" in line:
                    rcvd += int(float(line.strip().split()[-1]))

        all_numSent.append(numSent)
        all_offTotal.append(offTotal)
        all_retrieval.append(retrieval_mean)
        all_totalOnline.append(totalOnline_mean)
        all_rcvd.append(rcvd)

    if not all_numSent:
        print(f"{config}: NO DATA")
        continue

    n = len(all_numSent)
    avg_numSent = sum(all_numSent) / n
    avg_offTotal = sum(all_offTotal) / n
    avg_retrieval = sum(all_retrieval) / n
    avg_totalOnline = sum(all_totalOnline) / n
    avg_rcvd = sum(all_rcvd) / n
    avg_pdr = (avg_rcvd / avg_numSent * 100) if avg_numSent > 0 else 0
    avg_throughput = avg_numSent / 300.0

    rows.append({
        "config": config,
        "scheme": scheme,
        "dbSize": dbSize,
        "repeats": n,
        "avg_numSent": round(avg_numSent, 1),
        "avg_offTotal_ms": round(avg_offTotal, 4),
        "avg_retrieval_us": round(avg_retrieval, 6),
        "avg_totalOnline_ms": round(avg_totalOnline, 6),
        "avg_rcvd": round(avg_rcvd, 1),
        "avg_pdr_pct": round(avg_pdr, 2),
        "avg_throughput_pps": round(avg_throughput, 4),
    })

    print(f"{config} (x{n}): sent={avg_numSent:.0f}, off={avg_offTotal:.1f}ms, "
          f"ret={avg_retrieval:.4f}us, online={avg_totalOnline:.4f}ms, "
          f"rcvd={avg_rcvd:.0f}, pdr={avg_pdr:.1f}%, tput={avg_throughput:.2f}pps")

with open(output_file, "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)

print(f"\nSaved to: {output_file}")
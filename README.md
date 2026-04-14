# OOSEDrone

OMNeT++/INET network simulation for evaluating ID-based on/offline signcryption schemes in Internet of Drones (IoD) environments.


## Overview

This project provides a network-level performance evaluation of three ID-based on/offline signcryption schemes using the OMNeT++ discrete event simulator. The simulation demonstrates the practical advantage of O(1) token(OffSigncrypt output) retrieval (AOOSE/COOSE) over O(n) retrieval (Baseline) as the number of pre-loaded OffSigncrypt outputs increases.


## Schemes Compared

| Scheme | Token Retrieval | OffSigncrypt Output | Ciphertext Size |
|--------|----------------|---------------------|-----------------|
| AOOSE | O(1) stack pop | 188 bytes | 406 bytes |
| COOSE | O(1) stack pop | 346 bytes | 406 bytes |
| Baseline (Sun et al.) | O(n) linear scan | 220 bytes | 366 bytes |

- **AOOSE/COOSE**: Chameleon hash enables message-independent generation of OffSigncrypt outputs. Any outputs can be used for any message, allowing O(1) retrieval via stack pop.
- **Baseline**: OffSigncrypt outputs are message-dependent. OnSigncrypt requires searching for a specific R value, resulting in O(n) linear scan over the token database.


## Project Structure
```
OOSEDrone/
├── src/                              # OMNeT++ simulation source
│   ├── SigncryptApp.ned              # Module parameter definitions (NED)
│   ├── SigncryptApp.cc               # Core simulation logic (C++)
│   └── package.ned                   # Package declaration
│
├── simulations/                      # Simulation configuration
│   ├── OOSEDroneNetwork.ned          # Network topology definition
│   ├── omnetpp.ini                   # Experiment configurations (18 configs)
│   ├── droneMission.xml              # Sender drone flight path (TurtleMobility)
│   ├── package.ned                   # Package declaration
│   ├── extract_results.py            # Result extraction script (repeat averaging)
│   ├── plot_results.py               # Graph generation (matplotlib)
│   └── results/                      # Output graphs and CSV
│       ├── summary_avg.csv           # Aggregated simulation results
│       └── graph_*.png               # Result graphs (a-f)
│
├── benchmark/                        # C benchmark implementations
│   ├── aoose/                        # AOOSE scheme benchmark
│   │   ├── main.c                    # OffSigncrypt/OnSigncrypt/UnSigncrypt timing
│   │   └── a.param                   # PBC pairing parameters
│   ├── coose/                        # COOSE scheme benchmark
│   │   ├── main.c
│   │   └── a.param
│   ├── baseline/                     # Baseline (Sun et al.) benchmark
│   │   ├── main.c
│   │   └── a.param
│   └── retrieval/                    # Token retrieval benchmark
│       ├── retrieval_benchmark.c     # O(1) vs O(n) retrieval timing
│       ├── retrieval_benchmark.csv   # Retrieval timing results
│       ├── retrieval_plot_results.py # Retrieval graph generation
│       └── space_complexity_line.png # Memory usage comparison based on retrieval algorithms
│
├── .gitignore
└── README.md
```

## Requirements

- **OMNeT++** 6.0.3
- **INET Framework** 4.5.x
- **PBC Library** 1.0.0 (for benchmarks)
- **Python 3** with matplotlib (for graph generation)


## Network Topology
```
Ground Station (GCS)         Receiver Drones [0..9]
|                            /  |     |
| OffSigncrypt              /   |     |
| Outputs preload          /    |     |
v                         v     v     v
Sender Drone  ──────────>  receiver[i]
(TurtleMobility)          (MassMobility)
OnSigncrypt + send         UnSigncrypt
```

- **Ground Station**: Pre-flight OffSigncrypt, loads token(OffSigncrypt output) DB onto sender drone
- **Sender Drone**: Flies the waypoint mission, performs OnSigncrypt per message, dynamically selects the receiver
- **Receiver Drones**: Fly at random positions, receive and UnSigncrypt ciphertext
- **Wireless**: IEEE 802.11 Ad-hoc mode with GlobalArp


## Build

```bash
cd src
opp_makemake -f --deep \
  -KINET_PROJ=../../inet4.5 \
  -DINET_IMPORT \
  -I'$(INET_PROJ)/src' \
  -L'$(INET_PROJ)/src' \
  -lINET'$(D)'
make MODE=release -j$(nproc)
```


## Run

### GUI mode
```bash
cd simulations
../src/out/clang-release/src \
  -n .:../src:/path/to/inet4.5/src \
  -l /path/to/inet4.5/src/INET \
  -c AOOSE_N100 omnetpp.ini
```


### Batch mode (all 18 experiments)
```bash
cd simulations
for config in AOOSE_N10 AOOSE_N100 AOOSE_N200 AOOSE_N400 AOOSE_N600 AOOSE_N1000 \
              COOSE_N10 COOSE_N100 COOSE_N200 COOSE_N400 COOSE_N600 COOSE_N1000 \
              Baseline_N10 Baseline_N100 Baseline_N200 Baseline_N400 Baseline_N600 Baseline_N1000; do
    ../src/out/clang-release/src -u Cmdenv \
        -n .:../src:/path/to/inet4.5/src \
        -l /path/to/inet4.5/src/INET \
        -c $config omnetpp.ini
done
```


### Extract results and plot
```bash
python3 extract_results.py
python3 plot_results.py
```


## Simulation Parameters

| Parameter | Value |
|-----------|-------|
| Network | IEEE 802.11 Ad-hoc |
| Sender mobility | TurtleMobility (waypoint-based) |
| Receiver mobility | MassMobility (Random Movement) |
| Number of receivers | 10 |
| Simulation time | 300s |
| Send interval | 0.1s |
| DB sizes | 10, 100, 200, 400, 600, 1000 |
| Repeat per config | Configurable (default: 2) |
| ARP | GlobalArp |


## Metrics

| Metric | Description |
|--------|-------------|
| OffSigncrypt delay | Total pre-flight token generation time (dbSize × per-token time) |
| Token retrieval time | Wall-clock measured: O(1) stack pop vs O(n) linear scan |
| Online phase latency | Token retrieval + OnSigncrypt computation time |
| Sender throughput | Packets successfully sent per second |
| Packet delivery ratio | Packets received by receivers/packets sent by sender |
| Total latency | OffSigncrypt delay + online phase latency |


## Benchmark

Cryptographic operation times are measured using C implementations with the PBC (Pairing-Based Cryptography) library. Each benchmark iterates the scheme N times and records the average execution time for OffSigncrypt, OnSigncrypt, and UnSigncrypt. For the retrieval benchmark, the space complexity is measured since additional memory overheads can be introduced by each algorithm as the DB size increases.


# Build benchmark (requires PBC library)
```bash
cd benchmark/aoose
gcc -o main main.c -lpbc -lgmp
./main
```

## License

LGPL-3.0

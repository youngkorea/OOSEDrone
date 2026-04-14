import matplotlib.pyplot as plt
import numpy as np

plt.rcParams.update({
    'font.size': 12,
    'axes.titlesize': 14,
    'axes.labelsize': 12,
    'xtick.labelsize': 11,
    'ytick.labelsize': 11,
    'legend.fontsize': 10,
    'figure.dpi': 300,
    'savefig.dpi': 300,
    'axes.grid': True,
    'grid.alpha': 0.3,
    'grid.linestyle': '--',
    'axes.axisbelow': True 
})

def plot_space_complexity_line():
    # Xticks (DB Size) 
    db_sizes = [1, 10, 100, 200, 400, 600, 1000]

    # Benchmark Datasets (KB)
    aoose   = [0.19, 1.91, 19.14, 38.28, 76.56, 114.84, 191.41]
    coose   = [0.35, 3.46, 34.57, 69.14, 138.28, 207.42, 345.70]
    bl_lin  = [0.22, 2.23, 22.27, 44.53, 89.06, 133.59, 222.66]
    bl_bs   = [0.24, 2.38, 23.83, 47.66, 95.31, 142.97, 238.28]
    bl_hmap = [0.25, 2.46, 24.61, 49.22, 98.44, 147.66, 246.09]

    # Graph Generation
    fig, ax = plt.subplots(figsize=(9, 5))

    # Plot Generation
    ax.plot(db_sizes, aoose,   marker='o', color="#05967E", label='AOOSE (Stack)', linewidth=2, markersize=7)
    ax.plot(db_sizes, coose,   marker='s', color="#4573D7", label='COOSE (Stack)', linewidth=2, markersize=7)
    ax.plot(db_sizes, bl_lin,  marker='^', color="#DC6626", label='Baseline (Linear)', linewidth=2, markersize=7)
    ax.plot(db_sizes, bl_bs,   marker='d', color="#8c564b", label='Baseline (Binary Search)', linewidth=2, markersize=7)
    ax.plot(db_sizes, bl_hmap, marker='x', color="#d62728", label='Baseline (Hash Map)', linewidth=2, markersize=7)

    ax.set_xscale('log')

    ax.set_xlabel('DB Size ($N$)')
    ax.set_ylabel('Memory Size (KB)')
    
    ax.set_xticks(db_sizes)
    ax.set_xticklabels([str(x) for x in db_sizes])
    
    ax.legend(loc='upper left')

    fig.tight_layout()

    # Save the Result as image
    filename = 'space_complexity_line.png'
    plt.savefig(filename)
    plt.show()
    
    print(f"Saved: space_complexity_line.png")

if __name__ == '__main__':
    plot_space_complexity_line()
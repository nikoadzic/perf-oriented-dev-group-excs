import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def main():
    try:
        df = pd.read_csv("benchmark_results.csv", comment="#")
    except FileNotFoundError:
        print("Error: benchmark_results.csv not found. Benchmark results must be in the same folder as this script.")
        return

    # Improve column names for legend
    df = df.rename(columns={
        "label": "Data Structure",
        "ins_del_percent": "Ins/Del Rate (%)"
    })
    df["Ins/Del Rate (%)"] = df["Ins/Del Rate (%)"].astype(str) + "%"

    # Get element sizes used in benchmark
    byte_sizes = sorted(df["bytes"].unique())
    n_subplots = len(byte_sizes)

    # Create subplots
    fig, axes = plt.subplots(1, n_subplots, figsize=(6 * n_subplots, 6), sharey=True)
    
    if n_subplots == 1:
        axes = [axes]

    for i, size in enumerate(byte_sizes):
        ax = axes[i]
        subset = df[df["bytes"] == size]
        
        sns.lineplot(
            data=subset,
            x="elements",
            y="ops_per_second",
            hue="Data Structure",
            style="Ins/Del Rate (%)",
            markers=True,
            ax=ax
        )
        
        ax.set_xscale("log")
        ax.set_yscale("log")
                
        ax.set_title(f"Element Size: {size} bytes", fontsize=18)
        ax.set_xlabel("Element Amount", fontsize=16)
        ax.set_ylabel("Ops per Second (log scale)", fontsize=16)
        ax.grid(True, which="major", alpha=0.3)

    plt.tight_layout()
    plt.savefig("benchmark_plot.png")
    print("Plot saved to benchmark_plot.png")

if __name__ == "__main__":
    main()

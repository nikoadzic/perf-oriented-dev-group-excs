#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.ticker import LogLocator, LogFormatterMathtext

def main():
    try:
        df = pd.read_csv("final_comparison_results.csv", comment="#")
    except FileNotFoundError:
        print("Error: final_comparison_results.csv not found.")
        return

    # Clean up column names for better legends
    df = df.rename(columns={
        "label": "Data Structure",
        "ins_del_percent": "Ins/Del Mix"
    })
    
    # Map technical labels to readable names
    ds_map = {
        "array": "Array (Baseline)",
        "tiered_array": "Tiered Array (Ours)",
        "linked_list_sequential": "List (Sequential Alloc)",
        "linked_list_random": "List (Random Alloc)"
    }
    df["Data Structure"] = df["Data Structure"].map(ds_map).fillna(df["Data Structure"])
    
    # Format insertion rates
    df["Ins/Del Mix"] = df["Ins/Del Mix"].astype(str) + "%"

    # Unique element sizes
    byte_sizes = sorted(df["bytes"].unique())
    n_subplots = len(byte_sizes)

    # Create subplots
    fig, axes = plt.subplots(1, n_subplots, figsize=(7 * n_subplots, 7), sharey=True)
    fig.suptitle('Exercise B: Final Comparison - Tiered Array vs Baselines', fontsize=20, y=1.05)
    
    if n_subplots == 1:
        axes = [axes]

    for i, size in enumerate(byte_sizes):
        ax = axes[i]
        subset = df[df["bytes"] == size]
        
        if subset.empty:
            continue
            
        sns.lineplot(
            data=subset,
            x="elements",
            y="ops_per_second",
            hue="Data Structure",
            style="Ins/Del Mix",
            markers=True,
            dashes=True,
            ax=ax
        )
        
        ax.set_xscale("log")
        ax.set_yscale("log")
                
        # Format Y-axis with clear powers of 10
        ax.yaxis.set_major_locator(LogLocator(base=10.0))
        ax.yaxis.set_major_formatter(LogFormatterMathtext())
        
        # Consistent title based on size
        size_label = "8B" if size == 8 else "512B" if size == 512 else "8MB"
        ax.set_title(f"Payload Size: {size_label}", fontsize=18)
        ax.set_xlabel("Number of Elements (N)", fontsize=14)
        
        if i == 0:
            ax.set_ylabel("Operations per Second", fontsize=14)
        else:
            ax.set_ylabel("")
            
        ax.grid(True, which="major", ls="--", alpha=0.5)
        
        # Consolidate the legend
        if i != n_subplots - 1:
            ax.get_legend().remove()
        else:
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', borderaxespad=0.)

    plt.tight_layout()
    output_name = "final_comparison_plot.png"
    plt.savefig(output_name, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Final plot saved to {output_name}")

if __name__ == "__main__":
    main()

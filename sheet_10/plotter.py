import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

def main():
    try:
        df = pd.read_csv("benchmark_results.csv", comment="#")
    except FileNotFoundError:
        print("Error: benchmark_results.csv not found. Benchmark results must be in the same folder as this script.")
        return

    # Create combined label for legend
    df["Configuration"] = df["label"] + " (" + df["access_pattern"] + ")"

    # Create faceted plot
    g = sns.relplot(
        data=df,
        x="elements",
        y="ops_per_second",
        hue="Configuration",
        col="ins_del_percent",
        row="bytes",
        kind="line",
        markers=True,
        dashes=False,
        facet_kws={'margin_titles': True}
    )

    # Set log scales for both axes and add grid
    g.set(xscale="log", yscale="log")
    for ax in g.axes.flat:
        ax.grid(True, which="major", alpha=0.3)

    g.set_axis_labels("Number of Elements", "Operations per Second")
    g.set_titles(col_template="Ins/Del: {col_name}%", row_template="Size: {row_name} bytes")

    plt.savefig("benchmark_plot.png")
    print("Plot saved to benchmark_plot.png")

if __name__ == "__main__":
    main()

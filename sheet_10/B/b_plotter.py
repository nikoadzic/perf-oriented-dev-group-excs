#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# 1. Load and clean the data
col_names = ['label', 'n_elem', 'elem_size_bytes', 'insdel_pct', 'ops_per_sec', 'checksum', 'chunk_size']
df = pd.read_csv('data/tuning_results_tiered.csv', names=col_names, comment='#')

if df.iloc[0]['label'] == 'label':
    df = df[1:].reset_index(drop=True)

for col in ['n_elem', 'elem_size_bytes', 'insdel_pct', 'ops_per_sec', 'chunk_size']:
    df[col] = pd.to_numeric(df[col])

# Create categorical labels for the legend
df['Chunk Size'] = df['chunk_size'].astype(str) + " elements"
df['Workload'] = df['insdel_pct'].astype(str) + "% Ins/Del"

elem_sizes = sorted(df['elem_size_bytes'].unique())

# 2. Setup a 1x3 plotting grid (1 row, 3 columns)
fig, axes = plt.subplots(1, len(elem_sizes), figsize=(18, 6), sharey=False)
fig.suptitle('Tiered Array: Chunk Size Tuning by Payload', fontsize=18, y=1.05)

# 3. Generate the 3 plots
for i, es in enumerate(elem_sizes):
    ax = axes[i]
    subset = df[df['elem_size_bytes'] == es]
    
    if not subset.empty:
        # Hue controls Color (Chunk Size), Style controls the line dash/markers (Workload)
        sns.lineplot(data=subset, x='n_elem', y='ops_per_sec', hue='Chunk Size', 
                     style='Workload', markers=True, dashes=True, ax=ax, errorbar=None)
    
    ax.set_xscale('log')
    ax.set_yscale('log')
    
    # Clean Titles and Labels
    es_label = "8 Bytes" if es == 8 else "512 Bytes" if es == 512 else "8 MB"
    ax.set_title(f'Payload Size: {es_label}')
    ax.set_xlabel('Number of Elements (N)')
    
    if i == 0:
        ax.set_ylabel('Operations per Second')
    else:
        ax.set_ylabel('')
        
    ax.grid(True, which="major", ls="--", alpha=0.5)
    
    # Consolidate the legend to the far right to keep the graphs clean
    if ax.get_legend():
        if i == len(elem_sizes) - 1:
            # Move the legend completely outside the last plot
            ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        else:
            ax.get_legend().remove()

# 4. Save the figure
plt.tight_layout()
output_filename = 'tiered_tuning_3plots.png'
plt.savefig(output_filename, dpi=300, bbox_inches='tight')
plt.close()

print(f"Plot successfully generated and saved to '{output_filename}'")

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
from pathlib import Path
import numpy as np

def find_project_root(start: Path) -> Path:
    """Finds the project root by looking for a .git directory."""
    for parent in [start] + list(start.parents):
        if (parent / ".git").exists():
            return parent
    raise FileNotFoundError("Project root with .git folder not found")

# Use a try-except block for robustness in different environments
try:
    PROJECT_ROOT = find_project_root(Path(__file__).resolve().parent)
except (FileNotFoundError, NameError):
    PROJECT_ROOT = Path('.').resolve()
    print("Warning: Could not find .git root. Using current directory as project root.")

# Define the output folder within the project structure
output_folder = PROJECT_ROOT / 'results/graphs'

def create_graphs_folder():
    """Creates the 'graphs' folder if it doesn't already exist."""
    os.makedirs(output_folder, exist_ok=True)
    print(f"Ensured output folder exists: {output_folder}")

def clean_data(df):
    """
    Cleans and prepares the DataFrame by handling missing values and converting types.
    """
    df.replace(['N/A', 'inf'], pd.NA, inplace=True)
    numeric_cols = [
        'num_processors', 'num_threads', 'task_factor',
        'solving_time', 'total_time', 'speedup', 'efficiency',
        'colors_removed'
    ]
    for col in numeric_cols:
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors='coerce')
            
    df.dropna(subset=['puzzle_name', 'implementation', 'total_time'], inplace=True)
    return df

def plot_precoloring_total_time_comparison(df, save_path):
    """
    Generates and saves a bar plot styled for a paper layout with horizontal labels,
    a log scale indicator in the title, and no background grid.

    Args:
        df (pd.DataFrame): The cleaned DataFrame containing the results.
        save_path (Path): The Path object for the output folder.
    """
    print("\n--- Generating Final Pre-coloring Comparison Plot ---")

    # 1. Filter the DataFrame for sequential implementation
    seq_df = df[df['implementation'] == 'seq'].copy()
    seq_df = seq_df[seq_df['total_time'] > 0]
    if seq_df.empty:
        print("No valid 'seq' data found. Skipping plot generation.")
        return

    # 2. Prepare data columns
    if 'colors_removed' not in seq_df.columns:
        print("Error: 'colors_removed' column not found.")
        return
    seq_df['colors_removed'] = seq_df['colors_removed'].fillna(0)
    seq_df['puzzle_name'] = seq_df['puzzle_name'].apply(os.path.basename)
    seq_df['Pre-coloring Status'] = np.where(
        seq_df['colors_removed'] > 0,
        'With Pre-coloring',
        'Without Pre-coloring'
    )

    # --- Plot Generation (Styled for Publication) ---
    plt.style.use('seaborn-v0_8-paper')
    fig, ax = plt.subplots(figsize=(5.0, 3.5))

    # Generate the bar plot
    barplot = sns.barplot(
        ax=ax,
        data=seq_df,
        x='puzzle_name',
        y='total_time',
        hue='Pre-coloring Status',
        palette={'Without Pre-coloring': 'skyblue', 'With Pre-coloring': 'coral'}
    )

    # --- Customization for Final Layout ---
    ax.set_yscale('log')
    
    ax.set_title('Pre-coloring Time Comparison (Log Scale)', fontsize=10)
    
    ax.set_xlabel('Puzzle Name', fontsize=8)
    ax.set_ylabel('Total Time (s)', fontsize=8)
    
    ax.tick_params(axis='x', labelsize=6, rotation=0)
    ax.tick_params(axis='y', labelsize=7)
    
    ax.legend(title=None, loc='upper right', fontsize=6)

    # Annotations on bars
    for p in barplot.patches:
        if p.get_height() > 0:
            barplot.annotate(format(p.get_height(), '.1f'),
                           (p.get_x() + p.get_width() / 2., p.get_height()),
                           ha='center', va='center',
                           xytext=(0, 5),
                           textcoords='offset points',
                           fontsize=5)

    # --- Save the Plot to File ---
    output_filename = save_path / 'precoloring_comparison.pdf'
    fig.tight_layout()
    plt.savefig(output_filename, dpi=300)
    plt.close(fig)
    
    print(f"Plot successfully saved to: {output_filename}")


def main():
    """Main function to read the CSV, create folders, and generate the plot."""
    results_file = PROJECT_ROOT / 'results/results_dataset.csv'
    
    try:
        df_raw = pd.read_csv(results_file)
    except FileNotFoundError:
        print(f"Error: '{results_file}' not found. Please ensure the CSV file is present.")
        return

    create_graphs_folder()
    df_cleaned = clean_data(df_raw)

    plot_precoloring_total_time_comparison(df_cleaned, output_folder)

    print(f"\nTask complete. Plot has been generated in the '{output_folder}' folder.")

if __name__ == '__main__':
    main()
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import os
from pathlib import Path
import numpy as np
import argparse

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

def plot_precoloring_total_time_comparison(df, save_path, dimension=None):
    """
    Generates and saves a bar plot, optionally filtering for a specific puzzle dimension.

    Args:
        df (pd.DataFrame): The cleaned DataFrame containing the results.
        save_path (Path): The Path object for the output folder.
        dimension (int, optional): The puzzle dimension to filter for (e.g., 9 for 9x9).
    """
    print("\n--- Generating Plot ---")

    # Set Custom Plot Style using rcParams
    plt.rcParams.update({
        'figure.figsize': (3.5, 2.8),
        'font.size': 8,
        'axes.titlesize': 10,
        'axes.labelsize': 8,
        'xtick.labelsize': 8,
        'ytick.labelsize': 8,
        'legend.fontsize': 7,
        'lines.linewidth': 1,
        'lines.markersize': 2.6,
        'savefig.bbox': 'tight'
    })

    # 1. Filter and prepare data
    seq_df = df[df['implementation'] == 'seq'].copy()
    seq_df = seq_df[seq_df['total_time'] > 0]
    if 'colors_removed' not in seq_df.columns:
        print("Error: 'colors_removed' column not found.")
        plt.rcdefaults()
        return

    # Clean puzzle names FIRST
    seq_df['puzzle_name'] = seq_df['puzzle_name'].apply(os.path.basename).str.replace('.txt', '', regex=False)

    # *** Conditionally filter by dimension ***
    if dimension:
        pattern = f"{dimension}x{dimension}"
        print(f"Filtering for puzzles starting with '{pattern}'...")
        seq_df = seq_df[seq_df['puzzle_name'].str.startswith(pattern)]
    else:
        print("No dimension provided. Using all sequential puzzles.")

    # Check if any data remains after filtering
    if seq_df.empty:
        print("No data found for the specified criteria. Skipping plot generation.")
        plt.rcdefaults()
        return

    seq_df['colors_removed'] = seq_df['colors_removed'].fillna(0)
    seq_df['Pre-coloring Status'] = np.where(
        seq_df['colors_removed'] > 0,
        'With Pre-coloring',
        'Without Pre-coloring'
    )

    # --- Plot Generation ---
    fig, ax = plt.subplots()
    barplot = sns.barplot(
        ax=ax,
        data=seq_df,
        x='puzzle_name',
        y='total_time',
        hue='Pre-coloring Status',
        palette={'Without Pre-coloring': 'skyblue', 'With Pre-coloring': 'coral'}
    )

    # --- Customization ---
    ax.set_yscale('log')
    ax.set_title('Pre-coloring Time Comparison (Log Scale)')
    ax.set_xlabel('Puzzle Name')
    ax.set_ylabel('Total Time (s)')
    ax.tick_params(axis='x', rotation=90)
    ax.legend(title=None, loc='upper right')

    for p in barplot.patches:
        if p.get_height() > 0:
            barplot.annotate(format(p.get_height(), '.1f'),
                           (p.get_x() + p.get_width() / 2., p.get_height()),
                           ha='center', va='center',
                           xytext=(0, 4), textcoords='offset points', fontsize=5)

    # --- Save the Plot to File with a dynamic name ---
    filename_suffix = f"{dimension}x{dimension}" if dimension else "all_puzzles"
    output_filename = save_path / f'precoloring_comparison_{filename_suffix}.pdf'
    
    plt.savefig(output_filename, dpi=300)
    plt.close(fig)
    plt.rcdefaults()
    
    print(f"Plot successfully saved to: {output_filename}")


def main(dimension_arg):
    """Main function to read the CSV, create folders, and generate the plot."""
    results_file = PROJECT_ROOT / 'results/results_dataset.csv'
    
    try:
        df_raw = pd.read_csv(results_file)
    except FileNotFoundError:
        print(f"Error: '{results_file}' not found. Please ensure the CSV file is present.")
        return

    create_graphs_folder()
    df_cleaned = clean_data(df_raw)

    # Pass the dimension argument to the plotting function
    plot_precoloring_total_time_comparison(df_cleaned, output_folder, dimension=dimension_arg)

    print(f"\nTask complete.")

if __name__ == '__main__':
    # --- Set up command-line argument parsing ---
    parser = argparse.ArgumentParser(description="Generate a comparison plot for puzzle solving times.")
    parser.add_argument(
        '-d', '--dimension',
        type=int,
        default=None, # Default to None if not provided
        help="Filter puzzles by a specific dimension (e.g., provide 9 for 9x9 puzzles)."
    )
    args = parser.parse_args()

    # Call main with the parsed dimension
    main(dimension_arg=args.dimension)
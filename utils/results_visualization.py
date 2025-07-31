import pandas as pd
import matplotlib.pyplot as plt
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
    
output_folder = PROJECT_ROOT / 'results/graphs'

def create_graphs_folder():
    """Creates the 'graphs' folder if it doesn't already exist."""
    os.makedirs(output_folder, exist_ok=True)
    print(f"Ensured output folder exists: {output_folder}")

def filter_by_job_id(df):
    """
    Filters out duplicate runs for the same configuration, keeping the one with the highest job_id.
    """
    print("Filtering duplicate runs, keeping latest job_id...")
    df = df.sort_values('job_id', ascending=False)
    config_cols = ['puzzle_name', 'implementation', 'test_type', 'num_processors', 'num_threads', 'task_factor']
    df_filtered = df.drop_duplicates(subset=config_cols, keep='first')
    print(f"Removed {len(df) - len(df_filtered)} duplicate row(s).")
    return df_filtered

def clean_data(df):
    """
    Cleans and prepares the DataFrame by handling missing values and converting types.
    """
    df.replace('N/A', pd.NA, inplace=True)
    numeric_cols = [
        'num_processors', 'num_threads', 'task_factor', 
        'solving_time', 'total_time', 'speedup', 'efficiency'
    ]
    for col in numeric_cols:
        df[col] = pd.to_numeric(df[col], errors='coerce')
    # Ensure all necessary columns for plotting have valid data
    df.dropna(subset=['puzzle_name', 'implementation', 'solving_time', 'speedup', 'efficiency'], inplace=True)
    return df

def get_best_scaling_results(df):
    """
    Filters for 'scaling' tests and then finds the best-performing run (minimum solving time)
    for each combination of puzzle, implementation, and number of computational units.
    """
    print("Filtering for 'scaling' tests and finding the best run for each configuration...")
    df_scaling = df[df['test_type'] == 'scaling'].copy()

    mpi_best = df_scaling[df_scaling['implementation'] == 'mpi'].copy()
    if not mpi_best.empty:
        mpi_best = mpi_best.loc[mpi_best.groupby(['puzzle_name', 'num_processors'])['solving_time'].idxmin()]

    omp_best = df_scaling[df_scaling['implementation'] == 'omp'].copy()
    if not omp_best.empty:
        omp_best = omp_best.loc[omp_best.groupby(['puzzle_name', 'num_threads'])['solving_time'].idxmin()]
        
    hybrid_best = df_scaling[df_scaling['implementation'] == 'hybrid'].copy()
    if not hybrid_best.empty:
        hybrid_best = hybrid_best.loc[hybrid_best.groupby(['puzzle_name', 'num_processors', 'num_threads'])['solving_time'].idxmin()]

    return pd.concat([mpi_best, omp_best, hybrid_best]).sort_index()

def _add_computational_units(df):
    """Helper function to add a 'computational_units' column."""
    df_copy = df.copy()
    conditions = [
        (df_copy['implementation'] == 'hybrid'),
        (df_copy['implementation'] == 'mpi'),
        (df_copy['implementation'] == 'omp')
    ]
    choices = [
        df_copy['num_processors'] * df_copy['num_threads'],
        df_copy['num_processors'],
        df_copy['num_threads']
    ]
    df_copy['computational_units'] = np.select(conditions, choices, default=1)
    return df_copy

def plot_comparison_efficiency(df):
    """Plots a comparison of efficiency vs. total computational units for all implementations."""
    print("Generating comparison plots for efficiency...")
    
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange', 'hybrid': 'C2'}

    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        plt.figure(figsize=(12, 8))
        
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                plt.plot(sorted_df['computational_units'], sorted_df['efficiency'], 
                         marker='o', linestyle='-', label=impl.upper(), color=color_map.get(impl, 'black'))

        plt.title(f'Efficiency vs. Computational Units for {os.path.basename(puzzle)}')
        plt.xlabel('Total Computational Units (Cores)')
        plt.ylabel('Efficiency')
        plt.grid(True, which='both', linestyle='--')
        plt.legend()
        plt.savefig(output_folder / f'comparison_efficiency_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_factor_solving_time(df):
    """Plots Solving Time vs. Task Factor, with a line for each processor/thread count."""
    print("Generating Task Factor analysis plots...")
    # MPI
    for puzzle, puzzle_df in df[df['implementation'] == 'mpi'].groupby('puzzle_name'):
        plt.figure(figsize=(12, 8))
        for procs in sorted(puzzle_df['num_processors'].unique()):
            proc_df = puzzle_df[puzzle_df['num_processors'] == procs].sort_values('task_factor')
            if not proc_df.empty:
                plt.plot(proc_df['task_factor'], proc_df['solving_time'], marker='o', linestyle='-', label=f'{procs} Processors')
        plt.title(f'MPI Solving Time vs. Task Factor for {os.path.basename(puzzle)}')
        plt.xlabel('Task Factor')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        if len(puzzle_df['num_processors'].unique()) > 1:
            plt.legend()
        plt.savefig(output_folder / f'factor_solving_time_mpi_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()
    
    # OpenMP
    for puzzle, puzzle_df in df[df['implementation'] == 'omp'].groupby('puzzle_name'):
        plt.figure(figsize=(12, 8))
        for threads in sorted(puzzle_df['num_threads'].unique()):
            thread_df = puzzle_df[puzzle_df['num_threads'] == threads].sort_values('task_factor')
            if not thread_df.empty:
                plt.plot(thread_df['task_factor'], thread_df['solving_time'], marker='o', linestyle='-', label=f'{threads} Threads')
        plt.title(f'OpenMP Solving Time vs. Task Factor for {os.path.basename(puzzle)}')
        plt.xlabel('Task Factor')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        if len(puzzle_df['num_threads'].unique()) > 1:
            plt.legend()
        plt.savefig(output_folder / f'factor_solving_time_omp_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_comparison_solving_time(df):
    """Plots a comparison of solving time vs. total computational units for all implementations."""
    print("Generating comparison plots for solving time...")
    
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange', 'hybrid': 'C2'}
    
    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        plt.figure(figsize=(12, 8))
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                plt.plot(sorted_df['computational_units'], sorted_df['solving_time'], 
                         marker='o', linestyle='-', label=impl.upper(), color=color_map.get(impl, 'black'))
        
        plt.title(f'Solving Time vs. Computational Units for {os.path.basename(puzzle)}')
        plt.xlabel('Total Computational Units (Cores)')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        plt.legend()
        plt.savefig(output_folder / f'comparison_solving_time_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_comparison_speedup(df):
    """Plots a comparison of speedup vs. total computational units, with an ideal line drawn within the bounds of the data."""
    print("Generating comparison plots for speedup...")
    
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange', 'hybrid': 'C2'}
    
    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        fig, ax = plt.subplots(figsize=(12, 8))
        
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                ax.plot(sorted_df['computational_units'], sorted_df['speedup'], 
                        marker='o', linestyle='-', label=f'{impl.upper()} Speedup', 
                        color=color_map.get(impl, 'black'), zorder=2)

        xlim = ax.get_xlim()
        ylim = ax.get_ylim()
        start_point = max(xlim[0], ylim[0])
        end_point = min(xlim[1], ylim[1])
        
        ax.plot([start_point, end_point], [start_point, end_point], 
                color='red', linestyle='--', label='Ideal Speedup', zorder=1)

        ax.set_xlim(xlim)
        ax.set_ylim(ylim)
        
        ax.set_title(f'Speedup vs. Computational Units for {os.path.basename(puzzle)}')
        ax.set_xlabel('Total Computational Units (Cores)')
        ax.set_ylabel('Speedup')
        ax.grid(True, which='both', linestyle='--')
        ax.legend()
        
        # *** BUG FIX ***: Changed the filename to correctly save the speedup plot.
        plt.savefig(output_folder / f'comparison_speedup_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close(fig)

def main():
    """Main function to read the CSV, create folders, and generate all plots."""
    results_file = PROJECT_ROOT / 'results/results_dataset.csv'
    try:
        df_raw = pd.read_csv(results_file)
    except FileNotFoundError:
        print(f"Error: '{results_file}' not found. Please ensure the CSV file from the parser is present.")
        return

    create_graphs_folder()
    
    df_filtered = filter_by_job_id(df_raw)
    df_cleaned = clean_data(df_filtered)

    # Separate data for scaling analysis vs. factor analysis
    df_scaling = get_best_scaling_results(df_cleaned)
    df_factor = df_cleaned[df_cleaned['test_type'] == 'factor'].copy()

    # Generate all requested plots
    if not df_scaling.empty:
        print("\n--- Generating Scaling Analysis Plots ---")
        plot_comparison_solving_time(df_scaling)
        plot_comparison_speedup(df_scaling)
        plot_comparison_efficiency(df_scaling)
    else:
        print("\nNo valid 'scaling' data found. Skipping scaling analysis plots.")

    if not df_factor.empty:
        print("\n--- Generating Factor Analysis Plots ---")
        plot_factor_solving_time(df_factor)
    else:
        print("\nNo valid 'factor' data found. Skipping factor analysis plots.")

    print(f"\nAll tasks complete. Plots have been generated in the '{output_folder}' folder.")

if __name__ == '__main__':
    main()
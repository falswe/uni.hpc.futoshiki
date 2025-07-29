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
    # Fallback for environments where __file__ is not defined (e.g., some notebooks)
    PROJECT_ROOT = Path('.').resolve()
    print("Warning: Could not find .git root. Using current directory as project root.")
    
output_folder = PROJECT_ROOT / 'results/graphs'

def create_graphs_folder():
    """Creates the 'graphs' folder if it doesn't already exist."""
    if not os.path.exists(output_folder):
        os.makedirs(output_folder)
        print(f"Created folder: {output_folder}")

def filter_by_job_id(df):
    """
    Filters out duplicate runs for the same configuration, keeping the one with the highest job_id.
    """
    print("Filtering duplicate runs, keeping latest job_id...")
    # Sort by job_id so the highest is last
    df = df.sort_values('job_id')
    # Define the columns that make a run unique
    config_cols = ['puzzle_name', 'implementation', 'num_processors', 'num_threads', 'task_factor']
    # Drop duplicates, keeping the last one (which has the highest job_id)
    df_filtered = df.drop_duplicates(subset=config_cols, keep='last')
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
    
    # Drop rows where essential data for plotting is missing
    df.dropna(subset=['puzzle_name', 'implementation', 'solving_time'], inplace=True)
    return df

def get_best_results(df):
    """
    Filters the DataFrame to keep only the best-performing run (minimum solving time)
    for each combination of puzzle, implementation, and number of computational units.
    """
    print("Finding best performing runs for scalability plots...")
    # Identify best run based on minimum solving time for each processor/thread count
    mpi_best = df[df['implementation'] == 'mpi'].copy()
    if not mpi_best.empty:
        mpi_best = mpi_best.loc[mpi_best.groupby(
            ['puzzle_name', 'num_processors']
        )['solving_time'].idxmin()]

    omp_best = df[df['implementation'] == 'omp'].copy()
    if not omp_best.empty:
        omp_best = omp_best.loc[omp_best.groupby(
            ['puzzle_name', 'num_threads']
        )['solving_time'].idxmin()]
        
    hybrid_best = df[df['implementation'] == 'hybrid'].copy()
    if not hybrid_best.empty:
        hybrid_best = hybrid_best.loc[hybrid_best.groupby(
            ['puzzle_name', 'num_processors', 'num_threads']
        )['solving_time'].idxmin()]

    return pd.concat([mpi_best, omp_best, hybrid_best]).sort_index()

def plot_speedup(df):
    """Plots Speedup vs. Number of Processors/Threads for MPI and OpenMP."""
    print("Generating Speedup plots...")
    # MPI
    for puzzle, puzzle_df in df[df['implementation'] == 'mpi'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_processors')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_processors'], sorted_df['speedup'], marker='o', linestyle='-')
        plt.title(f'MPI Speedup vs. Processors for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Processors')
        plt.ylabel('Speedup')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/speedup_mpi_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

    # OpenMP
    for puzzle, puzzle_df in df[df['implementation'] == 'omp'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_threads')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_threads'], sorted_df['speedup'], marker='o', linestyle='-')
        plt.title(f'OpenMP Speedup vs. Threads for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Threads')
        plt.ylabel('Speedup')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/speedup_omp_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_solving_time(df):
    """Plots Solving Time vs. Number of Processors/Threads for MPI and OpenMP."""
    print("Generating Solving Time plots...")
    # MPI
    for puzzle, puzzle_df in df[df['implementation'] == 'mpi'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_processors')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_processors'], sorted_df['solving_time'], marker='o', linestyle='-')
        plt.title(f'MPI Solving Time vs. Processors for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Processors')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/solving_time_mpi_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

    # OpenMP
    for puzzle, puzzle_df in df[df['implementation'] == 'omp'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_threads')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_threads'], sorted_df['solving_time'], marker='o', linestyle='-')
        plt.title(f'OpenMP Solving Time vs. Threads for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Threads')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/solving_time_omp_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_efficiency(df):
    """Plots Efficiency vs. Number of Processors/Threads for MPI and OpenMP."""
    print("Generating Efficiency plots...")
    # MPI
    for puzzle, puzzle_df in df[df['implementation'] == 'mpi'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_processors')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_processors'], sorted_df['efficiency'], marker='o', linestyle='-')
        plt.title(f'MPI Efficiency vs. Processors for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Processors')
        plt.ylabel('Efficiency')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/efficiency_mpi_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

    # OpenMP
    for puzzle, puzzle_df in df[df['implementation'] == 'omp'].groupby('puzzle_name'):
        sorted_df = puzzle_df.sort_values('num_threads')
        plt.figure(figsize=(10, 6))
        plt.plot(sorted_df['num_threads'], sorted_df['efficiency'], marker='o', linestyle='-')
        plt.title(f'OpenMP Efficiency vs. Threads for {os.path.basename(puzzle)}')
        plt.xlabel('Number of Threads')
        plt.ylabel('Efficiency')
        plt.grid(True, which='both', linestyle='--')
        plt.savefig(f'{output_folder}/efficiency_omp_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_factor_solving_time(df):
    """Plots Solving Time vs. Task Factor, with a line for each processor/thread count."""
    print("Generating Task Factor analysis plots...")
    # MPI
    for puzzle, puzzle_df in df[df['implementation'] == 'mpi'].groupby('puzzle_name'):
        plt.figure(figsize=(10, 6))
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
        plt.savefig(f'{output_folder}/factor_solving_time_mpi_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()
    
    # OpenMP
    for puzzle, puzzle_df in df[df['implementation'] == 'omp'].groupby('puzzle_name'):
        plt.figure(figsize=(10, 6))
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
        plt.savefig(f'{output_folder}/factor_solving_time_omp_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()

def plot_comparison_solving_time(df):
    """
    Plots a comparison of solving time vs. total computational units for all implementations.
    """
    print("Generating comparison plots for solving time...")
    
    df_copy = df.copy()

    # --- MODIFIED SECTION ---
    # Create a unified 'computational_units' column for a fair comparison.
    conditions = [
        (df_copy['implementation'] == 'hybrid'),
        (df_copy['implementation'] == 'mpi'),
        (df_copy['implementation'] == 'omp')
    ]
    choices = [
        df_copy['num_processors'] * df_copy['num_threads'], # Hybrid: procs * threads
        df_copy['num_processors'],                          # MPI: procs
        df_copy['num_threads']                              # OMP: threads
    ]
    df_copy['computational_units'] = np.select(conditions, choices, default=0)
    # --- END MODIFIED SECTION ---
    
    # Group by puzzle to create a separate plot for each
    for puzzle, puzzle_df in df_copy.groupby('puzzle_name'):
        plt.figure(figsize=(12, 8))
        
        # Plot each implementation on the same graph
        for impl, impl_df in puzzle_df.groupby('implementation'):
            # Sort by the new unified computational units column
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                 # For hybrid, the label should be more descriptive
                if impl == 'hybrid':
                    # Create a descriptive label for each hybrid configuration
                    label = f"Hybrid ({int(sorted_df['num_processors'].iloc[0])}p x {int(sorted_df['num_threads'].iloc[0])}t)"
                    plt.plot(sorted_df['computational_units'], sorted_df['solving_time'], marker='s', linestyle='--', label=label)
                else: # MPI and OMP
                    plt.plot(sorted_df['computational_units'], sorted_df['solving_time'], marker='o', linestyle='-', label=impl.upper())
        
        plt.title(f'Solving Time vs. Total Computational Units for {os.path.basename(puzzle)}')
        plt.xlabel('Total Computational Units (Cores)')
        plt.ylabel('Solving Time (s)')
        plt.grid(True, which='both', linestyle='--')
        plt.legend()
        plt.savefig(f'{output_folder}/comparison_solving_time_{os.path.basename(puzzle).replace(".txt", "")}.png')
        plt.close()


def main():
    """Main function to read the CSV, create folders, and generate all plots."""
    results_file = PROJECT_ROOT / 'results/results_dataset.csv'
    try:
        df_raw = pd.read_csv(results_file)
    except FileNotFoundError:
        print(f"Error: '{results_file}' not found. Please ensure the CSV file is in the correct directory.")
        return

    create_graphs_folder()
    
    # First, filter out any older, duplicate runs
    df_filtered = filter_by_job_id(df_raw)
    
    # Then, clean the data (type conversion, drop NA)
    df_cleaned = clean_data(df_filtered)
    
    # Create a new dataframe that contains only the best results for scalability plots
    df_best = get_best_results(df_cleaned)

    # Generate Scalability Plots using the best-performing data
    plot_speedup(df_best)
    plot_solving_time(df_best)
    plot_efficiency(df_best)

    # Generate Task Factor analysis plots using the cleaned (but not best-of) data
    plot_factor_solving_time(df_cleaned)
    
    # Generate the new, corrected comparison plot
    plot_comparison_solving_time(df_best)

    print(f"\nAll plots have been generated successfully in the '{output_folder}' folder.")

if __name__ == '__main__':
    main()
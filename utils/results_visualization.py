import pandas as pd
import matplotlib.pyplot as plt
import os
from pathlib import Path
import numpy as np

# Global Matplotlib style settings for LaTeX paper format
plt.rcParams.update({
    'figure.figsize': (3.5, 2.8), # Size for a two-column layout
    'font.size': 8,
    'axes.titlesize': 10,
    'axes.labelsize': 8,
    'xtick.labelsize': 8,
    'ytick.labelsize': 8,
    'legend.fontsize': 7,
    'lines.linewidth': 1,
    'lines.markersize': 2.6,
    'savefig.bbox': 'tight' # Ensures no extra whitespace
})


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
    df.dropna(subset=['puzzle_name', 'implementation', 'solving_time'], inplace=True)
    return df

def get_best_scaling_results(df, include_hybrid=False):
    """
    Filters for 'scaling' tests and finds the best-performing run for each configuration.
    By default, it excludes hybrid data unless specified.
    """
    print(f"Filtering for 'scaling' tests (include_hybrid={include_hybrid})...")
    df_scaling = df[df['test_type'] == 'scaling'].copy()
    
    if not include_hybrid:
        df_scaling = df_scaling[df_scaling['implementation'] != 'hybrid']

    implementations_to_process = df_scaling['implementation'].unique()
    
    best_results = []
    if 'mpi' in implementations_to_process:
        mpi_best = df_scaling[df_scaling['implementation'] == 'mpi'].copy()
        if not mpi_best.empty:
            best_results.append(mpi_best.loc[mpi_best.groupby(['puzzle_name', 'num_processors'])['solving_time'].idxmin()])

    if 'omp' in implementations_to_process:
        omp_best = df_scaling[df_scaling['implementation'] == 'omp'].copy()
        if not omp_best.empty:
            best_results.append(omp_best.loc[omp_best.groupby(['puzzle_name', 'num_threads'])['solving_time'].idxmin()])
            
    if 'hybrid' in implementations_to_process and include_hybrid:
        hybrid_best = df_scaling[df_scaling['implementation'] == 'hybrid'].copy()
        if not hybrid_best.empty:
            best_results.append(hybrid_best.loc[hybrid_best.groupby(['puzzle_name', 'num_processors', 'num_threads'])['solving_time'].idxmin()])

    if not best_results:
        return pd.DataFrame()

    return pd.concat(best_results).sort_index()

def _add_computational_units(df):
    """Helper function to add a 'computational_units' column."""
    df_copy = df.copy()
    choices = [
        df_copy['num_processors'] * df_copy['num_threads'],
        df_copy['num_processors'],
        df_copy['num_threads']
    ]
    conditions = [
        (df_copy['implementation'] == 'hybrid'),
        (df_copy['implementation'] == 'mpi'),
        (df_copy['implementation'] == 'omp')
    ]
    df_copy['computational_units'] = np.select(conditions, choices, default=1)
    return df_copy


def get_hybrid_config_type(row):
    """Helper to classify hybrid configurations."""
    if row['num_processors'] > row['num_threads']:
        return 'MPI-heavy'
    elif row['num_threads'] > row['num_processors']:
        return 'OMP-heavy'
    elif row['num_processors'] == row['num_threads']:
        return 'Balanced'
    return 'Other'

def plot_hybrid_configuration_comparison(df_hybrid, df_seq):
    """Plots a comparison of hybrid configuration solving times against the sequential baseline."""
    print("Generating hybrid configuration comparison plots...")
    if df_hybrid.empty: return
    
    df_hybrid = _add_computational_units(df_hybrid)
    df_hybrid['config_type'] = df_hybrid.apply(get_hybrid_config_type, axis=1)
    color_map = {'MPI-heavy': 'darkblue', 'OMP-heavy': '#FF4500', 'Balanced': 'green'}
    for puzzle, puzzle_df in df_hybrid.groupby('puzzle_name'):
        fig, ax = plt.subplots()
        seq_time_row = df_seq[df_seq['puzzle_name'] == puzzle]
        if not seq_time_row.empty:
            seq_time = seq_time_row['solving_time'].iloc[0]
            ax.axhline(y=seq_time, color='r', linestyle='--', label='Sequential', zorder=1)
        for config_type, group_df in puzzle_df.groupby('config_type'):
            if config_type in color_map:
                sorted_group = group_df.sort_values('computational_units')
                ax.plot(sorted_group['computational_units'], sorted_group['solving_time'], marker='o', linestyle='-', label=config_type, color=color_map.get(config_type), zorder=2)
        ax.set_title(f'Hybrid Configurations: {os.path.basename(puzzle)}')
        ax.set_xlabel('Total Computational Units (Cores)')
        ax.set_ylabel('Solving Time (s)')
        ax.set_xscale('log', base=2)
        ax.set_xticks(sorted(puzzle_df['computational_units'].unique()), labels=sorted(puzzle_df['computational_units'].unique()))
        ax.grid(True, which='both', linestyle='--')
        ax.legend()
        plt.savefig(output_folder / f'hybrid_config_comparison_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close(fig)

def plot_hybrid_speedup_comparison(df_hybrid):
    """Plots a comparison of speedup for different hybrid configurations."""
    print("Generating hybrid speedup comparison plots...")
    if df_hybrid.empty: return
    df_hybrid = _add_computational_units(df_hybrid)
    df_hybrid['config_type'] = df_hybrid.apply(get_hybrid_config_type, axis=1)
    color_map = {'MPI-heavy': 'darkblue', 'OMP-heavy': '#FF4500', 'Balanced': 'green'}
    for puzzle, puzzle_df in df_hybrid.groupby('puzzle_name'):
        fig, ax = plt.subplots()
        all_units = puzzle_df['computational_units'].dropna()
        if not all_units.empty:
            ideal_range = np.linspace(min(all_units), max(all_units), 100)
            ax.plot(ideal_range, ideal_range, color='red', linestyle='--', label='Ideal Speedup', zorder=1)
        for config_type, group_df in puzzle_df.groupby('config_type'):
            if config_type in color_map:
                sorted_group = group_df.sort_values('computational_units').dropna(subset=['speedup'])
                if not sorted_group.empty:
                    ax.plot(sorted_group['computational_units'], sorted_group['speedup'], marker='o', linestyle='-', label=config_type, color=color_map.get(config_type), zorder=2)
        ax.set_title(f'Hybrid Speedup: {os.path.basename(puzzle)}')
        ax.set_xlabel('Total Computational Units (Cores)')
        ax.set_ylabel('Speedup')
        ax.grid(True, which='both', linestyle='--')
        ax.legend()
        plt.savefig(output_folder / f'hybrid_speedup_comparison_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close(fig)

def plot_hybrid_efficiency_comparison(df_hybrid):
    """Plots a comparison of efficiency for different hybrid configurations."""
    print("Generating hybrid efficiency comparison plots...")
    if df_hybrid.empty: return
    df_hybrid = _add_computational_units(df_hybrid)
    df_hybrid['config_type'] = df_hybrid.apply(get_hybrid_config_type, axis=1)
    color_map = {'MPI-heavy': 'darkblue', 'OMP-heavy': '#FF4500', 'Balanced': 'green'}
    for puzzle, puzzle_df in df_hybrid.groupby('puzzle_name'):
        fig, ax = plt.subplots()
        for config_type, group_df in puzzle_df.groupby('config_type'):
            if config_type in color_map:
                sorted_group = group_df.sort_values('computational_units').dropna(subset=['efficiency'])
                if not sorted_group.empty:
                    ax.plot(sorted_group['computational_units'], sorted_group['efficiency'], marker='o', linestyle='-', label=config_type, color=color_map.get(config_type))
        ax.set_title(f'Hybrid Efficiency: {os.path.basename(puzzle)}')
        ax.set_xlabel('Total Computational Units (Cores)')
        ax.set_ylabel('Efficiency')
        ax.set_xscale('log', base=2)
        ax.set_xticks(sorted(puzzle_df['computational_units'].unique()), labels=sorted(puzzle_df['computational_units'].unique()))
        ax.grid(True, which='both', linestyle='--')
        ax.legend()
        plt.savefig(output_folder / f'hybrid_efficiency_comparison_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close(fig)

def plot_comparison_efficiency(df):
    """Plots a comparison of efficiency vs. total computational units (MPI and OMP)."""
    print("Generating comparison plots for efficiency...")
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange'} 
    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        plt.figure()
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                plt.plot(sorted_df['computational_units'], sorted_df['efficiency'], marker='o', linestyle='-', label=impl.upper(), color=color_map.get(impl))
        plt.title(f'Efficiency: {os.path.basename(puzzle)}')
        plt.xlabel('Total Computational Units (Cores)')
        plt.ylabel('Efficiency')
        plt.xscale('log', base=2)
        plt.xticks(sorted(puzzle_df['computational_units'].unique()), labels=sorted(puzzle_df['computational_units'].unique()))
        plt.grid(True, which='both', linestyle='--')
        plt.legend()
        plt.savefig(output_folder / f'comparison_efficiency_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close()

def plot_all_factor_analyses(df_factor):
    """
    Plots Solving Time vs. Task Factor for MPI, OpenMP, and Hybrid implementations.
    """
    if df_factor.empty:
        print("No factor data to plot.")
        return
        
    print("Generating all Task Factor analysis plots...")

    # Group by implementation, then by puzzle
    for impl_type, impl_df in df_factor.groupby('implementation'):
        for puzzle, puzzle_df in impl_df.groupby('puzzle_name'):
            plt.figure()
            
            if impl_type == 'mpi':
                for procs, group in puzzle_df.groupby('num_processors'):
                    group = group.sort_values('task_factor')
                    plt.plot(group['task_factor'], group['solving_time'], marker='o', linestyle='-', label=f'{int(procs)} Procs')
                plt.title(f'MPI Time vs. Task Factor: {os.path.basename(puzzle)}')
                plt.xlabel('Task Factor')
                plt.legend(title="Processes")
                filename = f'factor_analysis_mpi_{os.path.basename(puzzle).replace(".txt", "")}.pdf'

            elif impl_type == 'omp':
                for threads, group in puzzle_df.groupby('num_threads'):
                    group = group.sort_values('task_factor')
                    plt.plot(group['task_factor'], group['solving_time'], marker='o', linestyle='-', label=f'{int(threads)} Threads')
                plt.title(f'OMP Time vs. Task Factor: {os.path.basename(puzzle)}')
                plt.xlabel('Task Factor')
                plt.legend(title="Threads")
                filename = f'factor_analysis_omp_{os.path.basename(puzzle).replace(".txt", "")}.pdf'

            elif impl_type == 'hybrid':
                for (procs, threads), group in puzzle_df.groupby(['num_processors', 'num_threads']):
                    group = group.sort_values('task_factor')
                    plt.plot(group['task_factor'], group['solving_time'], marker='o', linestyle='-', label=f'{int(procs)}p x {int(threads)}t')
                plt.title(f'Hybrid Time vs. Task Factor: {os.path.basename(puzzle)}')
                # *** MODIFICATION: Set a specific, clear x-axis label for the hybrid case ***
                plt.xlabel('Symmetric Task Factor (mf=of)')
                plt.legend(title="Config")
                filename = f'factor_analysis_hybrid_{os.path.basename(puzzle).replace(".txt", "")}.pdf'
            
            plt.ylabel('Solving Time (s)')
            plt.xscale('log', base=2)
            plt.grid(True, which='both', linestyle='--')
            
            if 'task_factor' in puzzle_df.columns:
                factor_values = sorted(puzzle_df['task_factor'].unique())
                plt.xticks(factor_values, labels=[str(int(f)) if f.is_integer() else str(f) for f in factor_values])
            
            plt.savefig(output_folder / filename)
            plt.close()

def plot_comparison_solving_time(df, df_seq):
    """Plots a comparison of solving time vs. total computational units (MPI and OMP)."""
    print("Generating comparison plots for solving time...")
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange'}
    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        plt.figure()
        seq_time_row = df_seq[df_seq['puzzle_name'] == puzzle]
        if not seq_time_row.empty:
            seq_time = seq_time_row['solving_time'].iloc[0]
            plt.axhline(y=seq_time, color='r', linestyle='--', label='Sequential', zorder=1)
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                plt.plot(sorted_df['computational_units'], sorted_df['solving_time'], marker='o', linestyle='-', label=impl.upper(), color=color_map.get(impl), zorder=2)
        plt.title(f'Solving Time: {os.path.basename(puzzle)}')
        plt.xlabel('Total Computational Units (Cores)')
        plt.ylabel('Solving Time (s)')
        plt.xscale('log', base=2)
        plt.xticks(sorted(puzzle_df['computational_units'].unique()), labels=sorted(puzzle_df['computational_units'].unique()))
        plt.grid(True, which='both', linestyle='--')
        plt.legend()
        plt.savefig(output_folder / f'comparison_solving_time_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close()

def plot_comparison_speedup(df):
    """Plots a comparison of speedup vs. total computational units (MPI and OMP)."""
    print("Generating comparison plots for speedup...")
    df_with_units = _add_computational_units(df)
    color_map = {'mpi': 'C0', 'omp': 'orange'}
    for puzzle, puzzle_df in df_with_units.groupby('puzzle_name'):
        fig, ax = plt.subplots()
        for impl, impl_df in puzzle_df.groupby('implementation'):
            sorted_df = impl_df.sort_values('computational_units')
            if not sorted_df.empty:
                sorted_df.dropna(subset=['speedup'], inplace=True)
                ax.plot(sorted_df['computational_units'], sorted_df['speedup'], marker='o', linestyle='-', label=f'{impl.upper()} Speedup', color=color_map.get(impl), zorder=2)
        if not puzzle_df.dropna(subset=['speedup', 'computational_units']).empty:
            all_units = puzzle_df['computational_units'].dropna()
            ideal_range = np.linspace(min(all_units), max(all_units), 100)
            ax.plot(ideal_range, ideal_range, color='red', linestyle='--', label='Ideal Speedup', zorder=1)
        ax.set_title(f'Speedup: {os.path.basename(puzzle)}')
        ax.set_xlabel('Total Computational Units (Cores)')
        ax.set_ylabel('Speedup')
        ax.grid(True, which='both', linestyle='--')
        ax.legend()
        plt.savefig(output_folder / f'comparison_speedup_{os.path.basename(puzzle).replace(".txt", "")}.pdf')
        plt.close(fig)

def main():
    """Main function to read the CSV, create folders, and generate all plots."""
    results_file = PROJECT_ROOT / 'results/results_dataset.csv'
    try:
        df_raw = pd.read_csv(results_file)
    except FileNotFoundError:
        print(f"Error: '{results_file}' not found. Please ensure the CSV file is present.")
        return

    create_graphs_folder()
    
    df_filtered = filter_by_job_id(df_raw)
    df_cleaned = clean_data(df_filtered)
    
    # --- Data Separation for Different Plotting Needs ---
    df_scaling = df_cleaned[df_cleaned['test_type'] == 'scaling']
    df_factor = df_cleaned[df_cleaned['test_type'] == 'factor']
    
    df_scaling_comparison = get_best_scaling_results(df_scaling, include_hybrid=False)
    df_scaling_hybrid = get_best_scaling_results(df_scaling, include_hybrid=True)
    if not df_scaling_hybrid.empty:
        df_scaling_hybrid = df_scaling_hybrid[df_scaling_hybrid['implementation'] == 'hybrid']
    df_seq = df_cleaned[df_cleaned['implementation'] == 'seq'].copy()
    
    # --- Generate Scaling Plots ---
    if not df_scaling_comparison.empty:
        print("\n--- Generating MPI vs. OMP Comparison Plots ---")
        plot_comparison_solving_time(df_scaling_comparison, df_seq)
        plot_comparison_speedup(df_scaling_comparison)
        plot_comparison_efficiency(df_scaling_comparison)
    else:
        print("\nNo valid data for MPI/OMP scaling comparison plots.")

    if not df_scaling_hybrid.empty:
        print("\n--- Generating Hybrid Configuration Analysis Plots ---")
        if not df_seq.empty:
            plot_hybrid_configuration_comparison(df_scaling_hybrid, df_seq)
        plot_hybrid_speedup_comparison(df_scaling_hybrid)
        plot_hybrid_efficiency_comparison(df_scaling_hybrid)
    else:
        print("\nNo valid data for hybrid configuration plots.")
    
    # --- Generate Factor Plots ---
    if not df_factor.empty:
        plot_all_factor_analyses(df_factor)
    else:
        print("\nNo valid 'factor' data found. Skipping factor analysis plots.")

    print(f"\nAll tasks complete. PDF plots have been generated in the '{output_folder}' folder.")

if __name__ == '__main__':
    main()
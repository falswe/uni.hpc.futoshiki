import re
import csv
import os
import argparse
from datetime import datetime
from pathlib import Path

def find_project_root(start: Path) -> Path:
    for parent in [start] + list(start.parents):
        if (parent / ".git").exists():
            return parent
    raise FileNotFoundError("Project root not found")

PROJECT_ROOT = find_project_root(Path(__file__).resolve().parent)
output_folder = PROJECT_ROOT / 'results'

def _parse_single_run_block(block_content, job_id_from_file=None, file_path=None):
    """
    Parses a single block of text corresponding to one solver run.
    """
    data = {}

    # Determine test_type from filename
    if file_path:
        filename = os.path.basename(file_path).lower()
        if 'scaling' in filename:
            data['test_type'] = 'scaling'
        elif 'factor' in filename:
            data['test_type'] = 'factor'
        else:
            data['test_type'] = 'single'
    else:
        data['test_type'] = 'N/A'

    # Determine the implementation type from the header
    if 'Futoshiki MPI Parallel Solver' in block_content:
        data['implementation'] = 'mpi'
    elif 'Futoshiki OpenMP Parallel Solver' in block_content:
        data['implementation'] = 'omp'
    elif 'Futoshiki Sequential Solver' in block_content:
        data['implementation'] = 'seq'
    elif 'Futoshiki Hybrid Solver' in block_content:
        data['implementation'] = 'hybrid'
    else:
        data['implementation'] = 'unknown'

    # --- Regex patterns to find the data ---
    patterns = {
        'puzzle_name': r"Puzzle(?: file)?: (.+)",
        'task_factor': r"\* ([\d.]+) factor",
        'depth': r"Chosen depth: (\d+)",
        'work_units': r"Generated (\d+) work units",
        'colors_removed': r"Colors removed by pre-coloring: (\d+)",
        'colors_remaining': r"Colors remaining: (\d+)",
        'space_reduction': r"Search space reduction: ([\d.]+)%",
        'solving_time': r"Solving phase:\s+([\d.]+) seconds",
        'total_time': r"Total time:\s+([\d.]+) seconds"
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, block_content)
        data[key] = match.group(1).strip() if match else 'N/A'

    # --- Specific patterns for Processors and Threads ---
    if data['implementation'] == 'mpi':
        match = re.search(r"Running with (\d+) process", block_content)
        data['num_processors'] = match.group(1).strip() if match else '1'
        data['num_threads'] = '1'
    elif data['implementation'] == 'omp':
        match = re.search(r"Running with (\d+) OpenMP thread", block_content)
        data['num_threads'] = match.group(1).strip() if match else '1'
        data['num_processors'] = '1'
    elif data['implementation'] == 'seq':
        data['num_processors'] = '1'
        data['num_threads'] = '1'
    elif data['implementation'] == 'hybrid':
         proc_match = re.search(r"Running with (\d+) process", block_content)
         thread_match = re.search(r"and (\d+) OpenMP thread\(s\) per process", block_content)
         data['num_processors'] = proc_match.group(1).strip() if proc_match else 'N/A'
         data['num_threads'] = thread_match.group(1).strip() if thread_match else 'N/A'
    else:
        data['num_processors'] = 'N/A'
        data['num_threads'] = 'N/A'
    
    data['job_id'] = job_id_from_file if job_id_from_file else 'N/A'
    return data

def parse_runs_from_file(file_path):
    """
    Parses a log file that may contain multiple solver runs using a robust block-finding strategy.
    """
    try:
        with open(file_path, 'r') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading file '{os.path.basename(file_path)}': {e}")
        return []

    job_id_match = re.search(r"Job ID: (\d+)", content)
    file_level_job_id = job_id_match.group(1).strip() if job_id_match else None

    run_start_pattern = re.compile(
        r"^(?:\[INFO\](?:\[RANK \d+\])?\s*)?={10,}\n"
        r"^(?:\[INFO\](?:\[RANK \d+\])?\s*)?Futoshiki.*?Solver\n"
        r"^(?:\[INFO\](?:\[RANK \d+\])?\s*)?={10,}",
        re.MULTILINE
    )
    
    start_matches = list(run_start_pattern.finditer(content))
    if not start_matches:
        return []

    parsed_runs = []
    for i, start_match in enumerate(start_matches):
        start_pos = start_match.start()
        end_pos = start_matches[i + 1].start() if i + 1 < len(start_matches) else len(content)
        block = content[start_pos:end_pos]

        if "Time Distribution" not in block:
            continue
                
        if block.strip():
            # Pass file_path to the parsing function to determine the test_type
            run_data = _parse_single_run_block(block, file_level_job_id, file_path)
            parsed_runs.append(run_data)
            
    return parsed_runs

def save_as_formatted_text(data_list, input_path):
    """
    Saves the extracted data from one or more runs to a single text file.
    """
    if not data_list: return
        
    base_name = os.path.basename(input_path)
    output_filename = f"parsed_{os.path.splitext(base_name)[0]}.txt"
    output_dir = os.path.join(output_folder, "parsed_summaries")
    os.makedirs(output_dir, exist_ok=True)
    
    full_output_path = os.path.join(output_dir, output_filename)
    print(f"  -> Saving formatted text summary to: {full_output_path}")

    with open(full_output_path, 'w') as f:
        f.write(f"Source File: {base_name}\nGenerated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Total Runs Found: {len(data_list)}\n")
        
        for i, data in enumerate(data_list, 1):
            f.write("\n" + "="*20 + f" RUN {i} " + "="*20 + "\n")
            f.write("--- Run Configuration ---\n")
            f.write(f"  Puzzle Name:         {data['puzzle_name']}\n")
            f.write(f"  Test Type:           {data['test_type']}\n")
            f.write(f"  Implementation:      {data['implementation'].upper()}\n")
            f.write(f"  Job ID:              {data['job_id']}\n")
            f.write(f"  MPI Processes:       {data['num_processors']}\n")
            f.write(f"  OMP Threads:         {data['num_threads']}\n\n")
            f.write("--- Performance Metrics ---\n")
            f.write(f"  Total Time:          {data['total_time']} seconds\n")
            f.write(f"  Speedup:             {data.get('speedup', 'N/A')}\n")
            f.write(f"  Efficiency:          {data.get('efficiency', 'N/A')}\n")
        f.write("="*50 + "\n")

def update_results_csv(newly_parsed_data, csv_path):
    """
    Reads, updates, calculates, and overwrites the master CSV file.
    """
    key_columns = ('puzzle_name', 'implementation', 'test_type', 'job_id', 'num_processors', 'num_threads', 'task_factor')
    
    all_records = {}
    if os.path.isfile(csv_path):
        try:
            with open(csv_path, 'r', newline='') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    record_key = tuple(row.get(k, 'N/A') for k in key_columns)
                    all_records[record_key] = row
        except Exception as e:
            print(f"Warning: Could not read existing CSV file. A new one will be created. Error: {e}")

    for data in newly_parsed_data:
        record_key = tuple(data.get(k, 'N/A') for k in key_columns)
        all_records[record_key] = data

    # Find baseline sequential run times for each puzzle
    sequential_times = {}
    for record in all_records.values():
        if record.get('implementation') == 'seq' and record.get('total_time') != 'N/A':
            try:
                seq_time = float(record['total_time'])
                sequential_times[record['puzzle_name']] = seq_time
            except (ValueError, TypeError):
                continue

    final_data_list = []
    for record in all_records.values():
        puzzle_name = record.get('puzzle_name')
        impl = record.get('implementation')
        
        record['speedup'] = 'N/A'
        record['efficiency'] = 'N/A'

        if impl != 'seq' and puzzle_name in sequential_times:
            try:
                parallel_time = float(record.get('total_time'))
                seq_time = sequential_times[puzzle_name]

                if parallel_time > 0:
                    speedup = seq_time / parallel_time
                    record['speedup'] = f"{speedup:.4f}"
                    
                    procs = int(record.get('num_processors', 1))
                    threads = int(record.get('num_threads', 1))
                    
                    total_cores = 1
                    if impl == 'mpi': total_cores = procs
                    elif impl == 'omp': total_cores = threads
                    elif impl == 'hybrid': total_cores = procs * threads
                    
                    if total_cores > 0:
                        efficiency = speedup / total_cores
                        record['efficiency'] = f"{efficiency:.4f}"
            except (ValueError, TypeError, ZeroDivisionError):
                continue
        
        final_data_list.append(record)

    header = [
        'puzzle_name', 'implementation', 'test_type', 'job_id', 'num_processors', 
        'num_threads', 'task_factor', 'depth', 'work_units', 
        'colors_removed', 'colors_remaining', 'space_reduction', 
        'solving_time', 'total_time', 'speedup', 'efficiency'
    ]
    
    try:
        os.makedirs(os.path.dirname(csv_path), exist_ok=True)
        with open(csv_path, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=header, extrasaction='ignore')
            writer.writeheader()
            final_data_list.sort(key=lambda r: (
                r.get('puzzle_name', ''), 
                r.get('test_type', ''),
                r.get('implementation', ''), 
                int(r.get('num_processors', 1)), 
                int(r.get('num_threads', 1))
            ))
            writer.writerows(final_data_list)
        print(f"\nCSV Update Complete: '{csv_path}' has been updated with {len(final_data_list)} total records.")
    except Exception as e:
        print(f"\nError writing to CSV file: {e}")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Parse Futoshiki solver output and calculate performance.")
    parser.add_argument("path", help="Path to the solver's output log file or a directory of log files.")
    parser.add_argument("--csv", default=f"{output_folder}/results_dataset.csv", help="Path for the output CSV file.")
    args = parser.parse_args()

    input_path = Path(args.path)
    files_to_process = []

    if not input_path.exists():
        print(f"Error: The path '{input_path}' does not exist."); exit(1)

    if input_path.is_dir():
        files_to_process.extend(f for f in input_path.iterdir() if f.is_file())
    else:
        files_to_process.append(input_path)

    if not files_to_process:
        print("No files found to process."); exit(0)

    all_newly_parsed_data = []
    for file_path in files_to_process:
        print(f"\nParsing file: {file_path.name}...")
        runs_in_file = parse_runs_from_file(file_path)
        if runs_in_file:
            print(f"  Found {len(runs_in_file)} run(s) in this file.")
            all_newly_parsed_data.extend(runs_in_file)
            save_as_formatted_text(runs_in_file, file_path)
        else:
            print("  No valid run data found in this file.")

    update_results_csv(all_newly_parsed_data, args.csv)
    
    print("\nAll tasks complete!")
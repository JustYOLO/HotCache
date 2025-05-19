import re
import os
import json
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # registers 3D projection
import matplotlib.cm as cm

import re
import json

def parse_rocksdb_log(filename):
    data = []
    with open(filename, 'r') as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i]

        # First line: input/output SSTs, including levels (e.g. "Compacted 9@0 + 9@1 files to L1")
        if "Compacted" in line and "files to L" in line:
            match_input_levels = re.findall(r'(\d+)@(\d+)', line)
            if not match_input_levels:
                i += 1
                continue
            input_levels = [int(lvl) for _, lvl in match_input_levels]
            highest_input_level = max(input_levels)
            
            # Second line should have records in/out/dropped
            if (i + 1 < len(lines)) and "compacted to:" in lines[i + 1]:
                comp_line = lines[i + 1]
                
                # Parse compacted_number from 'files in(x, y)'
                match_in_files = re.search(r'files in\((\d+),', comp_line)
                if not match_in_files:
                    i += 1
                    continue
                compacted_number = int(match_in_files.group(1))
                
                # Parse records
                match_in = re.search(r'records in:\s*(\d+)', comp_line)
                match_dropped = re.search(r'records dropped:\s*(\d+)', comp_line)
                match_output = re.search(r'records output:\s*(\d+)', comp_line)
                if not (match_in and match_dropped and match_output):
                    i += 1
                    continue

                records_in = int(match_in.group(1))
                written_keys = int(match_output.group(1))
                percentage = (written_keys / records_in) * 100 if records_in else 0

                # Third line: JSON log with compaction time
                if (i + 2 < len(lines)) and 'compaction_finished' in lines[i + 2]:
                    try:
                        json_str = lines[i + 2].split("EVENT_LOG_v1")[1].strip()
                        json_data = json.loads(json_str)
                        compaction_time = int(json_data.get("compaction_time_micros", 0))
                    except Exception:
                        compaction_time = 0
                else:
                    compaction_time = 0

                # Save: (compacted_number, written_keys, percentage, compaction_time, highest_input_level)
                data.append((
                    compacted_number,
                    written_keys,
                    percentage,
                    compaction_time,
                    highest_input_level  # This is your desired level
                ))
                i += 3
                continue
        i += 1
    return data


def parse_files(file_list):
    """
    Parses multiple log files.
    
    Returns a dictionary:
      { filename: list of tuples (compacted_number, written_keys, percentage, compaction_time, level) }.
    """
    file_data = {}
    for file in file_list:
        file_data[file] = parse_rocksdb_log(file)
    return file_data

def plot_3d_per_level_figures(file_data, filter_value, output_dir, files_list, legend_list):
    """
    Creates a separate 3D scatter plot figure for each distinct compaction level found in file_data.
    
    If filter_value is nonzero, only records with compacted_number == filter_value are used.
    Otherwise, all data are plotted.
    
    In each figure:
      - X-axis: Duplicate Keys Percentage.
      - Y-axis: Compaction Time (micros).
      - Z-axis: Written Keys.
    
    Each benchmark (from files_list) is shown using a fixed color via the tab10 colormap.
    All points in each figure use the same marker ('o').
    
    The plots are saved in the specified output directory, with filenames like:
      "scatter_plot_level_{level}.png"
    """
    # Filter data by compacted_number if needed.
    if filter_value != 0:
        filtered_data = {
            f: [record for record in records if record[0] == filter_value]
            for f, records in file_data.items()
        }
    else:
        filtered_data = file_data
    
    # Identify all distinct levels across the filtered data.
    levels_found = set()
    for f in filtered_data:
        for record in filtered_data[f]:
            # record = (compacted_number, written_keys, percentage, compaction_time, level)
            levels_found.add(record[4])
    levels_found = sorted(levels_found)
    
    # Ensure the output directory exists.
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Assign a fixed color for each file (benchmark) using tab10.
    color_map = cm.get_cmap('tab10')
    file_to_color = {file: color_map(i) for i, file in enumerate(files_list)}
    
    # For each level, create a 3D scatter figure.
    for level in levels_found:
        fig = plt.figure(figsize=(11, 9))
        ax = fig.add_subplot(111, projection='3d')
        ax.view_init(elev=30, azim=-40)
        for idx, file in enumerate(files_list):
            # Extract points for this level.
            points = [(pct, ct, wk) for (cnum, wk, pct, ct, lvl) in filtered_data[file] if lvl == level]
            if not points:
                continue
            x_vals = [pt[0] for pt in points]  # Duplicate Keys Percentage (X)
            y_vals = [pt[2] for pt in points]  # Written Keys (Z)
            z_vals = [pt[1] for pt in points]  # Compaction Time (Y)
            color = file_to_color[file]
            label_text = legend_list[idx]
            ax.scatter(x_vals, y_vals, z_vals, color=color, marker='o', label=label_text)
        
        ax.set_xlabel("Written Keys Percentage")
        ax.set_ylabel("Written Keys")
        ax.tick_params(axis='y', which='major', pad=0)
        ax.tick_params(axis='z', which='major', pad=0)
        ax.set_zlabel("Compaction Time (micros)")
        title_text = f"3D Scatter Plot - Level {level}"
        if filter_value != 0:
            title_text += f" (Compacted = {filter_value})"
        ax.set_title(title_text)
        ax.legend()
        ax.set_box_aspect(None, zoom=0.85)
        # pos = ax.get_position()
        # ax.set_position([pos.x0 - 500, pos.y0, pos.width, pos.height])
        # fig.subplots_adjust(left=0.2, right=0.95, top=0.9, bottom=0.1)
        plt.tight_layout()
        output_path = os.path.join(output_dir, f"scatter_plot_level_{level}.png")
        plt.savefig(output_path, bbox_inches='tight')
        plt.close()

# files_list = [
#     "LOG_rand",
#     "LOG_zipf_dot8",
#     "LOG_zipf_dot9",
#     "LOG_zipf_1",
#     "LOG_zipf_1dot1",
# ]
# legend_list = ["rand", "zipfian 0.8", "zipfian 0.9", "zipfian 1", "zipfian 1.1"]

files_list = [
    "LOG_rand",
    "LOG_zipf_1",
]
legend_list = ["rand", "zipfian 1"]

data = parse_files(files_list)

plot_3d_per_level_figures(
    file_data=data,
    filter_value=0,
    output_dir="./output_per_level_3d",
    files_list=files_list,
    legend_list=legend_list
)

import re
import os
import json
import matplotlib.pyplot as plt
import matplotlib.cm as cm

def parse_rocksdb_log(filename):
    data = []
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    i = 0
    while i < len(lines) - 2:
        line = lines[i]

        # Look for the "files to L#" line to get the output level
        if "Compacted" in line and "files to L" in line:
            match_output_level = re.search(r'files to L(\d+)', line)
            if not match_output_level:
                i += 1
                continue
            output_level = int(match_output_level.group(1))
            print(output_level)
            level = output_level - 1  # Use output level minus 1 as desired
            if(level > 0):
                print("lv > 0")

            # Next line should be the compaction detail with record stats
            detail_line = lines[i + 1]
            if "compacted to:" in detail_line and "records in:" in detail_line:
                # Get number of compacted files
                match_in_files = re.search(r'files in\((\d+),', detail_line)
                if not match_in_files:
                    i += 1
                    continue
                compacted_number = int(match_in_files.group(1))

                # Get records in/dropped/output
                match_in = re.search(r'records in:\s*(\d+)', detail_line)
                match_output = re.search(r'records output:\s*(\d+)', detail_line)
                if not (match_in and match_output):
                    i += 1
                    continue
                records_in = int(match_in.group(1))
                written_keys = int(match_output.group(1))
                percentage = (written_keys / records_in) * 100 if records_in else 0

                # Next line is the JSON line with compaction_time_micros
                compaction_time = 0
                if (i + 2 < len(lines)) and "compaction_finished" in lines[i + 2]:
                    try:
                        json_str = lines[i + 2].split("EVENT_LOG_v1")[1].strip()
                        json_data = json.loads(json_str)
                        compaction_time = int(json_data.get("compaction_time_micros", 0))
                    except Exception:
                        compaction_time = 0

                data.append((compacted_number, written_keys, percentage, compaction_time, level))
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
    print(file_data)
    return file_data

files_list = [
    "LOG_rand",
    "LOG_zipf_dot8",
    "LOG_zipf_dot9",
    "LOG_zipf_1",
    "LOG_zipf_1dot1",
]
legend_list = ["rand", "zipfian 0.8", "zipfian 0.9", "zipfian 1", "zipfian 1.1"]

data = parse_files(files_list)
print(data)
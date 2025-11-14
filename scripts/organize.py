#!/usr/bin/env python3
import csv
from collections import defaultdict
import os
import sys

input_file = sys.argv[1]
output_dir = "metrics"

# Create output directory if it doesn't exist
os.makedirs(output_dir, exist_ok=True)

# Dictionary to hold rows per payload_len
payload_groups = defaultdict(list)

# Read the CSV and group rows by payload_len
with open(input_file, newline='') as f:
    reader = csv.DictReader(f)
    headers = reader.fieldnames
    for row in reader:
        payload_len = row['payload_len']
        payload_groups[payload_len].append(row)

# Write separate CSVs for each payload_len
for payload_len, rows in payload_groups.items():
    output_file = os.path.join(output_dir, f"metrics_{payload_len}.csv")
    with open(output_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        writer.writeheader()
        writer.writerows(rows)

    print(f"Wrote {len(rows)} rows to {output_file}")


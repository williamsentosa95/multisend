#! /usr/bin/python
import sys
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt

MS_TO_NANO = 1000000

def find_minimum(all_rtts):
  min_length = 0
  for i in range(0, len(all_rtts)):
    if (len(all_rtts[i]) < min_length):
      min_length = len(all_rtts[i])
  return min_length

def missing_seq(seq_nums):
  seq_nums.sort()
  missing = []
  current = 0
  for seq in seq_nums:
    if (seq != current):
      while (current < seq):
        missing.append(current)
        current += 1
      current += 1
    else:
      current += 1
  return missing

def eliminate_redundant_missing(miss1, miss2):
  for miss in miss2:
    if miss in miss1:
      miss2.remove(miss)
  return miss2

def get_trace(fh):
  delay_trace = []
  curr_time = 0
  time_gap = 1
  for line in fh.readlines() :
    fields = line.split();
    if (fields[0] == "Command"):
      time_gap = 1000 / int(fields[5]);
    elif (fields[0] == "INCOMING"):
      rtt_ms = float(fields[7].split("=")[1].strip(",")) * 1000
      one_way_ms = int(rtt_ms / 2)
      delay_trace.append((int(curr_time), one_way_ms)) 
      curr_time = curr_time + time_gap
  return delay_trace

def write_to_file(fh, delay_trace):
  for i in range(0, len(delay_trace)):
    fh.write(str(delay_trace[i][0]) + " " + str(delay_trace[i][1]) + "\n")

if(len(sys.argv) < 3 ) : 
  print("Usage : prep-for-simulation-fixed.py <trace-file> <output-name>")
  exit

trace_file = sys.argv[1]
output_name = sys.argv[2]

fh = open(trace_file, "r")
delay_trace = get_trace(fh)

fh_write = open(output_name, "w")

write_to_file(fh_write, delay_trace)

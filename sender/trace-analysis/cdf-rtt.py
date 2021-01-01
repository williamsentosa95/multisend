#! /usr/bin/python
import sys
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt


fig, ax = plt.subplots()
fig.set_size_inches(20, 6)
plt.margins(x=0, y=0)

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

def get_rtts(fh):
  rtts = []
  for line in fh.readlines() :
      fields = line.split();
      if (fields[0] == "INCOMING"):
        rtt = float(fields[7].split("=")[1].strip(",")) * 1000
        rtts.append(rtt)
  return rtts

def plot_cdf(rtt, i):
  global plt
  num_bins = len(rtt)
  counts, bin_edges = np.histogram (rtt, bins=num_bins)
  cdf = np.cumsum (counts)
  plt.plot (bin_edges[1:], cdf/cdf[-1], label='RTT' + str(i))

if(len(sys.argv) < 2 ) : 
  print("Usage : prep-for-simulation-fixed.py <num-trace> <trace-1> <trace-2> ...")
  exit

num_trace = int(sys.argv[1])

fhs = []
all_rtts = []

for i in range(2, 2+num_trace):
  file_name = sys.argv[i]
  fhs.append(open(file_name, "r"))

for i in range(0, len(fhs)):
  all_rtts.append(get_rtts(fhs[i]))
  print("%d : RTTS min=%.2f 25th=%.2f 50th=%.2f 75th=%.2f 90th=%.2f 95th=%.2f 99th=%.2f max=%.2f" % (i, np.min(all_rtts[i]), np.percentile(all_rtts[i], 25), np.percentile(all_rtts[i], 50), np.percentile(all_rtts[i], 75), np.percentile(all_rtts[i], 90), np.percentile(all_rtts[i], 95), np.percentile(all_rtts[i], 99), np.max(all_rtts[i])))


for i in range(0, len(fhs)):
  plot_cdf(all_rtts[i], i)

SMALL_SIZE = 8
MEDIUM_SIZE = 15
BIGGER_SIZE = 20

ax.set_ylabel('CDF', fontsize=BIGGER_SIZE)
ax.set_xlabel('RTT (ms)', fontsize=BIGGER_SIZE)
ax.legend()
plt.grid(True)
# ax.set_xticks(result_time_up, True)
ax.set_xlim(0, 200)
#plt.tight_layout()

plt.xticks(fontsize=BIGGER_SIZE)
plt.yticks(fontsize=BIGGER_SIZE)
plt.legend(fontsize=MEDIUM_SIZE)

plt.show()
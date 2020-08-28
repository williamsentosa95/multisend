#! /usr/bin/python
import sys
import numpy as np


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

server_seq_nums = []
client_seq_nums = []
rtts = []

if(len(sys.argv) < 2 ) : 
  print("Usage : prep-for-simulation-fixed.py <trace-client> <trace-server>")
  exit

client_file_name = sys.argv[1];
server_file_name = sys.argv[2];
fh_client =open(client_file_name,"r");
fh_server = open(server_file_name, "r");


# Process server trace
# Parse file_name to determine client vs server log_pos-timestamp-sessionID
log_details = server_file_name.split("-");

for line in fh_server.readlines() :
  fields = line.split();
  if (fields[0] == "OUTGOING"):
    seq_num = int(fields[4].split("=")[1].strip(","));
    server_seq_nums.append(seq_num)


# Process client trace
log_details = client_file_name.split("-");


for line in fh_client.readlines() :
  fields = line.split();
  if (fields[0] == "INCOMING"):
    seq_num = int(fields[4].split("=")[1].strip(","));
    client_seq_nums.append(seq_num)
    rtt = float(fields[7].split("=")[1].strip(",")) * 1000
    rtts.append(rtt)


# Find missing seq num and out-of-order
up_miss = missing_seq(server_seq_nums)
down_miss = missing_seq(client_seq_nums)
down_miss = eliminate_redundant_missing(up_miss, down_miss)
print("Up miss=%s, miss %d of %d packets" % (str(up_miss), len(up_miss), (len(server_seq_nums) + len(up_miss))))
print("Down miss=%s, miss %d of %d packets" % (str(down_miss), len(down_miss), len(server_seq_nums)))

rtts_np = np.array(rtts)
print("RTTS min=%.2f 25th=%.2f 50th=%.2f 75th=%.2f 90th=%.2f 95th=%.2f 99th=%.2f max=%.2f" % (np.min(rtts_np), np.percentile(rtts_np, 25), np.percentile(rtts_np, 50), np.percentile(rtts_np, 75), np.percentile(rtts_np, 90), np.percentile(rtts_np, 95), np.percentile(rtts_np, 99), np.max(rtts_np)))
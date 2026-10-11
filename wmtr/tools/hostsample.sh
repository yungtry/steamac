#!/bin/bash
# hostsample.sh OUTFILE: every 30 s log load, CPU idle, swap and the top 3 CPU consumers (host only).
out=$1
while true; do
  t=$(top -l 2 -s 1 -n 3 -o cpu -stats pid,command,cpu 2>/dev/null)
  idle=$(printf '%s\n' "$t" | awk '/CPU usage/{l=$0} END{print l}' | sed -E 's/.* ([0-9.]+)% idle.*/\1/')
  tops=$(printf '%s\n' "$t" | tail -3 | awk '{printf "%s:%s:%s ", $1,$2,$NF}')
  echo "$(date +%H:%M:%S) idle=${idle}% load=$(sysctl -n vm.loadavg | tr -d '{}' | awk '{print $1}') swap=$(sysctl -n vm.swapusage | awk '{print $6}') top=[$tops]" >> "$out"
  sleep 28
done

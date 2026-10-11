#!/bin/bash
# lean-abmat.sh TAG [SECONDS]: d3d11bench A/B matrix, dxvk vs wmtr through Steam's launch chain
# (lean-steamrun.sh), modes alternating per config. CSVs: ~/vrc/lean/ab-TAG/<mode>_d<draws>_f<fill>.csv
set -u
TAG=$1; SEC=${2:-20}; L=$HOME/vrc/lean; O=$L/ab-$TAG; mkdir -p "$O"
for cfg in "200 0" "10000 0" "2000 4" "100 16" "100 32"; do
  set -- $cfg
  for m in dxvk wmtr; do
    n=${m}_d$1_f$2
    echo "$(date +%T) $n" >> "$O/run.log"
    timeout $((SEC + 120)) "$L/lean-steamrun.sh" $m "$L/d3d11bench.exe" --seconds "$SEC" --draws $1 --overdraw $2 \
      --label "$n" --csv "Z:${O//\//\\}\\$n.csv" > "$O/$n.out" 2>&1
    echo "$(date +%T) $n rc=$?" >> "$O/run.log"
  done
done
echo DONE >> "$O/run.log"

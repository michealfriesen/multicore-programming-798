#!/usr/local/bin/bash
(cat "$1" | grep -E "^(DS_TYPENAME|TOTAL_THREADS|MAXKEY|INS|DEL|prefill_elapsed_ms|tree_stats_numNodes|total_queries|query_throughput|total_inserts|total_deletes|update_throughput|total_ops|total_throughput)+=[a-zA-Z0-9\.\_]*$" | cut -d"=" -f2 | tr "\n" ","; echo ) | tee -a results.csv ;

for n in 1 10 20 30 40 ; do
	perf stat -e tx-abort ./benchmark -t 3000 -s 1000000 -k 16 -pin 0-9,20-29,10-19,30-39 -n $n 2>perfstat.txt >output.txt
	tput=`cat output.txt | grep "successful ops" | cut -d":" -f2 | tr -d " "` ; cat perfstat.txt | grep -v "counter stats for" | grep "," | tr -s " " | cut -d"(" -f1 | tr -d "," | awk '{$1=$1;print}' | while read val name; do
		 scaled=`echo "scale=2;$val/$tput" | bc`
		printf "%5s %12s %30s %s\n" "$n" "$tput" "$name/op" "$scaled"
	done
done

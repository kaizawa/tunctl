#!/bin/sh

for dev in tun tap
do
	for ppa in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 
	do
		sudo ./tunctl -t ${dev}${ppa} -b
		if [ $? -ne 0 ]; then
			echo "Faild."
		fi

		## See if ifconfig entry exists.
		sudo ifconfig ${dev}${ppa} > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			echo "Faild."
		fi

		sudo ./tunctl -d ${dev}${ppa} -b
		if [ $? -ne 0 ]; then
			echo "Faild."
		fi

		## See if ifconfig entry was removed.
		sudo ifconfig ${dev}${ppa} > /dev/null 2>&1
		if [ $? -eq 0 ]; then
			echo "Faild."
		fi
	done
done

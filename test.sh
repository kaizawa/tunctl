#!/bin/sh

for dev in tun tap
do
	for ppa in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 
	do
		sudo ./tunctl -t ${dev}${ppa} -b
		if [ $? -ne 0 ]; then
			echo "Failed to add ${dev}${ppa}."
		fi

		## See if ifconfig entry exists.
		sudo ifconfig ${dev}${ppa} > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			echo "Failed to plumb ${dev}${ppa}."
		fi

                if [ $dev -eq "$tun" ]; then
                        ## Destination address for PPP interface
                        dest="10.199.199.2"	       
                fi

		## See if ip address can be set
		sudo ifconfig ${dev}${ppa} 10.199.199.1 $dest > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			echo "Failed to set ip address to ${dev}${ppa}."
		fi

		## See if it's possble to bring it up
		sudo ifconfig ${dev}${ppa} up > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			echo "Failed to bring ${dev}${ppa} up."
		fi

		sudo ./tunctl -d ${dev}${ppa} -b
		if [ $? -ne 0 ]; then
			echo "Failed to delete ${dev}${ppa}."
		fi

		## See if ifconfig entry was removed.
		sudo ifconfig ${dev}${ppa} > /dev/null 2>&1
		if [ $? -eq 0 ]; then
			echo "Failed to unplumb ${dev}${ppa}."
		fi
	done
done

#!/bin/bash

fifo_method="NewReno" # CCA
fifo_tax=0.025
cebinae_tax=0.1
tax_id="newreno-option-6" # folder name

# remove folders with same names to avoid duplicates

rm -rf /mnt/c/Users/KB/FIFO${fifo_method}_$fifo_tax
rm -rf /mnt/c/Users/KB/Cebinae_$cebinae_tax

rm -rf /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}/FIFO_$fifo_tax
rm -rf /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}/Cebinae_$cebinae_tax

# create folders for saving files

mkdir /mnt/c/Users/KB/FIFO${fifo_method}_$fifo_tax
mkdir /mnt/c/Users/KB/Cebinae_$cebinae_tax

mkdir -p /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}
mkdir -p /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}

for ((i = 1; i <= 10; i++)); do
	echo Round: $i
	python3 cebinae.py ns run_batch -c fig1.json --parallel # comment out parallel when doing cebinae
	
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	tcpdump -nn -tt -r ns/Cebinae-0-0.pcap > Cebinae-0-0.out
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	tcpdump -nn -tt -r ns/Cebinae-0-1.pcap > Cebinae-0-1.out
	tcpdump -nn -tt -r ns/Cebinae-0-2.pcap > Cebinae-0-2.out
	tcpdump -nn -tt -r ns/Cebinae-0-3.pcap > Cebinae-0-3.out
	tcpdump -nn -tt -r ns/Cebinae-0-4.pcap > Cebinae-0-4.out
	tcpdump -nn -tt -r ns/Cebinae-12-0.pcap > Cebinae-12-0.out
	tcpdump -nn -tt -r ns/Cebinae-13-0.pcap > Cebinae-13-0.out
	tcpdump -nn -tt -r ns/Cebinae-14-0.pcap > Cebinae-14-0.out
	# tcpdump -nn -tt -r ns/Cebinae-7-0.pcap > Cebinae-7-0.out
	# tcpdump -nn -tt -r ns/Cebinae-8-0.pcap > Cebinae-8-0.out
	# tcpdump -nn -tt -r ns/Cebinae-9-0.pcap > Cebinae-9-0.out
	# tcpdump -nn -tt -r ns/Cebinae-11-0.pcap > Cebinae-11-0.out
	
	# find information only from senders
	# incoming tcp traffic
	grep "49153 >" Cebinae-0-1.out > Cebinae-0-1-1.out
	grep "49153 >" Cebinae-0-2.out > Cebinae-0-2-2.out
	grep "49153 >" Cebinae-0-3.out > Cebinae-0-3-3.out
	grep "49153 >" Cebinae-0-4.out > Cebinae-0-4-4.out
	
	# incoming udp traffic 
	grep "49153 >" Cebinae-12-0.out > Cebinae-12-0-0.out
	grep "49153 >" Cebinae-13-0.out > Cebinae-13-0-0.out
	grep "49153 >" Cebinae-14-0.out > Cebinae-14-0-0.out


	# outgoing tcp/udp traffic
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	grep "1.1.49153 >" Cebinae-0-0.out > Cebinae-0-0-1.out
	grep "2.1.49153 >" Cebinae-0-0.out > Cebinae-0-0-2.out
	grep "3.1.49153 >" Cebinae-0-0.out > Cebinae-0-0-3.out
	grep "4.1.49153 >" Cebinae-0-0.out > Cebinae-0-0-4.out
	grep "UDP" Cebinae-0-0.out > Cebinae-0-0-6.out
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	
	# throughput data
	# grep "49153 >" Cebinae-6-0.out > Cebinae-6-0-5.out
	# grep "49153 >" Cebinae-7-0.out > Cebinae-7-0-6.out
	# grep "49153 >" Cebinae-8-0.out > Cebinae-8-0-7.out
	# grep "49153 >" Cebinae-9-0.out > Cebinae-9-0-8.out
	# grep "49153 >" Cebinae-11-0.out > Cebinae-11-0-10.out
	
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	tcpdump -nn -tt -r ns/FIFO-0-0.pcap > FIFO-0-0.out
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	tcpdump -nn -tt -r ns/FIFO-0-1.pcap > FIFO-0-1.out
	tcpdump -nn -tt -r ns/FIFO-0-2.pcap > FIFO-0-2.out
	tcpdump -nn -tt -r ns/FIFO-0-3.pcap > FIFO-0-3.out
	tcpdump -nn -tt -r ns/FIFO-0-4.pcap > FIFO-0-4.out
	tcpdump -nn -tt -r ns/FIFO-12-0.pcap > FIFO-12-0.out
	tcpdump -nn -tt -r ns/FIFO-13-0.pcap > FIFO-13-0.out
	tcpdump -nn -tt -r ns/FIFO-14-0.pcap > FIFO-14-0.out
	# tcpdump -nn -tt -r ns/FIFO-7-0.pcap > FIFO-7-0.out
	# tcpdump -nn -tt -r ns/FIFO-8-0.pcap > FIFO-8-0.out
	# tcpdump -nn -tt -r ns/FIFO-9-0.pcap > FIFO-9-0.out
	# tcpdump -nn -tt -r ns/FIFO-11-0.pcap > FIFO-11-0.out
	
	# find information only from senders
	# incoming tcp traffic
	grep "49153 >" FIFO-0-1.out > FIFO-0-1-1.out
	grep "49153 >" FIFO-0-2.out > FIFO-0-2-2.out
	grep "49153 >" FIFO-0-3.out > FIFO-0-3-3.out
	grep "49153 >" FIFO-0-4.out > FIFO-0-4-4.out
	
	# incoming udp traffic
	grep "49153 >" FIFO-12-0.out > FIFO-12-0-0.out
	grep "49153 >" FIFO-13-0.out > FIFO-13-0-0.out
	grep "49153 >" FIFO-14-0.out > FIFO-14-0-0.out

	
	# outgoing tcp/udp traffic
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	grep "1.1.49153 >" FIFO-0-0.out > FIFO-0-0-1.out
	grep "2.1.49153 >" FIFO-0-0.out > FIFO-0-0-2.out
	grep "3.1.49153 >" FIFO-0-0.out > FIFO-0-0-3.out
	grep "4.1.49153 >" FIFO-0-0.out > FIFO-0-0-4.out
	grep "UDP" FIFO-0-0.out > FIFO-0-0-6.out
	# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	
	# throughput data
	# grep "49153 >" FIFO-6-0.out > FIFO-6-0-5.out
	# grep "49153 >" FIFO-7-0.out > FIFO-7-0-6.out
	# grep "49153 >" FIFO-8-0.out > FIFO-8-0-7.out
	# grep "49153 >" FIFO-9-0.out > FIFO-9-0-8.out
	# grep "49153 >" FIFO-11-0.out > FIFO-11-0-10.out
	
	# concatenate incoming traffic and sort by time
	cat FIFO-0-1-1.out FIFO-0-2-2.out FIFO-0-3-3.out FIFO-0-4-4.out FIFO-0-6-10.out > 	FIFO-incoming.out
	sed -e 's/^/+ /' FIFO-incoming.out > FIFO-incoming+.out
	
	# concatenate incoming traffic and sort by time
	cat Cebinae-0-1-1.out Cebinae-0-2-2.out Cebinae-0-3-3.out Cebinae-0-4-4.out 	Cebinae-0-6-10.out > Cebinae-incoming.out
	sed -e 's/^/+ /' Cebinae-incoming.out > Cebinae-incoming+.out
	
	tcpdump -nn -tt -r ns/Cebinae-0-0.pcap > Cebinae-0-0.out
	# outgoing traffic
	grep "49153 >" Cebinae-0-0.out > Cebinae-outgoing.out
	sed -e 's/^/- /' Cebinae-outgoing.out > Cebinae-outgoing-.out
	
	tcpdump -nn -tt -r ns/FIFO-0-0.pcap > FIFO-0-0.out
	# outgoing traffic
	grep "49153 >" FIFO-0-0.out > FIFO-outgoing.out
	sed -e 's/^/- /' FIFO-outgoing.out > FIFO-outgoing-.out
	
	# concatenate incoming and outgoing traffic
	cat FIFO-outgoing-.out FIFO-incoming+.out > FIFO-full.out
	
	# concatenate incoming and outgoing traffic
	cat Cebinae-outgoing-.out Cebinae-incoming+.out > Cebinae-full.out
	
	# sort according to time
	sort -k2,2 -n FIFO-full.out > FIFO-full_sorted.out
	
	# sort according to time
	sort -k2,2 -n Cebinae-full.out > Cebinae-full_sorted.out
	
	# create a duplicates file (sort on 3rd-> field [IP...], sort on 2nd field [time only])
	sort -k3 -k2,2n FIFO-full_sorted.out > FIFO-full_sorted_duplicates.out
	sort -k3 -k2,2n Cebinae-full_sorted.out > Cebinae-full_sorted_duplicates.out

	# grep for TCP only
	grep "HTTP" FIFO-full_sorted_duplicates.out > FIFO-full_sorted_duplicates_TCP.out
	grep "HTTP" Cebinae-full_sorted_duplicates.out > Cebinae-full_sorted_duplicates_TCP.out

	# concatenate 1, 2, 3, 4 files to build sorted total throughput file
	cat FIFO-0-0-1.out FIFO-0-0-2.out FIFO-0-0-3.out FIFO-0-0-4.out > FIFO-full-throughput.out
	cat Cebinae-0-0-1.out Cebinae-0-0-2.out Cebinae-0-0-3.out Cebinae-0-0-4.out > Cebinae-full-throughput.out
	
	grep "HTTP" FIFO-full-throughput.out > FIFO-full-throughput-HTTP.out
	grep "HTTP" Cebinae-full-throughput.out > Cebinae-full-throughput-HTTP.out

	sort -k1 -n FIFO-full-throughput-HTTP.out > FIFO-full-throughput-sorted.out
	sort -k1 -n Cebinae-full-throughput-HTTP.out > Cebinae-full-throughput-sorted.out

	# ----------------------- finding dropped packets in each flow ------------------ #
	# grep "1.1.1.1.49153 >" FIFO-0-0.out > FIFO-packetloss-0-1.out
	# grep "1.1.2.1.49153 >" FIFO-0-0.out > FIFO-packetloss-0-2.out
	# grep "1.1.3.1.49153 >" FIFO-0-0.out > FIFO-packetloss-0-3.out
	# grep "1.1.4.1.49153 >" FIFO-0-0.out > FIFO-packetloss-0-4.out

	# Source directory containing files .out files for throughput, fairness, and queue
	source_dir="/home/kimb112/Cebinae/"
	
	# Destination directory where you want to move files
	mkdir /home/kimb112/Cebinae/FIFO${fifo_method}_$fifo_tax_$i
	mkdir /home/kimb112/Cebinae/Cebinae_$cebinae_tax_$i
	destination_dir_FIFO="/home/kimb112/Cebinae/FIFO${fifo_method}_$fifo_tax_$i"
	destination_dir_Cebinae="/home/kimb112/Cebinae/Cebinae_$cebinae_tax_$i"

	# remove .pcap files to save space
	for dir in */
	do
	    cd $dir
	    rm *.pcap
	    cd ..
	    pwd
	    rm -rf Cebinae-0-0.out Cebinae-0-1.out Cebinae-0-1-1.out Cebinae-0-2.out Cebinae-0-2-2.out Cebinae-0-3.out Cebinae-0-3-3.out Cebinae-0-4.out Cebinae-0-4-4.out Cebinae-0-6.out Cebinae-6-0.out Cebinae-12-0.out Cebinae-13-0.out Cebinae-14-0.out Cebinae-full.out Cebinae-full_sorted_duplicates.out Cebinae-incoming.out Cebinae-incoming+.out Cebinae-outgoing.out Cebinae-outgoing-.out Cebinae-full-throughput.out Cebinae-full-throughput-HTTP.out Cebinae-full_sorted.out
	    rm -rf FIFO-0-0.out FIFO-0-1.out FIFO-0-1-1.out FIFO-0-2.out FIFO-0-2-2.out FIFO-0-3.out FIFO-0-3-3.out FIFO-0-4.out FIFO-0-4-4.out FIFO-0-6.out FIFO-6-0.out FIFO-12-0.out FIFO-13-0.out FIFO-14-0.out FIFO-full.out FIFO-full_sorted_duplicates.out FIFO-incoming.out FIFO-incoming+.out FIFO-outgoing.out FIFO-outgoing-.out FIFO-full-throughput.out FIFO-full-throughput-HTTP.out FIFO-full_sorted.out
	done

	# Move files matching the naming convention from source directory to destination directory
	find "$source_dir" -type f -name 'FIFO*' -exec mv {} "$destination_dir_FIFO" \;
	find "$source_dir" -type f -name 'Cebinae*' -exec mv {} "$destination_dir_Cebinae" \;

	mv $destination_dir_FIFO /mnt/c/Users/KB/FIFO${fifo_method}_$fifo_tax
	mv $destination_dir_Cebinae /mnt/c/Users/KB/Cebinae_$cebinae_tax
	
done

# Move files to a location on C: drive
mv /mnt/c/Users/KB/FIFO${fifo_method}_$fifo_tax /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}/FIFO_$fifo_tax
mv /mnt/c/Users/KB/Cebinae_$cebinae_tax /mnt/c/Users/KB/OneDrive\ -\ University\ of\ North\ Carolina\ at\ Chapel\ Hill/Documents/School/Research/KaurLabFall2023/Experiments/${tax_id}/Cebinae_$cebinae_tax

#!/bin/sh
# build optimized
# python3 cebinae.py ns configure -p optimized
# run experiment batch fig1
python3 cebinae.py ns run_batch -c fig1.json --parallel
# plot fig1
python3 cebinae.py plot --plot_target fig1 --data_path ns/tmp_index/fig1/

tcpdump -nn -tt -r ns/Cebinae-0-1.pcap > Cebinae-0-1.out
tcpdump -nn -tt -r ns/Cebinae-0-2.pcap > Cebinae-0-2.out
tcpdump -nn -tt -r ns/Cebinae-0-3.pcap > Cebinae-0-3.out
tcpdump -nn -tt -r ns/Cebinae-0-4.pcap > Cebinae-0-4.out
tcpdump -nn -tt -r ns/Cebinae-0-6.pcap > Cebinae-0-6.out
tcpdump -nn -tt -r ns/Cebinae-6-0.pcap > Cebinae-6-0.out
tcpdump -nn -tt -r ns/Cebinae-7-0.pcap > Cebinae-7-0.out
tcpdump -nn -tt -r ns/Cebinae-8-0.pcap > Cebinae-8-0.out
tcpdump -nn -tt -r ns/Cebinae-9-0.pcap > Cebinae-9-0.out
tcpdump -nn -tt -r ns/Cebinae-11-0.pcap > Cebinae-11-0.out

# find information only from senders
# incoming tcp traffic
grep "1.1.1.1.49153 >" Cebinae-0-1.out > Cebinae-0-1-1.out
grep "1.1.2.1.49153 >" Cebinae-0-2.out > Cebinae-0-2-2.out
grep "1.1.3.1.49153 >" Cebinae-0-3.out > Cebinae-0-3-3.out
grep "1.1.4.1.49153 >" Cebinae-0-4.out > Cebinae-0-4-4.out

# incoming udp traffic 
grep "10.0.0.1.49153 >" Cebinae-0-6.out > Cebinae-0-6-10.out

# throughput data
grep "1.1.1.1.49153 >" Cebinae-6-0.out > Cebinae-6-0-5.out
grep "1.1.2.1.49153 >" Cebinae-7-0.out > Cebinae-7-0-6.out
grep "1.1.3.1.49153 >" Cebinae-8-0.out > Cebinae-8-0-7.out
grep "1.1.4.1.49153 >" Cebinae-9-0.out > Cebinae-9-0-8.out
grep "10.0.0.1.49153 >" Cebinae-11-0.out > Cebinae-11-0-10.out

tcpdump -nn -tt -r ns/FIFO-0-1.pcap > FIFO-0-1.out
tcpdump -nn -tt -r ns/FIFO-0-2.pcap > FIFO-0-2.out
tcpdump -nn -tt -r ns/FIFO-0-3.pcap > FIFO-0-3.out
tcpdump -nn -tt -r ns/FIFO-0-4.pcap > FIFO-0-4.out
tcpdump -nn -tt -r ns/FIFO-0-6.pcap > FIFO-0-6.out
tcpdump -nn -tt -r ns/FIFO-6-0.pcap > FIFO-6-0.out
tcpdump -nn -tt -r ns/FIFO-7-0.pcap > FIFO-7-0.out
tcpdump -nn -tt -r ns/FIFO-8-0.pcap > FIFO-8-0.out
tcpdump -nn -tt -r ns/FIFO-9-0.pcap > FIFO-9-0.out
tcpdump -nn -tt -r ns/FIFO-11-0.pcap > FIFO-11-0.out

# find information only from senders
# incoming tcp traffic
grep "1.1.1.1.49153 >" FIFO-0-1.out > FIFO-0-1-1.out
grep "1.1.2.1.49153 >" FIFO-0-2.out > FIFO-0-2-2.out
grep "1.1.3.1.49153 >" FIFO-0-3.out > FIFO-0-3-3.out
grep "1.1.4.1.49153 >" FIFO-0-4.out > FIFO-0-4-4.out

# incoming udp traffic
grep "10.0.0.1.49153 >" FIFO-0-6.out > FIFO-0-6-10.out

# throughput data
grep "1.1.1.1.49153 >" FIFO-6-0.out > FIFO-6-0-5.out
grep "1.1.2.1.49153 >" FIFO-7-0.out > FIFO-7-0-6.out
grep "1.1.3.1.49153 >" FIFO-8-0.out > FIFO-8-0-7.out
grep "1.1.4.1.49153 >" FIFO-9-0.out > FIFO-9-0-8.out
grep "10.0.0.1.49153 >" FIFO-11-0.out > FIFO-11-0-10.out

tcpdump -nn -tt -r ns/Cebinae-0-0.pcap > Cebinae-0-0.out
# outgoing tcp traffic
grep "49153 > 100" Cebinae-0-0.out > Cebinae-0-0-tcp.out
# outgoing udp traffic
grep "49153 > 20" Cebinae-0-0.out > Cebinae-0-0-udp.out

tcpdump -nn -tt -r ns/FIFO-0-0.pcap > FIFO-0-0.out
# outgoing tcp traffic
grep "49153 > 100" FIFO-0-0.out > FIFO-0-0-tcp.out
# outgoing udp traffic
grep "49153 > 20" FIFO-0-0.out > FIFO-0-0-udp.out

# Source directory containing files .out files for throughput, fairness, and queue
source_dir="/home/kb/comp495/Cebinae/"

# Destination directory where you want to move files
destination_dir_FIFO="/home/kb/comp495/Cebinae/FIFOAlways_0.075"
destination_dir_Cebinae="/home/kb/comp495/Cebinae/Cebinae_0.5"

# Move files matching the naming convention from source directory to destination directory
find "$source_dir" -type f -name 'FIFO*' -exec mv {} "$destination_dir_FIFO" \;
find "$source_dir" -type f -name 'Cebinae*' -exec mv {} "$destination_dir_Cebinae" \;
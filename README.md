# Fast-File-Transfer-Protocol
## Design Methodology:
* TCP is used to exchange file name and size to ensure maximum reliability
* Memory map is used as a storage mechanism
* UDP is used in sending the data packets and control packets

## Steps to Compile and Run:
1. We need two computers one for reciever and other for sender and they should be connected to the network via 1GBps router and <CAT.5E ethernet cables

2. compile with the following commands in two machines
	g++ SEND.c -o send
	g++ REC.c -o receiver

3. Add delay and loss in both the machines with the following command,
	sudo tc qdisc add dev enp4s0 root netem delay 100ms loss 10%

NOTE: enp4s0 is the name of the interface

4. Limit the speed of the link to 100Mbps, so run the following commands in both machines
	sudo ethtool -s enp4so speed 100

5. In the sender side, create a temperory file system and mount over it and then create 1GB file in it
	sudo mkdir /mnt/ramdisk
	sudo mount -t tmpfs -o rw,size=2G tmpfs /mnt/ramdisk
	sudo dd if=/dev/urandom of=data.bin bs=1M count=1000

6. Run the sender executable from sender desktop with the following command
	./send /mnt/ramdisk/data.bin 192.168.0.137 2900 2901

7. Run the receiver executable from the receiver desktop with the following command
	./receiver 2500 2501

Note: The number '2500' can be any port number, the '198.168.1.8' is the IP address of server

8. After you enter the above command in machine, timestamp for start and end will be printed and finally throughput will appear once file is transfer

9. The connection will be terminated


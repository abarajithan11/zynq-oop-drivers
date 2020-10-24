## Results:

Set speed   : 1 Gbps
Speed       : 42.81 MB/s = 342.463819 Mbps
Time for 100 RGB frames (384,384) = 2.09 seconds


## Set fixed speed in computer:

Control Panel -> Network and Internet - > Network Connections -> Ethernet -> Properties -> Configure -> Advanced

Speed & Duplex = 1.0 Gbps Full Duplex

## Set PC's IP static

Settings -> Ethernet -> Unidentified network -> Edit (IP settings)

IP: 192.168.1.x
Subnet prefix length: 24
Gateway: 192.168.1.1

## RTOS BSP Settings

Change [these](https://xilinx-wiki.atlassian.net/wiki/spaces/A/pages/21430395/Zynq-7000+AP+SoC+Performance+Gigabit+Ethernet+achieving+the+best+performance) BSP settings

## Jumbo frame

PC: Ethernet > properties > configure > Advanced > Jumbo frame

ZYNQ: BSP settings > lwip > temac > jumbo
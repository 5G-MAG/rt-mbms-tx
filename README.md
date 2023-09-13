# LTE-based 5G Broadcast Transmitter

## Introduction

This repository holds the standalone LTE-based 5G Broadcast transmitter part of the 5G-MAG Reference Tools.

### Specifications

A list of specification related to this repository is available in
the [Standards Wiki](https://github.com/5G-MAG/Standards/wiki/MBMS-&-LTE-based-5G-Broadcast:-Relevant-Specifications).

### About the implementation

This implementation of an LTE-Based 5G Broadcast transmitter is based on the existing MBMS implementation
in [srsRAN_4G](https://github.com/srsran/srsRAN_4G) eNodeB, modified to include a feature set of 3GPP Rel-17 LTE-based
5G Terrestrial Broadcast. It also includes a basic MBMS gateway which creates a virtual network interface sgi_mb which
receives IP multimedia traffic. Note that an instance of an EPC is also required to be executed as part of the srsRAN_4G
implementation. The eNodeB generates an LTE-based 5G Broadcast radio-frequency signal (I/Q samples) which can then be
input to a USRP.

Note that the implementation lacks an M2 interface, therefore control plane data is obtained from configuration files.
Only a single MCH can be transmitted.

Additional information can be found in the srsRAN
documentation: https://docs.srsran.com/projects/4g/en/latest/app_notes/source/embms/source/index.html

A list of currently supported features is available [here](https://github.com/5G-MAG/rt-mbms-tx/wiki/Features).

## Install dependencies

In Ubuntu:

````
sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```` 

## Downloading

The source can be obtained by cloning the github repository.

```bash
cd ~
git clone --recurse-submodules https://github.com/5G-MAG/rt-mbms-tx.git
cd rt-mbms-tx
git submodule update
```

## Building

To build the LTE-based 5G Broadcast transmitter from the source:

```bash
cd ~/rt-mbms-tx
mkdir build
cd build
cmake ../
make
make test
```

## Installing

To install the LTE-based 5G Broadcast transmitter:

```
sudo make install
srsran_install_configs.sh user
```

## Running

Open the `build` directory and launch the MBMS gateway, and the srsEPC, in different terminals.

````  
sudo ./srsepc/src/srsmbms
sudo ./srsepc/src/srsepc
```` 

Both can be launched in the background appending `` & `` at the end of the command.

After running the MBMS gateway, executing `` ip a `` in the terminal should show a network interface
called `` sgi_mb ``. The IP traffic needs to be routed to this new interface. Since it's a multicast interface
`smcroutectl` is used.

Run the following command to restart the multicast route table:

`` sudo smcroutectl restart ``

Add a rule to redirect the traffic from the physical network interface of the PC with a specific IP address, to the
sgi_mb. To get the name of the physical network interface run `` ip a `` in the terminal and look for an interface with
a
name similar to en0 or enp0.

`` sudo smcroutectl add eno0 239.255.1.1 sgi_mb ``

The IP address is the source of the incoming IP traffic.

If the IP traffic is been generated on the PC using PCAP, ffmpeg or similar, it is recommended to create a rule that
avoids the multicast traffic to go to the network where the PC is connected to avoid flooding it with multicast traffic.
For that reason run the following command:

`` sudo route add -net 239.255.1.0 netmask 255.255.255.0 dev sgi_mb ``

At this point an IP transmission from a multimedia file using ffmpeg can be created. As an example, execute the following
command to transmit an infinite running MPEG TS to the transmitter:

`` while true; do ffmpeg -re -i file.ts -c copy -map 0 -f rtp_mpegts rtp://239.255.1.1:9988; done ``

The last step is to launch the eNodeB. The eNodeB needs the following configuration files:
Copy `` the sib.conf.mbsfn `` to the folder `` rt-tx-poc/build/ ``, and `enb.conf` to `` /root/.config/srsran/enb.conf ``.
Launch the eNB with `` sudo ./srsenb``.

## Development

This project follows
the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). The
`development` branch of this project serves as an integration branch for new features. Consequently, please make sure to
switch to the `development` branch before starting the implementation of a new feature.

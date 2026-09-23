<h1 align="center">Standalone 5G Broadcast Transmitter</h1>
<p align="center">
  <img src="https://img.shields.io/badge/Status-Under_Development-yellow" alt="Under Development">
  <img src="https://img.shields.io/github/v/tag/5G-MAG/rt-mbms-tx?label=version" alt="Version">
  <img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg" alt="License">
</p>

## Introduction

The 5G Broadcast Transmitter is an extension of an MBMS-enabled eNodeB tailored to operate as a 5G Broadcast transmitter without uplink.

Additional information can be found at: https://5g-mag.github.io/Getting-Started/pages/lte-based-5g-broadcast/

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

## Install dependencies

In Ubuntu:

```
sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```

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
```

## Configuration after installation
Install the configuration:

```
srsran_install_configs.sh user
```

After the installtion, you can adjust the enb, rr, epc config files to your desired frequency, bandwith, tx gain, MNC, MCC ...

[Configuration Templates](https://github.com/5G-MAG/rt-mbms-tx/tree/main/Config-Template) can be downloaded and placed in ``/root/.config/srsran/`` for execution after installation.

Also make sure to copy the adapted sib.conf.mbsfn file to the build directory:

```
cd rt-mbms-tx/Config-Template
cp sib.conf.mbsfn ../build/sib.conf.mbsfn
```

## Running
Starting the transmitter requires the follwing 3 steps:
1. Starting the MBMS-Gateway
2. Starting the EPC
3. Starting the eNodeB

Note that running the eNodeB may require an SDR platform. Check the following tutorial for support: https://5g-mag.github.io/Getting-Started/pages/3gpp-ran-and-core-platforms/tutorials/sdr-platforms.html

### Starting the MBMS-Gateway

MBMS-GW no longer lives in this repo -- it now ships from its own standalone repo,
`rt-mbms-gw` (submodules this repo for the shared code it still needs). Build and run it
from there instead:

```
cd rt-mbms-gw/build
sudo ./mbms-gw/mbms-gw mbms-gw.conf.example
```

The MBMS-GW receives multicast packets in one tunnel interface, which are packaged to GTP-U-Packets and sent to the eNodeB over another tunnel interface.
The command above creates the sgi_mb interface (visible with ``ifconfig``).

Incoming traffic can be redirected the sgi_mb:

Run the following command to restart the multicast route table:

```
sudo smcroutectl restart
```

Add a rule to redirect the traffic from the physical network interface of the PC with a specific IP address, to the
sgi_mb. To get the name of the physical network interface run `` ip a `` in the terminal and look for an interface with
a name similar to en0 or enp0.

```
sudo smcroutectl add eno0 239.255.1.1 sgi_mb
```

The IP address is the source of the incoming IP traffic.

If the IP traffic is generated on the PC using PCAP, ffmpeg or similar, it is recommended to create a rule that
avoids the multicast traffic to go to the network where the PC is connected to avoid flooding it with multicast traffic.
For that reason run the following command:

```
sudo route add -net 239.255.1.0 netmask 255.255.255.0 dev sgi_mb
```

### Starting the EPC

```
sudo ./srsepc/src/srsepc
```

### Starting the eNodeB

```
cd build
sudo ./srsenb/src/srsenb
```

## Development

This project follows
the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow). The
`development` branch of this project serves as an integration branch for new features. Consequently, please make sure to
switch to the `development` branch before starting the implementation of a new feature.

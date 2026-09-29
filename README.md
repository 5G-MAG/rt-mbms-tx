<p align="center">
  <img src=".github/banner.svg" width="100%" alt="Reference Tools · 5G Broadcast - TV and Radio Services: 5G Broadcast Transmitter">
</p>

<p align="center">
  An extension of the MBMS-enabled srsRAN_4G eNodeB that operates as an LTE-based 5G Broadcast
  transmitter without uplink.
</p>

<p align="center">
  <img alt="Status: under development"
    src="https://img.shields.io/badge/Status-Under%20Development-e67e22">
  <a href="https://github.com/5G-MAG/rt-mbms-tx/releases"><img alt="Version"
    src="https://img.shields.io/github/v/release/5G-MAG/rt-mbms-tx?label=Version"></a>
  <a href="LICENSE"><img alt="License: GNU Affero General Public License v3.0"
    src="https://img.shields.io/badge/License-AGPL%20v3.0-blue"></a>
</p>

<p align="center">
  <a href="https://www.5g-mag.com/reference-tools/5g-broadcast/">Project page</a> &nbsp;&middot;&nbsp;
  <a href="https://github.com/5G-MAG/rt-mbms-tx/issues">Issues</a> &nbsp;&middot;&nbsp;
  <a href="https://www.5g-mag.com/contributing">Contributing</a>
</p>

---

## At a glance

|  |  |
|---|---|
| **Part of** | [5G Broadcast - TV and Radio Services](https://www.5g-mag.com/reference-tools/5g-broadcast/), alongside [rt-libflute](https://github.com/5G-MAG/rt-libflute), [rt-mbms-application](https://github.com/5G-MAG/rt-mbms-application), [rt-mbms-application-provider](https://github.com/5G-MAG/rt-mbms-application-provider), [rt-mbms-bmsc](https://github.com/5G-MAG/rt-mbms-bmsc), [rt-mbms-client](https://github.com/5G-MAG/rt-mbms-client), [rt-mbms-examples](https://github.com/5G-MAG/rt-mbms-examples), [rt-mbms-gw](https://github.com/5G-MAG/rt-mbms-gw), [rt-mbms-modem](https://github.com/5G-MAG/rt-mbms-modem), [rt-mbms-mw-android](https://github.com/5G-MAG/rt-mbms-mw-android) and [rt-mbms-tx-for-qrd-and-crd](https://github.com/5G-MAG/rt-mbms-tx-for-qrd-and-crd) |

## Introduction

The transmitter takes the MBMS implementation in the [srsRAN_4G](https://github.com/srsran/srsRAN_4G)
eNodeB and adds a feature set of 3GPP Rel-17 LTE-based 5G Terrestrial Broadcast. The eNodeB
generates the LTE-based 5G Broadcast radio-frequency signal as I/Q samples, which can be fed to a
USRP. Background on LTE-based 5G Broadcast is at
<https://www.5g-mag.com/reference-tools/5g-broadcast/>.

### About the implementation

- A basic MBMS gateway creates a virtual network interface, `sgi_mb`, which receives the IP
  multimedia traffic.
- An EPC instance, part of the srsRAN_4G implementation, must also run.
- There is no M2 interface, so control plane data is taken from configuration files.
- Only a single MCH can be transmitted.

More detail is in the srsRAN
documentation: https://docs.srsran.com/projects/4g/en/latest/app_notes/source/embms/source/index.html

## Install dependencies

On Ubuntu:

```
sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev
```

## Downloading

Clone the repository:

```bash
cd ~
git clone --recurse-submodules https://github.com/5G-MAG/rt-mbms-tx.git
cd rt-mbms-tx
git submodule update
```

## Building

Build the transmitter from source:

```bash
cd ~/rt-mbms-tx
mkdir build
cd build
cmake ../
make
make test
```

## Installing

Install the transmitter:

```
sudo make install
```

### Configuration after installation

Install the configuration files:

```
srsran_install_configs.sh user
```

After installation, adjust the enb, rr and epc configuration files to your frequency, bandwidth,
TX gain, MNC, MCC and other settings.

[Configuration Templates](https://github.com/5G-MAG/rt-mbms-tx/tree/main/Config-Template) can be
downloaded and placed in `/root/.config/srsran/` for use after installation.

Copy the adapted `sib.conf.mbsfn` file to the build directory:

```
cd rt-mbms-tx/Config-Template
cp sib.conf.mbsfn ../build/sib.conf.mbsfn
```

## Running

Starting the transmitter takes three steps:

1. Start the MBMS gateway
2. Start the EPC
3. Start the eNodeB

Running the eNodeB may require an SDR platform; see the
[SDR platforms tutorial](https://www.5g-mag.com/reference-tools/3gpp-platforms/tutorials/sdr-platforms).

### Starting the MBMS gateway

```
cd build
sudo ./srsepc/src/srsmbms
```

The MBMS gateway receives multicast packets on one tunnel interface, packs them into GTP-U packets
and sends them to the eNodeB over another tunnel interface. The command above creates the `sgi_mb`
interface (visible with `ifconfig`).

Incoming traffic can be redirected to `sgi_mb`. First restart the multicast route table:

```
sudo smcroutectl restart
```

Then add a rule that redirects the traffic with a given IP address from the PC's physical network
interface to `sgi_mb`. To find the name of the physical interface, run `` ip a `` and look for an
interface named like en0 or enp0.

```
sudo smcroutectl add eno0 239.255.1.1 sgi_mb
```

The IP address is the source of the incoming IP traffic.

If the IP traffic is generated on the PC itself (with PCAP, ffmpeg or similar), add a route that
keeps the multicast traffic off the network the PC is connected to, so that network is not flooded:

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
the [Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow).
The `development` branch is the integration branch for new features, so switch to it before
starting work on a new feature.

## Contributing

Contributions are welcome. How to raise an issue, fork the repository and open a pull request, and
the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

## License

Distributed under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE). The srsRAN
copyright and the notices for third-party files used within srsRAN are in [COPYRIGHT](COPYRIGHT).

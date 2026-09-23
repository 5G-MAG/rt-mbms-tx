<p align="center">
  <img src=".github/banner.svg" width="100%" alt="Reference Tools · 5G Broadcast - TV and Radio Services: 5G Broadcast Transmitter">
</p>

<p align="center">
  EPC and eNB for an LTE-based 5G Terrestrial Broadcast cell: a standalone srsRAN fork that transmits FeMBMS, carries MBMS sessions over M3AP and relays Public Warning System messages over SBc-AP.
</p>

<p align="center">
  <img alt="Status: under development"
    src="https://img.shields.io/badge/Status-Under_Development-yellow">
  <a href="https://github.com/5G-MAG/rt-mbms-tx/releases"><img alt="Version"
    src="https://img.shields.io/github/v/release/5G-MAG/rt-mbms-tx?label=Version&sort=semver"></a>
  <a href="LICENSE"><img alt="License: GNU Affero General Public License v3.0"
    src="https://img.shields.io/badge/License-AGPL%20v3.0-blue"></a>
</p>

<p align="center">
  <a href="https://www.5g-mag.com/reference-tools/5g-broadcast">Project page</a> &nbsp;&middot;&nbsp;
  <a href="https://github.com/5G-MAG/rt-mbms-tx/issues">Issues</a> &nbsp;&middot;&nbsp;
  <a href="https://www.5g-mag.com/contributing">Contributing</a>
</p>

---

## At a glance

|  |  |
|---|---|
| **Implements** | TS 36.211, TS 36.212, TS 36.213 and TS 36.331 for the FeMBMS radio; TS 29.274 (GTPv2-C) on Sm; TS 36.300 clause 15 for multi-PMCH. The repository does not record the version of each document it was built against. |
| **Role** | Transmit side: the EPC (`srsepc`) and the eNB (`srsenb`) |
| **Works with** | [rt-mbms-gw](https://github.com/5G-MAG/rt-mbms-gw), [rt-mbms-bmsc](https://github.com/5G-MAG/rt-mbms-bmsc) and [rt-mbms-modem](https://github.com/5G-MAG/rt-mbms-modem) |
| **Part of** | [5G Broadcast - TV and Radio Services](https://www.5g-mag.com/reference-tools/5g-broadcast) |

## Specification

Built against the documents named above. Clause-by-clause coverage, and what is still absent, is
recorded on the project page rather than here:
<https://www.5g-mag.com/reference-tools/5g-broadcast>

## Install dependencies

The system packages for the whole broadcast chain, this component included, are listed in one
place and verified from a clean machine by `check-build-from-clean.sh`:
<https://github.com/5G-MAG/rt-mbms-examples/blob/main/scripts/mbms-broadcast-demo/README.md>

## Introduction

The 5G Broadcast Transmitter is an MBMS-enabled eNodeB extended to operate as an LTE-based
5G Terrestrial Broadcast transmitter (FeMBMS, no uplink). It is a fork of
[5G-MAG/rt-mbms-tx](https://github.com/5G-MAG/rt-mbms-tx), which is itself derived from the
MBMS implementation in [srsRAN_4G](https://github.com/srsran/srsRAN_4G). It generates the
LTE-based 5G Broadcast radio-frequency signal (I/Q samples) that is fed to an SDR (USRP,
BladeRF, SoapySDR device) or to a software radio (ZeroMQ virtual RF).

The feature set targets 3GPP LTE-based 5G Terrestrial Broadcast as profiled in
**ETSI TS 103 720**, spanning Rel-14 (FeMBMS baseline) through Rel-19 (Phase 2 PMCH
enhancements and CAS muting). For the full, feature-by-feature specification coverage audit,
see **the specification-coverage doc (kept separately, not in this repo)**.

Additional background: https://5g-mag.github.io/Getting-Started/pages/lte-based-5g-broadcast/

### What this repository builds

This repository builds **two binaries** via a single CMake project:

- **`srsenb`** - the LTE-based 5G Terrestrial Broadcast / FeMBMS eNodeB (srsRAN-derived).
  Implements the PHY (MIB-MBMS, CAS, PMCH), MAC (MCCH/MSI/MTCH scheduling), and RRC
  (SIB1-MBMS, SIB13, MBSFN area configuration, MCCH). It connects to the EPC over S1AP and,
  acting as its own distributed MCE (TS 23.246 clause 5.9.1), over M3AP. It also hosts an
  optional TCP control server for live eMBMS reconfiguration (`[control]` section).
- **`srsepc`** - the Evolved Packet Core: MME (+ HSS, + SP-GW). It carries S1AP toward the
  eNB, M3AP toward the eNB-as-MCE, Sm/GTPv2-C toward the MBMS-GW, and SBc-AP (Public Warning
  System / emergency alerts) toward a Cell Broadcast Centre. The MME additionally exposes a
  TCP "portal bridge" for SBc-AP (`[mme_sbc]` section).

The MBMS-GW no longer lives in this repository; it ships from the standalone `rt-mbms-gw`
repository. An `srsue` tree is present but is not part of the broadcast transmit chain and is
disabled in the FeMBMS build path.

> Note on interfaces: control-plane MBMS session data can be driven either from configuration
> files (the standalone default) or over the real M3AP/Sm/SBc-AP interfaces implemented here.

## Where it sits in the 5G Broadcast architecture

```
   Content / CBC                MBMS-GW (rt-mbms-gw)          srsepc                 srsenb
   ------------                 --------------------          ------                 ------
   IP multicast  ── M1-U ──▶  sgi_mb ─┐                                        ┌── eNB PHY ──▶ SDR / ZMQ ──▶ UE
   GTP-U                               ├── Sm (GTPv2-C) ──▶  MME  ── M3AP ──▶  │   (I/Q samples)
                                       └── M1-U (GTP-U)  ─────────────────────▶│
   CBC/CBE  ── SBc-AP (SCTP) ─────────────────────────────▶  MME  ── S1AP ──▶  eNB (WriteReplaceWarning)
                                                                     S1AP  ──▶  eNB (setup, NAS, warning)
```

## Prerequisites and dependencies

On Ubuntu:

```bash
sudo apt-get install build-essential cmake libfftw3-dev libmbedtls-dev \
    libboost-program-options-dev libconfig++-dev libsctp-dev
```

For a software (no-hardware) radio via ZeroMQ, also install `libzmq3-dev`. For real SDR
front-ends, install the corresponding driver (UHD, BladeRF, SoapySDR). `srsenb` requires at
least one RF back-end (UHD / BladeRF / SoapySDR / ZeroMQ) to be present at configure time.

The EPC/SP-GW additional runtime tools (multicast routing) use `smcroute` and `iproute2`.

## Downloading

This repo has no git submodules -- a plain clone is all that's needed:

```bash
cd ~
git clone <this-fork-url> rt-mbms-tx
cd rt-mbms-tx
```

## Building

Standard out-of-tree CMake build:

```bash
cd ~/rt-mbms-tx
mkdir build
cd build
cmake ..
make -jN          # N = number of parallel jobs, e.g. make -j8
make test         # optional: run the unit-test suite
```

The binaries are produced at:

- `build/srsenb/src/srsenb`
- `build/srsepc/src/srsepc`

### Building just srsenb or just srsepc

Each application is gated by a CMake option (all default `ON`):

```bash
# Build only the eNB (skip EPC and UE):
cmake .. -DENABLE_SRSENB=ON -DENABLE_SRSEPC=OFF -DENABLE_SRSUE=OFF

# Build only the EPC:
cmake .. -DENABLE_SRSENB=OFF -DENABLE_SRSEPC=ON -DENABLE_SRSUE=OFF
```

Relevant options:

| Option | Default | Purpose |
|---|---|---|
| `ENABLE_SRSENB` | ON | Build the `srsenb` eNB application |
| `ENABLE_SRSEPC` | ON | Build the `srsepc` EPC application |
| `ENABLE_SRSUE` | ON | Build the (non-broadcast) `srsue` application |
| `ENABLE_ZEROMQ` | ON | Enable the ZeroMQ software-radio back-end |
| `ENABLE_TIDY` | OFF | Run clang-tidy during the build (only if `clang-tidy` is found) |

Note: the 5G-NR/gNB stack link in `srsenb` is disabled in this LTE-only fork via an internal
flag (`SRSENB_ENABLE_5GNR`, hard-set to `FALSE` in `srsenb/src/CMakeLists.txt`). The NR ASN.1
codec libraries stay linked because the eNB config parser needs them.

## Installing (optional)

```bash
sudo make install
srsran_install_configs.sh user     # installs config templates under ~/.config/srsran/
```

## Configuration files and where they live

The transmit-chain configuration templates are kept in **`Config-Template/`** (the
recognizable starting point) and as `*.example` files next to their sources. The binaries
look for configs in this order: explicit path on the command line, then
`~/.config/srsran/<name>`, then `/etc/srsran/<name>`.

| File | Purpose | Ships as |
|---|---|---|
| `enb.conf` | eNB top-level config (`[enb]`, `[rf]`, `[embms]`, `[control]`, `[enb_files]`). References the SIB/RR/RB files. | `Config-Template/enb.conf`, `srsenb/enb.conf.example` |
| `sib.conf.mbsfn` | SIB1/2/3 plus SIB13 (MBSFN) for the eNB. Use this instead of `sib.conf` when MBMS is enabled. | `Config-Template/sib.conf.mbsfn`, `srsenb/sib.conf.mbsfn.example` |
| `rr.conf` | Radio-resource configuration. | `srsenb/rr.conf.example` |
| `rb.conf` | SRB/DRB configuration. | `srsenb/rb.conf.example` |
| `sib12_alert.conf` | SIB12 emergency-alert (ETWS/CMAS) template, loaded via SIGUSR1 (activate) / SIGUSR2 (cancel). | `Config-Template/sib12_alert.conf`, `srsenb/sib12_alert.conf.example` |
| `epc.conf` | EPC/MME config (`[mme]`, `[hss]`, `[spgw]`, `[mme_sm]`, `[mme_m3]`, `[mme_sbc]`). | `srsepc/epc.conf.example` |
| `user_db.csv` | HSS subscriber database. | `srsepc/user_db.csv.example` |

`enb.conf`'s `[enb_files]` section names the SIB/RR/RB files it loads; copy them (renamed
without `.example`) next to `enb.conf` in whichever directory you launch from.

Install the full set once with:

```bash
mkdir -p ~/.config/srsran
cp Config-Template/enb.conf         ~/.config/srsran/enb.conf
cp Config-Template/sib.conf.mbsfn   ~/.config/srsran/
cp srsenb/rr.conf.example           ~/.config/srsran/rr.conf
cp srsenb/rb.conf.example           ~/.config/srsran/rb.conf
cp srsepc/epc.conf.example          ~/.config/srsran/epc.conf
```

### Control endpoints (for a control portal such as rt-mbms-application-provider)

Both binaries expose an optional, unauthenticated TCP control endpoint that an external
control portal connects to instead of editing config files and sending SIGHUP.

- **eNB live eMBMS reconfiguration** - `enb.conf` `[control]` (default off; loopback when
  enabled). Line protocol supporting `GET` (dumps the current `embms.*` parameters) and `SET
  key=value ...` (validates and applies MCS, PMCH bandwidth, cyclic shift, time interleaving
  N/M, CAS muting k/n, MCH scheduling period, and related parameters):

  ```ini
  [control]
  enable = true
  bind_addr = 127.0.0.1   # loopback; use an internal address / 0.0.0.0 only for cross-container
  port = 2100             # default TCP port
  ```

- **MME SBc-AP portal bridge** - `epc.conf` `[mme_sbc]`. A TCP bridge that accepts one
  already-encoded SBc-AP PDU per connection (Write-Replace-Warning / Stop-Warning), used in
  place of a real SCTP CBC. The response reuses the same decode/dispatch/forwarding logic as
  the real SCTP path:

  ```ini
  [mme_sbc]
  sbc_bind_addr    = 127.0.1.100   # real SBc-AP SCTP bind
  sbc_bind_port    = 29168
  bridge_bind_addr = 127.0.0.1     # portal-facing TCP bridge, loopback by default
  bridge_port      = 2102
  ```

## Running

Start order: MBMS-GW, then EPC, then eNB.

### 1. MBMS-Gateway (separate repository)

The MBMS-GW ships from the standalone `rt-mbms-gw` repository:

```bash
cd rt-mbms-gw/build
sudo ./mbms-gw/mbms-gw mbms-gw.conf.example
```

It creates the `sgi_mb` virtual interface (visible with `ifconfig`/`ip a`), receives IP
multicast on one tunnel, packages it into GTP-U, and forwards it toward the eNB. Redirect
incoming multicast to `sgi_mb`:

```bash
sudo smcroutectl restart
sudo smcroutectl add eno0 239.255.1.1 sgi_mb        # eno0 = your physical NIC
sudo route add -net 239.255.1.0 netmask 255.255.255.0 dev sgi_mb   # keep multicast off the LAN
```

### 2. EPC (MME + HSS + SP-GW, plus the SBc-AP bridge)

```bash
sudo ./build/srsepc/src/srsepc ~/.config/srsran/epc.conf
```

### 3. eNodeB

```bash
sudo ./build/srsenb/src/srsenb ~/.config/srsran/enb.conf
```

Running the eNB against a real SDR requires the corresponding driver and hardware; see
https://5g-mag.github.io/Getting-Started/pages/3gpp-ran-and-core-platforms/tutorials/sdr-platforms.html
For a hardware-free run, set `device_name = zmq` in `enb.conf`'s `[rf]` section (requires the
ZeroMQ build).

## Ports and interfaces exposed

| Interface | Spec | Transport | Default port | Where |
|---|---|---|---|---|
| S1-MME (S1AP) | TS 36.413 | SCTP | 36412 | MME `mme_bind_addr`; eNB connects to `mme_addr` |
| M3AP (MME ↔ eNB-as-MCE) | TS 36.444 | SCTP (PPID 39) | 36444 | MME `[mme_m3]`; IANA-registered, configurable |
| Sm (MME ↔ MBMS-GW) | TS 29.274 (GTPv2-C) | UDP | 2123 | MME `[mme_sm]` |
| SBc-AP (MME ↔ CBC) | TS 29.168 | SCTP | 29168 | MME `[mme_sbc]` `sbc_bind_port` |
| SBc-AP portal bridge | (non-3GPP) | TCP | 2102 | MME `[mme_sbc]` `bridge_port` |
| M1-U (MBMS user plane) | GTP-U | UDP | 2153 | eNB M1-U socket binds `GTPU_PORT + 1` |
| S1-U (unicast user plane) | GTP-U | UDP | 2152 | eNB/SP-GW generic GTP-U (`GTPU_PORT`) |
| eNB control endpoint | (non-3GPP) | TCP | 2100 | eNB `[control]` |

Note on the MBMS user plane: the MBMS-specific M1-U socket binds to `GTPU_PORT + 1` = **2153**
(`srsenb/src/stack/upper/gtpu.cc`), distinct from the standard S1-U GTP-U port 2152. Sending
M1-U traffic to 2152 will be rejected with a GTP-U Error Indication.

## Security

The two TCP control endpoints (eNB `[control]` on 2100, MME SBc-AP bridge `[mme_sbc]`
`bridge_port` on 2102) are **unauthenticated**. Anyone who can reach them can change live
broadcast parameters or inject emergency-alert (Write-Replace Warning) PDUs. Keep both on
loopback or a trusted management network and firewall them. The eNB logs a warning when the
control endpoint is bound to a non-loopback address. The real SCTP interfaces (S1AP, M3AP,
SBc-AP) carry no application-layer authentication either and should be confined to a trusted
transport network.

## Development

This project follows the
[Gitflow workflow](https://www.atlassian.com/git/tutorials/comparing-workflows/gitflow-workflow).
The `development` branch is the integration branch for new features; branch from it before
starting a new feature.

## Contributing

Contributions are welcome. How to raise an issue, fork the repository and open a pull request, and
the Contributor License Agreement required before code can be merged, are described at
<https://www.5g-mag.com/contributing>.

## License

Distributed under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE).

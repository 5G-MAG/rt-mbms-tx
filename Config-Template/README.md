# rt-mbms-tx configuration templates

This directory holds the ready-to-edit configuration templates for the transmit chain
(`srsenb` eNB and `srsepc` EPC/MME). It is the recognizable place to start from when
launching this repository's binaries.

## Files here

- `enb.conf` — eNB configuration. References `sib.conf.mbsfn`, `rr.conf` and `rb.conf`
  (see its `[enb_files]` section).
- `sib.conf.mbsfn` — SIB1/2/3 + SIB13 (MBSFN) for the eNB.
- `sib12_alert.conf` — SIB12 template for Emergency Alerts (ETWS/CMAS).

The radio-resource files `enb.conf` refers to (`rr.conf`, `rb.conf`) ship as
`../srsenb/rr.conf.example` and `../srsenb/rb.conf.example`; copy them next to `enb.conf`
(renamed without the `.example` suffix) in whichever directory you launch from.
The EPC/MME template is `../srsepc/epc.conf.example`.

## How to launch

The binaries look for a config in this order: explicit path → `~/.config/srsran/<name>`
→ `/etc/srsran/<name>`. Either pass an explicit path, or install the set once:

```bash
mkdir -p ~/.config/srsran
cp Config-Template/enb.conf            ~/.config/srsran/enb.conf
cp Config-Template/sib.conf.mbsfn      ~/.config/srsran/
cp srsenb/rr.conf.example              ~/.config/srsran/rr.conf
cp srsenb/rb.conf.example              ~/.config/srsran/rb.conf
cp srsepc/epc.conf.example             ~/.config/srsran/epc.conf

sudo ./build/srsepc/src/srsepc ~/.config/srsran/epc.conf     # MME/EPC (incl. SBc-AP bridge)
sudo ./build/srsenb/src/srsenb ~/.config/srsran/enb.conf     # eNB
```

## Control endpoints (for a control portal such as rt-mbms-application-provider)

Both binaries expose an optional TCP control endpoint the portal connects to. They are
off/loopback by default; set them per deployment:

- eNB — `enb.conf` `[control]`:
  ```ini
  [control]
  enable = true
  bind_addr = 127.0.0.1   # internal address / 0.0.0.0 only for cross-container
  port = 2100
  ```
- MME SBc-AP portal bridge — `epc.conf` `[mme_sbc]`:
  ```ini
  bridge_bind_addr = 127.0.0.1
  bridge_port = 2102
  ```

SECURITY: these control endpoints are unauthenticated. Keep them on loopback or a trusted
management network and firewall them.

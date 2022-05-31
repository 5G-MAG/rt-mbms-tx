rt-tx-poc
======

The 5G Braodcast transmitter builds a proof of concept development for the 3GPP LTE-based Terrestrial Broadcast (FeMBMS), base in the [srsRAN](https://www.srsran.com) implementation. This software is provided as is and are not part of the reference tools. It doesn't (and won't) have support from the developers team, it's main porpuse is to show and demonstrate an implementation of the standard and its capabilities.

The baseline of the transmitter is a Rel-9 eNodeB from the srsRAN suite, the actual tx includes:
 * Release 14 CAS subframe.
 * Subcarrier spacing of 1.25 kHz.
 * Release 14 frame structure (1 CAS + 39 MCH subframes).

The rest of the features are the same as the srsRAN implementation.

For build instructions see the [srsRAN documentation](https://docs.srsran.com).

# PWS interface ASN.1 (SBc-AP / S1AP)

Raw ASN.1 modules extracted from the 3GPP spec text, for the Public Warning
System interfaces not currently implemented in this codebase (see SIB12/ETWS
discussion): CBC-MME (SBc-AP) and MME-eNB warning delivery (S1AP
Write-Replace-Warning / Kill procedures).

Not hand-written: pulled verbatim from the spec `.docx` (unzipped, XML
stripped, module boundaries sliced out), then validated by compiling with
the `mouse07410/asn1c` fork (github.com/mouse07410/asn1c), which has working
support for Information Object Classes/object sets — the plain Debian/Ubuntu
`asn1c` package does not and will fail to parse either file.

## sbc-ap/sbc-ap_ts29168-j00.asn
Source: 3GPP TS 29.168 version j00. Full SBc-AP module set (PDU-Descriptions,
PDU-Contents, IEs, CommonDataTypes, Constants, Containers).

Validated: grammar + semantic fix pass clean (0 errors). Full C codegen
(`asn1c -pdu=all`) produces 270 files; 267 compile cleanly. The only failures
are `CancelledCellinTAI-5GS.c` / `ScheduledCellinTAI-5GS.c` (an unrelated
self-referential include-ordering bug in the asn1c fork for a 5GS TAI list
type) and the generic `converter-example.c` stub (expects `-DPDU=<Type>`,
not a real error). `Write-Replace-Warning-Request/Response` and
`Stop-Warning-Request/Response` compile with zero errors.

## s1ap/s1ap_ts36413-j20_core.asn
Source: 3GPP TS 36.413 version j20. Core S1AP modules (PDU-Descriptions,
PDU-Contents, IEs, CommonDataTypes, Constants, Containers).

## s1ap/s1ap_ts36413-j20_sontransfer.asn
The `SonTransfer-IEs` module, split out on purpose: in the real spec it's
carried as an opaque embedded blob inside S1AP (its own module/namespace),
and it happens to redefine a type name (`MobilityInformation`) already used
in `S1AP-IEs`. Compiling both in one pass causes a name clash. Real
toolchains compile it separately for the same reason.

Validated: grammar + semantic fix pass clean for both files individually.
Full C codegen (`asn1c -pdu=all -fcompound-names`) produces 849 files; **838
compile cleanly**, including `WriteReplaceWarningRequest`,
`WriteReplaceWarningResponse`, `KillRequest`, and `KillResponse` with **zero
errors**. Two source-level patches were applied to get there (both are
already baked into `s1ap_ts36413-j20_core.asn`, diffable against 3GPP TS
36.413 j20 if you need to check exact wording against the spec):

1. **11 `E-RAB*List` types** (`E-RABSubjecttoDataForwardingList`,
   `E-RABToBeSetupListHOReq`, `E-RABAdmittedList`, etc., used only by
   Handover/Bearer-Setup/Bearer-Modify procedures) were defined via a
   two-level generic alias (`E-RAB-IE-ContainerList{X}` ->
   `ProtocolIE-ContainerList{1,maxnoofE-RABs,{X}}`). The asn1c fork loses the
   correctly-suffixed struct name at the second level of parameterization
   and emits a reference to a `struct` that's never defined. Fix: inlined
   the alias so each type instantiates `ProtocolIE-ContainerList` directly,
   skipping the buggy nesting.
2. **3 "future extension" CHOICE arms** (`MDTMode-Extension`,
   `SONInformation-Extension`, `SourceNodeID-Extension`) created a genuine
   circular *by-value* struct dependency: each is an instantiation of the
   same generic `ProtocolIE-Field` template used for the giant shared IE
   union, and each is itself embedded by value inside that same union via a
   parent IE (`SONInformation`, etc.) — a real cycle, not just an ordering
   quirk (confirmed by testing asn1c's `-findirect-choice` flag, which
   exists for exactly this class of problem for CHOICE members but doesn't
   cover this SEQUENCE-embedding case and made things worse). All three were
   unused placeholder extension points with zero or one concrete IE, so they
   were removed (CHOICE arm + type + object set) rather than patched.

**Still open**: 11 files fail — the same `E-RAB*List` types from fix #1
above, but now hitting a *second*, distinct bug: their own generated header
transitively re-includes itself (via `ProtocolIE-ContainerList.h` ->
`ProtocolIE-SingleContainer.h` -> `ProtocolIE-Field.h` -> back to their own
header) before their typedef is visible. This is a genuine mutual/circular
C dependency inherent to how E-RAB lists nest IE-containers within
IE-containers; fixing it needs the compiler to emit a forward declaration +
pointer for this SEQUENCE-embedding case (the same fix `-findirect-choice`
does for CHOICE, extended to SEQUENCE), which is a real compiler patch, not
a `.asn` source workaround. None of the 11 affected files are used by
`WriteReplaceWarningRequest/Response` or `KillRequest/Response` — confirmed
by direct compilation of all four in isolation, exit 0, no warnings.

## Why this exists
`srsenb/src/stack/s1ap/s1ap.cc` has no handler for `WriteReplaceWarningRequest`
/ `KillRequest`, and there is no SBc-AP client/CBC simulator anywhere in the
tree. SIB12 content today comes from a local `.conf` file plus a `SIGUSR1`
signal (see `srsenb/src/stack/rrc/rrc.cc` `reload_sib12()`), bypassing the
standard CBE -> CBC -> MME -> eNB signaling chain entirely. These modules are
a starting point if that chain ever needs to be built for real.

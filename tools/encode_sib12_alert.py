#!/usr/bin/env python3
"""
Encode a text message for SIB12 warning_msg_segment_r9 (TS 36.331 / TS 23.038).

For LTE ETWS/CMAS secondary notification, warning_msg_segment_r9 contains the
message text encoded per the Data Coding Scheme (DCS).  There is NO CBS page
header or length prefix — LTE handles segmentation via the separate
warning_msg_segment_num_r9 and warning_msg_segment_type_r9 fields.

Ref: 3GPP TS 23.038 §6.1.2.1 (7-bit packing)
     3GPP TS 36.331 §6.3.1 (SIB12 / SystemInformationBlockType12-r9)
     3GPP TS 23.041 §9.4.2.2 (Warning Message Contents for E-UTRAN)

Usage:
    python3 encode_sib12_alert.py "YOUR EMERGENCY MESSAGE"
    python3 encode_sib12_alert.py --dcs ucs2 "Mesaj de urgenta"

Output is ready to paste into sib12_alert.conf.
"""

import sys
import argparse

# GSM 7-bit basic character set (TS 23.038 Table 1).
# Maps Unicode char to 7-bit code value (0..127).
# Characters not in this table are replaced by '?' (0x3F).
_GSM7 = {
    '@':  0x00, '£':  0x01, '$':  0x02, '¥':  0x03,
    'è':  0x04, 'é':  0x05, 'ù':  0x06, 'ì':  0x07,
    'ò':  0x08, 'Ç':  0x09, '\n': 0x0A, 'Ø':  0x0B,
    'ø':  0x0C, '\r': 0x0D, 'Å':  0x0E, 'å':  0x0F,
    'Δ':  0x10, '_':  0x11, 'Φ':  0x12, 'Γ':  0x13,
    'Λ':  0x14, 'Ω':  0x15, 'Π':  0x16, 'Ψ':  0x17,
    'Σ':  0x18, 'Θ':  0x19, 'Ξ':  0x1A,
    # 0x1B = escape prefix (handled separately if needed)
    ' ':  0x20, '!':  0x21, '"':  0x22, '#':  0x23,
    '¤':  0x24, '%':  0x25, '&':  0x26, "'":  0x27,
    '(':  0x28, ')':  0x29, '*':  0x2A, '+':  0x2B,
    ',':  0x2C, '-':  0x2D, '.':  0x2E, '/':  0x2F,
    ':':  0x3A, ';':  0x3B, '<':  0x3C, '=':  0x3D,
    '>':  0x3E, '?':  0x3F, '¡':  0x40,
    'Ä':  0x5B, 'Ö':  0x5C, 'Ñ':  0x5D, 'Ü':  0x5E,
    '§':  0x5F, '¿':  0x60,
    'ä':  0x7B, 'ö':  0x7C, 'ñ':  0x7D, 'ü':  0x7E,
    'à':  0x7F,
}
# Digits, A-Z, a-z: same code points as ASCII in GSM 7-bit basic table
for _c in '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz':
    _GSM7[_c] = ord(_c)


def _pack7(codes: list[int]) -> bytes:
    """Pack a list of 7-bit code values into bytes (GSM 7-bit packing, TS 23.038 §6.1.2.1)."""
    buf = 0
    nbits = 0
    result = bytearray()
    for c in codes:
        buf |= (c & 0x7F) << nbits
        nbits += 7
        if nbits >= 8:
            result.append(buf & 0xFF)
            buf >>= 8
            nbits -= 8
    if nbits > 0:
        result.append(buf & 0xFF)
    return bytes(result)


def encode_gsm7(text: str) -> tuple[bytes, int]:
    """
    Encode text as GSM 7-bit packed bytes.
    Returns (packed_bytes, dcs) where dcs=0x48.
    DCS 0x48: General DCI, not compressed, message class defined, 7-bit alphabet, Class 0.
    """
    codes = []
    for ch in text:
        if ch in _GSM7:
            codes.append(_GSM7[ch])
        else:
            codes.append(0x3F)   # '?' fallback
    return _pack7(codes), 0x48


def encode_ucs2(text: str) -> tuple[bytes, int]:
    """
    Encode text as UCS-2 (UTF-16 big-endian) bytes.
    Returns (encoded_bytes, dcs) where dcs=0x48 with UCS2 bits.
    DCS 0x48|0x08 = 0x48 group but UCS2: actually DCS for UCS2 class 0 = 0x18.
    TS 23.038 §4: bits 7-4=0001, bits 3-2=10 (UCS2), bits 1-0=00 (Class 0) = 0x18.
    """
    return text.encode('utf-16-be'), 0x18


def split_segments(data: bytes, max_bytes: int = 1200) -> list[bytes]:
    """Split encoded data into segments of at most max_bytes bytes."""
    return [data[i:i + max_bytes] for i in range(0, len(data), max_bytes)]


def main():
    parser = argparse.ArgumentParser(
        description='Encode a text string for SIB12 warning_msg_segment_r9.')
    parser.add_argument('text', help='Alert text to encode')
    parser.add_argument('--dcs', choices=['gsm7', 'ucs2'], default='gsm7',
                        help='Character encoding (default: gsm7)')
    args = parser.parse_args()

    text = args.text

    if args.dcs == 'gsm7':
        data, dcs = encode_gsm7(text)
        enc_name = 'GSM 7-bit (TS 23.038)'
    else:
        data, dcs = encode_ucs2(text)
        enc_name = 'UCS-2 / UTF-16-BE (TS 23.038)'

    segments = split_segments(data)
    n_segs = len(segments)

    print(f'Message : {text!r}  ({len(text)} characters)')
    print(f'Encoding: {enc_name}')
    print(f'DCS     : 0x{dcs:02X}')
    print(f'Segments: {n_segs}')
    print()

    for i, seg in enumerate(segments):
        seg_type = 'lastSegment' if i == n_segs - 1 else 'notLastSegment'
        hex_str  = seg.hex().upper()
        print(f'# Segment {i} of {n_segs - 1} ---')
        print(f'sib12_alert = {{')
        print(f'    message_identifier       = 0x1101;   # ETWS tsunami; change per 3GPP TS 23.041 §9.4.1.2.2 Table 9.4.1.2.2-1')
        print(f'    serial_number            = 0x3000;   # geo-scope=cell, update number=0')
        print(f'    data_coding_scheme       = 0x{dcs:02X};')
        print(f'    warning_msg_segment_type = "{seg_type}";')
        print(f'    warning_msg_segment_num  = {i};')
        print(f'    warning_msg_segment_r9   = "{hex_str}";')
        print(f'}};')
        print()

    if n_segs > 1:
        print('NOTE: Multi-segment message. Run the transmitter once per segment,')
        print('      incrementing warning_msg_segment_num and updating segment_type.')
        print('      SIB12 only carries one segment at a time.')


if __name__ == '__main__':
    main()

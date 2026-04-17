#!/usr/bin/env ork.python
###############################################################################
# ork.audio.ambidump.py
#
# Analyze a WAV file and report its ambisonic encoding characteristics.
#   - basic PCM facts (sample rate, bit depth, channels, duration)
#   - container chunks (fmt, bext, iXML, AMBI, chna, axml, ID3, LIST)
#   - WAVE_FORMAT_EXTENSIBLE SubFormat GUID decoding
#   - ambisonic order / channel-ordering / normalization inference
#   - per-channel level stats (RMS, peak, DC offset)
#   - optional JSON sidecar for engine consumption
#
# Usage:
#   ork.audio.ambidump.py -i <infile.wav> [--json <out.json>] [--seconds N]
###############################################################################

import argparse
import json
import math
import os
import re
import struct
import sys
import xml.etree.ElementTree as ET
from obt.deco import Deco

deco = Deco()

###############################################################################
# WAV subformat GUIDs (stored little-endian in the fmt EXTENSIBLE chunk).
# A "standard" text GUID xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx has fields
# {Data1(4 LE), Data2(2 LE), Data3(2 LE), Data4(8 bytes as-stored)}.
###############################################################################

def _guid_bytes(text):
    parts = text.replace('{', '').replace('}', '').split('-')
    d1 = struct.pack('<I', int(parts[0], 16))
    d2 = struct.pack('<H', int(parts[1], 16))
    d3 = struct.pack('<H', int(parts[2], 16))
    d4 = bytes.fromhex(parts[3] + parts[4])
    return d1 + d2 + d3 + d4

KSDATAFORMAT_SUBTYPE_PCM        = _guid_bytes('00000001-0000-0010-8000-00AA00389B71')
KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = _guid_bytes('00000003-0000-0010-8000-00AA00389B71')
# Legacy Microsoft "Ambisonic B-Format" subtype (PCM int + IEEE float variants).
# Files tagged with these GUIDs are, by convention, FuMa channel order + MaxN.
SUBTYPE_AMBISONIC_B_PCM         = _guid_bytes('00000001-0721-11D3-8644-C8C1CA000000')
SUBTYPE_AMBISONIC_B_FLOAT       = _guid_bytes('00000003-0721-11D3-8644-C8C1CA000000')

def _guid_pretty(g):
    if len(g) != 16: return g.hex()
    d1 = struct.unpack('<I', g[0:4])[0]
    d2 = struct.unpack('<H', g[4:6])[0]
    d3 = struct.unpack('<H', g[6:8])[0]
    return f'{d1:08X}-{d2:04X}-{d3:04X}-{g[8:10].hex().upper()}-{g[10:16].hex().upper()}'

def _guid_name(g):
    if g == KSDATAFORMAT_SUBTYPE_PCM:        return 'KSDATAFORMAT_SUBTYPE_PCM'
    if g == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT: return 'KSDATAFORMAT_SUBTYPE_IEEE_FLOAT'
    if g == SUBTYPE_AMBISONIC_B_PCM:         return 'SUBTYPE_AMBISONIC_B_FORMAT_PCM'
    if g == SUBTYPE_AMBISONIC_B_FLOAT:       return 'SUBTYPE_AMBISONIC_B_FORMAT_IEEE_FLOAT'
    return None

###############################################################################
# Chunk reader
###############################################################################

class WavChunks:
    def __init__(self, path):
        self.path = path
        self.size = os.path.getsize(path)
        self.f = open(path, 'rb')
        hdr = self.f.read(12)
        if len(hdr) < 12:
            raise ValueError('file too small to be a WAV')
        self.container = hdr[0:4].decode('latin-1', errors='replace')
        if self.container not in ('RIFF', 'RF64', 'BW64'):
            raise ValueError(f'not a RIFF/RF64/BW64 container (got {self.container!r})')
        if hdr[8:12] != b'WAVE':
            raise ValueError('RIFF container is not WAVE')
        self.chunks = []     # list of (id, offset, size) — offset points to chunk payload
        self.ds64 = None
        self._scan()

    def _scan(self):
        f = self.f
        f.seek(12)
        while True:
            hdr = f.read(8)
            if len(hdr) < 8: break
            cid  = hdr[0:4]
            csz  = struct.unpack('<I', hdr[4:8])[0]
            payload_off = f.tell()
            # RF64: if size == 0xFFFFFFFF and ds64 available, use 64-bit size
            if csz == 0xFFFFFFFF and self.ds64 is not None:
                if cid == b'data':
                    csz = self.ds64['dataSize']
            self.chunks.append((cid, payload_off, csz))
            # ds64 must be read as soon as encountered
            if cid == b'ds64':
                data = f.read(csz)
                if len(data) >= 24:
                    riffSize = struct.unpack('<Q', data[0:8])[0]
                    dataSize = struct.unpack('<Q', data[8:16])[0]
                    sampleCount = struct.unpack('<Q', data[16:24])[0]
                    self.ds64 = {'riffSize': riffSize,
                                 'dataSize': dataSize,
                                 'sampleCount': sampleCount}
                # pad to even
                if csz & 1: f.read(1)
                continue
            # skip past payload (+ pad byte if odd)
            f.seek(payload_off + csz + (csz & 1))

    def find(self, cid):
        for c in self.chunks:
            if c[0] == cid: return c
        return None

    def read(self, cid, maxlen=None):
        c = self.find(cid)
        if c is None: return None
        self.f.seek(c[1])
        n = c[2] if maxlen is None else min(c[2], maxlen)
        return self.f.read(n)

    def close(self):
        try: self.f.close()
        except Exception: pass

###############################################################################
# fmt chunk parser
###############################################################################

WAVE_FORMAT_PCM        = 0x0001
WAVE_FORMAT_IEEE_FLOAT = 0x0003
WAVE_FORMAT_EXTENSIBLE = 0xFFFE

def parse_fmt(buf):
    out = {}
    out['wFormatTag']      = struct.unpack('<H', buf[0:2])[0]
    out['nChannels']       = struct.unpack('<H', buf[2:4])[0]
    out['nSamplesPerSec']  = struct.unpack('<I', buf[4:8])[0]
    out['nAvgBytesPerSec'] = struct.unpack('<I', buf[8:12])[0]
    out['nBlockAlign']     = struct.unpack('<H', buf[12:14])[0]
    out['wBitsPerSample']  = struct.unpack('<H', buf[14:16])[0]
    if len(buf) >= 18 and out['wFormatTag'] == WAVE_FORMAT_EXTENSIBLE:
        cbSize = struct.unpack('<H', buf[16:18])[0]
        out['cbSize'] = cbSize
        if cbSize >= 22 and len(buf) >= 40:
            out['wValidBitsPerSample'] = struct.unpack('<H', buf[18:20])[0]
            out['dwChannelMask']       = struct.unpack('<I', buf[20:24])[0]
            out['SubFormat']           = buf[24:40]
            out['SubFormatName']       = _guid_name(out['SubFormat'])
            out['SubFormatText']       = _guid_pretty(out['SubFormat'])
    return out

###############################################################################
# bext / iXML / AMBI / chna parsers (minimal)
###############################################################################

def parse_bext(buf):
    if buf is None or len(buf) < 346: return None
    dec = lambda b: b.split(b'\x00', 1)[0].decode('latin-1', errors='replace')
    o = {}
    o['Description']    = dec(buf[0:256])
    o['Originator']     = dec(buf[256:288])
    o['OriginatorRef']  = dec(buf[288:320])
    o['OriginationDate']= buf[320:330].decode('latin-1', errors='replace')
    o['OriginationTime']= buf[330:338].decode('latin-1', errors='replace')
    if len(buf) > 602:
        o['CodingHistory'] = dec(buf[602:])
    return o

def parse_chna(buf):
    """EBU ADM chna chunk: track UIDs → track formats. HOA format ID = 0x0003."""
    if buf is None or len(buf) < 4: return None
    out = {'entries': []}
    n = struct.unpack('<H', buf[0:2])[0]
    n_uids = struct.unpack('<H', buf[2:4])[0]
    off = 4
    rec_size = 40
    for _ in range(min(n_uids, (len(buf) - off) // rec_size)):
        r = buf[off:off+rec_size]
        trackNo  = struct.unpack('<H', r[0:2])[0]
        trackUID = r[2:14].decode('latin-1', errors='replace').rstrip('\x00 ')
        trackFmt = r[14:28].decode('latin-1', errors='replace').rstrip('\x00 ')
        packFmt  = r[28:39].decode('latin-1', errors='replace').rstrip('\x00 ')
        out['entries'].append({
            'trackNumber': trackNo,
            'trackUID':    trackUID,
            'trackFormat': trackFmt,
            'packFormat':  packFmt,
        })
        off += rec_size
    return out

def chna_is_ambisonic(chna):
    if not chna or not chna.get('entries'): return False
    for e in chna['entries']:
        tf = e.get('trackFormat', '')
        # ADM format ID "AT_00030001_01" → type 0x0003 = HOA
        if tf.startswith('AT_0003') or tf.startswith('AT_00030'): return True
    return False

def parse_ambi(buf):
    """Placeholder for the (informal) AMBI chunk. Most real ambisonic wavs
       don't actually have this — if it's here, surface raw contents."""
    if buf is None: return None
    return {'size': len(buf), 'preview': buf[:256].hex()}

def _decode_xml_text(buf):
    if buf is None: return None
    # strip trailing null padding; iXML is written as a nul-terminated text blob
    text = buf.rstrip(b'\x00 \t\r\n')
    for enc in ('utf-8', 'utf-16', 'latin-1'):
        try:
            return text.decode(enc)
        except UnicodeDecodeError:
            continue
    return text.decode('latin-1', errors='replace')

def _walk_elems(root):
    """Yield every element under root (ElementTree)."""
    yield root
    for child in root:
        yield from _walk_elems(child)

def _localname(tag):
    if tag is None: return ''
    # strip XML namespace "{uri}name"
    return tag.rsplit('}', 1)[-1] if '}' in tag else tag

def parse_ixml(buf):
    """Parse an iXML text chunk and pull anything relevant to ambisonic.
       Surfaces: raw text (verbatim), ambisonic hits, per-track names,
       and an inferred channel-ordering guess (W,X,Y,Z vs W,Y,Z,X).
    """
    if buf is None: return None
    text = _decode_xml_text(buf)
    out = {
        'raw': text,
        'size': len(buf),
        'ambisonic_hits': [],   # list of {tag, value}
        'tracks': [],           # list of {index, name, interleave_index}
        'inferred_ordering': None,
        'root_tag': None,
    }
    # Regex pass first — works even if XML is malformed
    for m in re.finditer(r'<([^/!?][^\s/>]*)[^>]*>\s*([^<]*?)\s*</\1>', text):
        tag, value = m.group(1), (m.group(2) or '').strip()
        lname = _localname(tag)
        if re.search(r'ambi|bformat|acn|sn3d|fuma|maxn|n3d', lname, re.I):
            out['ambisonic_hits'].append({'tag': lname, 'value': value})
        elif re.search(r'ambi|bformat|acn|sn3d|fuma|maxn|n3d', value, re.I):
            out['ambisonic_hits'].append({'tag': lname, 'value': value})
    # ElementTree pass — better for structured track lists
    try:
        root = ET.fromstring(text)
        out['root_tag'] = _localname(root.tag)
        for el in _walk_elems(root):
            if _localname(el.tag).upper() == 'TRACK':
                rec = {}
                for child in el:
                    ln = _localname(child.tag).upper()
                    if ln in ('NAME', 'INTERLEAVE_INDEX',
                              'CHANNEL_INDEX', 'RECORDER_CHANNEL'):
                        rec[ln.lower()] = (child.text or '').strip()
                if 'name' in rec:
                    try:
                        idx = int(rec.get('interleave_index',
                                  rec.get('channel_index', 0)))
                    except ValueError:
                        idx = 0
                    out['tracks'].append({
                        'index': idx,
                        'name': rec['name'],
                        'interleave_index': rec.get('interleave_index'),
                        'channel_index': rec.get('channel_index'),
                    })
    except ET.ParseError:
        pass
    # Infer channel ordering from track names if they look like W/X/Y/Z
    if out['tracks']:
        byidx = sorted(out['tracks'], key=lambda t: t['index'])
        names = [re.sub(r'^amb[a-z]*\s*', '', t['name'], flags=re.I).strip().upper()
                 for t in byidx]
        first4 = names[:4]
        if set(first4) == set('WXYZ'):
            if first4 == list('WXYZ'):
                out['inferred_ordering'] = 'FuMa'
            elif first4 == list('WYZX'):
                out['inferred_ordering'] = 'ACN'
            else:
                out['inferred_ordering'] = 'custom:' + ','.join(first4)
    return out

###############################################################################
# PCM sampler: read a window of samples and compute per-channel stats
###############################################################################

def read_pcm_window(wc, fmt, data_chunk, seconds):
    _, data_off, data_size = data_chunk
    nch   = fmt['nChannels']
    bits  = fmt['wBitsPerSample']
    block = fmt['nBlockAlign']
    sr    = fmt['nSamplesPerSec']
    is_float = False
    if fmt['wFormatTag'] == WAVE_FORMAT_IEEE_FLOAT:
        is_float = True
    elif fmt['wFormatTag'] == WAVE_FORMAT_EXTENSIBLE:
        is_float = (fmt.get('SubFormat') == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)

    total_frames = data_size // max(block, 1)
    want_frames  = min(total_frames, int(sr * seconds))
    # start a bit into the file to skip fade-in silence, but not past end
    start_frame  = min(total_frames // 8, max(0, total_frames - want_frames))
    wc.f.seek(data_off + start_frame * block)
    raw = wc.f.read(want_frames * block)
    actual_frames = len(raw) // block

    sums = [0.0] * nch
    abs_sums = [0.0] * nch
    dc_sums = [0.0] * nch
    peaks = [0.0] * nch

    bytes_per_sample = bits // 8
    if is_float and bits == 32:
        scale = 1.0
    elif not is_float:
        scale = 1.0 / float(1 << (bits - 1))
    else:
        # unsupported bit depth for float
        return None

    # fast path per bit depth
    mv = memoryview(raw)
    for i in range(actual_frames):
        base = i * block
        for c in range(nch):
            off = base + c * bytes_per_sample
            b = mv[off:off + bytes_per_sample]
            if is_float:
                v = struct.unpack('<f', b)[0]
            elif bits == 16:
                v = struct.unpack('<h', b)[0] * scale
            elif bits == 24:
                raw24 = b[0] | (b[1] << 8) | (b[2] << 16)
                if raw24 & 0x800000: raw24 -= 0x1000000
                v = raw24 * scale
            elif bits == 32:
                v = struct.unpack('<i', b)[0] * scale
            elif bits == 8:
                v = (b[0] - 128) / 128.0
            else:
                return None
            sums[c]    += v * v
            abs_sums[c] += abs(v)
            dc_sums[c] += v
            av = abs(v)
            if av > peaks[c]: peaks[c] = av

    def db(x): return 20.0 * math.log10(x) if x > 1e-30 else -200.0

    stats = []
    for c in range(nch):
        rms = math.sqrt(sums[c] / max(actual_frames, 1))
        mean_abs = abs_sums[c] / max(actual_frames, 1)
        dc = dc_sums[c] / max(actual_frames, 1)
        stats.append({
            'channel':     c,
            'rms':         rms,
            'rms_dbfs':    db(rms),
            'peak':        peaks[c],
            'peak_dbfs':   db(peaks[c]),
            'mean_abs':    mean_abs,
            'dc_offset':   dc,
        })
    return {
        'frames_analyzed': actual_frames,
        'seconds_analyzed': actual_frames / float(sr) if sr else 0.0,
        'start_frame': start_frame,
        'per_channel': stats,
    }

###############################################################################
# Ambisonic inference
###############################################################################

FULL_SPHERE_ORDERS = {(n + 1) * (n + 1): n for n in range(0, 8)}   # 1,4,9,16,25,36,49,64
HORIZ_ONLY_ORDERS  = {(2 * n + 1): n for n in range(1, 8)}        # 3,5,7,9,11,13,15

def infer_ambisonic(fmt, chna, ambi_chunk, ixml, stats):
    evidence = []
    detected = False
    order = None
    order_conf = None
    full_sphere = None
    ordering = None   # 'FuMa' | 'ACN' | 'unknown'
    normalization = None  # 'MaxN' | 'SN3D' | 'N3D' | 'unknown'
    confidence = 'low'

    nch = fmt['nChannels']

    # --- container-level ---
    if fmt.get('SubFormat') in (SUBTYPE_AMBISONIC_B_PCM, SUBTYPE_AMBISONIC_B_FLOAT):
        detected = True
        ordering = 'FuMa'
        normalization = 'MaxN'
        confidence = 'certain'
        evidence.append(f"fmt EXTENSIBLE SubFormat = {fmt.get('SubFormatName')} "
                        f"(Microsoft ambisonic B-format ⇒ FuMa / MaxN)")

    if chna_is_ambisonic(chna):
        detected = True
        confidence = 'certain' if confidence != 'certain' else 'certain'
        evidence.append("ADM chna chunk declares HOA track format (AT_0003*) ⇒ ambisonic")
        if ordering is None:
            ordering = 'ACN'          # ADM uses ACN + SN3D as the baseline
            normalization = 'SN3D'

    if ambi_chunk is not None:
        evidence.append(f"AMBI chunk present ({ambi_chunk['size']} bytes) "
                        f"— surface raw data, format-specific parser TBD")
        detected = True

    # --- iXML declarative hints ---
    if ixml is not None:
        for hit in ixml.get('ambisonic_hits', []):
            evidence.append(f"iXML <{hit['tag']}> = {hit['value']!r}")
            v = hit['value'].upper()
            t = hit['tag'].upper()
            if 'FUMA' in v or 'FUMA' in t or 'MAXN' in v or 'MAXN' in t:
                detected = True
                if ordering is None: ordering = 'FuMa'
                if normalization is None: normalization = 'MaxN'
                confidence = 'certain'
            elif 'ACN' in v or 'ACN' in t:
                detected = True
                if ordering is None: ordering = 'ACN'
                confidence = 'certain'
            elif 'SN3D' in v or 'SN3D' in t:
                detected = True
                if normalization is None: normalization = 'SN3D'
                if ordering is None: ordering = 'ACN'
                confidence = 'certain'
            elif 'N3D' in v or 'N3D' in t:
                detected = True
                if normalization is None: normalization = 'N3D'
                if ordering is None: ordering = 'ACN'
                confidence = 'certain'
        if ixml.get('inferred_ordering'):
            io = ixml['inferred_ordering']
            evidence.append(f"iXML track names imply channel order: {io}")
            detected = True
            if ordering is None and io in ('FuMa', 'ACN'):
                ordering = io
                if confidence == 'low': confidence = 'high'

    # --- channel count → order ---
    if nch in FULL_SPHERE_ORDERS:
        order = FULL_SPHERE_ORDERS[nch]
        full_sphere = True
        order_conf = 'inferred-from-channel-count'
        evidence.append(f"channel count {nch} = {_order_name(order)} full-sphere "
                        f"((N+1)² with N={order})")
    elif nch in HORIZ_ONLY_ORDERS:
        order = HORIZ_ONLY_ORDERS[nch]
        full_sphere = False
        order_conf = 'inferred-from-channel-count'
        evidence.append(f"channel count {nch} = {_order_name(order)} horizontal-only "
                        f"(2N+1 with N={order})")
    else:
        evidence.append(f"channel count {nch} does not match any canonical "
                        f"(N+1)² or (2N+1) ambisonic layout")

    # only continue content-analysis if channel count looks ambisonic
    if stats is None or order is None or nch < 3:
        return _finalize(detected, confidence, order, order_conf, full_sphere,
                         ordering, normalization, evidence)

    # --- content: W dominance ---
    ch = stats['per_channel']
    def rms_db(i): return ch[i]['rms_dbfs']
    w_db = rms_db(0)
    dir_idx = list(range(1, nch))
    dir_db_avg = sum(rms_db(i) for i in dir_idx) / len(dir_idx)
    w_excess = w_db - dir_db_avg
    evidence.append(f"ch0 RMS = {w_db:+.2f} dBFS; "
                    f"mean of ch1..{nch-1} = {dir_db_avg:+.2f} dBFS "
                    f"(ch0 excess = {w_excess:+.2f} dB)")

    # --- content: FOA normalization invariance test ---
    # For any valid first-order soundfield, the ratio
    #   E[X² + Y² + Z²] / E[W²]
    # depends only on normalization convention (content-independent):
    #   SN3D (AmbiX):  1.0  →  0.00 dB   (W and directional sum carry equal power)
    #   MaxN (FuMa):   2.0  → +3.01 dB   (W pre-attenuated by 1/√2)
    #   N3D:           3.0  → +4.77 dB   (directionals boosted by √3)
    inv_ratio_db = None
    inv_hint = None
    if nch == 4:
        w_rms2 = ch[0]['rms'] ** 2
        dir_rms2_sum = sum(ch[i]['rms'] ** 2 for i in (1, 2, 3))
        if w_rms2 > 1e-30 and dir_rms2_sum > 1e-30:
            inv_ratio_db = 10.0 * math.log10(dir_rms2_sum / w_rms2)
            evidence.append(
                f"FOA invariance ratio 10·log10(Σ(dir²)/W²) = "
                f"{inv_ratio_db:+.2f} dB  "
                f"(SN3D=0.00, MaxN=+3.01, N3D=+4.77 dB)")
            # Classify with ±1.0 dB tolerance around each canonical value.
            if -1.0 < inv_ratio_db < 1.5:
                inv_hint = 'SN3D'
            elif 1.5 <= inv_ratio_db < 4.0:
                inv_hint = 'MaxN'
            elif 4.0 <= inv_ratio_db < 5.8:
                inv_hint = 'N3D'
            if inv_hint:
                dist_map = {'SN3D': abs(inv_ratio_db - 0.00),
                            'MaxN': abs(inv_ratio_db - 3.01),
                            'N3D':  abs(inv_ratio_db - 4.77)}
                evidence.append(
                    f"invariance ratio lands in the {inv_hint} band "
                    f"(distance to canonical = {dist_map[inv_hint]:.2f} dB)")
                detected = True
                if normalization is None or normalization == 'unknown':
                    normalization = inv_hint
                # Cross-imply ordering from normalization when not otherwise set.
                if (ordering is None or ordering == 'unknown'):
                    if inv_hint == 'MaxN':
                        ordering = 'FuMa'   # MaxN pairs with FuMa
                    elif inv_hint in ('SN3D', 'N3D'):
                        ordering = 'ACN'    # SN3D/N3D pair with ACN
                # Real-world recordings can sit 0.5–1.2 dB off canonical
                # (mic imbalance, non-plane-wave content, processing chain).
                # The ±1.25 dB bands already provide the safety margin, so
                # any hit within 1.2 dB of canonical is a strong verdict.
                if confidence != 'certain' and dist_map[inv_hint] < 1.2:
                    confidence = 'high'

    # Legacy ch0-excess fallback (weaker signal, used when invariance unavailable)
    if inv_ratio_db is None and w_excess > 0.5:
        detected = True
        if w_excess < 2.7:
            norm_hint = 'MaxN'
            evidence.append("ch0 excess consistent with MaxN (FuMa) W pre-attenuation")
        else:
            norm_hint = 'SN3D'
            evidence.append("ch0 excess consistent with SN3D (AmbiX) normalization")
        if normalization is None:
            normalization = norm_hint

    # --- content: find the Z (lowest-RMS) channel, usually weakest in field rec ---
    rms_rank = sorted(range(nch), key=lambda i: ch[i]['rms_dbfs'])
    quietest = rms_rank[0]
    if quietest == 0:
        evidence.append("ch0 is the quietest channel — unusual for ambisonic material")
    elif nch == 4:
        if quietest == 2:
            hint = 'ACN'  # W, Y, Z, X  → Z at idx 2
            evidence.append("quietest channel is idx 2 — consistent with ACN (W,Y,Z,X); "
                            "Z is typically weakest in horizontal-biased content")
        elif quietest == 3:
            hint = 'FuMa'  # W, X, Y, Z → Z at idx 3
            evidence.append("quietest channel is idx 3 — consistent with FuMa (W,X,Y,Z); "
                            "Z is typically weakest in horizontal-biased content")
        else:
            hint = None
            evidence.append(f"quietest channel is idx {quietest} — doesn't match "
                            f"a clear Z-position in either FuMa or ACN")
        if hint and ordering is None:
            ordering = hint

    # --- confidence rollup (monotonic: never downgrade) ---
    rank = {'low': 0, 'medium': 1, 'high': 2, 'certain': 3}
    target = 'low'
    if detected: target = 'low'
    if ordering is not None and normalization is not None: target = 'medium'
    if rank.get(target, 0) > rank.get(confidence, 0):
        confidence = target

    if ordering is None:      ordering = 'unknown'
    if normalization is None: normalization = 'unknown'

    result = _finalize(detected, confidence, order, order_conf, full_sphere,
                       ordering, normalization, evidence)
    result['foa_invariance_ratio_db'] = inv_ratio_db
    return result

def _finalize(detected, confidence, order, order_conf, full_sphere,
              ordering, normalization, evidence):
    return {
        'detected':         detected,
        'confidence':       confidence,
        'order':            order,
        'order_confidence': order_conf,
        'full_sphere':      full_sphere,
        'channel_ordering': ordering if ordering else 'unknown',
        'normalization':    normalization if normalization else 'unknown',
        'evidence':         evidence,
    }

def _order_name(n):
    names = {0: '0th-order', 1: '1st-order (FOA)', 2: '2nd-order',
             3: '3rd-order', 4: '4th-order', 5: '5th-order',
             6: '6th-order', 7: '7th-order'}
    return names.get(n, f'{n}th-order')

###############################################################################
# Reporting
###############################################################################

def print_report(path, wc, fmt, bext, chna, ambi_chunk, ixml, stats, ambi, seconds):
    print(deco.yellow('═══ ork.audio.ambidump ════════════════════════════════════'))
    print(f"{deco.key('file:')}      {deco.val(path)}")
    print(f"{deco.key('size:')}      {deco.val(f'{wc.size:,} bytes')}")
    print(f"{deco.key('container:')} {deco.val(wc.container)}")

    # PCM basics
    print()
    print(deco.yellow('── PCM ─────────────────────────────────────────────────────'))
    tag = fmt['wFormatTag']
    codec = {
        WAVE_FORMAT_PCM: 'PCM int',
        WAVE_FORMAT_IEEE_FLOAT: 'IEEE float',
        WAVE_FORMAT_EXTENSIBLE: 'EXTENSIBLE',
    }.get(tag, f'0x{tag:04X}')
    sr_str = f"{fmt['nSamplesPerSec']} Hz"
    print(f"{deco.key('codec:')}         {deco.val(codec)}")
    print(f"{deco.key('channels:')}      {deco.val(str(fmt['nChannels']))}")
    print(f"{deco.key('sample rate:')}   {deco.val(sr_str)}")
    print(f"{deco.key('bits/sample:')}   {deco.val(str(fmt['wBitsPerSample']))}")
    dc = wc.find(b'data')
    if dc:
        frames = dc[2] // max(fmt['nBlockAlign'], 1)
        dur = frames / float(max(fmt['nSamplesPerSec'], 1))
        frames_str = f"{frames:,}"
        dur_str = f"{dur:.3f} s"
        print(f"{deco.key('frames:')}        {deco.val(frames_str)}")
        print(f"{deco.key('duration:')}      {deco.val(dur_str)}")

    if tag == WAVE_FORMAT_EXTENSIBLE and 'SubFormat' in fmt:
        mask_str = f"0x{fmt.get('dwChannelMask',0):08X}"
        print(f"{deco.key('valid bits:')}    {deco.val(str(fmt.get('wValidBitsPerSample','?')))}")
        print(f"{deco.key('chan mask:')}     {deco.val(mask_str)}")
        print(f"{deco.key('SubFormat:')}     {deco.val(fmt['SubFormatText'])}")
        nm = fmt.get('SubFormatName')
        if nm:
            print(f"{deco.key('SubFormatName:')} {deco.magenta(nm)}")

    # Chunks
    print()
    print(deco.yellow('── chunks ──────────────────────────────────────────────────'))
    for cid, off, csz in wc.chunks:
        tag = cid.decode('latin-1', errors='replace')
        print(f"  {deco.cyan(tag):<8}  {deco.key('off=')}{off:>10}  {deco.key('size=')}{csz:>12,}")

    # bext
    if bext:
        print()
        print(deco.yellow('── bext (BWF) ──────────────────────────────────────────────'))
        for k in ('Description', 'Originator', 'OriginatorRef',
                  'OriginationDate', 'OriginationTime'):
            v = bext.get(k, '')
            if v:
                print(f"  {deco.key(k+':'):<22} {deco.val(v)}")

    # iXML
    if ixml:
        print()
        print(deco.yellow('── iXML ────────────────────────────────────────────────────'))
        if ixml.get('root_tag'):
            print(f"  {deco.key('root:')} {deco.val('<' + ixml['root_tag'] + '>')}")
        if ixml.get('ambisonic_hits'):
            print(f"  {deco.key('ambisonic hits:')}")
            for h in ixml['ambisonic_hits']:
                print(f"    {deco.cyan('•')} <{h['tag']}> = {deco.magenta(h['value'])}")
        if ixml.get('tracks'):
            print(f"  {deco.key('tracks:')}")
            for t in sorted(ixml['tracks'], key=lambda x: x['index'])[:32]:
                ii = t.get('interleave_index') or t.get('channel_index') or '?'
                print(f"    idx {t['index']:>2}  {deco.key('interleave=')}{ii!s:<4}  "
                      f"{deco.key('name=')}{deco.val(t['name'])}")
        if ixml.get('inferred_ordering'):
            print(f"  {deco.key('inferred ordering:')} "
                  f"{deco.magenta(ixml['inferred_ordering'])}")
        if not (ixml.get('ambisonic_hits') or ixml.get('tracks')
                or ixml.get('inferred_ordering')):
            print(f"  {deco.val('(no ambisonic-relevant fields found)')}")

    # chna (ADM)
    if chna and chna.get('entries'):
        print()
        print(deco.yellow('── chna (ADM) ──────────────────────────────────────────────'))
        for e in chna['entries'][:16]:
            print(f"  track {e['trackNumber']:>2}  "
                  f"{deco.key('uid=')}{e['trackUID']:<12}  "
                  f"{deco.key('fmt=')}{e['trackFormat']:<18}  "
                  f"{deco.key('pack=')}{e['packFormat']}")
        if len(chna['entries']) > 16:
            print(f"  ... {len(chna['entries'])-16} more entries")

    # Per-channel stats
    if stats:
        print()
        print(deco.yellow(f"── channel stats (analyzed {stats['seconds_analyzed']:.2f} s, "
                          f"{stats['frames_analyzed']:,} frames) ─"))
        print(f"  {'ch':>3}  {'RMS (dBFS)':>12}  {'peak (dBFS)':>12}  "
              f"{'mean|x|':>12}  {'DC':>12}")
        for s in stats['per_channel']:
            print(f"  {s['channel']:>3}  "
                  f"{s['rms_dbfs']:>+12.2f}  "
                  f"{s['peak_dbfs']:>+12.2f}  "
                  f"{s['mean_abs']:>12.6f}  "
                  f"{s['dc_offset']:>+12.6f}")

    # Ambisonic verdict
    print()
    print(deco.yellow('── ambisonic verdict ───────────────────────────────────────'))
    det_str = deco.green('yes') if ambi['detected'] else deco.red('no')
    print(f"  {deco.key('detected:')}        {det_str}")
    conf_color = {'certain': deco.green, 'high': deco.green,
                  'medium': deco.orange, 'low': deco.red}.get(ambi['confidence'], deco.val)
    print(f"  {deco.key('confidence:')}      {conf_color(ambi['confidence'])}")
    if ambi['order'] is not None:
        print(f"  {deco.key('order:')}           {deco.val(_order_name(ambi['order']))}")
        print(f"  {deco.key('full sphere:')}     {deco.val(str(ambi['full_sphere']))}")
    print(f"  {deco.key('channel order:')}   {deco.magenta(ambi['channel_ordering'])}")
    print(f"  {deco.key('normalization:')}   {deco.magenta(ambi['normalization'])}")
    if ambi['evidence']:
        print(f"  {deco.key('evidence:')}")
        for e in ambi['evidence']:
            print(f"    {deco.cyan('•')} {e}")

def build_json(path, wc, fmt, bext, chna, ambi_chunk, ixml, stats, ambi, seconds):
    chunks_out = [{'id': cid.decode('latin-1', errors='replace'),
                   'offset': off, 'size': csz}
                  for cid, off, csz in wc.chunks]
    out = {
        'schema_version': 1,
        'file': {
            'path': os.path.abspath(path),
            'size_bytes': wc.size,
            'container': wc.container,
        },
        'pcm': {
            'format_tag':    fmt['wFormatTag'],
            'channels':      fmt['nChannels'],
            'sample_rate':   fmt['nSamplesPerSec'],
            'bits_per_sample': fmt['wBitsPerSample'],
            'block_align':   fmt['nBlockAlign'],
        },
        'chunks': chunks_out,
        'bext':   bext,
        'chna':   chna,
        'ambi_chunk': ambi_chunk,
        'ixml':   ixml,
        'analysis': {
            'seconds_requested': seconds,
            'frames_analyzed':   stats['frames_analyzed'] if stats else 0,
            'seconds_analyzed':  stats['seconds_analyzed'] if stats else 0.0,
            'start_frame':       stats['start_frame'] if stats else 0,
            'per_channel':       stats['per_channel'] if stats else [],
        },
        'ambisonic': ambi,
    }
    # augment pcm with duration
    dc = wc.find(b'data')
    if dc:
        frames = dc[2] // max(fmt['nBlockAlign'], 1)
        out['pcm']['num_frames'] = frames
        out['pcm']['duration_seconds'] = frames / float(max(fmt['nSamplesPerSec'], 1))
    # fmt extensible
    if fmt.get('wFormatTag') == WAVE_FORMAT_EXTENSIBLE and 'SubFormat' in fmt:
        out['pcm']['extensible'] = {
            'valid_bits_per_sample': fmt.get('wValidBitsPerSample'),
            'channel_mask':          fmt.get('dwChannelMask'),
            'subformat_guid':        fmt['SubFormatText'],
            'subformat_name':        fmt.get('SubFormatName'),
        }
    return out

###############################################################################
# main
###############################################################################

def main():
    ap = argparse.ArgumentParser(
        description='Inspect a WAV file and report its ambisonic characteristics.')
    ap.add_argument('-i', '--input', required=True, help='input WAV file')
    ap.add_argument('-j', '--json', action='store_true',
                    help='write a sidecar JSON next to the input '
                         '(<wav>.json in the same directory)')
    ap.add_argument('-s', '--seconds', type=float, default=10.0,
                    help='seconds of audio to analyze for level stats (default 10)')
    ap.add_argument('-q', '--quiet', action='store_true',
                    help='suppress human report (useful with --json)')
    args = ap.parse_args()

    if not os.path.exists(args.input):
        print(deco.red(f'no such file: {args.input}'), file=sys.stderr)
        return 2

    try:
        wc = WavChunks(args.input)
    except Exception as e:
        print(deco.red(f'not a WAV: {e}'), file=sys.stderr)
        return 2

    fmt_chunk = wc.find(b'fmt ')
    if fmt_chunk is None:
        print(deco.red('no fmt chunk'), file=sys.stderr)
        wc.close()
        return 2
    fmt = parse_fmt(wc.read(b'fmt '))

    bext = parse_bext(wc.read(b'bext'))
    chna = parse_chna(wc.read(b'chna'))
    ambi_chunk = parse_ambi(wc.read(b'AMBI'))
    ixml = parse_ixml(wc.read(b'iXML'))

    data_chunk = wc.find(b'data')
    stats = None
    if data_chunk and fmt['nBlockAlign'] > 0:
        stats = read_pcm_window(wc, fmt, data_chunk, args.seconds)

    ambi = infer_ambisonic(fmt, chna, ambi_chunk, ixml, stats)

    if not args.quiet:
        print_report(args.input, wc, fmt, bext, chna, ambi_chunk, ixml,
                     stats, ambi, args.seconds)

    if args.json:
        payload = build_json(args.input, wc, fmt, bext, chna, ambi_chunk,
                             ixml, stats, ambi, args.seconds)
        base, _ = os.path.splitext(args.input)
        json_path = base + '.json'
        with open(json_path, 'w') as f:
            json.dump(payload, f, indent=2)
        if not args.quiet:
            print()
            print(f"{deco.key('json written:')} {deco.val(json_path)}")

    wc.close()
    return 0

if __name__ == '__main__':
    sys.exit(main())

import json, os, re, sys, time
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed

TMP = os.path.join(os.environ['TEMP'], 'loldrivers_drivers.json')
OUTDIR = os.path.join(os.environ['TEMP'], 'loldrivers_samples')
OUTJSON = r'C:\Users\m4a1_\Documents\deepseek-harness\default-workspace\membridge\evidence\loldrivers_binary_scan.json'
os.makedirs(OUTDIR, exist_ok=True)
data = json.load(open(TMP, encoding='utf-8'))
md5s = []
meta = {}
for r in data:
    for s in r.get('KnownVulnerableSamples', []) or []:
        m = (s.get('MD5') or '').lower()
        if not m:
            continue
        md5s.append(m)
        ev = s.get('ResearchEvidence') or {}
        meta[m] = {
            'filename': ev.get('OriginalFilename') or ev.get('Filename') or '',
            'sha256': s.get('SHA256') or '',
        }
md5s = sorted(set(md5s))
print('unique', len(md5s), flush=True)

UA = {'User-Agent': 'Mozilla/5.0'}
BASE = 'https://media.githubusercontent.com/media/magicsword-io/LOLDrivers/main/drivers/{}.bin'

def fetch(m):
    path = os.path.join(OUTDIR, m + '.bin')
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return (m, 'cached')
    url = BASE.format(m)
    try:
        req = urllib.request.Request(url, headers=UA)
        with urllib.request.urlopen(req, timeout=30) as resp:
            b = resp.read()
        if not b:
            return (m, 'empty')
        open(path, 'wb').write(b)
        return (m, 'ok')
    except Exception as e:
        return (m, 'err:' + type(e).__name__)

def scan(m):
    path = os.path.join(OUTDIR, m + '.bin')
    try:
        d = open(path, 'rb').read()
    except Exception:
        return None
    cr = len(re.findall(rb'\x0f\x20[\xd8-\xdf]', d))
    cw = len(re.findall(rb'\x0f\x22[\xd8-\xdf]', d))
    needles = {b'CR3', b'DirectoryTableBase', b'PsGetProcessPcb', b'MmCopyVirtualMemory',
               b'KeStackAttachProcess', b'PsLookupProcessByProcessId'}
    hits = [n.decode('latin1') for n in needles if n in d]
    if not cr and not cw and not hits:
        return None
    return {'md5': m, 'cr3_read_insns': cr, 'crx_write_insns': cw,
            'meta': meta.get(m, {}), 'hits': hits}

res = []
start = time.time()
with ThreadPoolExecutor(max_workers=12) as ex:
    futs = {ex.submit(fetch, m): m for m in md5s}
    done = 0
    for f in as_completed(futs):
        done += 1
        if done % 100 == 0:
            print('downloaded', done, '/', len(md5s), 'elapsed', round(time.time()-start,1), flush=True)

print('download phase done', round(time.time()-start,1), flush=True)
with ThreadPoolExecutor(max_workers=16) as ex:
    futs = [ex.submit(scan, m) for m in md5s]
    for f in as_completed(futs):
        r = f.result()
        if r:
            res.append(r)

res.sort(key=lambda x: (x['cr3_read_insns'], x['crx_write_insns'], x['hits']), reverse=True)
json.dump({'scanned': len(md5s), 'candidates': res}, open(OUTJSON, 'w', encoding='utf-8'),
          indent=2, ensure_ascii=False)
print('candidates', len(res), 'output', OUTJSON, flush=True)
for r in res[:60]:
    print(r['md5'], r['meta'].get('filename'), 'cr3read=', r['cr3_read_insns'],
          'crxwrite=', r['crx_write_insns'], 'hits=', ','.join(r['hits']), flush=True)

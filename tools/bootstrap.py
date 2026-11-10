#!/usr/bin/env python3
"""Install the pinned Linux x64 SDK locally, without sudo."""
from pathlib import Path
import hashlib,json,urllib.request,zipfile,io
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'toolchain-manifest.json').read_text())
entry=manifest['pvsneslib_archive']
print('Downloading',entry['url'])
with urllib.request.urlopen(entry['url']) as response:data=response.read()
assert hashlib.sha256(data).hexdigest()==entry['sha256'],'SDK archive checksum mismatch'
with zipfile.ZipFile(io.BytesIO(data)) as z:
 dest=root/'toolchain/pvsnes';dest.mkdir(parents=True,exist_ok=True)
 z.extractall(dest)
 for f in z.infolist():
  p=dest/f.filename
  if p.is_file():p.chmod((f.external_attr>>16) or 0o644)
print('SDK ready. Run python3 tools/build.py')

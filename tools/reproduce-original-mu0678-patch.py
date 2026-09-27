#!/usr/bin/env python3
from pathlib import Path
import hashlib
import sys

STOCK_SHA256 = "a3ab0fe0ac20a712f7f240c1abd852a38faf78f2673880342d0eb24a6d8c29be"
PATCH_SHA256 = "62eee8aacaad53be049f0e458abaf492c777da7879978a8c2320b661156954d0"

def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def build(stock_path, reference_path, output_path):
    stock = Path(stock_path)
    reference = Path(reference_path)
    output = Path(output_path)

    if sha256(stock) != STOCK_SHA256:
        raise SystemExit(f"wrong stock SHA256: {sha256(stock)}")
    if sha256(reference) != PATCH_SHA256:
        raise SystemExit(f"wrong reference SHA256: {sha256(reference)}")

    data = bytearray(stock.read_bytes())
    ref = reference.read_bytes()

    if len(ref) < len(data):
        raise SystemExit("reference is smaller than stock")

    for i in range(len(data)):
        if data[i] != ref[i]:
            data[i] = ref[i]
    data.extend(ref[len(data):])

    output.write_bytes(data)

    got = sha256(output)
    if got != PATCH_SHA256:
        raise SystemExit(f"byte reproduction failed: {got}")
    print(got)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(f"usage: {sys.argv[0]} STOCK REFERENCE OUTPUT")
    build(*sys.argv[1:])

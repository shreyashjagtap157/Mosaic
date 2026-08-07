#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / 'VERSION').read_text().strip()


def run(cmd, cwd=None) -> str:
    return subprocess.check_output([str(x) for x in cmd], cwd=cwd, text=True).strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        'archive',
        nargs='?',
        default=str(ROOT / f'dist/mosaic-tokenizer-{VERSION}-linux-x86_64.tar.gz'),
    )
    args = parser.parse_args()
    archive = Path(args.archive).resolve()
    if not archive.exists():
        raise SystemExit(f'missing archive: {archive}')

    with tempfile.TemporaryDirectory() as temp_dir:
        temp = Path(temp_dir)
        with tarfile.open(archive, 'r:gz') as tf:
            tf.extractall(temp, filter='data')

        roots = [path for path in temp.iterdir() if path.is_dir()]
        if len(roots) != 1:
            raise SystemExit('archive must contain exactly one root directory')

        dist = roots[0]
        cli = dist / 'bin/mosaic-tokenizer'
        model = dist / 'share/mosaic/packs/model-v1.mpack'
        unicode_pack = dist / 'share/mosaic/packs/unicode17-v1.mpack'

        if run([cli, '--version']) != f'mosaic-tokenizer {VERSION}':
            raise SystemExit('packaged CLI version mismatch')

        manifest = json.loads((dist / 'share/mosaic/release-manifest.json').read_text())
        fingerprint = run([cli, 'fingerprint', model, unicode_pack])
        if fingerprint != manifest['tokenizer_fingerprint_sha256']:
            raise SystemExit('packaged fingerprint mismatch')

        for line in (dist / 'SHA256SUMS').read_text().splitlines():
            expected, relative = line.split('  ', 1)
            actual = hashlib.sha256((dist / relative).read_bytes()).hexdigest()
            if actual != expected:
                raise SystemExit(f'checksum mismatch: {relative}')

        client = temp / 'client.c'
        client.write_text(
            '''#include <mosaic.h>\n'''
            '''#include <stddef.h>\n'''
            '''int main(int argc, char **argv) {\n'''
            '''    if (argc != 3) return 2;\n'''
            '''    mosaic_tokenizer *tokenizer = 0;\n'''
            '''    if (mosaic_tokenizer_load_files(argv[1], argv[2], &tokenizer) != MOSAIC_OK) return 3;\n'''
            '''    const unsigned char input[3] = {'a', 0xff, 'b'};\n'''
            '''    unsigned int *ids = 0;\n'''
            '''    size_t count = 0;\n'''
            '''    if (mosaic_tokenizer_encode(tokenizer, input, 3, &ids, &count) != MOSAIC_OK) return 4;\n'''
            '''    unsigned char *output = 0;\n'''
            '''    size_t output_len = 0;\n'''
            '''    if (mosaic_tokenizer_decode(tokenizer, ids, count, &output, &output_len) != MOSAIC_OK) return 5;\n'''
            '''    int ok = output_len == 3 && output[0] == 'a' && output[1] == 0xff && output[2] == 'b';\n'''
            '''    mosaic_free(ids);\n'''
            '''    mosaic_free(output);\n'''
            '''    mosaic_tokenizer_free(tokenizer);\n'''
            '''    return ok ? 0 : 6;\n'''
            '''}\n'''
        )

        subprocess.run(
            [
                'cc',
                '-std=c11',
                '-Wall',
                '-Wextra',
                '-Wpedantic',
                '-Werror',
                f'-I{dist}/include',
                client,
                dist / 'lib/libmosaic.a',
                '-o',
                temp / 'client',
            ],
            check=True,
        )
        subprocess.run([temp / 'client', model, unicode_pack], check=True)

    print(
        f'OK: packaged release {archive.name} passes CLI, manifest, checksums, '
        'and external static-client smoke'
    )
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

#!/usr/bin/env python3

import argparse
import glob
import os
import shutil
import sys


def isNotStub(path: str) -> bool:
    return (os.path.getsize(path) >= (10 * 1024))


def main() -> int:
    parser = argparse.ArgumentParser(description='Gather English and Simplified Chinese Qt translations for NSIS packaging.')
    parser.add_argument("qt_translations_folder", help="Qt's translations folder")
    parser.add_argument("nsis_packaging_folder", help="NSIS packaging translations folder")
    args = parser.parse_args()

    tmp_translations: list[str] = glob.glob(f'{args.qt_translations_folder}/qt_en.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qt_zh_CN.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qtbase_en.qm')
    tmp_translations += glob.glob(f'{args.qt_translations_folder}/qtbase_zh_CN.qm')

    filtered = filter(isNotStub, tmp_translations)
    for file in filtered:
        shutil.copy2(file, args.nsis_packaging_folder)

    return 0


if __name__ == '__main__':
    sys.exit(main())

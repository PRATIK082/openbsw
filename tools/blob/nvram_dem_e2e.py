#!/usr/bin/env python3
# *******************************************************************************
# Copyright (c) 2026 Accenture
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
"""Phase 6-8 blob converter extension: nvram / dem / e2e YAML -> OpenBSW blob JSON.

Usage:
    python tools/blob/nvram_dem_e2e.py --nvram libs/bsw/nvram/config/nvram_config.yaml \
        --dem libs/bsw/dem/config/dem_config.yaml \
        --e2e libs/bsw/e2e/config/e2e_config.yaml --out comm_stack.blob.json
"""
import argparse
import json
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    print("PyYAML is required: pip install pyyaml", file=sys.stderr)
    raise


def load_yaml(path):
    with open(path, "r", encoding="utf-8") as handle:
        return yaml.safe_load(handle) or {}


def convert_nvram_to_blob(config, blob_data):
    if "nvram" not in config:
        return
    nv = config["nvram"]
    blob_data["nvramDevices"] = nv.get("devices", [])
    blob_data["nvramBlocks"] = nv.get("blocks", [])
    blob_data["nvramWearLeveling"] = nv.get("wearLeveling", {})
    blob_data["nvramRamMirror"] = nv.get("ramMirror", {})


def convert_dem_to_blob(config, blob_data):
    if "dem" not in config:
        return
    dem = config["dem"]
    blob_data["demDtcs"] = dem.get("dtcs", [])
    blob_data["demFreezeFrame"] = dem.get("freezeFrame", {})
    blob_data["demOperationCycles"] = dem.get("operationCycles", [])
    blob_data["demEventMemory"] = dem.get("eventMemory", {})


def convert_e2e_to_blob(config, blob_data):
    if "e2e" not in config:
        return
    e2e = config["e2e"]
    blob_data["e2eProfiles"] = e2e.get("profiles", [])
    blob_data["e2eGateway"] = e2e.get("gateway", {})


def main():
    parser = argparse.ArgumentParser(description="Phase 6-8 YAML -> blob converter")
    parser.add_argument("--nvram", default="")
    parser.add_argument("--dem", default="")
    parser.add_argument("--e2e", default="")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    blob = {}
    if args.nvram:
        convert_nvram_to_blob(load_yaml(args.nvram), blob)
    if args.dem:
        convert_dem_to_blob(load_yaml(args.dem), blob)
    if args.e2e:
        convert_e2e_to_blob(load_yaml(args.e2e), blob)

    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(blob, handle, indent=2)
    print(f"Wrote {args.out} ({len(blob)} sections)")


if __name__ == "__main__":
    main()

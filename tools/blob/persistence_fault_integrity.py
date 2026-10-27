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
"""Phase 6-8 blob converter extension: persistence / faultmanager / integrity YAML -> OpenBSW blob JSON.

Usage:
    python tools/blob/persistence_fault_integrity.py --persistence libs/bsw/persistence/config/persistence_config.yaml \
        --faultmanager libs/bsw/faultmanager/config/faultmanager_config.yaml \
        --integrity libs/bsw/integrity/config/integrity_config.yaml --out comm_stack.blob.json
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


def convert_persistence_to_blob(config, blob_data):
    if "persistence" not in config:
        return
    nv = config["persistence"]
    blob_data["persistenceDevices"] = nv.get("devices", [])
    blob_data["persistenceBlocks"] = nv.get("blocks", [])
    blob_data["persistenceWearLeveling"] = nv.get("wearLeveling", {})
    blob_data["persistenceRamMirror"] = nv.get("ramMirror", {})


def convert_fault_to_blob(config, blob_data):
    if "faultmanager" not in config:
        return
    faultmanager = config["faultmanager"]
    blob_data["faultDtcs"] = faultmanager.get("dtcs", [])
    blob_data["faultFreezeFrame"] = faultmanager.get("freezeFrame", {})
    blob_data["faultOperationCycles"] = faultmanager.get("operationCycles", [])
    blob_data["faultEventMemory"] = faultmanager.get("eventMemory", {})


def convert_integrity_to_blob(config, blob_data):
    if "integrity" not in config:
        return
    integrity = config["integrity"]
    blob_data["integrityProfiles"] = integrity.get("profiles", [])
    blob_data["integrityGateway"] = integrity.get("gateway", {})


def main():
    parser = argparse.ArgumentParser(description="Phase 6-8 YAML -> blob converter")
    parser.add_argument("--persistence", default="")
    parser.add_argument("--faultmanager", default="")
    parser.add_argument("--integrity", default="")
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    blob = {}
    if args.persistence:
        convert_persistence_to_blob(load_yaml(args.persistence), blob)
    if args.faultmanager:
        convert_fault_to_blob(load_yaml(args.faultmanager), blob)
    if args.integrity:
        convert_integrity_to_blob(load_yaml(args.integrity), blob)

    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(blob, handle, indent=2)
    print(f"Wrote {args.out} ({len(blob)} sections)")


if __name__ == "__main__":
    main()

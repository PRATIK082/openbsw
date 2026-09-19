#!/usr/bin/env python3
"""DBC to OpenBSW canstack code generator.

Extracts frames and signals from a Vector DBC file and generates code-first
configuration tables for the OpenBSW ``canstack`` module.

Outputs (into --output-dir, prefix configurable via --prefix):

- ``<prefix>Tables.h``        extern declarations of the generated tables
- ``FrameConfig_table.cpp``   frame table (id, name, dlc, channel)
- ``SignalConfig_table.cpp``  signal table (name, id, start, length, ...)

The generated .cpp files are compiled into the OpenBSW build and passed to
``canstack::CanInterface::init()`` via a ``canstack::CanStackConfig``.

Signal extraction uses ``cantools`` when installed; a small built-in parser
handles the common ``BO_``/``SG_`` subset otherwise. Multiplexing information
(multiplexer switch and multiplexed signals) is preserved.

Example:
    python tools/can/dbc_to_openbsw.py vehicle.dbc --channel-id 0 --output-dir src/config
"""

import argparse
import os
import re
import sys


class Frame(object):
    def __init__(self, frame_id, name, dlc):
        self.frame_id = frame_id
        self.name = name
        self.dlc = dlc
        self.signals = []


class Signal(object):
    def __init__(
        self,
        name,
        start,
        length,
        is_motorola,
        is_signed,
        factor,
        offset,
        minimum,
        maximum,
        unit,
        is_multiplexer,
        multiplexer_value,
    ):
        self.name = name
        self.start = start
        self.length = length
        self.is_motorola = is_motorola
        self.is_signed = is_signed
        self.factor = factor
        self.offset = offset
        self.minimum = minimum
        self.maximum = maximum
        self.unit = unit
        self.is_multiplexer = is_multiplexer
        self.multiplexer_value = multiplexer_value


DBC_EXTENDED_ID_MASK = 0x1FFFFFFF

SG_PATTERN = re.compile(
    r"^\s*SG_\s+(\S+)\s+(?:(M)\s+|(m(\d+))\s+)?:\s*"
    r"(\d+)\|(\d+)@([01])([+-])\s+"
    r"\(([^,]+),([^)]+)\)\s+"
    r"\[([^|]*)\|([^\]]*)\]\s+"
    r'"([^"]*)"'
)

BO_PATTERN = re.compile(r"^BO_\s+(\d+)\s+(\S+)\s*:\s*(\d+)\s+(\S+)")


def parse_dbc_fallback(dbc_path):
    """Minimal DBC parser covering the BO_/SG_ subset (no cantools needed)."""
    frames = []
    current_frame = None

    with open(dbc_path, "r") as dbc_file:
        for line in dbc_file:
            frame_match = BO_PATTERN.match(line)
            if frame_match:
                raw_id = int(frame_match.group(1))
                frame_id = raw_id & DBC_EXTENDED_ID_MASK
                current_frame = Frame(frame_id, frame_match.group(2), int(frame_match.group(3)))
                frames.append(current_frame)
                continue

            signal_match = SG_PATTERN.match(line)
            if signal_match:
                if current_frame is None:
                    continue
                current_frame.signals.append(
                    Signal(
                        signal_match.group(1),
                        int(signal_match.group(5)),
                        int(signal_match.group(6)),
                        signal_match.group(7) == "1",
                        signal_match.group(8) == "-",
                        float(signal_match.group(9)),
                        float(signal_match.group(10)),
                        float(signal_match.group(11) or 0.0),
                        float(signal_match.group(12) or 0.0),
                        signal_match.group(13),
                        signal_match.group(2) is not None,
                        int(signal_match.group(4)) if signal_match.group(4) else -1,
                    )
                )
                continue

    return frames


def parse_dbc_cantools(dbc_path):
    """Signal extraction via cantools (preferred, handles full DBC grammar)."""
    import cantools  # pylint: disable=import-outside-toplevel

    database = cantools.database.load_file(dbc_path)
    frames = []

    for message in database.messages:
        frame = Frame(message.frame_id, message.name, message.length)
        for sig in message.signals:
            multiplexer_value = -1
            if sig.multiplexer_ids:
                multiplexer_value = sig.multiplexer_ids[0]
            frame.signals.append(
                Signal(
                    sig.name,
                    sig.start,
                    sig.length,
                    sig.byte_order == "big_endian",
                    sig.is_signed,
                    sig.scale,
                    sig.offset,
                    sig.minimum if sig.minimum is not None else 0.0,
                    sig.maximum if sig.maximum is not None else 0.0,
                    sig.unit if sig.unit is not None else "",
                    sig.is_multiplexer,
                    multiplexer_value,
                )
            )
        frames.append(frame)

    return frames


def load_dbc(dbc_path):
    if _try_import_cantools():
        return parse_dbc_cantools(dbc_path)
    return parse_dbc_fallback(dbc_path)


def _try_import_cantools():
    try:
        import cantools  # noqa: F401 pylint: disable=import-outside-toplevel

        return True
    except ImportError:
        return False


def format_float(value):
    """Formats a float compactly and loss-free for C++ source code."""
    if value == int(value) and abs(value) < 1e15:
        return "%d.0" % int(value)
    return repr(value)


def sanitize_name(name):
    return re.sub(r"\W", "_", name)


def generate_header(dbc_name, prefix, header_name):
    return """// Generated by dbc_to_openbsw.py from {dbc} -- DO NOT EDIT.
#pragma once

#include "canstack/SignalDb.h"

#include <cstddef>

extern canstack::FrameTableEntry const g_{prefix}FrameTable[];
extern size_t const g_{prefix}FrameTableCount;
extern canstack::SignalTableEntry const g_{prefix}SignalTable[];
extern size_t const g_{prefix}SignalTableCount;
""".format(dbc=dbc_name, prefix=prefix)


def generate_frame_table(dbc_name, prefix, header_name, frames, channel_id):
    lines = ["// Generated by dbc_to_openbsw.py from %s -- DO NOT EDIT." % dbc_name, ""]
    lines.append('#include "%s"' % header_name)
    lines.append("")
    lines.append("canstack::FrameTableEntry const g_%sFrameTable[] = {" % prefix)
    for frame in frames:
        lines.append(
            "    {0x%XU, \"%s\", %dU, %dU}," % (frame.frame_id, frame.name, frame.dlc, channel_id)
        )
    lines.append("};")
    lines.append("")
    lines.append(
        "size_t const g_%sFrameTableCount = sizeof(g_%sFrameTable) / sizeof(g_%sFrameTable[0]);"
        % (prefix, prefix, prefix)
    )
    lines.append("")
    return "\n".join(lines)


def generate_signal_table(dbc_name, prefix, header_name, frames, channel_id):
    lines = ["// Generated by dbc_to_openbsw.py from %s -- DO NOT EDIT." % dbc_name, ""]
    lines.append('#include "%s"' % header_name)
    lines.append("")
    lines.append("canstack::SignalTableEntry const g_%sSignalTable[] = {" % prefix)
    for frame in frames:
        for sig in frame.signals:
            lines.append(
                '    {"%s", 0x%XU, %dU, %dU, %s, %s, %s, %s, %s, "%s", %dU, %s, %d, %s},'
                % (
                    sig.name,
                    frame.frame_id,
                    sig.start,
                    sig.length,
                    "true" if sig.is_motorola else "false",
                    format_float(sig.factor),
                    format_float(sig.offset),
                    format_float(sig.minimum),
                    format_float(sig.maximum),
                    sig.unit,
                    channel_id,
                    "true" if sig.is_multiplexer else "false",
                    sig.multiplexer_value,
                    "true" if sig.is_signed else "false",
                )
            )
    lines.append("};")
    lines.append("")
    lines.append(
        "size_t const g_%sSignalTableCount = sizeof(g_%sSignalTable) / sizeof(g_%sSignalTable[0]);"
        % (prefix, prefix, prefix)
    )
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dbc_file", help="input DBC file")
    parser.add_argument(
        "--channel-id", type=int, default=0, help="CAN channel id the frames belong to (default 0)"
    )
    parser.add_argument("--output-dir", default=".", help="output directory (default .)")
    parser.add_argument("--prefix", default="can", help="table name prefix (default 'can')")
    parser.add_argument(
        "--header-name", default=None, help="generated header file name (default '<prefix>Tables.h')"
    )
    args = parser.parse_args()

    if args.channel_id < 0 or args.channel_id > 255:
        parser.error("--channel-id must be within [0, 255]")

    header_name = args.header_name or ("%sTables.h" % sanitize_name(args.prefix))

    frames = load_dbc(args.dbc_file)
    if not frames:
        print("No frames found in %s" % args.dbc_file, file=sys.stderr)
        return 1

    signal_count = sum(len(frame.signals) for frame in frames)

    if not os.path.isdir(args.output_dir):
        os.makedirs(args.output_dir)

    outputs = {
        header_name: generate_header(os.path.basename(args.dbc_file), sanitize_name(args.prefix), header_name),
        "FrameConfig_table.cpp": generate_frame_table(
            os.path.basename(args.dbc_file), sanitize_name(args.prefix), header_name, frames,
            args.channel_id,
        ),
        "SignalConfig_table.cpp": generate_signal_table(
            os.path.basename(args.dbc_file), sanitize_name(args.prefix), header_name, frames,
            args.channel_id,
        ),
    }

    for file_name, content in outputs.items():
        path = os.path.join(args.output_dir, file_name)
        with open(path, "w") as output_file:
            output_file.write(content)
        print("wrote %s" % path)

    print(
        "extracted %d frames and %d signals from %s"
        % (len(frames), signal_count, args.dbc_file)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

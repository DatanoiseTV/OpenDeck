#!/usr/bin/env bash

# Used to generate firmware UID for specified target.
# Call example: ./gen_fw_uid.sh target_name
# Hash is generated by hashing the target name and cutting the output to 4 bytes.
# After that, each byte is ANDed with 127:
# FWUID needs to be sent via MIDI SysEx and values larger than 127 aren't allowed.

if [ "$(uname)" == "Darwin" ]
then
    sha=gsha256sum
elif [ "$(uname -s)" == "Linux" ]
then
    sha=sha256sum
fi

targetname_hash=0x$(printf '%s' "$1" | $sha | head -c 64 | cut -c -8)
printf '0x%02x%02x%02x%02x\n' "$(((targetname_hash >> 0) & 127))" "$(((targetname_hash >> 8) & 127))" "$(((targetname_hash >> 16) & 127))" "$(((targetname_hash >> 24) & 127))"
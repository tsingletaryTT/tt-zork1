#!/bin/bash
# Extract actual Zork command from model output that includes <think> tags

# Usage: echo "$response" | ./extract-command.sh

# Read all input
input=$(cat)

# Extract content after </think> tag if present
if echo "$input" | grep -q "</think>"; then
  # Get everything after the last </think>, trim whitespace, get first non-empty line
  echo "$input" | sed 's/.*<\/think>//g' | grep -v '^[[:space:]]*$' | head -1 | sed 's/^[[:space:]]*//g;s/[[:space:]]*$//g'
else
  # No think tags, return trimmed first non-empty line
  echo "$input" | grep -v '^[[:space:]]*$' | head -1 | sed 's/^[[:space:]]*//g;s/[[:space:]]*$//g'
fi

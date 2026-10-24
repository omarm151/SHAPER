#!/bin/bash
# Runs a command and, if it fails, posts the important lines as a GitHub annotation
# (annotations are readable from the API even where raw logs are not).
log=$(mktemp)
"$@" 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
if [ "$status" -ne 0 ]; then
  {
    grep -E "error|Error|FAIL|failed|fatal|undefined" "$log" | grep -v "^--" | head -40
    echo "---- last lines ----"
    tail -30 "$log"
  } > "$log.sum"
  msg=$(sed -e 's/%/%25/g' -e 's/\r//g' "$log.sum" | awk '{printf "%s%%0A", $0}')
  echo "::error title=Failed: $1 $2 $3::${msg}"
fi
exit "$status"

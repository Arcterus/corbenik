#!/bin/sh

# Downloads the XML from 3dsdb, parses it, and using this information generates
# a langemu config for single-language single-region games, which can have langemu
# without any ill consequences.

ROOT=.@localedir@/emu

mkdir -p $ROOT

# Fetch XML.
wget "http://3dsdb.com/xml.php" -O 3ds.tmp

# Extract all needed fields: titleID, region, language
grep '<titleid>'   3ds.tmp | sed -e 's|[[:space:]]*||g' -e 's|</*titleid>||g'   > titleid.tmp
grep '<region>'    3ds.tmp | sed -e 's|[[:space:]]*||g' -e 's|</*region>||g'    > region.tmp
grep '<languages>' 3ds.tmp | sed -e 's|[[:space:]]*||g' -e 's|</*languages>||g' > languages.tmp

ENTS=0

# Open all three files and read into full file, checking validity as we go
while true; do
  read -r titleid <&3 || break
  read -r region <&4 || break
  read -r languages <&5 || break

  if [ "${#titleid}" = "16" ]; then
    # Length of TID is correct.
    echo "$languages" | grep ',' 2>&1 >/dev/null
    R=$?
    if [ ! $R = 0 ]; then
      # Only one language found, since no comma.
      # Output an entry.
      echo "$region $languages" | tr [:lower:] [:upper:] | sed -e 's|GER|EUR|g' -e 's|ITA|EUR|g' -e 's|FRA|EUR|g' -e 's|UKV|EUR|g' -e 's|NLD|EUR|g' > "$ROOT/$titleid"
      ENTS=$((ENTS + 1))
    fi
  fi
done 3<titleid.tmp 4<region.tmp 5<languages.tmp

echo "$(date) - $ENTS entries" > $ROOT/info

rm *.tmp

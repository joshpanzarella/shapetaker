#!/bin/bash
# Torsion Panel Position Sync Script
# Run this after making changes in Inkscape to update the code with new coordinates

set -e

SVG_FILE="res/panels/Torsion.svg"
CPP_FILE="src/torsion.cpp"

echo "=== Torsion Panel Position Sync ==="
echo "Reading from: $SVG_FILE"
echo ""

# Extract slider group transform
echo "1. Extracting slider group transform..."
TRANSFORM=$(grep -A 2 'id="g33"' "$SVG_FILE" | grep 'transform=' | grep -oE 'translate\([^)]*\)' | sed 's/translate(\(.*\))/\1/')
OFFSET_X=$(echo "$TRANSFORM" | cut -d',' -f1)
OFFSET_Y=$(echo "$TRANSFORM" | cut -d',' -f2)
echo "   Transform: translate($OFFSET_X,$OFFSET_Y)"

# Extract stage light positions
echo ""
echo "2. Extracting stage light positions..."
LIGHT_X_VALUES=()
LIGHT_Y=""
for i in {1..6}; do
  result=$(xmllint --xpath "//*[@id='stage_${i}_light']/@cx | //*[@id='stage_${i}_light']/@cy" "$SVG_FILE" 2>/dev/null)
  if [ -n "$result" ]; then
    cx=$(echo "$result" | grep -oE 'cx="[^"]*"' | cut -d'"' -f2)
    cy=$(echo "$result" | grep -oE 'cy="[^"]*"' | cut -d'"' -f2)
    LIGHT_X_VALUES+=("$cx")
    LIGHT_Y="$cy"
    echo "   stage_${i}_light: ($cx, $cy)"
  fi
done

# Format arrays for C++
LIGHT_X_ARRAY=""
for i in "${!LIGHT_X_VALUES[@]}"; do
  if [ $i -eq 0 ]; then
    LIGHT_X_ARRAY="${LIGHT_X_VALUES[$i]}f"
  else
    LIGHT_X_ARRAY="${LIGHT_X_ARRAY}, ${LIGHT_X_VALUES[$i]}f"
  fi
done

echo ""
echo "3. Updating $CPP_FILE..."

# Update slider transform
sed -i '' "s/constexpr float SLIDER_GROUP_OFFSET_X = [0-9.-]*f;/constexpr float SLIDER_GROUP_OFFSET_X = ${OFFSET_X}f;/" "$CPP_FILE"
sed -i '' "s/constexpr float SLIDER_GROUP_OFFSET_Y = [0-9.-]*f;/constexpr float SLIDER_GROUP_OFFSET_Y = ${OFFSET_Y}f;/" "$CPP_FILE"

# Update stage lights
# This is trickier due to the array format, so we'll do a multi-line replacement
TEMP_FILE=$(mktemp)
awk -v new_x="$LIGHT_X_ARRAY" -v new_y="${LIGHT_Y}f" '
/constexpr float stageLightFallbackX\[Torsion::kNumStages\] = \{/ {
    print "        constexpr float stageLightFallbackX[Torsion::kNumStages] = {"
    print "            " new_x
    getline
    next
}
/constexpr float stageLightFallbackY = [0-9.]*f;/ {
    print "        constexpr float stageLightFallbackY = " new_y ";"
    next
}
{ print }
' "$CPP_FILE" > "$TEMP_FILE"
mv "$TEMP_FILE" "$CPP_FILE"

# Update slider transform comment
sed -i '' "s|// The slider group in the SVG has transform=\"translate([^\"]*)\"|// The slider group in the SVG has transform=\"translate($OFFSET_X,$OFFSET_Y)\"|" "$CPP_FILE"

echo "   ✓ Updated slider group offset"
echo "   ✓ Updated stage light positions"
echo ""
echo "=== Summary of Changes ==="
echo "Slider Group Transform:"
echo "  X: ${OFFSET_X}f"
echo "  Y: ${OFFSET_Y}f"
echo ""
echo "Stage Light Y: ${LIGHT_Y}f"
echo ""
echo "Done! Run 'make -j4' to rebuild."

#!/usr/bin/env bash
#
# generate_markdown.sh
#
# Extracts embedded documentation from tutorial C++ source files
# and generates standalone Markdown files into tutorial/docs/.
#
# Conventions:
#   - Lines starting with `///` (column 0) = documentation (Markdown)
#   - All other lines = C++ code
#
# Navigation: prev/next links are auto-appended at the bottom of each
# generated Markdown file, using the tutorial's own title (from the
# `/// # Tutorial N: Title` first line) as link text.
#
# Usage:
#   ./generate_markdown.sh                       # generate all .md files
#   ./generate_markdown.sh tutorial_1.cpp         # generate a single file
#
# Output: tutorial/docs/tutorial_<name>.md for each input file
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/docs"
SRC_DIR="${SCRIPT_DIR}"

# --- discover tutorials dynamically ---

# Returns lines of "number|stem|title" sorted by number, e.g.:
#   1|tutorial_1_node_lifecycle|Creating and Starting a libp2p Node
discover_tutorials() {
    local dir="$1"
    for f in "$dir"/tutorial_*.cpp; do
        [ -f "$f" ] || continue
        local base
        base="$(basename "$f" .cpp)"

        # Extract number: the first number after "tutorial_"
        local num
        num="$(echo "$base" | sed 's/tutorial_\([0-9]\+\).*/\1/')"

        # Extract title from the first line: "/// # Tutorial N: Title"
        local title
        title="$(head -1 "$f" | sed 's|^/// # Tutorial [0-9]*: ||' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
        if [ -z "$title" ]; then
            # Fallback: derive from filename stem
            local stem
            stem="$(echo "$base" | sed "s/tutorial_${num}_//" | tr '_' ' ' | sed 's/\b\(.\)/\u\1/g')"
            title="$stem"
        fi

        echo "${num}|${base}|${title}"
    done | sort -t'|' -k1 -n
}

# Build arrays by reading discovered tutorials
TUTORIAL_STEMS=()
TUTORIAL_NAMES=()
TUTORIAL_NUMBERS=()

populate_tutorials() {
    local idx=0
    while IFS='|' read -r num stem title; do
        TUTORIAL_NUMBERS[$idx]="$num"
        TUTORIAL_STEMS[$idx]="$stem"
        TUTORIAL_NAMES[$idx]="$title"
        idx=$((idx + 1))
    done < <(discover_tutorials "$SRC_DIR")
}

# --- helpers ---

# Get index in the ordered array for a given filename
tutorial_index() {
    local src="$1"
    local base
    base="$(basename "$src" .cpp)"
    local num
    num="$(echo "$base" | sed 's/tutorial_\([0-9]\+\).*/\1/')"

    for i in "${!TUTORIAL_NUMBERS[@]}"; do
        if [ "${TUTORIAL_NUMBERS[$i]}" = "$num" ]; then
            echo "$i"
            return 0
        fi
    done
    echo "-1"
    return 1
}

# Convert a tutorial filename to its markdown name
md_name() {
    local base
    base="$(basename "$1" .cpp)"
    echo "${base}.md"
}

# --- generate a single tutorial ---
generate() {
    local src="$1"
    local base_name
    base_name="$(basename "$src")"
    local md="${OUTPUT_DIR}/$(md_name "$src")"

    mkdir -p "${OUTPUT_DIR}"
    echo "  -> ${base_name}  ->  docs/$(basename "${md}")"

    # Determine nav links from ordered list
    local idx
    idx="$(tutorial_index "$src")"
    local prev_link=""
    local next_link=""
    local prev_title=""
    local next_title=""

    if [ "$idx" -gt 0 ]; then
        local prev_i=$((idx - 1))
        prev_link="${TUTORIAL_STEMS[$prev_i]}.md"
        prev_title="${TUTORIAL_NAMES[$prev_i]}"
    fi

    if [ "$idx" -lt "$(( ${#TUTORIAL_NUMBERS[@]} - 1 ))" ]; then
        local next_i=$((idx + 1))
        next_link="${TUTORIAL_STEMS[$next_i]}.md"
        next_title="${TUTORIAL_NAMES[$next_i]}"
    fi

    # State machine: DOC (/// at col 0) or CODE
    local state="DOC"
    local code_content=""

    flush_code() {
        if [ -n "$code_content" ]; then
            code_content="${code_content%$'\n'}"
            echo "\`\`\`cpp"
            echo "$code_content"
            echo "\`\`\`"
            echo ""
            code_content=""
        fi
    }

    {
        while IFS= read -r line; do
            # Doc comment: /// at column 0
            if echo "$line" | grep -q '^///'; then
                if [ "$state" = "CODE" ] || [ "$state" = "CODE_BLANK" ]; then
                    flush_code
                fi
                state="DOC"

                local md_line
                md_line="$(echo "$line" | sed 's|^///||' | sed 's/^ //')"
                echo "$md_line"

            # Blank line
            elif echo "$line" | grep -q '^[[:space:]]*$'; then
                if [ "$state" = "CODE" ]; then
                    state="CODE_BLANK"
                    code_content+="${line}"$'\n'
                elif [ "$state" = "CODE_BLANK" ]; then
                    code_content+="${line}"$'\n'
                elif [ "$state" = "DOC" ]; then
                    echo ""
                fi

            # Code line
            else
                if [ "$state" = "DOC" ]; then
                    flush_code
                    state="CODE"
                elif [ "$state" = "CODE_BLANK" ]; then
                    state="CODE"
                fi
                code_content+="${line}"$'\n'
            fi
        done < "$src"

        # Flush any remaining code at EOF
        if [ "$state" = "CODE" ] || [ "$state" = "CODE_BLANK" ]; then
            flush_code
        fi

        # Append navigation links
        echo "---"
        echo ""
        if [ -n "$prev_link" ] && [ -n "$next_link" ]; then
            echo "< [${prev_title}](${prev_link}) -- [${next_title}](${next_link}) >"
        elif [ -n "$prev_link" ]; then
            echo "< [${prev_title}](${prev_link})"
        elif [ -n "$next_link" ]; then
            echo "[${next_title}](${next_link}) >"
        fi

    } > "$md"
}

# --- main ---

# Discover tutorials dynamically
populate_tutorials

echo "Discovered ${#TUTORIAL_NUMBERS[@]} tutorials:"
for i in "${!TUTORIAL_NUMBERS[@]}"; do
    echo "  ${TUTORIAL_NUMBERS[$i]}: ${TUTORIAL_NAMES[$i]}"
done
echo ""

if [ $# -eq 0 ]; then
    echo "Generating markdown from all tutorials..."
    echo ""
    for src in "${SRC_DIR}"/tutorial_*.cpp; do
        [ -f "$src" ] && generate "$src"
    done
    echo ""
    echo "Done. Markdown files in: ${OUTPUT_DIR}/"
    ls -1 "${OUTPUT_DIR}/"
else
    for src in "$@"; do
        if [ -f "$src" ]; then
            generate "$src"
        else
            echo "File not found: $src" >&2
        fi
    done
fi

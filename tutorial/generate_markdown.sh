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
# Usage:
#   ./generate_markdown.sh                       # generate all .md files
#   ./generate_markdown.sh tutorial_1.cpp         # generate a single file
#
# Output: tutorial/docs/tutorial_<name>.md for each input file
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/docs"

# --- helpers ---

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
    echo "  → ${base_name}  →  docs/$(basename "${md}")"

    # State machine tracks whether we're in DOC (/// at col 0) or CODE.
    local state="DOC"
    local code_content=""

    flush_code() {
        if [ -n "$code_content" ]; then
            # Remove trailing newline from accumulated code
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

                # Strip leading "///" and optional space
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
                    # Starting a new code block — first output any trailing
                    # doc blank lines as paragraph breaks, then code block.
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
    } > "$md"
}

# --- main ---

if [ $# -eq 0 ]; then
    echo "Generating markdown from all tutorials..."
    echo ""
    for src in "${SCRIPT_DIR}"/tutorial_*.cpp; do
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

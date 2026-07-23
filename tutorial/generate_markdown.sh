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

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
OUTPUT_DIR="${SCRIPT_DIR}/docs"
SRC_DIR="${SCRIPT_DIR}"

# --- discover tutorials dynamically ---

# Returns lines of "number|stem|title" sorted by number, e.g.:
#   1|tutorial_1_node_lifecycle|Creating and Starting a libp2p Node
discover_tutorials() {
    local dir="$1"
    for f in "$dir"/tutorial_*.cpp; do
        [[ -f "$f" ]] || continue
        local base
        base="$(basename "$f" .cpp)"

        # Extract number: the first number after "tutorial_"
        local num
        num="${base#tutorial_}"
        num="${num%%[!0-9]*}"

        # Extract title from the first line: "/// # Tutorial N: Title"
        local first_line
        local title
        IFS= read -r first_line < "$f" || first_line=""
        if [[ "$first_line" == "/// # Tutorial "*": "* ]]; then
            title="${first_line#*: }"
        else
            title="$first_line"
        fi
        title="${title#"${title%%[![:space:]]*}"}"
        title="${title%"${title##*[![:space:]]}"}"
        if [[ -z "$title" ]]; then
            # Fallback: derive from filename stem
            local stem
            stem="${base#tutorial_${num}_}"
            stem="${stem//_/ }"
            title=""
            local word
            local first
            for word in $stem; do
                first="${word:0:1}"
                title+="${first^^}${word:1} "
            done
            title="${title% }"
        fi

        printf '%s|%s|%s\n' "$num" "$base" "$title"
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
    num="${base#tutorial_}"
    num="${num%%[!0-9]*}"

    for i in "${!TUTORIAL_NUMBERS[@]}"; do
        if [[ "${TUTORIAL_NUMBERS[$i]}" == "$num" ]]; then
            printf '%s\n' "$i"
            return 0
        fi
    done
    printf '%s\n' "-1"
    return 1
}

# Convert a tutorial filename to its markdown name
md_name() {
    local base
    base="$(basename "$1" .cpp)"
    printf '%s.md\n' "$base"
}

# --- generate a single tutorial ---
generate() {
    local src="$1"
    local base_name
    base_name="$(basename "$src")"
    local md="${OUTPUT_DIR}/$(md_name "$src")"

    mkdir -p "${OUTPUT_DIR}"
    printf '  -> %s  ->  docs/%s\n' "$base_name" "$(basename "${md}")"

    # Determine nav links from ordered list
    local idx
    idx="$(tutorial_index "$src")"
    local prev_link=""
    local next_link=""
    local prev_title=""
    local next_title=""

    if [[ "$idx" -gt 0 ]]; then
        local prev_i=$((idx - 1))
        prev_link="${TUTORIAL_STEMS[$prev_i]}.md"
        prev_title="${TUTORIAL_NAMES[$prev_i]}"
    fi

    if [[ "$idx" -lt "$(( ${#TUTORIAL_NUMBERS[@]} - 1 ))" ]]; then
        local next_i=$((idx + 1))
        next_link="${TUTORIAL_STEMS[$next_i]}.md"
        next_title="${TUTORIAL_NAMES[$next_i]}"
    fi

    # State machine: DOC (/// at col 0) or CODE
    local state="DOC"
    local code_content=""

    flush_code() {
        if [[ -n "$code_content" ]]; then
            printf '```cpp\n'
            printf '%s' "$code_content"
            printf '```\n\n'
            code_content=""
        fi
    }

    nav_cell() {
        local align="$1"
        local href="$2"
        local text="$3"

        if [[ -z "$href" ]]; then
            printf '<td width="50%%" style="border: 0;"></td>\n'
        else
            printf '<td width="50%%" align="%s" style="border: 0;"><a href="%s">%s</a></td>\n' \
                "$align" "$href" "$text"
        fi
    }

    {
        while IFS= read -r line || [[ -n "$line" ]]; do
            # Doc comment: /// at column 0
            if [[ "$line" == ///* ]]; then
                if [[ "$state" == "CODE" ]] || [[ "$state" == "CODE_BLANK" ]]; then
                    flush_code
                fi
                state="DOC"

                local md_line
                md_line="${line#///}"
                md_line="${md_line# }"
                printf '%s\n' "$md_line"

            # Blank line
            elif [[ "$line" =~ ^[[:space:]]*$ ]]; then
                if [[ "$state" == "CODE" ]]; then
                    state="CODE_BLANK"
                    code_content+="${line}"$'\n'
                elif [[ "$state" == "CODE_BLANK" ]]; then
                    code_content+="${line}"$'\n'
                elif [[ "$state" == "DOC" ]]; then
                    printf '\n'
                fi

            # Code line
            else
                if [[ "$state" == "DOC" ]]; then
                    flush_code
                    state="CODE"
                elif [[ "$state" == "CODE_BLANK" ]]; then
                    state="CODE"
                fi
                code_content+="${line}"$'\n'
            fi
        done < "$src"

        # Flush any remaining code at EOF
        if [[ "$state" == "CODE" ]] || [[ "$state" == "CODE_BLANK" ]]; then
            flush_code
        fi

        # Append navigation links
        printf '%s\n\n' "---"
        if [[ -n "$prev_link" ]] || [[ -n "$next_link" ]]; then
            printf '%s\n' '<table width="100%" border="0" cellspacing="0" cellpadding="0" style="border: 0;">'
            printf '%s\n' '  <tr style="border: 0;">'
            nav_cell "left" "$prev_link" "&larr; ${prev_title}"
            nav_cell "right" "$next_link" "${next_title} &rarr;"
            printf '%s\n' "  </tr>"
            printf '%s\n' "</table>"
        fi

    } > "$md"
}

# --- main ---

# Discover tutorials dynamically
populate_tutorials

printf 'Discovered %s tutorials:\n' "${#TUTORIAL_NUMBERS[@]}"
for i in "${!TUTORIAL_NUMBERS[@]}"; do
    printf '  %s: %s\n' "${TUTORIAL_NUMBERS[$i]}" "${TUTORIAL_NAMES[$i]}"
done
printf '\n'

if [[ $# -eq 0 ]]; then
    printf '%s\n\n' "Generating markdown from all tutorials..."
    for src in "${SRC_DIR}"/tutorial_*.cpp; do
        [[ -f "$src" ]] && generate "$src"
    done
    printf '\nDone. Markdown files in: %s/\n' "$OUTPUT_DIR"
    ls -1 "${OUTPUT_DIR}/"
else
    for src in "$@"; do
        if [[ -f "$src" ]]; then
            generate "$src"
        else
            printf 'File not found: %s\n' "$src" >&2
        fi
    done
fi

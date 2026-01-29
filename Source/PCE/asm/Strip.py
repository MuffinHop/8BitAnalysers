def strip_comments_and_indent(input_text: str) -> str:
    output_lines = []

    for line in input_text.splitlines():
        # Remove comment (anything after ;)
        line = line.split(';', 1)[0]

        # Strip leading/trailing whitespace
        line = line.strip()

        # Skip empty lines
        if not line:
            continue

        output_lines.append(line)

    return "\n".join(output_lines)


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        print("Usage: python strip_asm.py <inputfile>")
        sys.exit(1)

    with open(sys.argv[1], "r", encoding="utf-8") as f:
        input_text = f.read()

    result = strip_comments_and_indent(input_text)
    print(result)

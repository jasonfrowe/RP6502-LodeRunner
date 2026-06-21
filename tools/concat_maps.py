import os
import sys

def main():
    # Paths are relative to the project root directory
    maps_dir = "maps"
    output_file = "maps/maps.bin"
    
    if not os.path.exists(maps_dir):
        print(f"Error: Maps directory '{maps_dir}' not found.", file=sys.stderr)
        sys.exit(1)
        
    try:
        with open(output_file, "wb") as outfile:
            for i in range(1, 151):
                filename = os.path.join(maps_dir, f"{i:03d}.bin")
                if not os.path.exists(filename):
                    print(f"Error: Map file '{filename}' is missing.", file=sys.stderr)
                    sys.exit(1)
                with open(filename, "rb") as infile:
                    data = infile.read()
                    if len(data) != 448:
                        print(f"Warning: Map file '{filename}' size is {len(data)} bytes (expected 448).", file=sys.stderr)
                    outfile.write(data)
        print(f"Successfully concatenated 150 maps into {output_file} ({150 * 448} bytes).")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()

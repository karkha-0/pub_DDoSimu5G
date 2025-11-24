import json
import argparse

def convert_json_to_bonnmotion(input_file, output_file):
    # Load the JSON file
    with open(input_file, "r") as file:
        data = json.load(file)

    # Prepare the output file in single-line format
    with open(output_file, "w") as f:
        for node_id, node_data in data.items():
            positions = node_data["positions"]
            timestamps = node_data["timestamps"]

            # Construct a single line for the node
            line = []
            for time, position in zip(timestamps, positions):
                x = position["x"]
                y = position["y"]
                line.extend([time, x, y])
            
            # Write the line to the file
            f.write(" ".join(map(str, line)) + "\n")

    print(f"Converted mobility data saved to {output_file}.")

# Parse command-line arguments
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert JSON mobility data to BonnMotion single-line format.")
    parser.add_argument("input_file", help="Path to the input JSON file.")
    parser.add_argument("output_file", help="Path to the output CSV file.")
    args = parser.parse_args()

    convert_json_to_bonnmotion(args.input_file, args.output_file)

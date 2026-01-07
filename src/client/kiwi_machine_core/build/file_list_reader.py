import json
import os
from pathlib import Path

def read_from_file(directory, list_file_name = 'files.json'):
    """
    Read the JSON file at the specified path, which should contain a string array, and return it directly.

    Parameters:
        directory (str): Path to the directory.
        list_file_name (str): Name of the list file.

    Returns:
        list: A list of paths.
    """
    file_path = os.path.join(directory, list_file_name) 
    if not os.path.isfile(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")

    with open(file_path, 'r', encoding='utf-8') as f:
        data = json.load(f)

    if not isinstance(data, list):
        raise ValueError("JSON content is not an array")

    if not all(isinstance(item, str) for item in data):
        raise ValueError("Not all elements in the JSON array are strings")

    data = sorted(Path(os.path.join(directory, item)) for item in data)
    print("File list: ", data)
    return data


# Example usage (optional, uncomment as needed)
# if __name__ == "__main__":
#     path = "example.json"
#     try:
#         result = read_json_string_array(path)
#         print(result)
#     except Exception as e:
#         print("Error:", e)

import os
import json

def check_if_changed(cache_file, mtime_dict):
  """Compares a cache file with a dictionary of modification times."""
  if not os.path.exists(cache_file):
    return True

  with open(cache_file, 'r') as f:
    try:
      old_mtime_dict = json.load(f)
    except (ValueError, IOError):
      # If the cache is invalid, assume it's changed.
      return True

  return old_mtime_dict != mtime_dict


def write_cache_file(cache_file, mtime_dict):
  """Writes a cache file."""
  with open(cache_file, 'w') as f:
    f.write(json.dumps(mtime_dict, indent=2, sort_keys=True))


def are_inputs_changed(input_path, cache_path):
  """Traverses a directory, and returns true if contents have changed.

  This function traverses a directory, and checks the modification time for all
  files in the directory. It compares this with a cache file, and if the
  modification times have changed, it updates the cache file and returns true.
  Otherwise, it returns false.

  Args:
    input_path: The path to the directory to traverse.
    cache_path: The path to the cache file.

  Returns:
    True if the directory contents have changed, false otherwise.
  """
  mtime_dict = {}
  for root, _, files in os.walk(input_path):
    for f in files:
      path = os.path.join(root, f)
      mtime_dict[path] = os.path.getmtime(path)

  changed = check_if_changed(cache_path, mtime_dict)
  if changed:
    write_cache_file(cache_path, mtime_dict)
  return changed

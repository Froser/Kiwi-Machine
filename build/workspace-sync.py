import os
import subprocess

# The absolute path to the root of the project.
# os.path.abspath(__file__) will get the absolute path of the current script.
# os.path.dirname() will get the directory of the script.
# So, os.path.dirname(os.path.abspath(__file__)) is the root directory of the project.
PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))

# The URL of the repository to clone.
REPO_URL = "https://github.com/Froser/Kiwi-Machine-Workspace.git"

# The path to the directory where the repository should be cloned.
# This is relative to the project root.
DEPS_DIR = os.path.join(PROJECT_ROOT, "..", "src", "third_party")

# The name of the repository directory.
REPO_NAME = "Kiwi-Machine-Workspace"

# The full path to the repository directory.
REPO_PATH = os.path.join(DEPS_DIR, REPO_NAME)

def sync_repo():
    """
    Clones or syncs the repository.
    """
    if not os.path.exists(DEPS_DIR):
        os.makedirs(DEPS_DIR)

    if not os.path.exists(REPO_PATH):
        print(f"Cloning {REPO_URL} into {REPO_PATH}...")
        subprocess.run(["git", "clone", REPO_URL, REPO_PATH], check=True)
    else:
        print(f"Repository already exists at {REPO_PATH}. Syncing...")
        # Change the current working directory to the repository path.
        os.chdir(REPO_PATH)
        # Fetch the latest changes from the remote.
        subprocess.run(["git", "fetch"], check=True)
        # Reset the local repository to the latest commit from the remote's main branch.
        # This will discard any local changes.
        subprocess.run(["git", "reset", "--hard", "origin/main"], check=True)

if __name__ == "__main__":
    sync_repo()
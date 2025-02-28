import os
import shutil
import sys

if __name__ == "__main__":
    source_folder = sys.argv[1]
    print("Source raw folder is ", source_folder)
    destination_folder = sys.argv[2]
    if len(sys.argv) > 3:
        print("${EFFECTIVE_PLATFORM_NAME} is", sys.argv[3])
        destination_folder = destination_folder.replace("${EFFECTIVE_PLATFORM_NAME}", sys.argv[3])
        print("Final destination folder is ", destination_folder)
    if not os.path.exists(destination_folder):
        os.makedirs(destination_folder)
    for root, dirs, files in os.walk(source_folder):
        for file in files:
            if file.endswith(".pak"):
                source_file = os.path.join(root, file)
                destination_file = os.path.join(destination_folder, file)
                print("Coping ", source_file, " to ", destination_file)
                shutil.copy(source_file, destination_file)

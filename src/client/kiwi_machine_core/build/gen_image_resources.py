import io
import json
import shutil
import sys
from pathlib import Path
import os


def GenTokenName(filename):
    result = 'k'
    next_is_upper = True
    for char in filename:
        if char.isalpha():
            if next_is_upper:
                result += char.upper()
                next_is_upper = False
            else:
                result += char.lower()
        elif char.isnumeric():
            result += char
        elif char.isspace() or char == '_':
            next_is_upper = True
    return result


def GenCPP(file):
    image_filename = file.absolute()
    token = GenTokenName(file.stem)

    print("Parsing: ", image_filename)
    raw_data = ''
    raw_size = ''
    with open(image_filename, 'rb') as f:
        raw_data = f.read()
        raw_size = f.tell()

    output_string = ''
    with (io.StringIO('')) as o:
        o.write('const unsigned char ' + token + '[] = {')
        for byte in raw_data:
            o.write(f'0x{byte:02x},')
        o.write('};\n')
        o.write('size_t ' + token + 'Size = sizeof(' + token + ');\n\n')
        o.seek(0)
        output_string = o.read()
    return output_string, token


def main():
    output_dir = sys.argv[1]
    print("Image resources output dir is", output_dir)
    use_wasm_ignore = len(sys.argv) > 2 and sys.argv[2] == 'ON'
    wasm_ignores = []
    print("Font resources output dir is", output_dir)
    if use_wasm_ignore:
        print("Generated for wasm env.")
        with open('./resources/images/wasm_ignore.json', 'r', encoding='utf-8') as f:
            wasm_ignores = json.load(f)

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    header = \
        '''#ifndef IMAGE_RESOURCES_H_
#define IMAGE_RESOURCES_H_

#include <stddef.h>

namespace image_resources {
'''

    all_externs = ''
    all_tokens = ''
    all_data = ''
    all_switches = ''
    for f in sorted(Path('./resources/images').iterdir()):
        if f.suffix == '.png' or f.suffix == '.jpg':
            if not f.name in wasm_ignores:
                data, token = GenCPP(f)
                token_size = token + 'Size'
                all_externs += 'extern const unsigned char ' + token + '[];\n'
                all_externs += 'extern size_t ' + token_size + ';\n'
                all_tokens += '  ' + token + ',\n'
                all_data += data
                all_switches += '    case ImageID::' + token + ': {\n'
                all_switches += '      if (size_out)\n'
                all_switches += '        *size_out = image_resources::' + token_size + ';\n'
                all_switches += '      return image_resources::' + token + ';\n'
                all_switches += '    }\n'
            else:
                print(f.name, 'is in ignore list. Ignored.')

    header_file = output_dir + '/image_resources.h'
    with open(header_file, "w") as o:
        o.write('\n')
        o.write(header)
        o.write('\n')
        o.write(all_externs)
        o.write('enum class ImageID {\n')
        o.write(all_tokens)
        o.write('\n  kLast,\n')
        o.write('};\n\n')

        o.write('const unsigned char* GetData(ImageID id, size_t* size_out);\n\n')

        o.write('}  // namespace image_resources\n')
        o.write('\n')

        # End of the header
        o.write('#endif  // IMAGE_RESOURCES_H_')
    print("Generated: ", header_file)

    cc_file = output_dir + '/image_resources.cc'
    with open(cc_file, "w") as o:
        o.write('#include "resources/image_resources.h"\n')
        o.write('\n')
        o.write('#include <SDL_assert.h>\n')
        o.write('\n')
        o.write('namespace image_resources {\n')
        o.write(all_data)
        o.write('\n')

        o.write('const unsigned char* GetData(ImageID id, size_t* size_out) {\n')
        o.write('  switch (id) {\n')
        o.write(all_switches)
        o.write('    default: {\n')
        o.write('      SDL_assert(false);  // Wrong id, shouldn\'t happen.\n')
        o.write('      return nullptr;\n')
        o.write('    }\n')
        o.write('  }\n')

        o.write('}\n\n')

        o.write('} // namespace image_resources\n')

    print("Generated: ", cc_file)

# Usage: gen_image_resources.py {OutputDir}
if __name__ == "__main__":
    main()

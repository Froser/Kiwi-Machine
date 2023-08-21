import shutil
from pathlib import Path
import os
import zlib


def GenNamespace(filename):
    result = ''
    for char in filename:
        if char.isalpha():
            result += char.lower()
        elif char.isnumeric():
            result += char
        elif char.isspace() or char == '_':
            result += '_'
    return result


def GenCPP(dir, filename):
    nes_filename = './nes/' + dir + '/' + filename + '.nes'
    cover_filename = './nes/' + dir + '/' + filename + '.jpg'
    namespace = GenNamespace(filename)
    output_filename = namespace + '.inc'
    title = filename
    file_size = 0

    print("Generating: ", nes_filename)

    with open(nes_filename, 'rb') as f:
        compress = zlib.compress(f.read())
        file_size = f.tell()

    with open(cover_filename, 'rb') as f:
        cover = f.read()

    with open('../preset_roms/' + output_filename, "w") as o:
        o.write('namespace ')
        o.write(namespace)
        o.write(' {\n')
        o.write('bool ROM_COMPRESSED = true;\n')
        o.write('size_t ROM_SIZE = ')
        o.write(str(file_size))
        o.write(';\n')
        o.write('const char ROM_NAME[] = "')
        o.write(title)
        o.write('";\n')
        # Rom data
        o.write('const kiwi::nes::Byte ROM_DATA[] = {\n')
        for byte in compress:
            o.write(f'0x{byte:02x},')
        o.write('\n};\n')
        # Rom compressed size:
        o.write('size_t ROM_COMPRESSED_SIZE = sizeof(ROM_DATA);\n')
        # Rom cover
        o.write('const kiwi::nes::Byte ROM_COVER[] = {\n')
        for byte in cover:
            o.write(f'0x{byte:02x},')
        o.write('\n};\n')
        # Rom cover size
        o.write('size_t ROM_COVER_SIZE = sizeof(ROM_COVER);\n')

        o.write('\n')
        o.write('\n} // namespace ')
        o.write(namespace)

    print("Generated: ../preset_roms/" + output_filename)
    return output_filename, namespace


def main():
    if not os.path.isdir('../preset_roms/'):
        os.mkdir('../preset_roms/')
    else:
        shutil.rmtree('../preset_roms/')
        os.mkdir('../preset_roms/')

    header = \
        '''#ifndef PRESET_ROMS_PRESET_ROMS_H_
#define PRESET_ROMS_PRESET_ROMS_H_
#include <kiwi_nes.h>

namespace preset_roms {
struct PresetROM {
  const char* name;
  const kiwi::nes::Byte* data;
  bool compressed;
  size_t raw_size;
  size_t compressed_size;
  const kiwi::nes::Byte* cover;
  size_t cover_size;
};

#define PRESET_ROM(name) \\
  name::ROM_NAME, name::ROM_DATA, name::ROM_COMPRESSED, \\
  name::ROM_SIZE, name::ROM_COMPRESSED_SIZE, \\
  name::ROM_COVER, name::ROM_COVER_SIZE
  
#define EXTERN_ROM(name) \\
namespace name { \\
  extern const char ROM_NAME[]; \\
  extern const kiwi::nes::Byte ROM_DATA[]; \\
  extern bool ROM_COMPRESSED; \\
  extern size_t ROM_SIZE; \\
  extern size_t ROM_COMPRESSED_SIZE; \\
  extern const kiwi::nes::Byte ROM_COVER[]; \\
  extern size_t ROM_COVER_SIZE; \\
}

'''
    all_includes = ''
    all_externs = ''
    all_namespaces = ''
    default_index = 0

    # Sub directories of NES
    other_roms = {}

    for f in sorted(Path('./nes').iterdir()):
        if f.suffix == '.nes':
            output_filename, namespace = GenCPP('', f.stem)
            all_includes += '#include "' + output_filename + '"\n'
            all_externs += 'EXTERN_ROM(' + namespace + ')\n'
            all_namespaces += '  {PRESET_ROM(' + namespace + ')},\n'

        # Create special ROMs (generally hacks):
        if f.is_dir():
            dir_name = f.name
            sub_all_includes = ''
            sub_all_externs = ''
            sub_all_namespaces = ''
            for s in sorted(Path('./nes/' + dir_name).iterdir()):
                if s.suffix == '.nes':
                    output_filename, namespace = GenCPP(dir_name, s.stem)
                    sub_all_includes += '#include "' + output_filename + '"\n'
                    sub_all_externs += 'EXTERN_ROM(' + namespace + ')\n'
                    sub_all_namespaces += '  {PRESET_ROM(' + namespace + ')},\n'
            other_roms[dir_name] = {
                'all_includes': sub_all_includes,
                'all_externs': sub_all_externs,
                'all_namespaces': sub_all_namespaces
            }

    with open('../preset_roms/preset_roms.h', "w") as o:
        o.write('\n')
        o.write(header)
        o.write(all_externs)
        o.write('\n')
        o.write('\n')
        o.write('const PresetROM kPresetRoms[] = {\n')
        o.write(all_namespaces)
        o.write('};\n')

        # Writes all other ROMs
        for dir_ns in other_roms:
            o.write('namespace ' + dir_ns + ' {\n')
            o.write(other_roms[dir_ns]['all_externs'])
            o.write('\n')
            o.write('const PresetROM kPresetRoms[] = {\n')
            o.write(other_roms[dir_ns]['all_namespaces'])
            o.write('};\n')
            o.write('} // namespace ' + dir_ns + ' {\n')

        o.write('} // namespace preset_roms\n')
        o.write('\n')
        o.write('#undef EXTERN_ROM\n')

        # End of the header
        o.write('#endif  // PRESET_ROMS_PRESET_ROMS_H_')

    print("Generated: ../preset_roms/preset_roms.h")

    with open('../preset_roms/preset_roms.cc', "w") as o:
        o.write('#include "preset_roms/preset_roms.h"\n')
        o.write('\n')
        o.write('namespace preset_roms {\n')
        o.write(all_includes)

        # Writes all other ROMs
        for dir_ns in other_roms:
            o.write('namespace ' + dir_ns + ' {\n')
            o.write(other_roms[dir_ns]['all_includes'])
            o.write('}  // namespace ' + dir_ns + ' {\n')

        o.write('}  // namespace preset_roms\n')

    print("Generated: ../preset_roms/preset_roms.cc")


if __name__ == "__main__":
    main()

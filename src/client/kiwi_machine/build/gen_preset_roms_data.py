import shutil
from pathlib import Path
import os


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
    zip_filename = './nes' + dir + '/' + filename + '.zip'
    namespace = GenNamespace(filename)
    output_filename = namespace + '.inc'
    title = filename
    file_size = 0

    print("Generating: ", zip_filename)

    with open(zip_filename, 'rb') as f:
        rom_data = f.read()
        file_size = f.tell()

    with open('../preset_roms/roms/' + output_filename, "w") as o:
        o.write('namespace ')
        o.write(namespace)
        o.write(' {\n')
        o.write('size_t ROM_ZIP_SIZE = ')
        o.write(str(file_size))
        o.write(';\n')
        o.write('const char ROM_NAME[] = "')
        o.write(title)
        o.write('";\n')
        # Rom data
        o.write('const kiwi::nes::Byte ROM_ZIP[] = {\n')
        for byte in rom_data:
            o.write(f'0x{byte:02x},')
        o.write('\n};\n')
        o.write('\n')
        o.write('\n} // namespace ')
        o.write(namespace)

    print("Generated: ../preset_roms/" + output_filename)
    return output_filename, namespace


def main():
    if not os.path.isdir('../preset_roms/roms'):
        os.mkdir('../preset_roms/roms')
    else:
        shutil.rmtree('../preset_roms/roms')
        os.mkdir('../preset_roms/roms')

    all_includes = ''
    all_externs = ''
    all_namespaces = ''
    default_index = 0

    # Sub directories of NES
    other_roms = {}

    last_title = ''
    for f in sorted(Path('./nes').iterdir()):
        if f.suffix == '.zip':
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
                if s.suffix == '.zip':
                    output_filename, namespace = GenCPP('/' + dir_name, s.stem)
                    sub_all_includes += '#include "' + output_filename + '"\n'
                    sub_all_externs += 'EXTERN_ROM(' + namespace + ')\n'
                    sub_all_namespaces += '  {PRESET_ROM(' + namespace + ')},\n'
            other_roms[dir_name] = {
                'all_includes': sub_all_includes,
                'all_externs': sub_all_externs,
                'all_namespaces': sub_all_namespaces
            }

    with open('../preset_roms/roms/preset_roms.inc', "w") as o:
        o.write('\n')
        o.write(all_externs)
        o.write('\n')
        o.write('\n')
        o.write('const PresetROM kPresetRoms[] = {\n')
        o.write(all_namespaces)
        o.write('};\n')
        o.write('constexpr size_t kPresetRomsCount = sizeof(kPresetRoms) / sizeof(PresetROM);\n')

        # Writes all other ROMs
        for dir_ns in other_roms:
            o.write('namespace ' + dir_ns + ' {\n')
            o.write(other_roms[dir_ns]['all_externs'])
            o.write('\n')
            o.write('const PresetROM kPresetRoms[] = {\n')
            o.write(other_roms[dir_ns]['all_namespaces'])
            o.write('};\n')
            o.write('constexpr size_t kPresetRomsCount = sizeof(kPresetRoms) / sizeof(PresetROM);\n')
            o.write('} // namespace ' + dir_ns + '\n')

    print("Generated: ../preset_roms/roms/preset_roms.inc")

    with open('../preset_roms/roms/preset_roms.cc', "w") as o:
        o.write('#include "preset_roms/preset_roms.h"\n')
        o.write('\n')
        o.write('namespace preset_roms {\n')
        o.write(all_includes)

        # Writes all other ROMs
        for dir_ns in other_roms:
            o.write('namespace ' + dir_ns + ' {\n')
            o.write(other_roms[dir_ns]['all_includes'])
            o.write('}  // namespace ' + dir_ns + '\n')

        o.write('}  // namespace preset_roms\n')

    print("Generated: ../preset_roms/roms/preset_roms.cc")


if __name__ == "__main__":
    main()

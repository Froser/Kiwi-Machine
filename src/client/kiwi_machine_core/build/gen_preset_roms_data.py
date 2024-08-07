import shutil
import sys
from pathlib import Path
import os
import zipfile
import json

output_dir = sys.argv[1]
gen_embed_roms = sys.argv[2] != "OFF"


def GenNamespace(filename):
    result = '_'
    for char in filename:
        if char.isalpha():
            result += char.lower()
        elif char.isnumeric():
            result += char
        elif char.isspace() or char == '_':
            result += '_'
    return result


def GenCPP(output_dir, dir, filename, zip):
    zip_filename = './nes' + dir + '/' + filename + '.zip'
    namespace = GenNamespace(filename)
    output_filename = namespace + '.inc'
    title = filename
    file_size = 0

    print("Generating: ", zip_filename)
    package_name = filename + '.zip'
    zip.write(zip_filename, package_name)

    if gen_embed_roms:
        with open(zip_filename, 'rb') as f:
            rom_data = f.read()
            file_size = f.tell()

        with open(output_dir + '/' + output_filename, "w") as o:
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


def GenPackageManifest(dir):
    manifest_filename = './nes' + dir + '/manifest.json'
    with open(manifest_filename, 'r', encoding='utf-8') as json_file:
        return json.load(json_file)

def WritePackageManifest(dir, zip):
    manifest_filename = './nes' + dir + '/manifest.json'
    zip.write(manifest_filename, 'manifest.json')


def WritePackage(o, package_manifest):
    o.write('struct PackageImpl : public Package { \n')
    o.write('  size_t GetRomsCount() override { return GetPresetRomsCount(); }\n')
    o.write('  PresetROM& GetRomsByIndex(size_t index) override { return GetPresetRoms()[index]; }\n')
    o.write('  kiwi::nes::Bytes GetSideMenuImage() override { static std::string g = R"(' + package_manifest["icons"][
        "normal"] + ')"; static kiwi::nes::Bytes b = kiwi::nes::Bytes(g.begin(), g.end()); return b; }\n')
    o.write('  kiwi::nes::Bytes GetSideMenuHighlightImage() override { static std::string g = R"(' +
            package_manifest["icons"][
                "highlight"] + ')"; static kiwi::nes::Bytes b =  kiwi::nes::Bytes(g.begin(), g.end()); return b; }\n')
    o.write('  std::string GetTitleForLanguage(SupportedLanguage language) override {\n')
    o.write('    return titles_[ToLanguageCode(language)];\n')
    o.write('  }\n')
    o.write('private:\n')
    o.write('  std::map<std::string, std::string> titles_ = {\n')
    for k, v in package_manifest['titles'].items():
        o.write('    { std::string("' + k + '"), std::string("' + v + '")},\n')
    o.write('  };\n')
    o.write('} package;\n')


def main():
    print("Preset roms output dir is", output_dir)
    print("Generate embed_roms:", gen_embed_roms)

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)
    else:
        shutil.rmtree(output_dir)
        os.mkdir(output_dir)

    all_includes = ''
    all_externs = ''
    all_namespaces = ''
    package_manifest = {}

    # Sub directories of NES
    other_roms = {}

    main_package_name = 'main.pak'
    with zipfile.ZipFile(output_dir + '/' + main_package_name, 'w', zipfile.ZIP_DEFLATED) as main_zip:
        if not gen_embed_roms:
            WritePackageManifest('', main_zip)
        for f in sorted(Path('./nes').iterdir()):
            if f.suffix == '.zip':
                output_filename, namespace = GenCPP(output_dir, '', f.stem, main_zip)
                all_includes += '#include "' + output_filename + '"\n'
                all_externs += 'EXTERN_ROM(' + namespace + ')\n'
                all_namespaces += '  {PRESET_ROM(' + namespace + ')},\n'

            # Generates manifest data
            package_manifest = GenPackageManifest('')

            # Create special ROMs (generally hacks):
            if f.is_dir():
                dir_name = f.name
                sub_all_includes = ''
                sub_all_externs = ''
                sub_all_namespaces = ''
                with zipfile.ZipFile(output_dir + '/' + dir_name + '.pak', 'w', zipfile.ZIP_DEFLATED) as z:
                    if not gen_embed_roms:
                        WritePackageManifest('/' + f.name, z)
                    for s in sorted(Path('./nes/' + dir_name).iterdir()):
                        if s.suffix == '.zip':
                            output_filename, namespace = GenCPP(output_dir, '/' + dir_name, s.stem, z)
                            sub_all_includes += '#include "' + output_filename + '"\n'
                            sub_all_externs += 'EXTERN_ROM(' + namespace + ')\n'
                            sub_all_namespaces += '  {PRESET_ROM(' + namespace + ')},\n'

                print(output_dir + '/' + dir_name + '.pak', "generated.")
                other_roms[dir_name] = {
                    'all_includes': sub_all_includes,
                    'all_externs': sub_all_externs,
                    'all_namespaces': sub_all_namespaces,
                    'package_manifest': GenPackageManifest('/' + f.name)
                }
    print(output_dir + '/' + main_package_name, "generated.")

    with open(output_dir + '/preset_roms.cc', "w") as o:
        o.write('#include "preset_roms/preset_roms.h"\n')
        o.write('\n')
        o.write('namespace preset_roms {\n')

        if gen_embed_roms:
            o.write('\n')
            o.write(all_externs)
            o.write('\n')
            o.write('\n')
            o.write('const PresetROM kPresetRoms[] = {\n')
            o.write(all_namespaces)
            o.write('};\n')
            o.write('size_t GetPresetRomsCount() { return sizeof(kPresetRoms) / sizeof(PresetROM); }\n')
            o.write('PresetROM* GetPresetRoms() { return const_cast<PresetROM*>(kPresetRoms); }\n')
            WritePackage(o, package_manifest)

            # Writes all other ROMs
            for dir_ns in other_roms:
                o.write('namespace ' + dir_ns + ' {\n')
                o.write(other_roms[dir_ns]['all_externs'])
                o.write('\n')
                o.write('const PresetROM kPresetRoms[] = {\n')
                o.write(other_roms[dir_ns]['all_namespaces'])
                o.write('};\n')
                o.write('size_t GetPresetRomsCount() { return sizeof(kPresetRoms) / sizeof(PresetROM); }\n')
                o.write('PresetROM* GetPresetRoms() { return const_cast<PresetROM*>(kPresetRoms); }\n')
                WritePackage(o, other_roms[dir_ns]["package_manifest"])
                o.write('} // namespace ' + dir_ns + '\n')

            # Write packages end.
            o.write('std::vector<Package*> g_packages = { &package, \n')
            for dir_ns in other_roms:
                o.write('  &' + dir_ns + '::package,\n')
            o.write('}; \n')
            o.write('std::vector<Package*> GetPresetRomsPackages() { return g_packages; }\n')
        else:
            o.write('extern std::vector<PresetROM> kPresetRoms;\n')
            o.write('extern const char kPackageName[];\n')
            o.write('size_t GetPresetRomsCount() { return kPresetRoms.size(); }\n')
            o.write('std::vector<PresetROM>& GetPresetRoms() { return kPresetRoms; }\n')
            o.write('const char* GetPresetRomsPackageName() { return kPackageName; }\n')
            for dir_ns in other_roms:
                o.write('namespace ' + dir_ns + ' {\n')
                o.write('extern std::vector<PresetROM> kPresetRoms;\n')
                o.write('extern const char kPackageName[];\n')
                o.write('size_t GetPresetRomsCount() { return kPresetRoms.size(); }\n')
                o.write('std::vector<PresetROM>& GetPresetRoms() { return kPresetRoms; }\n')
                o.write('const char* GetPresetRomsPackageName() { return kPackageName; }\n')
                o.write('}  // namespace ' + dir_ns + '\n')

        if gen_embed_roms:
            o.write(all_includes)

            # Writes all other ROMs
            for dir_ns in other_roms:
                o.write('namespace ' + dir_ns + ' {\n')
                o.write(other_roms[dir_ns]['all_includes'])
                o.write('}  // namespace ' + dir_ns + '\n')

        else:
            o.write('std::vector<PresetROM> kPresetRoms;\n')
            o.write('const char kPackageName[] = "' + main_package_name + '";\n')
            for dir_ns in other_roms:
                o.write('namespace ' + dir_ns + ' {\n')
                o.write('std::vector<PresetROM> kPresetRoms;\n')
                o.write('const char kPackageName[] = "' + dir_ns + '.pak";\n')
                o.write('}  // namespace ' + dir_ns + '\n')

        o.write('}  // namespace preset_roms\n')

    print("Generated: preset_roms/preset_roms.cc")


if __name__ == "__main__":
    main()

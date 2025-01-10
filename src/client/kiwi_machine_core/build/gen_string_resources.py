import io
import sys
from pathlib import Path
import os
import json


def GetData(file):
    with open(file.absolute(), 'r', encoding = 'utf-8') as f:
        data = json.load(f)

    output = {}
    for idr in data:
        output[idr] = {}
        for lang in data[idr]:
            output[idr][lang.lower()] = data[idr][lang]

    return output


def main():
    output_dir = sys.argv[1]
    print("String resources output dir is", output_dir)

    if not os.path.isdir(output_dir):
        os.mkdir(output_dir)

    header = \
        '''#ifndef STRING_RESOURCES_H_
#define STRING_RESOURCES_H_

#include <stddef.h>
#include <string>
#include <unordered_map>

namespace string_resources {

using StringMap = std::unordered_map<int, std::unordered_map<std::string, std::string>>;

const StringMap& GetGlobalStringMap();
'''

    all_idrs = ''
    all_strings = ''

    for f in sorted(Path('./resources/strings').iterdir()):
        if f.suffix == '.json':
            data = GetData(f)
            for idr in data:
                all_idrs += '  ' + idr + ',\n'
                all_strings += '  { ' + idr + ' , { \n'
                for lang in data[idr]:
                    all_strings += '{ "' + lang + '", R"(' + data[idr][lang] + ')" },\n'
                all_strings += '  }},\n'

    header_file = output_dir + '/string_resources.h'
    with open(header_file, "w") as o:

        o.write('\n')
        o.write(header)
        o.write('\n')
        o.write('enum {\n')
        o.write(all_idrs)
        o.write('\n  END_OF_STRINGS\n')
        o.write('};\n\n')

        o.write('}  // namespace string_resources\n')
        o.write('\n')

        # End of the header
        o.write('#endif  // STRING_RESOURCES_H_')
    print("Generated: ", header_file)

    cc_file = output_dir + '/string_resources.cc'
    with open(cc_file, "w") as o:
        o.write('#include "resources/string_resources.h"\n')
        o.write('\n')
        o.write('namespace string_resources {\n')
        o.write('\n')
        o.write('StringMap g_global_strings = {')
        o.write(all_strings)
        o.write('};\n')
        o.write('const StringMap& GetGlobalStringMap() {\n')
        o.write('  return g_global_strings;\n')
        o.write('}\n\n')
        o.write('} // namespace string_resources\n')
    print("Generated: ", cc_file)


# Usage: gen_string_resources.py {OutputDir}
if __name__ == "__main__":
    main()

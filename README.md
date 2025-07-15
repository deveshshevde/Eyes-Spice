# Eyes-Spice
* Hardware and software architecture repo 




## Hardware 
* Ov5640
    * MIPI 30 pins connector
    * FFC L shape cable
    * Direct soldered FFC to PCB 

* ESP32-S3R8 
* W25Q16JVSSIQ


import os
import struct

def read_null_terminated_string(f):
    chars = []
    while True:
        c = f.read(1)
        if not c or c == b'\x00':
            break
        chars.append(c)
    return b''.join(chars).decode('utf-8')

def parse_ninja_deps(filename):
    deps = {}
    with open(filename, 'rb') as f:
        header = f.read(4)
        if header != b"#dep":
            raise ValueError("Not a valid .ninja_deps file")

        version = struct.unpack("<i", f.read(4))[0]
        if version != 1:
            raise ValueError("Unsupported .ninja_deps version")

        string_table = []
        string_offset_map = {}

        # Read entire file into memory
        data = f.read()
        i = 0
        while i < len(data):
            if i + 4 > len(data):
                break
            mtime, = struct.unpack("<I", data[i:i+4])
            i += 4

            start = i
            while i < len(data) and data[i] != 0:
                i += 1
            output = data[start:i].decode()
            i += 1

            if i + 4 > len(data):
                break
            n_inputs, = struct.unpack("<I", data[i:i+4])
            i += 4

            inputs = []
            for _ in range(n_inputs):
                start = i
                while i < len(data) and data[i] != 0:
                    i += 1
                inp = data[start:i].decode()
                i += 1
                inputs.append(inp)

            # Filter for .c and .h
            if output.endswith(".o") or output.endswith(".obj"):
                c_file = None
                header_files = []
                for inp in inputs:
                    if inp.endswith(".c"):
                        c_file = inp
                    elif inp.endswith(".h"):
                        header_files.append(inp)
                if c_file:
                    deps[c_file] = header_files
    return deps


# Example usage:
if __name__ == "__main__":
    path_to_ninja_deps = ".ninja_deps"
    if os.path.exists(path_to_ninja_deps):
        dep_map = parse_ninja_deps(path_to_ninja_deps)
        for c_file, headers in dep_map.items():
            print(f"{c_file} uses:")
            for h in headers:
                print(f"  -> {h}")
    else:
        print("File '.ninja_deps' not found.")

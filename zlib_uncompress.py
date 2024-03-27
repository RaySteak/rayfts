import glob
import zlib
import sys

for filename in sys.argv:
    with open(filename, 'rb') as compressed:
        with open(filename + '-decompressed', 'wb') as expanded:
            data = zlib.decompress(compressed.read())
            expanded.write(data)
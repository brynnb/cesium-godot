import subprocess
import tempfile
import os
import sys

def demangle(symbol: str) -> str:
    try:
        result = subprocess.run(['c++filt', symbol], capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running c++filt: {e}")
        return symbol

def nm_and_search(directory: str, demangled: str) -> list:
    matches = []
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.a'):
                lib_path = os.path.join(root, file)
                try:
                    with tempfile.NamedTemporaryFile(mode='w+', delete=False, encoding='utf-8') as tmp:
                        subprocess.run(['nm', lib_path], stdout=tmp, stderr=subprocess.DEVNULL, check=True)
                        tmp.seek(0)
                        for line in tmp:
                            if demangled in line:
                                matches.append([lib_path, line])
                except Exception as e:
                    print(f"Error processing {lib_path}: {e}")
    return matches

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python find_symbol.py <directory> <mangled_symbol>")
        sys.exit(1)

    directory = sys.argv[1]
    mangled = sys.argv[2]
    demangled = demangle(mangled)
    print(f"Demangled symbol: {demangled}")

    matches = nm_and_search(directory, mangled)
    if matches:
        print("\n🔍 Symbol found in:")
        for match in matches:
            print(f"- {match}")
    else:
        print("\n🚫 Symbol not found in any .a files.")



from pathlib import Path
import difflib
import shutil

ROOT = Path("src")

# Exact replacements for the paths we've identified.
replacements = {
    "main.cpp": [
        ('const string dblocation = "/home/sourcemaster/.local/share/synnical/test-root/var/lib/synnical/installed.db";',
         'const auto dblocation = paths::database();'),
        ('"/home/sourcemaster/.local/share/synnical/test-root"',
         'paths::root().string()'),
    ],
    "marea/build.cpp": [
        ('"/home/sourcemaster/.local/share/synnical/test-root/usr"',
         'paths::usr()'),
    ],
    "marea/solver.cpp": [
        ('constexpr const char *database_path = "/home/sourcemaster/.local/share/synnical/test-root/var/lib/synnical/installed.db";',
         'const std::string database_path = paths::database().string();'),
    ],
    "marea/remove.cpp": [
        ('"/home/sourcemaster/.local/share/synnical/test-root"',
         'paths::root()'),
    ],
    "marea/deploy.cpp": [
        ('constexpr const char *test_root = "/home/sourcemaster/.local/share/synnical/test-root";',
         'const std::string test_root = paths::root().string();'),
    ],
    "marea/install.cpp": [
        ('"/home/sourcemaster/.local/share/synnical/test-root"',
         'paths::root()'),
    ],
}

# Include paths depend on where the source file lives.
def include_for(filename):
    if "/" in filename:
        return '#include "../general/paths.hpp"\n'
    return '#include "general/paths.hpp"\n'


def migrate():
    changes = {}

    for filename, rules in replacements.items():
        path = ROOT / filename
        original = path.read_text()
        updated = original

        for old, new in rules:
            if old not in updated:
                raise RuntimeError(
                    f"{filename}: expected text not found:\n{old}"
                )

            updated = updated.replace(old, new)

        include = include_for(filename)

        if include.strip() not in updated:
            updated = include + updated

        changes[path] = (original, updated)

    print("Files to update:\n")

    for path, (old, new) in changes.items():
        print(f"  {path}")

        diff = difflib.unified_diff(
            old.splitlines(keepends=True),
            new.splitlines(keepends=True),
            fromfile=str(path),
            tofile=str(path),
        )

        print("".join(diff))

    answer = input("\nApply changes? [y/N]: ")

    if answer.lower() != "y":
        print("Cancelled.")
        return

    for path, (_, updated) in changes.items():
        backup = path.with_suffix(path.suffix + ".bak")
        shutil.copy2(path, backup)
        path.write_text(updated)

    print("\nDone. Original files saved as .cpp.bak")


if __name__ == "__main__":
    migrate()


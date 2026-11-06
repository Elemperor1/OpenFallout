"""Exercise generator failures without installing Lua or downloading dependencies."""

import ast
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


SOURCE = Path(__file__).resolve().parent / "source"


class LuaDocTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="lua docs ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "docs/source"
        self.source.mkdir(parents=True)
        shutil.copy(SOURCE / "generate_luadoc.sh", self.source)
        (self.root / "files/lua_api/openfallout").mkdir(parents=True)
        (self.root / "files/lua_api/openfallout/core.lua").touch()
        (self.root / "files/data").mkdir()
        self.bin = self.root / "bin"
        self.bin.mkdir()
        # Keep host-installed Lua tools out of the fixture's PATH.
        for command in ("bash", "dirname", "rm"):
            (self.bin / command).symlink_to(shutil.which(command))
        self.write_script(self.source / "luadoc_data_paths.sh",
                          "printf '%s\\n' one.lua two.lua\n")
        self.write_script(self.source / "install_luadocumentor_in_docker.sh", "exit 0\n")
        self.write_script(self.bin / "luarocks", 'exit "${ROCKS_EXIT:-0}"\n')
        self.write_script(self.bin / "openmwluadocumentor", '''
printf '%s\\n' "$*" >> "$LUADOC_LOG"
case "$*" in
    *"$FAIL_ON"*) if [ -n "$FAIL_ON" ]; then exit 23; fi ;;
esac
''')
        self.log = self.root / "calls.log"
        self.environment = dict(os.environ, PATH=str(self.bin), HOME=str(self.root),
                                LUADOC_LOG=str(self.log), FAIL_ON="", ROCKS_EXIT="0")

    @staticmethod
    def write_script(path, contents):
        path.write_text("#!/bin/bash\n" + contents)
        path.chmod(0o755)

    def run_generator(self, **environment):
        return subprocess.run([str(self.bin / "bash"),
                               str(self.source / "generate_luadoc.sh")],
                              cwd=self.root, env=dict(self.environment, **environment),
                              text=True, capture_output=True)

    def calls(self):
        return self.log.read_text().splitlines() if self.log.exists() else []

    def test_success_generates_api_and_data_pages_from_another_directory(self):
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.calls()), 3)
        self.assertTrue(self.calls()[-1].endswith("two.lua"))

    def test_api_check_treats_checkout_path_as_literal(self):
        checkout = self.root / "checkout [1]"
        checkout.symlink_to(self.root, target_is_directory=True)
        result = subprocess.run([str(self.bin / "bash"),
                                 str(checkout / "docs/source/generate_luadoc.sh")],
                                cwd=self.root, env=self.environment,
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.calls()), 3)

    def test_api_failure_stops_before_data_pages(self):
        result = self.run_generator(FAIL_ON="core.lua")
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertEqual(len(self.calls()), 1)

    def test_no_api_files_fails_before_invoking_documentor(self):
        (self.root / "files/lua_api/openfallout/core.lua").unlink()
        (self.root / "files/lua_api/openfallout/README.txt").touch()
        result = self.run_generator()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("No Lua API files found", result.stderr)
        self.assertEqual(self.calls(), [])

    def test_data_failure_stops_before_later_pages(self):
        result = self.run_generator(FAIL_ON="one.lua")
        self.assertEqual(result.returncode, 23, result.stderr)
        self.assertEqual(len(self.calls()), 2)

    def test_data_globs_still_expand_into_separate_pages(self):
        auxiliary = self.root / "files/data/aux"
        auxiliary.mkdir()
        for name in ("first.lua", "second.lua"):
            (auxiliary / name).touch()
        self.write_script(self.source / "luadoc_data_paths.sh",
                          "printf '%s\\n' 'aux/*.lua'\n")
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(self.calls()), 3)
        self.assertTrue(self.calls()[-1].endswith("aux/second.lua"))

    def test_luarocks_failure_is_not_hidden_by_eval(self):
        result = self.run_generator(ROCKS_EXIT="17")
        self.assertEqual(result.returncode, 17, result.stderr)
        self.assertEqual(self.calls(), [])

    def test_missing_generator_fails(self):
        (self.bin / "openmwluadocumentor").unlink()
        result = self.run_generator()
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn("openmwluadocumentor not found", result.stderr)

    def test_sphinx_setup_propagates_generator_failure(self):
        # Run the actual hook without importing the theme or the rest of conf.py.
        tree = ast.parse((SOURCE / "conf.py").read_text())
        hook = next(node for node in tree.body
                    if isinstance(node, ast.FunctionDef) and node.name == "setup")
        namespace = {"subprocess": subprocess, "project_root": str(self.root)}
        exec(compile(ast.Module(body=[hook], type_ignores=[]), "conf.py", "exec"), namespace)
        self.write_script(self.source / "generate_luadoc.sh", "exit 19\n")
        with self.assertRaises(subprocess.CalledProcessError) as raised:
            namespace["setup"](None)
        self.assertEqual(raised.exception.returncode, 19)


if __name__ == "__main__":
    unittest.main()

"""Configure the actual Apple packaging rules with stand-in Qt/OSG plugins."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent


class MacBundleTests(unittest.TestCase):
    def configure(self, game, editor, qt):
        temporary = tempfile.TemporaryDirectory(prefix="mac bundles ")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        source, build = root / "source", root / "build"
        (source / "files/mac").mkdir(parents=True)
        (source / "files/mac/qt.conf").write_text("[Paths]\nPlugins=../PlugIns\n")
        for name in ("README.md", "CHANGELOG.md", "LICENSE", "AUTHORS.md"):
            (source / name).touch()
        (source / "cmake").mkdir()
        (source / "cmake/osg.dylib").touch()
        (source / "cmake/FindOSGPlugins.cmake").write_text(
            'set(DUMMY_LIBRARY_RELEASE "${CMAKE_CURRENT_LIST_DIR}/osg.dylib")\n')
        shutil.copy(ROOT / "cmake/SignMacApplications.cmake", source / "cmake")
        # Execute the production block; no game dependencies or compiler are needed.
        packaging = (ROOT / "CMakeLists.txt").read_text().split("# Apple bundling\n", 1)[1]
        packaging = packaging.split("elseif(NOT APPLE)", 1)[0] + "endif()\n"
        (source / "CMakeLists.txt").write_text('''
cmake_minimum_required(VERSION 3.19)
project(BundleFixture NONE)
set(APPLE TRUE)
set(OpenFallout_SOURCE_DIR "${CMAKE_SOURCE_DIR}")
set(OpenFallout_BINARY_DIR "${CMAKE_BINARY_DIR}")
set(APP_BUNDLE_NAME "OpenFallout.app")
set(APP_BUNDLE_DIR "${CMAKE_BINARY_DIR}/${APP_BUNDLE_NAME}")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(OPENSCENEGRAPH_VERSION "3.6.5")
set(USED_OSG_PLUGINS dummy)
if(BUILD_OPENCS)
    add_executable(openfallout-cs IMPORTED)
    set_property(TARGET openfallout-cs PROPERTY OUTPUT_NAME "OpenFallout-CS")
endif()
if(USE_QT)
    set(QT_VERSION_MAJOR 6)
    foreach(plugin QCocoaIntegrationPlugin QMacStylePlugin QSvgPlugin QSvgIconPlugin)
        file(WRITE "${CMAKE_BINARY_DIR}/qt/${plugin}/plugin.dylib" "plugin")
        add_library(Qt6::${plugin} MODULE IMPORTED)
        set_property(TARGET Qt6::${plugin} PROPERTY IMPORTED_LOCATION_RELEASE
            "${CMAKE_BINARY_DIR}/qt/${plugin}/plugin.dylib")
    endforeach()
endif()
''' + packaging)
        result = subprocess.run([
            "cmake", "-S", str(source), "-B", str(build),
            f"-DBUILD_OPENFALLOUT={'ON' if game else 'OFF'}",
            f"-DBUILD_OPENCS={'ON' if editor else 'OFF'}",
            f"-DUSE_QT={'ON' if qt else 'OFF'}",
        ], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return root, build

    def check_bundles(self, game, editor, qt):
        root, build = self.configure(game, editor, qt)
        expected = (["OpenFallout.app"] if game else []) + (["OpenFallout-CS.app"] if editor else [])
        install = (build / "cmake_install.cmake").read_text()
        if not expected:
            self.assertFalse((build / "CPackConfig.cmake").exists())
            self.assertNotIn("fixup_bundle(", install)
            return
        config = (build / "CPackConfig.cmake").read_text()
        self.assertIn(f'set(CPACK_OPENFALLOUT_APPLICATIONS "{";".join(expected)}")', config)
        for bundle in ("OpenFallout.app", "OpenFallout-CS.app"):
            if bundle in expected:
                self.assertIn(f'fixup_bundle("${{CMAKE_INSTALL_PREFIX}}/{bundle}"', install)
            else:
                self.assertNotIn(bundle, install)
            plugins = list((build / bundle).rglob("plugin.dylib"))
            self.assertEqual(len(plugins), 4 if qt and bundle in expected else 0)

        # CPack must sign only the selected bundles, even when one app is disabled.
        stage = root / "package"
        for bundle in expected:
            (stage / "ALL_IN_ONE" / bundle / "Contents/PlugIns/osgPlugins-3.6.5").mkdir(parents=True)
        binary = root / "bin"
        binary.mkdir()
        signing_log = root / "signed.txt"
        codesign = binary / "codesign"
        codesign.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$SIGNING_LOG"\n')
        codesign.chmod(0o755)
        result = subprocess.run([
            "cmake", "-DAPPLE=TRUE", f"-DCPACK_OPENFALLOUT_APPLICATIONS={';'.join(expected)}",
            f"-DCPACK_TEMPORARY_INSTALL_DIRECTORY={stage}",
            "-P", str(ROOT / "cmake/SignMacApplications.cmake"),
        ], env=dict(os.environ, PATH=str(binary) + os.pathsep + os.environ["PATH"],
                    SIGNING_LOG=str(signing_log)), text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        signed = signing_log.read_text().splitlines()
        self.assertEqual(len(signed), len(expected))
        for line, bundle in zip(signed, expected):
            self.assertTrue(line.endswith(bundle), line)

    def test_tools_only_without_qt(self):
        self.check_bundles(game=False, editor=False, qt=False)

    def test_game_without_qt(self):
        self.check_bundles(game=True, editor=False, qt=False)

    def test_game_with_qt(self):
        self.check_bundles(game=True, editor=False, qt=True)

    def test_editor_only(self):
        self.check_bundles(game=False, editor=True, qt=True)

    def test_game_and_editor(self):
        self.check_bundles(game=True, editor=True, qt=True)


if __name__ == "__main__":
    unittest.main()

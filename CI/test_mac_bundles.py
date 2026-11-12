"""Configure the actual Apple packaging rules with stand-in Qt/OSG plugins."""

import os
import plistlib
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
MACHO = bytes.fromhex("cffaedfe") + bytes(28)
IDENTITY = "Developer ID Application: Example Studio (EXAMPLE123)"
TEAM = "EXAMPLE123"
ENTITLEMENTS = {
    "OpenFallout.app": "openfallout.entitlements",
    "OpenFallout-CS.app": "openfallout-cs.entitlements",
}


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
        entitlements = [f"{root}/source/files/mac/{ENTITLEMENTS[bundle]}" for bundle in expected]
        self.assertIn(f'set(CPACK_OPENFALLOUT_ENTITLEMENTS "{";".join(entitlements)}")', config)
        for bundle in ("OpenFallout.app", "OpenFallout-CS.app"):
            if bundle in expected:
                self.assertIn(f'fixup_bundle("${{CMAKE_INSTALL_PREFIX}}/{bundle}"', install)
            else:
                self.assertNotIn(bundle, install)
            plugins = list((build / bundle).rglob("plugin.dylib"))
            self.assertEqual(len(plugins), 4 if qt and bundle in expected else 0)

        # CPack must sign only the selected bundles, even when one app is disabled.
        signed = self.sign(root, expected)
        self.assertEqual(len(signed), len(expected))
        for call, bundle in zip(signed, expected):
            # With no identity the bundle keeps today's ad hoc signature.
            self.assertEqual(call, ["--force", "--deep", "-s", "-", str(self.app_root / bundle)])

    def stage(self, root, bundles):
        """Lay out bundles as CPack stages them, with stand-in Mach-O files."""
        stage = root / "package"
        for bundle in bundles:
            contents = stage / "ALL_IN_ONE" / bundle / "Contents"
            executable = bundle[:-len(".app")].lower()
            (contents / "PlugIns/osgPlugins-3.6.5").mkdir(parents=True)
            (contents / "Info.plist").write_text(
                "<plist><dict><key>CFBundleExecutable</key>"
                f"<string>{executable}</string></dict></plist>\n")
            framework = contents / "Frameworks/QtCore.framework"
            nested = {
                f"MacOS/{executable}": MACHO,
                "MacOS/helper": MACHO,
                "MacOS/launch.sh": b"#!/bin/sh\n",
                "Frameworks/libSDL2-2.0.0.dylib": MACHO,
                "Frameworks/QtCore.framework/Versions/A/QtCore": MACHO,
                "PlugIns/osgPlugins-3.6.5/osgdb_dds.so": MACHO,
                "PlugIns/platforms/libqcocoa.dylib": MACHO,
                "Resources/qt.conf": b"[Paths]\n",
            }
            for relative, data in nested.items():
                (contents / relative).parent.mkdir(parents=True, exist_ok=True)
                (contents / relative).write_bytes(data)
            (framework / "Versions/Current").symlink_to("A")
            (framework / "QtCore").symlink_to("Versions/Current/QtCore")
        return stage

    def sign(self, root, bundles, **environment):
        """Run the CPack signing script and return every recorded codesign call."""
        stage = self.stage(root, bundles)
        binary = root / "bin"
        binary.mkdir(exist_ok=True)
        signing_log = root / "signed.txt"
        codesign = binary / "codesign"
        codesign.write_text(
            "#!/bin/sh\nIFS=$(printf '\\t')\nprintf '%s\\n' \"$*\" >> \"$SIGNING_LOG\"\n")
        codesign.chmod(0o755)
        env = {k: v for k, v in os.environ.items() if not k.startswith("OPENFALLOUT_CODESIGN")}
        env.update(environment, PATH=str(binary) + os.pathsep + os.environ["PATH"],
                   SIGNING_LOG=str(signing_log))
        entitlements = [str(ROOT / "files/mac" / ENTITLEMENTS[bundle]) for bundle in bundles]
        result = subprocess.run([
            "cmake", "-DAPPLE=TRUE", f"-DCPACK_OPENFALLOUT_APPLICATIONS={';'.join(bundles)}",
            f"-DCPACK_OPENFALLOUT_ENTITLEMENTS={';'.join(entitlements)}",
            f"-DCPACK_TEMPORARY_INSTALL_DIRECTORY={stage}",
            "-P", str(ROOT / "cmake/SignMacApplications.cmake"),
        ], env=env, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.output = result.stdout + result.stderr
        self.app_root = stage / "ALL_IN_ONE"
        if not signing_log.exists():
            return []
        return [line.split("\t") for line in signing_log.read_text().splitlines()]

    def sign_with_identity(self, bundles, **environment):
        temporary = tempfile.TemporaryDirectory(prefix="mac signing ")
        self.addCleanup(temporary.cleanup)
        return self.sign(Path(temporary.name), bundles,
                         OPENFALLOUT_CODESIGN_IDENTITY=IDENTITY, **environment)

    def test_identity_signs_each_nested_binary_before_its_bundle(self):
        bundles = ["OpenFallout.app", "OpenFallout-CS.app"]
        calls = [call for call in self.sign_with_identity(bundles) if "--sign" in call]
        targets = [call[-1] for call in calls]
        for bundle in bundles:
            app = str(self.app_root / bundle)
            nested = [app + "/Contents/" + relative for relative in (
                "MacOS/helper",
                "Frameworks/libSDL2-2.0.0.dylib",
                "Frameworks/QtCore.framework",
                "PlugIns/osgPlugins/osgdb_dds.so",
                "PlugIns/platforms/libqcocoa.dylib",
            )]
            self.assertIn(app, targets)
            for item in nested:
                self.assertEqual(targets.count(item), 1, item)
                self.assertLess(targets.index(item), targets.index(app), item)
            # The main executable is sealed by the bundle signature; scripts,
            # resources and symlinks are not code.
            self.assertEqual(
                sorted(t for t in targets if t.startswith(app + "/")), sorted(nested))

    def test_identity_uses_hardened_runtime_timestamp_and_entitlements(self):
        bundles = ["OpenFallout.app", "OpenFallout-CS.app"]
        calls = [call for call in self.sign_with_identity(bundles) if "--sign" in call]
        self.assertTrue(calls)
        for call in calls:
            self.assertEqual(call[call.index("--sign") + 1], IDENTITY)
            self.assertEqual(call[call.index("--options") + 1], "runtime")
            self.assertIn("--timestamp", call)
        for bundle in bundles:
            app = str(self.app_root / bundle)
            entitlements = str(ROOT / "files/mac" / ENTITLEMENTS[bundle])
            # Executables carry their app's entitlements; libraries and plug-ins
            # take those of the process that loads them.
            for target in (app, app + "/Contents/MacOS/helper"):
                call = next(call for call in calls if call[-1] == target)
                self.assertEqual(call[call.index("--entitlements") + 1], entitlements)
            for call in calls:
                if call[-1].startswith(app + "/Contents/") and "/Contents/MacOS/" not in call[-1]:
                    self.assertNotIn("--entitlements", call)

    def test_entitlements_files_are_release_safe(self):
        for name in ENTITLEMENTS.values():
            with open(ROOT / "files/mac" / name, "rb") as entitlements:
                values = plistlib.load(entitlements)
            self.assertNotIn("com.apple.security.get-task-allow", values, name)
        game = (ROOT / "files/mac/openfallout.entitlements").read_text()
        self.assertIn("LUAJIT ENTITLEMENT", game)

    def test_identity_never_uses_deep_or_prints_secrets(self):
        calls = self.sign_with_identity(["OpenFallout.app", "OpenFallout-CS.app"],
                                        OPENFALLOUT_CODESIGN_TEAM_ID=TEAM)
        for call in calls:
            if "--sign" in call or "-s" in call:
                self.assertNotIn("--deep", call)
        self.assertNotIn(IDENTITY, self.output)
        self.assertNotIn(TEAM, self.output)

    def test_empty_identity_takes_ad_hoc_path(self):
        temporary = tempfile.TemporaryDirectory(prefix="mac signing ")
        self.addCleanup(temporary.cleanup)
        calls = self.sign(Path(temporary.name), ["OpenFallout.app"], OPENFALLOUT_CODESIGN_IDENTITY="")
        self.assertEqual(calls, [["--force", "--deep", "-s", "-", str(self.app_root / "OpenFallout.app")]])

    def test_identity_verifies_each_bundle_against_the_team(self):
        calls = self.sign_with_identity(["OpenFallout.app"], OPENFALLOUT_CODESIGN_TEAM_ID=TEAM)
        app = str(self.app_root / "OpenFallout.app")
        self.assertEqual(calls[-1], [
            "--verify", "--deep", "--strict",
            f'-R=anchor apple generic and certificate leaf[subject.OU] = "{TEAM}"', app])
        self.assertEqual(calls[-2][-1], app)

    def test_identity_without_team_verifies_strictly(self):
        calls = self.sign_with_identity(["OpenFallout.app"])
        self.assertEqual(calls[-1], ["--verify", "--deep", "--strict", str(self.app_root / "OpenFallout.app")])

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


class MacInfoPlistTests(unittest.TestCase):
    """Render the Info.plist templates with CMake's configure_file, as the build does."""

    def render(self, template):
        temporary = tempfile.TemporaryDirectory(prefix="mac plist ")
        self.addCleanup(temporary.cleanup)
        output = Path(temporary.name) / "Info.plist"
        script = Path(temporary.name) / "render.cmake"
        script.write_text(f'set(OPENFALLOUT_VERSION "0.1.0")\n'
                          f'configure_file("{ROOT / "files/mac" / template}" "{output}")\n')
        result = subprocess.run(["cmake", "-P", str(script)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with output.open("rb") as file:
            return plistlib.load(file)

    def test_game_plist_supports_game_mode(self):
        plist = self.render("openfallout-Info.plist.in")
        self.assertIs(plist.get("LSSupportsGameMode"), True)
        self.assertEqual(plist.get("LSApplicationCategoryType"), "public.app-category.role-playing-games")
        self.assertIs(plist.get("NSHighResolutionCapable"), True)
        self.assertEqual(plist.get("CFBundleShortVersionString"), "0.1.0")
        self.assertNotIn("LSRequiresCarbon", plist)

    def test_editor_plist_has_no_carbon_key(self):
        plist = self.render("openfallout-cs-Info.plist.in")
        self.assertNotIn("LSRequiresCarbon", plist)
        self.assertIs(plist.get("NSHighResolutionCapable"), True)


if __name__ == "__main__":
    unittest.main()

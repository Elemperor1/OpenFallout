"""Tests for the narrow documentation entrypoint link checker."""

from pathlib import Path
import tempfile
import unittest

import check_documentation


class DocumentationLinkTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "docs" / "guide").mkdir(parents=True)
        (self.root / "docs" / "source").mkdir(parents=True)
        (self.root / "docs" / "guide" / "topic.md").touch()
        (self.root / "docs" / "source" / "reference.rst").touch()

    def test_markdown_checks_relative_files_and_ignores_external_and_anchor_links(self):
        text = "[topic](topic.md#intro) [external](https://example.com) [self](#top)"
        self.assertEqual(
            check_documentation.check_markdown_links(
                text, Path("docs/guide/index.md"), self.root),
            [],
        )

    def test_markdown_reports_missing_relative_target(self):
        errors = check_documentation.check_markdown_links(
            "[missing](missing.md)", Path("docs/guide/index.md"), self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("missing link target missing.md", errors[0])

    def test_repository_rst_links_require_canonical_existing_target(self):
        text = (
            "`topic <https://github.com/Elemperor1/OpenFallout/blob/master/"
            "docs/guide/topic.md>`_\n"
            "`missing <https://github.com/Elemperor1/OpenFallout/blob/master/"
            "docs/guide/missing.md>`_\n"
            "`wrong branch <https://github.com/Elemperor1/OpenFallout/blob/main/"
            "docs/guide/topic.md>`_"
        )
        errors = check_documentation.check_repo_blob_links(
            text, Path("docs/source/openfallout.rst"), self.root)
        self.assertEqual(len(errors), 2)
        self.assertIn("missing repository link target", errors[0])
        self.assertIn("non-canonical repository link", errors[1])

    def test_sphinx_doc_roles_and_toctree_resolve_rst_files(self):
        text = """See :doc:`Reference <reference>`.

.. toctree::
   :maxdepth: 2

   reference
"""
        self.assertEqual(
            check_documentation.check_sphinx_targets(
                text, Path("docs/source/index.rst"), self.root),
            [],
        )

    def test_sphinx_reports_missing_doc_role_and_toctree_entries(self):
        text = """See :doc:`missing`.

.. toctree::

   absent
"""
        errors = check_documentation.check_sphinx_targets(
            text, Path("docs/source/index.rst"), self.root)
        self.assertEqual(len(errors), 2)
        self.assertTrue(all("missing Sphinx document" in error for error in errors))


if __name__ == "__main__":
    unittest.main()

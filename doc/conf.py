# -*- coding: utf-8 -*-

from sys import path as sys_path
from os.path import abspath
from pathlib import Path
from json import loads


ROOT = Path(__file__).resolve().parent

sys_path.insert(0, abspath("."))


from data import (
    ReadBoardDataFromYAML,
    BoardDataToTable,
    ReadFPGADataFromYAML,
    FPGADataToTable,
    ReadCableDataFromYAML,
    CableDataToTable
)

# -- General configuration ------------------------------------------------

extensions = [
    # Standard Sphinx extensions
    "sphinx.ext.extlinks",
    "sphinx.ext.intersphinx",
]

templates_path = ["_templates"]

source_suffix = {
    ".rst": "restructuredtext",
}

master_doc = "index"
project = u"openFPGALoader: universal utility for programming FPGA"
copyright = u"2019-2022, Gwenhael Goavec-Merou and contributors"
author = u"Gwenhael Goavec-Merou and contributors"

version = "latest"
release = version  # The full version, including alpha/beta/rc tags.

language = None

exclude_patterns = []

numfig = True

# -- Options for HTML output ----------------------------------------------

html_context = {}
ctx = ROOT / "context.json"
if ctx.is_file():
    html_context.update(loads(ctx.open("r").read()))

if (ROOT / "_theme").is_dir():
    html_theme_path = ["."]
    html_theme = "_theme"
    html_theme_options = {
        "home_breadcrumbs": False,
        "vcs_pageview_mode": "blob",
    }
else:
    html_theme = "alabaster"

htmlhelp_basename = "OFLDoc"

# -- Options for LaTeX output ---------------------------------------------

latex_elements = {
    "papersize": "a4paper",
}

latex_documents = [
    (master_doc, "OFLDoc.tex", u"openFPGALoader: universal utility for programming FPGA (Documentation)", author, "manual"),
]

# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [(master_doc, "openFPGALoader", u"Universal utility for programming FPGA (Documentation)", [author], 1)]

# -- Options for Texinfo output -------------------------------------------

texinfo_documents = [
    (
        master_doc,
        "openFPGALoader",
        u"Universal utility for programming FPGA (Documentation)",
        author,
        "openFPGALoader",
        "HDL verification.",
        "Miscellaneous",
    ),
]

# -- Sphinx.Ext.InterSphinx -----------------------------------------------

intersphinx_mapping = {
    "python":      ("https://docs.python.org/3/", None),
    "constraints": ("https://hdl.github.io/constraints", None)
}

# -- Sphinx.Ext.ExtLinks --------------------------------------------------

extlinks = {
    "wikipedia": ("https://en.wikipedia.org/wiki/%s", None),
    "ghsharp": ("https://github.com/trabucayre/openFPGALoader/issues/%s", "#"),
    "ghissue": ("https://github.com/trabucayre/openFPGALoader/issues/%s", "issue #"),
    "ghpull": ("https://github.com/trabucayre/openFPGALoader/pull/%s", "pull request #"),
    "ghsrc": ("https://github.com/trabucayre/openFPGALoader/blob/master/%s", None),
}

# -- Generate partial board compatibility page (`board.inc`) with data from `boards.yml`

with (ROOT / "compatibility/boards.inc").open("w", encoding="utf-8") as wptr:
    wptr.write(BoardDataToTable(ReadBoardDataFromYAML()))

# -- Generate partial FPGA compatibility page (`fpga.inc`) with data from `FPGAs.yml`

with (ROOT / "compatibility/fpga.inc").open("w", encoding="utf-8") as wptr:
    wptr.write(FPGADataToTable(ReadFPGADataFromYAML()))
# -- Generate partial Cable compatibility page (`cable.inc`) with data from `cable.yml`

with (ROOT / "compatibility/cable.inc").open("w", encoding="utf-8") as wptr:
    wptr.write(CableDataToTable(ReadCableDataFromYAML()))

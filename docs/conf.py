# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'pyaseba'
copyright = '2026, Jerome Guzzi'
author = 'Jerome Guzzi'
release = '0.0.0'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.napoleon',
    'sphinx.ext.autodoc',
    'sphinx_tabs.tabs',
    'sphinx_gallery.gen_gallery',
    'sphinx.ext.intersphinx'
    # 'nbsphinx'
    ]

intersphinx_mapping = {
# 'python': ('https://docs.python.org/3', None),
'pyaseba': ('../..', "build/objects.inv")
}

sphinx_gallery_conf = {
    'examples_dirs': 'examples',
    'gallery_dirs': 'gallery',
    'capture_repr': ('_repr_html_', '__repr__'),
    'filename_pattern': r'/',
    'doc_module': ('pyaseba', ),
    'backreferences_dir': 'gen_modules/backreferences',
    'reference_url': {
        'pyaseba': None,
        # 'pyaseba.client': None,
    },
    'inspect_global_variables': True,
    'prefer_full_module': {r'.*'},
    'write_computation_times': False,
    'within_subsection_order': "FileNameSortKey",
    # 'prefer_full_module': {r'pyaseba\.client'},
    'log_level': {'backreference_missing': 'error'},
}

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# html_theme = 'alabaster'
html_theme = 'sphinx_book_theme'
html_static_path = ['_static']

html_theme_options = {
    "show_toc_level": 2,
    "show_nav_level": 2,
}

toc_object_entries_show_parents = 'hide'
toc_object_entries = True

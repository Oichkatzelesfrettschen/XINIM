import os
import sys

project = 'XINIM'
author = 'XINIM Developers'

extensions = ['breathe']

breathe_projects = {
    'XINIM': os.path.join('..', 'doxygen', 'xml')
}

breathe_default_project = 'XINIM'

html_theme = 'sphinx_rtd_theme'

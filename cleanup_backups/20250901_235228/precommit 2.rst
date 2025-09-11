Pre-commit Hook
===============

The repository provides a `pre-commit` script to automatically run
``clang-format`` on staged C++ files. Enable it by linking the script
into your Git hooks directory:

.. code-block:: bash

   ln -s ../../tools/pre-commit-clang-format.sh .git/hooks/pre-commit

This ensures consistent style before every commit.

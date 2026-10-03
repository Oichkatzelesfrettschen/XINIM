# Analysis semgrep

```text
/root/.local/lib/python3.12/site-packages/opentelemetry/instrumentation/dependencies.py:4: UserWarning: pkg_resources is deprecated as an API. See https://setuptools.pypa.io/en/latest/pkg_resources.html. The pkg_resources package is slated for removal as early as 2025-11-30. Refrain from using this package or pin to Setuptools<81.
  from pkg_resources import (
                  
                  
┌────────────────┐
│ 1 Code Finding │
└────────────────┘
                
    tools/passwd
   ❯❯❱ generic.secrets.security.detected-etc-shadow.detected-etc-shadow
          linux shadow file detected  
          Details: https://sg.run/4ylL
                                      
            1┆ root::0:0::/:/bin/sh

```


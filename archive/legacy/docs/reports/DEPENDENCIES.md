# Dependency Lifecycle

This document describes how external dependencies are tracked, updated, and deprecated.

## Update Policy
- Monitor upstream releases quarterly.
- Apply minor updates as soon as they are verified to be backward compatible.
- Schedule major version upgrades during planned release cycles.

## Deprecation Policy
- Mark dependencies for deprecation when upstream support ends or security issues arise.
- Provide at least one release cycle of notice before removing a deprecated dependency.
- Maintain migration guides to assist with replacement libraries or tools.

## Review Process
- Dependency status is reviewed during each milestone planning session.
- Changes are recorded in the project changelog and reflected in relevant build documentation.

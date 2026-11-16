<!--
IUT-Food pull requests: keep one logical change per PR.

Before opening the PR, please make sure:
- `ctest --test-dir build --output-on-failure` passes locally (all suites),
- new behaviour ships with tests (the suite list lives in CMakeLists.txt),
- protocol changes are reflected in docs/PROTOCOL.md,
- user-visible changes get a line in CHANGELOG.md,
- every touched file is also reflected in the release notes if it is one
  of the shipped artifacts.
-->

## Summary

<!-- One paragraph: what does this PR change and why? -->

## Checklist

- [ ] Tests added / updated (`ctest` green locally)
- [ ] Docs updated (README / PROTOCOL / CHANGELOG where relevant)
- [ ] No new compiler warnings on the Linux or Windows leg
- [ ] Screenshots regenerated if a UI window changed (`scripts/screenshot_driver.cpp`)

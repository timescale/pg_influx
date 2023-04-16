# Contribution Guidelines

If you're interested in contributing, there are several ways you can
contribute.

- Report a bug
- Propose a patch to fix a bug
- Propose a new feature
- Propose a fix to the documentation (not that there is much yet)

## Reporting a bug

Bugs can be reported here on GitHub, just click on the "Issue" tab and
then report a new bug there. There are currently no templates for
bugs, so please try to provide as much information as possible to
reproduce the bug.

It is not necessary to have a reproduction case to report a bug
(sometimes it is just not possible or easy to reproduce), but having
one will make it significantly easier to fix the bug.

## Proposing a patch

You can propose a change to the system by creating a pull request. The
pull request will then be reviewed and if approved, be merged into the
repository code.

We use the following tags for pull requests, which are used when
creating a release.

- **enhancement**: An enhancement to the code, either a new feature or
  an improvement to an existing feature.
- **bug**: A pull request that fixes a bug in the code.
- **breaking-change**: Pull request breaks existing usage by changing
  an API so that existing code will not work.
- **CI/CD**: Changes to the CI or CD pipeline.
- **build**: Changes to the build system that does not affect the code.
- **packaging**: Changes to the packaging part of the
  repository. Either adding packages for new build systems, or fixing
  issues in existing package build.
- **ignore-for-release**: Do not show this pull request in the release
  notes.

## Versioning guidelines

We follow the [semantic versioning scheme][semver] for versioning
releases. This means that given a version number `MAJOR.MINOR.PATCH`
of the extension:

- We change `MAJOR` version when we make an incompatible interface
  change, that is, if a function or other object is removed, or
  parameters to some existing function drastically change in such a
  manner that users have to change how they use the system.

- We change `MINOR` version when we add functionality in a backwards
  compatible manner. Typically this occurs when we add a function,
  column, or object to the extension. A minor version change should
  still allow existing interfaces to work with the new version.

- We change `PATCH` version when we make a backwards compatible bug
  fix. This typically means that there are no interface changes at
  all, not even addition of new parameters. 

## Commit message guidelines

To make working with the releases as simple as possible we use the,
[Conventional Commits Specification][convcommit]. There are several
tools that support this convention and it also makes maintaining the
code a lot easier. Note that the conventions are focused on JavaScript
development, so we take some liberties with the scope since we are not
using npm.

A commit message typically looks like this:

```
feat: support for spawning background workers on startup

Add support for spawning workers automatically on server
startup...

Closes #9
```

The `feat` is the type of the commit, which is then followed by a
short message describing the change. This will go into the release
notes, so please try to be clear about what the commit changes.

The body of the commit message contains a more elaborate message about
what changed at a level sufficient for a user to understand what it
does without having to read through the code. In particular, if new
options or functions are added, this is a good place to mention this.

The trailer can contain meta-information about the commit, such as
what issue it fixes or closes. Here typically [GitHub
keywords][gh-keyword] are added to link pull requests to issues.

We support the following types for the commit message:

- **build**: Changes that affect the build system or external dependencies
- **ci**: Changes to our CI configuration files and scripts
- **docs**: Documentation only changes
- **feat**: A new feature
- **fix**: A bug fix
- **perf**: A code change that improves performance
- **refactor**: A code change that neither fixes a bug nor adds a feature
- **style**: Changes that do not affect the meaning of the code (white-space, formatting, missing semi-colons, etc)
- **test**: Adding missing tests or correcting existing tests

We currently do not use scope for any of the types.

[convcommit]: https://www.conventionalcommits.org/en/v1.0.0/
[semver]: https://semver.org/
[gh-keyword]: https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue

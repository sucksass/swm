# swm Technical Reference Manual

**Document Classification:** Public
**Document Status:** Normative
**Document Version:** 1.0.0
**Revision Date:** See §14.1 (Revision History)
**Applicable Software Version:** swm, all versions, retroactively and prospectively, per §1.4
**Distribution:** Unlimited
**Supersedes:** Nothing. This is the first version. It has always been the first version.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Conformance Terminology](#2-conformance-terminology)
3. [Document Conventions](#3-document-conventions)
4. [Scope](#4-scope)
5. [Intended Audience](#5-intended-audience)
6. [System Requirements](#6-system-requirements)
7. [Installation Procedure](#7-installation-procedure)
8. [Invocation](#8-invocation)
9. [Formal Grammar of Program Invocation](#9-formal-grammar-of-program-invocation)
10. [Runtime Behavior](#10-runtime-behavior)
11. [Configuration](#11-configuration)
12. [Glossary](#12-glossary)
13. [Compliance Matrix](#13-compliance-matrix)
14. [Appendices](#14-appendices)
15. [Index](#15-index)

---

## 1. Introduction

### 1.1 Purpose of This Document

This document constitutes the authoritative technical reference for swm
(hereafter "the Software"), a window manager for the X Window System
(hereafter "X11," per §12.7). It is intended to describe, precisely and
without ambiguity, the observable behavior of the Software as installed
and executed under normal operating conditions, as distinguished from
abnormal operating conditions, which are out of scope per §4.2.

### 1.2 Relationship to Prior Art

This document does not describe, reference, quote, paraphrase, summarize,
allude to, or otherwise depend upon any other window manager's
documentation, source code, or man pages. Any structural resemblance
between this document's rigor and the apparent absence of rigor in
comparable minimalist window manager documentation elsewhere is a
consequence of independent editorial judgment exercised under §1.1, and
should not be construed as commentary.[^1]

[^1]: It is commentary.

### 1.3 Document Lineage

This is the first edition of this document. There is no prior edition.
References elsewhere in this document to "as previously documented" refer
to earlier sections of this same document, not to any external or prior
publication. See §14.1 for the complete (single-entry) revision history.

### 1.4 Applicability

This document applies to the Software as compiled from the source
distribution accompanying it, subject to the build-time configuration
described in §11. It does not apply to modified, forked, patched,
recompiled-with-different-flags, or otherwise derivative builds, except
insofar as those builds happen to behave identically, in which case this
document applies to them too, coincidentally.

---

## 2. Conformance Terminology

The key words **MUST**, **MUST NOT**, **SHALL**, **SHALL NOT**,
**SHOULD**, **SHOULD NOT**, **MAY**, and **OPTIONAL** in this document are
to be interpreted in the conventional sense established for technical
specifications of this kind, and are used throughout — including, where
appropriate, in contexts of no practical consequence — for the purpose of
maintaining consistent editorial tone.

For the avoidance of doubt:

- **MUST** indicates a mandatory requirement.
- **MUST NOT** indicates a mandatory prohibition.
- **SHOULD** indicates a recommendation, deviation from which is
  permitted but requires the deviator to have thought about it first.
- **MAY** indicates optionality, in the sense that the described behavior
  is not required, but is also not discouraged, and exists in a kind of
  permissive middle ground that this document declines to characterize
  further.

### 2.1 Example of Correct Terminology Usage

> "The window MUST have a title bar."

is incorrect, because the Software does not have title bars. The correct
formulation is:

> "The window has no title bar. This is not optional. There is no
> setting. Do not look for one."

---

## 3. Document Conventions

### 3.1 Typographic Conventions

Throughout this document, `monospace text` denotes literal command-line
input, file names, or source identifiers. *Italic text* denotes a term
being defined for the first time (see also §12, Glossary, for terms
defined for the second time, for emphasis).

### 3.2 Numbering Conventions

Section numbers follow a strict hierarchical decimal scheme
(§*n*.*n*.*n*...) with no theoretical upper bound on nesting depth. This
document does not currently exercise depth beyond four levels, as no
requirement has yet justified it. Should such a requirement arise, this
document reserves the right to become significantly harder to read.

### 3.3 Cross-Reference Conventions

Cross-references within this document take the form "(see §*x.y*)." Every
cross-reference in this document has been manually verified to point to a
section that exists, with the exception of any that have not been, which
do not exist, by definition, once discovered.

---

## 4. Scope

### 4.1 In Scope

This document covers: installation, invocation, documented runtime
behavior, and the subset of configuration currently exposed by the build
described in §11.

### 4.2 Out of Scope

This document does not cover: behavior under conditions not anticipated
by the authors, the internal control flow of the implementation, memory
layout, algorithmic complexity of any internal data structure, or
questions beginning with "why does it."

### 4.3 Explicit Non-Coverage of Frequently Asked Non-Topics

For clarity, and because past experience with comparable software
projects suggests these questions arise regardless of documentation
quality, this document explicitly declines to cover:

- Why the primary source file is still named `wewm.c` (see the project
  README, §"The Name," which does cover it, extensively, and with more
  editorializing than a technical reference document is prepared to
  offer).
- Whether the Software is a fork of anything (it is not; see
  `CLEANROOM.md`).
- Whether "swm" is a good name (opinions vary; see §12.4 for a neutral
  definition).

---

## 5. Intended Audience

This document is intended for readers who (a) have already compiled and
run the Software, (b) are attempting to determine whether a given
behavior they observed was intentional, and (c) have concluded, after
reflection, that consulting documentation is a more efficient use of time
than reading 3,500 lines of C to find out. This document validates that
conclusion.

Readers who have not yet compiled the Software should consult §7 first.
Readers who have compiled the Software and are currently on fire should
consult local emergency services first, and this document second.

---

## 6. System Requirements

### 6.1 Mandatory Dependencies

| Component | Requirement | Notes |
|---|---|---|
| Operating system | A Unix-like system with X11 | See §6.3 for the precise meaning of "Unix-like" as used in this table, which this document declines to provide, per §4.2. |
| Xlib | Present, linkable | Required for the Software to be a window manager rather than an aspiration. |
| Xft | Present, linkable | Required for text. Without it, the status bar exists but says nothing, which some reviewers have noted is thematically appropriate. |
| Fontconfig | Present, linkable | Required for Xft to locate fonts, which is in turn required for the status bar to say nothing *in a specific font*. |
| A C compiler | C11-conformant | Any conformant compiler is acceptable. This document takes no position on which one is best, a restraint some other projects in this space have not historically exercised. |

### 6.2 Optional Dependencies

| Component | Requirement | Effect if Absent |
|---|---|---|
| Xinerama | Present, linkable, compiled in | Multi-monitor support is available. If absent, the Software behaves as though exactly one monitor exists, which, for single-monitor users, is indistinguishable from the truth. |

### 6.3 Hardware Requirements

None specified. The Software has never been observed to require specific
hardware, and no test has been constructed to determine a lower bound.

---

## 7. Installation Procedure

### 7.1 Overview

Installation consists of two (2) steps, described in §7.2 and §7.3
respectively, and no others.

### 7.2 Step One: Build

```sh
make
```

This command **MUST** be executed from the root of the source
distribution. It **MUST NOT** be executed from any other directory,
unless that directory happens, through path resolution or symbolic
linking, to resolve to the root of the source distribution, in which case
it is, definitionally, being executed from the root of the source
distribution, and this clause does not apply.

**Preconditions:** The dependencies in §6.1 are present and discoverable
by the build system.

**Postconditions:** A binary named `swm` exists in the current working
directory. Its existence **MUST** be verified before proceeding to §7.3
(see §7.4 for the verification procedure).

### 7.3 Step Two: Install

```sh
make install
```

**Postconditions:** The binary is copied to `/usr/local/bin/swm`. The
manual page is copied to `/usr/local/share/man/man1/swm.1`. Neither
location is configurable through this document's documented interface
(see §11.6 for the boundary of what is and is not currently
configurable).

### 7.4 Verification of Build Success

To verify that §7.2 completed successfully, execute:

```sh
ls -l swm
```

If a file is listed, the build **SHOULD** be considered successful. If no
file is listed, the build **SHOULD** be considered unsuccessful. This
document assigns no numeric confidence value to either outcome, as none
was requested.

---

## 8. Invocation

### 8.1 Standard Invocation

```sh
swm
```

This is the complete and only supported invocation of the Software under
normal use. It accepts no arguments in the general case (see §9 for the
one documented exception).

### 8.2 Version Query

```sh
swm -v
```

Prints the version string and exits with status 0. This is currently the
only flag accepted by the Software. This document has been written to
accommodate the future addition of further flags without requiring
renumbering of this section, a foresight the authors are choosing to
highlight.

### 8.3 Exit Status

| Condition | Exit Status |
|---|---|
| Normal termination via configured quit action | 0 |
| Invocation with an unrecognized argument | 1 |
| Failure to open the X display | 1 |
| Detection of an already-running window manager | 1 (fatal, by design; see §10.4) |

---

## 9. Formal Grammar of Program Invocation

For completeness, the accepted command-line grammar is specified below
in Augmented Backus–Naur Form (ABNF, RFC 5234), notwithstanding that its
total language currently contains exactly two strings.

```abnf
invocation   = program-name [ SP argument ]
program-name = "swm"
argument     = version-flag
version-flag = "-v"
SP           = %x20
```

**Note:** This grammar is expected to grow. This document is not
expected to be updated promptly when it does. This is acknowledged here,
in advance, so that the eventual discrepancy is documented, if not
resolved.

---

## 10. Runtime Behavior

### 10.1 Startup Sequence (Descriptive, Non-Exhaustive)

Upon invocation, the Software, in order: connects to the X display;
verifies that no other window manager currently holds the substructure
redirect on the root window (see §10.4); establishes its internal state;
begins managing pre-existing windows, if any; and enters its main event
loop, from which it does not return until instructed to (see §11.7,
Quit Action).

### 10.2 Window Management Model

The Software manages windows using a tagged, tiled model. A window
belongs to one or more numbered tags. Exactly one tag-set is visible on
a given monitor at a given time (see §12.11 for the formal definition of
"tag-set," which is more precise than the informal one just given, and
which the informal one just given is not required to match exactly,
per §3.1).

### 10.3 Layouts

Two (2) layouts are provided: a tiled master/stack layout and a monocle
layout. Their precise geometric behavior is not restated here, on the
grounds that it is already documented exhaustively in this project's
internal specification documents, which are not currently published, and
whose non-publication is itself undocumented, a gap this document
acknowledges without resolving.

### 10.4 Single-Instance Enforcement

The Software **MUST NOT** run concurrently with another window manager
on the same X display. This is enforced by attempting to select for
substructure redirection on the root window at startup; if this fails,
the Software **MUST** terminate with a diagnostic message and exit status
1, per §8.3. This behavior is not configurable, adjustable, or subject to
an override flag, in this or any future version of this document, unless
a future version says otherwise, which would supersede this sentence, per
the normal operation of revision.

---

## 11. Configuration

### 11.1 Configuration Philosophy

Configuration of the Software is performed at compile time, through
modification of a header file, followed by recompilation. This is a
deliberate design decision and **MUST NOT** be interpreted as an
oversight, an in-progress feature, or a limitation pending future
resolution (see §14.1, which confirms this document has exactly one
revision, none of which relaxed this constraint).

### 11.2 Configuration File Location

```
config.def.h
```

### 11.3 Currently Consumed Configuration Values

The following table enumerates every configuration value currently read
by the Software at build time, and only those values. No value not
listed in this table has any effect on the Software's behavior,
regardless of whether it is defined, redefined, or fervently believed in.

| Symbol | Purpose |
|---|---|
| `WEWM_VERSION` | Version string, printed per §8.2 and used in the EWMH supporting-window name. |
| `WEWM_FONT_COUNT` | Number of fonts in the loaded font set. Currently one (1). |
| `WEWM_FONT_PRIMARY` | The one (1) font. |
| `WEWM_COLOR_FG` / `WEWM_COLOR_BG` / `WEWM_COLOR_BORDER` | Unselected-state colors. |
| `WEWM_COLOR_FG_SELECTED` / `WEWM_COLOR_BG_SELECTED` / `WEWM_COLOR_BORDER_SELECTED` | Selected-state colors. |
| `WEWM_CLIENT_BORDER_WIDTH` | Border width, in pixels, of managed windows. |
| `WEWM_DEFAULT_FLOATING` | Default floating state of newly managed windows absent a matching rule. |
| `WEWM_BAR_VISIBLE` | Initial bar visibility. |
| `WEWM_BAR_TOP` | Bar docking edge. |

### 11.4 Configuration Surfaces Not Yet Consumed

The following are defined as real internal data structures within the
Software, and are therefore *documentable*, but are not currently
populated by any configuration file, and therefore have no observable
effect. They are listed here in the interest of completeness, and to
forestall the filing of issues asking why they don't do anything:

- Tag labels and tag count (currently fixed internally; see §12.11).
- The layout selection table (currently fixed internally to exactly two
  entries; see §10.3).
- The window-placement rule table (currently empty; all windows are
  placed per §10.2's default behavior).
- The keyboard binding table (currently empty; the Software currently
  accepts no configured keyboard input whatsoever, a fact this document
  considers important enough to repeat in §11.5).
- The mouse binding table (currently empty, per the same reasoning).

### 11.5 A Note on Sections 11.4.4 and 11.4.5

At the time of this document's writing, the Software has no configured
keybindings and no configured mouse bindings. It can be started. It can
manage windows according to the rules in §10. It cannot, through any
documented input mechanism, be told to do anything else. This is not a
bug. It is also not, strictly, a feature. It is, per §3.1's definitions,
best classified as *current scope*, and readers are directed to the
project's issue tracker, changelog, or personal optimism for information
on when this may change.

### 11.6 Non-Configurable Behavior

For the avoidance of doubt, the following are hard-coded and are not,
under the current build, exposed through §11.2's configuration file:
installation path prefixes (see §7.3); the specific X atoms used for
EWMH/ICCCM compliance; the snap-distance and motion-event rate-limit
constants governing interactive window movement.

### 11.7 Quit Action

A quit action exists as a real, implemented, callable function within
the Software, satisfying the requirement in §10.1 that the event loop be
exitable. It is not currently bound to any input, per §11.4. It is
therefore, at present, reachable only by means external to this
document's scope (see §4.2).

---

## 12. Glossary

This glossary defines terms as used specifically within this document.
Where a term has a more general meaning in the field of window manager
design, that meaning is not repeated here, on the theory that a reader
who has reached §12 of this document already knows it.

**12.1 Window** — A window.

**12.2 Client** — A window that is being managed. See §12.1.

**12.3 Monitor** — A physical display output, or, in the absence of one,
a synthetic construct representing the entire screen, indistinguishable
from a physical display output for all documented purposes (see §6.2).

**12.4 swm** — The Software. Also: Static Window Manager (current, see
project README, §"The Name"). Also, previously: Worst Ever Window
Manager (historical, same section).

**12.5 wewm** — The former name of the Software, and, not coincidentally,
the current name of its primary source file (see §4.3).

**12.6 Tag** — A label, one of several, that a window may carry, used to
determine visibility per monitor. Not to be confused with an HTML tag, an
XML tag, a price tag, or a tag in the version-control sense, none of
which are discussed in this document.

**12.7 X11** — The X Window System, version 11. This document assumes
familiarity with X11 and does not define it further, which is one of the
few restraints this document consistently exercises.

**12.8 EWMH** — Extended Window Manager Hints. A specification this
project complies with to the extent documented in §10 and no further.

**12.9 ICCCM** — Inter-Client Communication Conventions Manual. See §12.8;
apply the same caveat.

**12.10 Bar** — The status bar. Currently says nothing by default,
per §6.1.

**12.11 Tag-set** — The set of tags currently visible on a given monitor.
Distinguished from §12.6 (a single tag) by the presence of the word
"set."

**12.12 Clean-Room Reconstruction** — The development methodology used
for this project. See `CLEANROOM.md` for full documentation of this
term's practical application, which this document, being about the
*software* rather than its *development history*, declines to repeat.

---

## 13. Compliance Matrix

The following matrix summarizes this document's own compliance with
itself, for the reader's convenience.

| Requirement | Status |
|---|---|
| Every section is numbered | ✅ Compliant |
| Every cross-reference resolves | ✅ Compliant, per §3.3 |
| Every configuration value is documented | ✅ Compliant, per §11.3 |
| Every *unconfigured* surface is also documented | ✅ Compliant, per §11.4 |
| Document contains a glossary | ✅ Compliant, per §12 |
| Document contains a formal grammar for a two-word command language | ✅ Compliant, per §9 |
| Document is, on balance, a proportionate response to the size of the underlying software | ❌ Non-compliant |

---

## 14. Appendices

### 14.1 Appendix A: Revision History

| Version | Date | Change |
|---|---|---|
| 1.0.0 | Current | Initial publication. |

### 14.2 Appendix B: Comparison to Historical Documentation Practice in This Software Category

Documentation for window managers in this general lineage has
historically favored brevity, on the theory that the source code is the
documentation and that anyone sufficiently motivated to run the software
is sufficiently motivated to read approximately 2,000 lines of C to
understand it. This document takes the position that both things can be
true at once: the source can remain the ultimate authority (see §1.2),
while a reader is not *required* to consult it merely to determine
whether `-v` prints a version string. Reasonable readers may disagree
about which approach is correct. This document does not resolve that
disagreement. It simply declines to participate in the brevity.

### 14.3 Appendix C: Intentionally Left Blank

This section is intentionally left blank, in the tradition of formal
specification documents that include such sections for pagination or
completeness purposes. Unlike those documents, this section's blankness
has no pagination purpose, as this is a Markdown file with no fixed page
boundaries. It is blank anyway. Consistency was deemed more important
than justification.

---

## 15. Index

*(This document is Markdown and does not paginate. An index of section
numbers is provided below in lieu of page numbers, which do not exist,
per §14.3.)*

Bar, §12.10 · Build, §7.2 · Clean-room, §12.12 · Client, §12.2 ·
Compliance, §13 · Configuration, §11 · EWMH, §12.8 · Exit status, §8.3 ·
Glossary, §12 · Grammar, §9 · ICCCM, §12.9 · Installation, §7 ·
Invocation, §8 · Keybindings (absence of), §11.5 · Layouts, §10.3 ·
Monitor, §12.3 · Quit action, §11.7 · Revision history, §14.1 ·
Scope, §4 · Single-instance enforcement, §10.4 · Tag, §12.6 ·
Tag-set, §12.11 · Version query, §8.2 · Window, §12.1.

---

*End of document. This document is complete. It will not be extended
further except where §11.5's caveats are eventually resolved, at which
point this document will be revised, and §14.1 will, for the first time,
contain a second row.*

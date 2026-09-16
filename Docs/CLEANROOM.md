# CLEANROOM.md

This document explains what "clean-room" actually meant for this project,
who did what, and what actually happened during the build, including the
parts that failed the first time. It's written for someone who wasn't in
any of the original conversations.

## The premise

Traditional clean-room reverse engineering uses two humans: one reads the
original implementation and writes a behavioral specification; a second,
who has never seen the original, implements from that spec alone. Neither
side ever crosses the wall. The point isn't secrecy for its own sake . it's
that the resulting spec-to-implementation chain is *auditable*: if the
final code ends up looking suspiciously like the original beyond what the
spec required, that's evidence something crossed the wall that shouldn't
have.

This project swapped the two humans for AI models and ran the same
process, mostly as an experiment in whether the discipline holds up when
neither side of the wall is a person. dwm was the target because it's
small (~2,200 lines), well-known, and its behavior is dense enough that a
sloppy spec would actually get tested.

One honest caveat up front: dwm is MIT-licensed, so there was never a
*legal* wall that needed building here . anyone can read, copy, or modify
the real source freely. This was purely a methodology exercise: could the
process itself be trusted, independent of whether it was strictly
necessary for this particular target.

## The three roles

**Descriptor** . reads the reference implementation (or, later, reasons
about required behavior in the abstract) and writes a specification
describing *what* a component must do, deliberately omitting *how* the
reference achieves it. A good spec in this process describes externally
observable behavior, public interfaces that other components depend on,
and invariants that must hold . and explicitly calls out which internal
mechanisms are left to the Implementer's discretion.

**Implementer** . receives only the specification, never the reference
source, and never any conversation history describing the reference
source. Writes code that satisfies the spec by whatever internal means it
chooses. A different chat session (in some cases a different model
entirely) was used for this role each time, specifically so it had no
memory of what the reference implementation looked like.

**Reviewer** . receives the specification and the Implementer's
submission, and checks two separate things that are easy to conflate but
matter differently:
1. Does it actually satisfy the spec? (an ordinary code review question)
2. Does it contain anything that couldn't plausibly have been derived from
   the spec alone . implementation-specific constants, data structures,
   variable-naming patterns, or control-flow shapes that match the
   reference too closely to be coincidence? (the actual clean-room
   question)

A submission can pass (1) and fail (2), which is exactly what happened
once, described below.

## What counted as evidence

The goal was not to make the final source merely look different from dwm. The useful evidence was the chain connecting requirements to implementation.

For this project, that meant keeping track of:

- what behavior was required;
- what implementation details were deliberately left open;
- which implementation was produced from the specification;
- what reviewers found;
- what was rejected or rewritten;
- and where the process itself had gaps.

A clean-room process is only as convincing as that trail. A different-looking implementation without a documented trail is still just a different-looking implementation.

The same standard applies to the uncomfortable parts. If a submission contains something that cannot be explained from the specification, it is more useful to record and investigate it than to quietly explain it away after the fact.

## What actually happened, in order

**1. Behavioral spec, full file.** The Descriptor read the real `dwm.c`
and `drw.c` and produced a complete behavioral specification: data model,
event dispatch, layout algorithms, client lifecycle, input handling, bar
rendering, multi-monitor behavior . described in terms of what happens,
not how the reference code was structured. No variable names, no code
snippets, no line-by-line structure.

**2. First `drw.c` attempt . rejected.** An Implementer produced a font-
rendering/text-drawing module from a spec that described bar-rendering
*behavior* but said nothing about the internal font-fallback caching
mechanism. The submission included a hash-based negative-result cache
using specific magic constants (`0x21F0AAAD`, `0xD35A2D97`) and a
two-slot lookup scheme. On review, those exact constants turned out to be
verbatim from the real upstream dwm/dmenu `drw.c` . not something
derivable from a behavioral description. Variable naming (`nomatches`,
`h0`/`h1`, `ellipsis_width`) matched the reference closely enough to look
like a renamed paraphrase rather than independent design.

This was the key finding of the whole experiment: the algorithm choice
itself was never the problem (an independently-invented caching scheme
would have been completely fine) . the problem was implementation detail
appearing in a submission that no spec had licensed it to know.

**3. Respecification.** The spec for that module was rewritten to lock
down only the actual interop boundary (the public function signatures
other code depends on) and to explicitly enumerate what was *not*
required: no specific cache data structure, no hash function, no
internal variable names or control flow. The rewritten spec went further
than just omitting detail . it stated outright that reproducing any
reference-specific mechanism would be treated as evidence of leakage, not
as a correct answer.

**4. Second attempt . accepted, then refined twice more.** A fresh
Implementer session produced a font module using a completely different
data structure for the same requirement (a growable per-context linked
list of failed code points, rather than a fixed hash cache) . genuinely
different design solving the same spec. Review caught two small, ordinary
bugs unrelated to clean-room concerns (a measurement/render width
mismatch for the ellipsis string, and a memory leak on font-set
replacement); both were fixed cleanly in later passes and the module was
frozen.

**5. Data model, then everything else.** The core struct/global-state
spec was written next (field-level shape and cross-cutting invariants
only, no struct layout requirements, since this file has no external
callers dictating an ABI). An Implementer built the data model, then .
across a process gap where specs for several remaining components were
produced without the original Reviewer session in the loop . a large
completion pass covering monitor geometry, client lifecycle, EWMH/ICCCM
handling, and startup arrived for review essentially all at once.

**6. The process gap, and what it teaches.** That completion pass turned
out to be well-built on inspection (invariant checks beyond what the spec
required, a genuinely different monitor-reconciliation algorithm than the
reference, correct X-resource ownership throughout) . but it couldn't be
properly *audited* for clean-room purposes, because no reviewed
specification existed yet for most of what it implemented. This is worth
stating plainly rather than glossing over: the method only produces a real
audit trail when review happens at every spec-to-implementation
handoff. Skip a handoff and you get code that might be perfectly clean,
but you can no longer *prove* it, which defeats the point of doing this at
all. The honest fix was catching up the specs before trusting the code
that had gotten ahead of them, not retroactively writing specs to match
what already existed (which would just be rationalizing after the fact).

**7. Completion.** A final consolidated specification covered everything
remaining . focus management, fullscreen transitions, ICCCM protocol
messaging, layout algorithms, the full event dispatch loop and its 14
handlers, all input actions, bar creation and drawing, and shutdown .
written against the field and function names already committed by earlier
accepted components, so the Implementer could extend the existing file
rather than starting over. The result was reviewed function-by-function
against that spec. One real bug was found (`UnmapNotify` was never wired
into the event dispatch table, meaning a withdrawn-but-not-destroyed
window would leak as a phantom client) and needed a fix; everything else
held up.

## What this did and didn't prove

It did show that the failure mode this process exists to catch . an
Implementer leaking reference-specific internals through an
underspecified spec . is real and catchable, at least in the one case
where it happened here. It also showed that the audit only works if
nobody skips the review gate, and that skipping it even once (even with
good intentions, even when the resulting code turns out fine) breaks the
chain of evidence, not just the vibes.

It didn't prove the resulting code is bug-free (see: the `UnmapNotify`
gap, caught only because someone happened to check for it explicitly), and
it didn't prove anything about performance, security, or production
readiness. It also isn't a rigorous study . one target, an inconsistent
number of models and sessions involved, no control group, and at least
one process gap partway through. Take the conclusions as "this seems to
work when followed" rather than as a validated methodology.

## What to inspect if you want to verify it

The most useful way to evaluate the claim is to inspect the artifacts rather than take the README on faith.

Start with the behavioral specifications, then compare them with the corresponding implementation and review notes. In particular, the rejected `drw.c` attempt is an important artifact because it shows the process detecting a real reference-specific leak instead of presenting a perfectly clean story after the fact.

The process also documents its own limitation: the completion-pass gap means not every intermediate handoff has the same evidentiary strength. That is part of the record, not something being hidden from it.

If a future change introduces a new component, the intended process is straightforward: specify the behavior first, implement from that specification, review the result, record any rejection or refinement, and only then treat the component as part of the audited chain.

## Why this exists as a separate file

The main README is a parody of suckless's own tone and doesn't pretend to
be a serious engineering writeup. This document is the serious version,
for anyone who actually wants to know what was checked and how, or wants
to argue that something here isn't as clean as claimed. That argument is
welcome . the whole point of writing this down is so it can be checked.

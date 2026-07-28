# Social posts for the blocker survey

These drafts announce the roadmap blocker survey and the one-at-a-time
execution plan for the ray-tracing tet-cage reimplementation. They keep the
project framed as proof-driven work rather than a demo or performance claim.

## LinkedIn

I just finished a full blocker survey for my ray-tracing tet-cage
reimplementation work and turned it into a one-by-one execution plan.

What I wanted at this stage was not a vague “next steps” list, but a document
that forces me to say, clearly, which roadmap items are still open, which ones
are blocked, why they are blocked, and what evidence would actually unblock
them.

That distinction matters a lot in this kind of work. Some milestones are
blocked because the implementation is still wrong. Some are blocked because I
don’t yet have the right hardware. Some are blocked by source access, renderer
integration constraints, or licensing realities. Those are very different
problems, and it helps to separate them instead of flattening everything into
“in progress.”

The resulting plan breaks the remaining work into concrete packages: expanding
the portable truth corpus, closing Metal correctness gaps, qualifying an
NVIDIA/Vulkan host, implementing the Vulkan path, deciding whether
CUDA/Vulkan interop is worth keeping, comparing robustness methods, running the
real scale study, validating authoring, and then moving into Cycles, UE5,
RenderMan, and production hardening.

One thing I’m trying to stay disciplined about is not treating a small demo as
proof. If a milestone requires representative animated assets, renderer-visible
correctness, or real hardware behavior, I want that written down explicitly so
I can’t hand-wave past it later.

That feels like real progress to me: not just more code, but a roadmap that’s
much harder to lie to.

## X

I finished a full blocker survey for the ray-tracing tet-cage reimplementation
and turned it into a one-at-a-time execution plan.

Not just “what’s left,” but:

- what’s actually open
- what’s blocked
- why it’s blocked
- what evidence would unblock it

That feels way more useful than a vague backlog.

A big part of the exercise was separating different kinds of blockers:

- implementation/correctness problems
- missing hardware
- missing source access
- renderer/API constraints
- licensing/vendor gates

Those are not the same thing, and treating them like they are makes roadmap
work much fuzzier than it needs to be.

The remaining work now breaks down into concrete packages: portable truth
corpus, Metal correctness, NVIDIA/Vulkan qualification, Vulkan backend, interop
keep/remove decision, robustness comparison, scale study, authoring validation,
Cycles, UE5, RenderMan, and production hardening.

Trying to keep this project honest by making every later claim rest on earlier
proof.

## Reddit

I just finished a blocker survey for a ray-tracing tet-cage reimplementation
project I’ve been working on, and it was one of those “the planning artifact is
actually the progress” moments.

What I needed at this point was not another generic to-do list. I needed
something that forced me to answer, milestone by milestone:

- what is genuinely complete
- what is only partially proven
- what is blocked
- what it depends on
- what evidence would count as closure

That ended up being surprisingly clarifying.

A lot of the remaining work looks similar from far away, but up close it’s
really different categories of problems. Some things are blocked because the
implementation still has correctness gaps. Some are blocked because I don’t
have the right NVIDIA/Vulkan host yet. Some are blocked by source access for
renderer integrations. Some are blocked by whether a public API or licensed
runtime even exposes the right hook. If you don’t separate those, everything
just becomes one big blob of “still working on it.”

The new plan turns the rest of the roadmap into concrete work packages: expand
the portable truth corpus, close Metal correctness, qualify the NVIDIA/Vulkan
environment, implement the Vulkan path, decide whether CUDA/Vulkan interop is
even worth keeping, compare robustness methods, do the real scale/crossover
study, validate authoring on representative assets, then tackle Cycles, UE5,
RenderMan, and production hardening.

The part I care about most is that it explicitly resists fake completion. A
tiny synthetic demo is useful, but it’s not the same thing as representative
animated content, real renderer-visible correctness, or stable behavior on
actual target hardware. Writing that down sounds obvious, but it changes how
you evaluate progress.

Anyway, I’m trying to make the project harder to fool myself about. That seems
like the right kind of momentum at this stage.

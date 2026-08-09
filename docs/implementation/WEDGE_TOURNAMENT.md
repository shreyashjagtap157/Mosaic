# Wedge Tournament Protocol (M4)

Status: protocol draft; gate values are ADR-promotable after the first measurement cycle.

## Purpose

Select the first major product investment using measurement rather than architectural preference.

## Candidates

A. Multilingual LLM preprocessing  
B. Compiler + LLM developer tooling  
C. High-assurance token-processing SDK  
H. Hybrid, eligible only after constituent wedges independently qualify

`NONE` is a valid result.

## Execution model

- Freeze the common M3 substrate for tournament purposes.
- Implement only thin wedge adapters.
- Run candidates in parallel.
- Suggested wedge-specific budget: <=8 engineer-weeks each.
- Suggested global duration: 6-8 weeks for 4-6 engineers; 8-12 weeks for two.
- Engineering effort is itself a scored/adoption-cost measurement.

## Fatal qualification gates

### A: multilingual LLM

- exact reconstruction: 100%;
- unknown input: 0;
- mean non-English fertility gain: starting floor >=8% against strong comparable baseline;
- worst evaluated language token-count regression <=2%;
- IDs-only throughput >=70% semantics-equivalent baseline;
- English token-count regression <=2%;
- downstream benchmark regression <=0.5 percentage point unless statistically insignificant.

### B: compiler + LLM

- declared lexical compatibility: 100% on reference corpus;
- model-ID compatibility: 100% in compatibility mode;
- source-offset disagreement: 0;
- exact reconstruction: 100%;
- shared-pipeline CPU reduction: starting floor >=10%;
- temporary memory/allocation reduction: starting floor >=15%.

### C: assurance SDK

- arbitrary-byte reconstruction: 100%;
- streaming/full equivalence: 100% for supported features;
- malformed-pack fail-closed result: 100% tournament corpus;
- user text activating privileged controls: 0;
- unbounded ordinary-pack execution: 0;
- cross-platform canonical divergence: 0;
- thin tournament scope measures fail-closed/integration properties, not a feature-complete security product.

## Selection

1. Reject candidates failing a fatal gate.
2. Compare survivors on a Pareto frontier across measured benefit, engineering cost, adoption friction, differentiation, platform leverage, and commercial opportunity.
3. Use a predeclared weighted score only if the Pareto comparison does not produce a practical decision.
4. Record rejected alternatives and uncertainty.

## Hybrid eligibility

A true hybrid requires:

- each constituent wedge independently passes;
- >=70% shared implementation after M3;
- <=30% incremental engineering cost over stronger single wedge;
- >=15% combined measurable product value beyond primary-only configuration;
- no conflicting architecture requirements;
- team capacity supports release scope.

A primary wedge that simply inherits core assurance properties is PRIMARY+SECONDARY, not automatically a hybrid.

## Output

Produce `WEDGE-EVAL-001.md` plus machine-readable raw results containing exact commit, corpus, pack, manifest, baseline, hardware, and build identities.

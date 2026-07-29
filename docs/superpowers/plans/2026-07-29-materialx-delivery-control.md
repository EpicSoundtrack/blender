# MaterialX Delivery Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the 802-node parity program measurable through a validated ledger and deterministic reports.

**Architecture:** A standard-library Python module consumes the existing catalog and a checked-in JSON state file. It validates every row and generates counts; source references are not evidence.

**Tech Stack:** Python standard library, `tools/materialx/materialx_catalog.py`, unittest.

## Global Constraints

- Preserve MaterialX semantics; use explicit rejection instead of approximation.
- Full parity requires GPU and golden-review evidence.
- Do not block node implementation on control-plane work.

### Task 1: Ledger and validator

**Files:** Create `tools/materialx/materialx_delivery_ledger.py`, `tools/materialx/materialx_delivery_ledger.json`, and `tools/materialx/test_materialx_delivery_ledger.py`.

- [ ] Write failing tests for deterministic 802-row creation, missing fields, duplicate/unknown IDs, and summary totals.
- [ ] Implement standard-library JSON loading, validation, row generation, and summary output.
- [ ] Run `python -m unittest tools.materialx.test_materialx_delivery_ledger -v` and require green output.

### Task 2: Report and evidence integration

**Files:** Modify `tools/materialx/materialx_delivery_ledger.py`, `tools/materialx/materialx_delivery_ledger.json`, and `doc/materialx_project_operations.md`.

- [ ] Add `--summary` deterministic JSON output.
- [ ] Enter only current, exact focused-test evidence; leave unverified forms unclassified.
- [ ] Require every implementation batch and status update to use the generated counts.

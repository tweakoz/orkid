#!/usr/bin/env ork.python
################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################

"""
Tests for CORE and CATALOG subsystem initialization.

Usage:
  ./test_subsystem_core.py                  # catalog disabled (default)
  ./test_subsystem_core.py --enable-catalog # catalog enabled
"""

import argparse
import sys
from orkengine import core
from orkengine.core import CrcStringProxy

tokens = CrcStringProxy()
results = []

def test(name, condition):
    results.append(condition)
    print(f"  {'PASS' if condition else 'FAIL'}: {name}")

def main():
    parser = argparse.ArgumentParser(description='Test CORE/CATALOG subsystem initialization')
    parser.add_argument('--enable-catalog', action='store_true', default=False,
                        help='Enable the CATALOG subsystem (default: disabled)')
    args = parser.parse_args()

    print(f"Creating Application (std_asset_catalog={args.enable_catalog})")
    app = core.Application.create(std_asset_catalog=args.enable_catalog)

    opq = app.getSubsystem("opq")
    core_sub = app.getSubsystem("core")
    catalog = app.getSubsystem("catalog")

    print("\n[OPQ Subsystem]")
    test("OPQ registered", opq is not None)
    test("OPQ in READY state", opq and opq.currentState() == opq.state_ready)

    print("\n[CORE Subsystem]")
    test("CORE registered", core_sub is not None)
    test("CORE in READY state", core_sub and core_sub.currentState() == core_sub.state_ready)
    test("CORE depends on OPQ", core_sub and core_sub.hasDependency(tokens.opq))

    print("\n[CATALOG Subsystem]")
    if args.enable_catalog:
        test("CATALOG registered", catalog is not None)
        test("CATALOG in READY state", catalog and catalog.currentState() == catalog.state_ready)
        test("CATALOG depends on OPQ", catalog and catalog.hasDependency(tokens.opq))
        test("CORE depends on CATALOG", core_sub and core_sub.hasDependency(tokens.catalog))
    else:
        test("CATALOG not registered (disabled)", catalog is None)
        test("CORE does not depend on CATALOG", core_sub and not core_sub.hasDependency(tokens.catalog))

    print("\n[OPQ Queues]")
    test("mainq available", app.mainq is not None)
    test("updq available", app.updq is not None)
    test("conq available", app.conq is not None)

    passed, failed = results.count(True), results.count(False)
    print(f"\n{'='*40}")
    print(f"Results: {passed} passed, {failed} failed")
    return 0 if failed == 0 else 1

if __name__ == '__main__':
    sys.exit(main())

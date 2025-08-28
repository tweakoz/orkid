#!/usr/bin/env ork.python 

from orkengine import core
import os
import obt.deco

deco = obt.deco.Deco()

core.coreappinit()
catalog = core.AssetCatalog.instance

# Get all namespaces
try:
    all_namespaces = catalog.list_namespaces("*")
    
    if not all_namespaces:
        print(deco.red("No namespaces found in catalog."))
    else:
        print(deco.yellow(f"Found {len(all_namespaces)} namespaces:") + "\n")
        for ns in sorted(all_namespaces):
            print(deco.cyan(ns))
except Exception as e:
    print(deco.red(f"Error listing namespaces: {e}"))

core.coreappexit()
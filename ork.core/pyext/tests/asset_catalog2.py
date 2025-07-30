#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path

ddir = ork_path.data/"cdntest"
# Initialize core
core.coreappinit()

cfg = core.AssetConfig.loadFromFile(ddir/"config.json")

print("AssetConfig", cfg)
print("AssetConfig namespace_keys:", cfg.namespace_keys)
print("AssetConfig locations:", cfg.locations)
print("AssetConfig destinations:", cfg.destinations)




cat = core.AssetCatalog(cfg)
cat.load_manifest(ddir/"catalog.json")
ns_list = cat.list_namespaces()
print("Namespaces in catalog:", ns_list)
ns = cat.find_namespace("cdntest")
a = cat.list_assets_in_namespace(ns.id)
i = cat.get_asset_info(a[0])
print(cat, ns, a, i)
print("asset.id", i.id)
print("asset.fqid", i.fqid)
print("asset.type", i.type)
print("asset.ns", i.namespace)
print("asset.nsid", i.namespace.id)
print("asset.priority", i.priority)
print("asset.merge", i.merge)

print("asset.local_loc", i.local_loc)
print("asset.remote_loc", i.remote_loc)
print("asset.filename", i.filename)
print("asset.relative_path", i.relative_path)
print("asset.size", i.size)
print("asset.storage_hash", i.storage_hash)
print("asset.content_hash", i.content_hash)
print("asset.hash_algorithm", i.hash_algorithm)
print("asset.platforms", i.platforms)
print("asset.is_chunked", i.is_chunked)

print("ALLFQIDS:")
print( cat.all_fqids )
req = cat.get(i.fqid)
print( req )
core.coreappexit()

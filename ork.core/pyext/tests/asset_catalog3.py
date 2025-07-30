#!/usr/bin/env ork.python

from orkengine import core
from ork import path as ork_path

ddir = ork_path.data/"cdntest"
ddir2 = ork_path.data/"cdntest2"
# Initialize core
core.coreappinit()

##################################
# AssetConfigSpace : Configuration Space
#  manages full composite configuration
##################################

cfgspc = core.AssetConfigSpace.loadGlobalConfigs()
cfg1 = cfgspc.createConfig( 
         # uuid assigned automatically
         id="test1",
         file=ddir/"config.json"
         ) # individual config
cfg2 = cfgspc.createConfig(
         # uuid assigned automatically
         id="test2", 
         file=ddir2/"config2.json") # individual config

cfg1.addNamespaceKey(id="cdntest", key="api_key")
cfg1.addRemoteLocation(id="cdntest_remote", loc="https://localhost:8443")
cfg1.addLocalLocation(id="stage", loc="<stage>") # stage is built-in immutable location -> points to Path::stage()
cfg1.addLocalLocation(id="cache", loc="<assetcache>") # assetcache is built-in immutable location -> points to Path::stage()/"assetcache"

as_json = cfg1.toJson()
print("AssetConfigDB JSON:", as_json)
#{
#  id: "my_config_1",
#  uuid: "12345678-1234-5678-9abc-123456789abc",
#  "namespace_keys": {
#    "cdntest": "api_key"
#  },
#  "remotes": {
#    "cdntest_remote": "https://localhost:8443"
#  },
#  "locals": {
#    "stage": "<stage>",
#    "cache": "<assetcache>"
#  }
#}

cfgspc.writeToDisk() # writes all configs to disk (as json)

pth1 = ork_path.data/"cdntest/config.json"
pth2 = ork_path.data/"cdntest2/config2.json"
pthlist = [pth1, pth2] 
cfgspc2 = core.AssetConfigSpace.loadFromDisk(pthlist) # loads all configs from disk into one space

cat = core.AssetCatalog(space=cfgspc)

m1 = cat.createManifest( 
    # uuid assigned automatically
    id="cdntest",
    version="1.0.0",
    namespace="cdntest",
    file = ddir/"catalog.json")

a1 = m1.createAsset(
    # uuid assigned automatically
    id="user_id",
    priority=100,
    type="text",
    remote = "<cdntest_remote>/my_remote_asset_dir",
    local = "<cache>/my_local_asset_dir",
    filename="my_asset_file.txt",
    platforms=["mac", "linux"],
    dependencies = ["fqid1", "fqid2"]  # optional dependencies
)

# at this point, asset already packaged, chunked, etc ... 

as_json = a1.toJson()
#{
#  id: "user_id",
#  uuid: "12345678-1234-5678-9abc-999456789abc",
#  namespace: "cdntest"     
#  type: "text",
#  priority: 100,
#  remote: "<cdntest_remote>/my_remote_asset_dir",
#  local: "<cache>/my_local_asset_dir",
#  filename: "my_asset_file.txt",
#  platforms: ["mac", "linux"],
#  dependencies: ["fqid1", "fqid2"],
#  content_hash: "computed_md5_hash_here",  # computed on asset content   
#  storage_hash: "computed_md5_hash_here",  # computed on asset storage
#  hash_algorithm: "md5",                   # algorithm used for hashing
#  native_size: 123456,                     # size in bytes
#  compressed_size: 123456,                 # size in bytes (if compressed)
#  chunks: {                                # chunked assets only
#    "chunk1": {                            # chunk ID
#    }
#  }
#}

as_json = m1.toJson()
#{
#  id: "my_manifest_1",
#  uuid: "12345678-1234-5678-9abc-999456789abc",
#  namespace: "cdntest"
#  version: "1.0.0",
#  "assets": {
#    "user_id": {
#      "type": "text",
#      "priority": 100,
#      "remote": "<cdntest_remote>/my_remote_asset_dir",
#      "local": "<cache>/my_local_asset_dir",
#      "filename": "my_asset_file.txt",
#      "platforms": ["mac", "linux"],
#      "dependencies": ["fqid1", "fqid2"],
#      "content_hash": "computed_md5_hash_here",
#      "storate_hash": "computed_md5_hash_here",
#      "hash_algorithm": "md5",
#      "native_size": 123456, <- size in bytes
#      "compressed_size": 123456, <- size in bytes
#      "chunks": { <- chunked assets only
#      }
#    }
#  }
#}

a1.repackage() # repackages the asset, rechunks, updates content_hash, storage_hash, etc
a1.upload() # uploads the asset to remote location
m1.repackage() # repackages all assets in manifest
m1.upload() # uploads all assets in manifest to remote locations

cat.repackage() # repackages all manifests in catalog

cat.writeToDisk() # writes whole catalog to disk
                  # basically write all manifests jsons 

cat.upload() # uploads all assets in catalog to remote locations

jsons = cat.toJson() # dumps catalog as dictionary of manifest_path -> manifest_json

cat.download() # downloads all assets in catalog from remote locations

core.coreappexit()

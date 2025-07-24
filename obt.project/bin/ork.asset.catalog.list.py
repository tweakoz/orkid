#!/usr/bin/env ork.python 

from orkengine import core

core.coreappinit()
fetcher = core.AssetFetcher()
fetcher.reload()

a = fetcher.assets

for asset_item in a:
  print(a)
    
    
core.coreappexit()
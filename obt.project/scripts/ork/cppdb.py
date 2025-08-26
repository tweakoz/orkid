#!/usr/bin/env python3
"""
Orkid-specific C++ database functionality
Wrapper around obt.cpp_db_builder with Orkid project structure
"""

from pathlib import Path
from ork import path as ork_path
from obt import path as obt_path
from obt.cpp_db_builder import CppDatabaseBuilder
from ork.cppsearch import get_orkid_paths

def get_orkid_db_path(project_name: str = "orkid", modules: list = None) -> Path:
    """Get database path for Orkid project"""
    if modules:
        module_suffix = "_" + "_".join(sorted(modules))
    else:
        module_suffix = ""
    db_name = f"{project_name}_cpp{module_suffix}.db"
    return Path(obt_path.stage()) / db_name

def create_orkid_db_builder(project_name: str = "orkid") -> CppDatabaseBuilder:
    """Create database builder for Orkid project"""
    db_path = get_orkid_db_path(project_name)
    
    # Create wrapper for get_orkid_paths that matches expected signature
    def get_paths_wrapper(specific_dirs=None, module_names=None):
        return get_orkid_paths(
            specific_dirs=specific_dirs,
            module_names=module_names, 
            orkid_root=ork_path.root
        )
    
    return CppDatabaseBuilder(
        db_path=db_path,
        project_name=project_name,
        get_paths_func=get_paths_wrapper
    )

class CppDatabase:
    """Convenience wrapper for Orkid C++ database operations"""
    
    def __init__(self, project_name: str = "orkid"):
        self.project_name = project_name
        self.builder = create_orkid_db_builder(project_name)
        
    @property
    def db_path(self) -> Path:
        return self.builder.db_path
        
    @property
    def db(self):
        return self.builder.get_database()
        
    def exists(self) -> bool:
        return self.builder.exists()
        
    def build(self, modules=None, directories=None, paths=None, rebuild=False):
        return self.builder.build_database(modules, directories, paths, rebuild)
        
    def update(self, modules=None, directories=None, paths=None):
        return self.builder.update_database(modules, directories, paths)
        
    def get_stats(self):
        return self.builder.get_stats()
        
    def search_entities(self, **kwargs):
        return self.db.search_entities(**kwargs)
        
    def get_entity_members(self, entity_id, member_type=None):
        return self.db.get_entity_members(entity_id, member_type)
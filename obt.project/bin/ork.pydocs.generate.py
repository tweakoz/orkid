#!/usr/bin/env python3

import os, sys, subprocess, pydoc, re
from obt import path, command

# Constants for documentation generation
DOCS_DIR = path.stage()/"pydocs-orkengine"

command.run(["rm","-rf",DOCS_DIR])
command.run(["mkdir","-p",DOCS_DIR])

##########################################################
# Ensure the directories are set up
##########################################################

command.run(["rm", "-rf", DOCS_DIR])
command.run(["mkdir", "-p", DOCS_DIR])

##########################################################

def generate_docs(module_name, output_dir):
    """Generate HTML documentation for a specific module."""
    # Dynamically import the module
    __import__(module_name)
    module = sys.modules[module_name]

    # Use pydoc to generate the HTML documentation
    doc_html = pydoc.HTMLDoc().document(module)

    ##########################################################

    def replace_outside_quotes(text, target, replacement):
        # Regex to find target not inside quotes
        # Negative lookbehind and lookahead to ensure target is not surrounded by quotes
        # This regex does not account for escaped quotes within quoted strings
        pattern = rf'(?<!["\'])\b{re.escape(target)}\b(?![\'"])'
        
        # Function to replace target if not inside quotes
        def replace(match):
            # Check if the match is within quotes
            start_index = match.start()
            # Count quotes before the match
            quotes_before = text[:start_index].count('"') + text[:start_index].count("'")
            # If odd, match is within quotes; if even, it's outside
            if quotes_before % 2 == 0:
                return replacement
            else:
                return match.group(0)  # Return the original match if it's inside quotes
        
        # Replace all occurrences of target with replacement outside quotes
        X = re.sub(pattern, replace, text)
        X = X.replace("self: "+target,"self: ")
        X = X.replace("self:&nbsp;"+target,"self:&nbsp;")
        #
        X = X.replace("-&gt;&nbsp;"+target,"-&gt&nbsp;")
        for i in range(10):
          X = X.replace(f"arg{i}: "+target,f"arg{i}: ")
          X = X.replace(f"arg{i}:&nbsp;"+target,f"arg{i}:&nbsp;")
        return X
        
    ##########################################################
    # replace all "orkengine.core._core." with "" if it is not enclosed in quotes
    doc_html = replace_outside_quotes(doc_html,"orkengine.core._core.","")
    doc_html = replace_outside_quotes(doc_html,"orkengine.lev2._lev2.","")

    # Define the output file path
    output_file = os.path.join(output_dir, module_name + ".html")

    # Write the documentation to the output file
    with open(output_file, 'w') as f:
        f.write(doc_html)

    print(f"Documentation generated for {module_name} at {output_file}")

##########################################################

import orkengine.core
MODULES = ['orkengine',
           'orkengine.core', 
           'orkengine.core._core', 
           'orkengine.core._core.dataflow', 
           'orkengine.lev2',
           'orkengine.lev2._lev2',
            'orkengine.lev2._lev2.meshutil',
            'orkengine.lev2._lev2.orkidvr',
            'orkengine.lev2._lev2.primitives',
            'orkengine.lev2._lev2.particles',
            'orkengine.lev2._lev2.singularity',
            'orkengine.lev2._lev2.midi',
            'orkengine.lev2._lev2.scenegraph',
            'orkengine.lev2._lev2.ui',
]
for module in MODULES:
    generate_docs(module, DOCS_DIR)
    
command.run(["open",DOCS_DIR/"orkengine.html"])


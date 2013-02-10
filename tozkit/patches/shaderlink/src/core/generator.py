##################################################################################
#
#  Shaderlink - A RenderMan Shader Authoring Toolkit 
#  http://libe.ocracy.org/shaderlink.html
#  2010 Libero Spagnolini (libero.spagnolini@gmail.com)
#
##################################################################################

class ShaderBuilder(object):
    def __init__(self, shaderLink, rootNodes, renderSettings):
        self.shaderLink = shaderLink
        
        self.rootNodes = rootNodes
        
        self.renderSettings = renderSettings
        
        # conversion table for shader type
        self.shaderType = {'surface' : 'surface',
                           'displacement' : 'displacement',
                           'interior' : 'volume',
                           'exterior' : 'volume',
                           'atmosphere' : 'volume',
                           'imager' : 'imager'}
        
        # shaders
        self.shaders = {}
        
        # rib 
        self.rib = {}
        
    def buildShaders(self):
        self.shaders = {}
        
        for rootNode in self.rootNodes:
            print 'Generating code for: %s' % rootNode.name
            shaderListing = ''
            
            # includes
            includes = []
            rootNode.getIncludes(includes)
            for include in includes:
                shaderListing += '#include \"%s\"\n' % include 
            
            # type and name
            shaderType = self.shaderType[rootNode.type]
            shaderListing += '%s %s' % (shaderType, rootNode.name)
            
            # parameters
            params = []
            rootNode.getShaderParameters(params)
            shaderListing += '('
            if params != []:
                shaderListing += ''.join(param + '; ' for param in params[: - 1]) + str(params[ - 1])
            shaderListing += ')\n'
            
            # opening shader
            shaderListing += '''{\n'''
            
            # code
            code = ''
            code = rootNode.getCode(code)
            shaderListing += code

            # closing shader
            shaderListing += '''}\n'''
            
            # build shader code file            
            import os
            shaderFileName = rootNode.name + '.sl'
            shaderFilePath = os.path.join(self.shaderLink.paths['temp'], shaderFileName) 
            f = open(shaderFilePath, 'w')
            f.write(shaderListing)
            f.close()            
            
            self.shaders[shaderType] = {'name' : rootNode.name, 'path' : shaderFilePath}
    
    def buildRib(self):
        self.rib = {}
        
        # get rib file from settings
        ribFileName = self.renderSettings['Rib']
        
        # read it into a string in order to replace settings
        import os
        ribFilePath = os.path.join(self.shaderLink.paths['rib'], ribFileName)
        f = open(ribFilePath, 'r')
        ribListing = f.read()
        f.close()
        
        # replace settings
        ribListing = ribListing.replace('$(ShaderPath)', self.shaderLink.paths['shader'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(TempPath)', self.shaderLink.paths['temp'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(ArchivePath)', self.shaderLink.paths['archive'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(ImageFile)',
                                        os.path.join(self.shaderLink.paths['temp'], 'preview.tif').replace('\\', '\\\\'))
        
        if self.renderSettings['Preview']:
            ribListing = ribListing.replace('$(Width)', '512')
            ribListing = ribListing.replace('$(Height)', '512')
            ribListing = ribListing.replace('$(AspectRatio)', '1')
        else:
            ribListing = ribListing.replace('$(Width)', str(int(self.renderSettings['Format'][0])))
            ribListing = ribListing.replace('$(Height)', str(int(self.renderSettings['Format'][1])))
            ribListing = ribListing.replace('$(AspectRatio)', str(self.renderSettings['AspectRatio']))
        
        ribListing = ribListing.replace('$(SamplesX)', str(self.renderSettings['Samples'][0]))
        
        ribListing = ribListing.replace('$(SamplesY)', str(self.renderSettings['Samples'][1]))
        
        ribListing = ribListing.replace('$(Filter)', self.renderSettings['Filter'])
        
        ribListing = ribListing.replace('$(FilterWidth)', str(self.renderSettings['FilterWidth'][0]))
        
        ribListing = ribListing.replace('$(FilterHeight)', str(self.renderSettings['FilterWidth'][1]))
        
        ribListing = ribListing.replace('$(ShadingRate)', str(self.renderSettings['ShadingRate']))
        
        # replace shaders
        rootNodeTypes = [node.type for node in self.rootNodes]
        if 'surface' in rootNodeTypes:
            ribListing = ribListing.replace('$(Surface)', self.shaders['surface']['name'])
        else:
            ribListing = ribListing.replace('Surface "$(Surface)"', '')
        
        if 'displacement' in rootNodeTypes:
            ribListing = ribListing.replace('$(Displacement)', self.shaders['displacement']['name'])
        else:
            ribListing = ribListing.replace('Displacement "$(Displacement)"', '')
        
        if 'interior' in rootNodeTypes:
            ribListing = ribListing.replace('$(Interior)', self.shaders['interior']['name'])
        else:
            ribListing = ribListing.replace('Interior "$(Interior)"', '')
        
        if 'exterior' in rootNodeTypes:
            ribListing = ribListing.replace('$(Exterior)', self.shaders['exterior']['name'])
        else:
            ribListing = ribListing.replace('Exterior "$(Exterior)"', '')
        
        if 'atmosphere' in rootNodeTypes:
            ribListing = ribListing.replace('$(Atmosphere)', self.shaders['atmosphere']['name'])
        else:
            ribListing = ribListing.replace('Atmosphere "$(Atmosphere)"', '')
        
        if 'imager' in rootNodeTypes:
            ribListing = ribListing.replace('$(Imager)', self.shaders['imager']['name'])
        else:
            ribListing = ribListing.replace('Imager "$(Imager)"', '')

        # build rib code file            
        import os
        ribFilePath = os.path.join(self.shaderLink.paths['temp'], ribFileName) 
        f = open(ribFilePath, 'w')
        f.write(ribListing)
        f.close()
        
        self.rib = {'name' : ribFileName, 'path' : ribFilePath}    

    def compileShaders(self):
        compileCommandExecutions = {}
        # change current directory
        import os, subprocess        
        os.chdir(self.shaderLink.paths['temp'])#.replace('\\', '\\\\'))
        
        # compile shaders
        #compileCommand = ['shader', '-I%s' % self.shaderLink.paths['include']]#.replace('\\', '\\\\')]
        command = self.shaderLink.renderers[self.shaderLink.currentRendererIndex]['compileTool']
        compileCommand = [command, '-I%s' % self.shaderLink.paths['include']]#.replace('\\', '\\\\')]
        for shader in self.shaders.values():            
            shaderFileNamePath = shader['path'] 
            stdoutFileName = os.path.join(os.curdir, 'stdout-%s.log' % shader['name'])
            stdout = file(stdoutFileName, 'w')
            stderr = file(stdoutFileName, 'w')

            # call the process
            retval = subprocess.call(compileCommand + [shaderFileNamePath], 0, None, None, stdout, stderr)

            stdout.close()
            stderr.close()

            stdout = file(stdoutFileName, 'r')                        
            stderr = file(stdoutFileName, 'r')
            compileCommandExecution = {'command' : ''.join(s + ' ' for s in compileCommand + [shaderFileNamePath]),
                                       'commandResult' : stdout.read() + stderr.read(),
                                       'retval' : retval}
            stdout.close()
            stderr.close()
            
            compileCommandExecutions[shaderFileNamePath] = compileCommandExecution 
        
        return compileCommandExecutions
        
    def renderRib(self):
        renderCommandExecution = None
        # change current directory
        import os, subprocess        
        os.chdir(self.shaderLink.paths['temp'])#.replace('\\', '\\\\'))                

        # render rib
        #renderCommand = ['prman']            
        renderCommand = [self.shaderLink.renderers[self.shaderLink.currentRendererIndex]['renderTool']]
        ribFileNamePath = self.rib['path'] 
        stdoutFileName = os.path.join(os.curdir, 'stdout-%s.log' % self.rib['name'])
        stdout = file(stdoutFileName, 'w')
        stderr = file(stdoutFileName, 'w')

        # call the process
        retval = subprocess.call(renderCommand + [ribFileNamePath], 0, None, None, stdout, stderr)

        stdout.close()
        stderr.close()
        
        stdout = file(stdoutFileName, 'r')                                
        stderr = file(stdoutFileName, 'r')
        renderCommandExecution = {'command' : ''.join(s + ' ' for s in renderCommand + [ribFileNamePath]),
                                  'commandResult' : stdout.read(),
                                  'retval' : retval}
        stdout.close()
        stderr.close()
                        
        return renderCommandExecution   
                
class PreviewBuilder(object):
    def __init__(self, shaderLink, gfxNode):
        self.shaderLink = shaderLink
        
        self.gfxNode = gfxNode
        self.node = self.gfxNode.node        
        
        # conversion table for shader type
        self.shaderType = {'surface' : 'surface',
                           'displacement' : 'displacement',
                           'interior' : 'volume',
                           'exterior' : 'volume',
                           'atmosphere' : 'volume',
                           'imager' : 'imager'}
        
        # shaders
        self.shaders = {}
        
        # rib 
        self.rib = {}
        
        self.renderSettings = self.shaderLink.renderingSettings

    def buildShaders(self):
        self.shaders = {}
        
        print 'Generating preview code for: %s' % self.node.name

        for type in self.node.previewCodes.keys():        
            # type and name
            shaderType = self.shaderType[type]
            
            shaderListing = ''
            
            # includes
            includes = []
            self.node.getIncludes(includes)
            for include in includes:
                shaderListing += '#include \"%s\"\n' % include             
            
            shaderListing += '%s %s' % (shaderType, self.node.name + '_' + shaderType)
            
            # parameters
            params = []
            self.node.getShaderParameters(params)
            shaderListing += '('
            if params != []:
                shaderListing += ''.join(param + '; ' for param in params[: - 1]) + str(params[ - 1])
            shaderListing += ')\n'
            
            # opening shader
            shaderListing += '''{\n'''
            
            # code
            code = ''
            code = self.node.getCode(code)
            shaderListing += code
            
            # preview code
            shaderListing += self.node.getPreviewCode(shaderType)
    
            # closing shader
            shaderListing += '''}\n'''
            
            # build shader code file            
            import os
            shaderFileName = self.node.name + '_' + shaderType + '.sl'
            shaderFilePath = os.path.join(self.shaderLink.paths['temp'], shaderFileName) 
            f = open(shaderFilePath, 'w')
            f.write(shaderListing)
            f.close()            
            
            self.shaders[shaderType] = {'name' : self.node.name + '_' + shaderType, 'path' : shaderFilePath}
    
    def buildRib(self):
        self.rib = {}
        
        # rib file name
        ribFileName = 'sphere.rib'
        
        # read it into a string in order to replace settings
        import os
        ribFilePath = os.path.join(self.shaderLink.paths['rib'], ribFileName)
        f = open(ribFilePath, 'r')
        ribListing = f.read()
        f.close()
        
        # replace settings
        ribListing = ribListing.replace('$(ShaderPath)', self.shaderLink.paths['shader'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(TempPath)', self.shaderLink.paths['temp'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(ArchivePath)', self.shaderLink.paths['archive'].replace('\\', '\\\\'))
        
        ribListing = ribListing.replace('$(ImageFile)',
                                        os.path.join(self.shaderLink.paths['temp'], self.node.name + '.tif').replace('\\', '\\\\'))
        
        prevW = float(self.renderSettings['Format'][0])
        #prevH = float(self.renderSettings['Format'][1])
        #aspc = prevW/prevH
        #prevW = str(int(self.renderSettings['Format'][0]))
        #prevH = str(int(self.renderSettings['Format'][1]))

        ribListing = ribListing.replace('$(Width)', str(int(prevW)) ) #str(int(self.gfxNode.gfxNodeBuilder.previewPixmapWidth)))
        ribListing = ribListing.replace('$(Height)', str(int(prevW)) ) #str(int(self.gfxNode.gfxNodeBuilder.previewPixmapHeight)))
        ribListing = ribListing.replace('$(AspectRatio)', '1')
        
        ribListing = ribListing.replace('$(SamplesX)', '3')
        
        ribListing = ribListing.replace('$(SamplesY)', '3')
        
        ribListing = ribListing.replace('$(Filter)', 'box')
        
        ribListing = ribListing.replace('$(FilterWidth)', '3')
        
        ribListing = ribListing.replace('$(FilterHeight)', '3')
        
        ribListing = ribListing.replace('$(ShadingRate)', '4')
        
        # replace shaders
        nodeTypes = self.node.previewCodes.keys()
        if 'surface' in nodeTypes:
            ribListing = ribListing.replace('$(Surface)', self.shaders['surface']['name'])
        else:
            ribListing = ribListing.replace('Surface "$(Surface)"', '')
        
        if 'displacement' in nodeTypes:
            ribListing = ribListing.replace('$(Displacement)', self.shaders['displacement']['name'])
        else:
            ribListing = ribListing.replace('Displacement "$(Displacement)"', '')
        
        if 'interior' in nodeTypes:
            ribListing = ribListing.replace('$(Interior)', self.shaders['interior']['name'])
        else:
            ribListing = ribListing.replace('Interior "$(Interior)"', '')
        
        if 'exterior' in nodeTypes:
            ribListing = ribListing.replace('$(Exterior)', self.shaders['exterior']['name'])
        else:
            ribListing = ribListing.replace('Exterior "$(Exterior)"', '')
        
        if 'atmosphere' in nodeTypes:
            ribListing = ribListing.replace('$(Atmosphere)', self.shaders['atmosphere']['name'])
        else:
            ribListing = ribListing.replace('Atmosphere "$(Atmosphere)"', '')
        
        if 'imager' in nodeTypes:
            ribListing = ribListing.replace('$(Imager)', self.shaders['imager']['name'])
        else:
            ribListing = ribListing.replace('Imager "$(Imager)"', '')

        # build rib code file            
        import os
        ribFilePath = os.path.join(self.shaderLink.paths['temp'], ribFileName) 
        f = open(ribFilePath, 'w')
        f.write(ribListing)
        f.close()
        
        self.rib = {'name' : ribFileName, 'path' : ribFilePath}    

    def compileShaders(self):
        compileCommandExecutions = {}
        # change current directory
        import os, subprocess        
        os.chdir(self.shaderLink.paths['temp'])#.replace('\\', '\\\\'))
        
        # compile shaders
        #compileCommand = ['shader', '-I%s' % self.shaderLink.paths['include']]#.replace('\\', '\\\\')]
        command = self.shaderLink.renderers[self.shaderLink.currentRendererIndex]['compileTool']
        compileCommand = [command, '-I%s' % self.shaderLink.paths['include']]#.replace('\\', '\\\\')]
        for shader in self.shaders.values():            
            shaderFileNamePath = shader['path'] 
            stdoutFileName = os.path.join(os.curdir, 'stdout-%s.log' % shader['name'])
            stdout = file(stdoutFileName, 'w')
            stderr = file(stdoutFileName, 'w')

            # call the process
            retval = subprocess.call(compileCommand + [shaderFileNamePath], 0, None, None, stdout, stderr)

            stdout.close()
            stderr.close()

            stdout = file(stdoutFileName, 'r')                        
            stderr = file(stdoutFileName, 'r')
            compileCommandExecution = {'command' : ''.join(s + ' ' for s in compileCommand + [shaderFileNamePath]),
                                       'commandResult' : stdout.read() + stderr.read(),
                                       'retval' : retval}
            stdout.close()
            stderr.close()
            
            compileCommandExecutions[shaderFileNamePath] = compileCommandExecution 
        
        return compileCommandExecutions
        
    def renderRib(self):
        renderCommandExecution = None
        # change current directory
        import os, subprocess        
        os.chdir(self.shaderLink.paths['temp'])#.replace('\\', '\\\\'))                

        # render rib
        #renderCommand = ['prman']            
        renderCommand = [self.shaderLink.renderers[self.shaderLink.currentRendererIndex]['renderTool']]
        ribFileNamePath = self.rib['path'] 
        stdoutFileName = os.path.join(os.curdir, 'stdout-%s.log' % 'sphere.rib')
        stdout = file(stdoutFileName, 'w')
        stderr = file(stdoutFileName, 'w')

        # call the process
        retval = subprocess.call(renderCommand + [ribFileNamePath], 0, None, None, stdout, stderr)

        stdout.close()
        stderr.close()
        
        stdout = file(stdoutFileName, 'r')                                
        stderr = file(stdoutFileName, 'r')
        renderCommandExecution = {'command' : ''.join(s + ' ' for s in renderCommand + [ribFileNamePath]),
                                  'commandResult' : stdout.read(),
                                  'retval' : retval}
        stdout.close()
        stderr.close()
                        
        return renderCommandExecution   
                


            
            
            
            

            
            
            
            
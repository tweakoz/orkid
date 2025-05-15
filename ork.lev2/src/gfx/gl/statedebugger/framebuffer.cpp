////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateCurrentFramebuffer() {

  using namespace ftxui;
  GLint currentFBO = 0;
  GL_ERRORCHECK();
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
  GL_ERRORCHECK();
  GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  node_vect_t NODES;
  bool complete = (status == GL_FRAMEBUFFER_COMPLETE);

  // get framebuffer statistics (w,h,attachments, formats)
  GLint fbo_w = 0;
  GLint fbo_h = 0;
  GLint fbo_d = 0;

  _colortext(NODES, WHI, BLK, "currentFBO<%d> status<%x> complete<%d>\n", currentFBO, status, int(complete));

  // Track if we're rendering to any special targets
  bool has_cube_face = false;
  bool has_array_slice = false;
  int cube_face_index = -1;
  int array_slice_index = -1;
  GLenum err = GL_NO_ERROR;
  
  // get number of attachments
  GLint attached_obj_type = GL_NONE;
  std::string attachment_type;
  for (int a = 0; a < 8; a++) {
    GL_ERRORCHECK();
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + a, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
    GL_ERRORCHECK();
    attachment_type = GLenumToString(attached_obj_type);
    
    if (attached_obj_type != GL_NONE) {
      GLint tex_id = 0;
      GL_ERRORCHECK();
      glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + a, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
      GL_ERRORCHECK();
      
      // Get texture target level (mip level)
      GLint tex_level = 0;
      GL_ERRORCHECK();
      glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + a, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &tex_level);
      GL_ERRORCHECK();
      
      // Check for layer attachment (for 3D or array textures)
      GLint layer = -1;
      GL_ERRORCHECK();
      glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + a, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &layer);
      GL_ERRORCHECK();
      
      if (attached_obj_type == GL_TEXTURE) {
        // We need to determine what type of texture is attached
        
        // Save previous texture bindings
        GLint prev_tex_2d = 0, prev_tex_cube = 0, prev_tex_3d = 0, prev_tex_2d_array = 0;
        GL_ERRORCHECK();
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_2d);
        glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &prev_tex_cube);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &prev_tex_3d);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev_tex_2d_array);
        GL_ERRORCHECK();
        
        // Try different texture types to find which one matches our texture ID
        GLenum target_type = GL_NONE;
        GLenum target_face = GL_NONE;
        
        // Check if it's a cube map texture (try first since this is common in rendering to cubemaps)
        GL_ERRORCHECK();
        glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
        err = glGetError(); // Clear any binding error
        GLint width_cube = 0;
        if (err == GL_NO_ERROR) {
            // Try each face to see if any respond
            for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++) {
                glGetTexLevelParameteriv(face, tex_level, GL_TEXTURE_WIDTH, &width_cube);
                err = glGetError();
                if (err == GL_NO_ERROR && width_cube > 0) {
                    // Valid cubemap face found
                    break;
                }
                width_cube = 0; // Reset if this face failed
            }
        }
        GL_ERRORCHECK();
        
        // Check if it's a 2D texture
        glBindTexture(GL_TEXTURE_2D, tex_id);
        err = glGetError(); // Clear any binding error
        GLint width_2d = 0, height_2d = 0;
        if (err == GL_NO_ERROR) {
            glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_WIDTH, &width_2d);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                width_2d = 0; // Reset on error
            }
        }
        GL_ERRORCHECK();
        
        // Check if it's a 3D texture
        glBindTexture(GL_TEXTURE_3D, tex_id);
        err = glGetError(); // Clear any binding error
        GLint width_3d = 0;
        if (err == GL_NO_ERROR) {
            glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_WIDTH, &width_3d);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                width_3d = 0; // Reset on error
            }
        }
        GL_ERRORCHECK();
        
        // Check if it's a 2D array texture
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
        err = glGetError(); // Clear any binding error
        GLint width_2d_array = 0;
        if (err == GL_NO_ERROR) {
            glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_WIDTH, &width_2d_array);
            err = glGetError();
            if (err != GL_NO_ERROR) {
                width_2d_array = 0; // Reset on error
            }
        }
        GL_ERRORCHECK();
        
        // Restore previous bindings
        glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
        glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
        glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
        glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
        GL_ERRORCHECK();
        
        // Determine texture type based on valid dimensions
        GLint width = 0, height = 0, depth = 0;
        has_cube_face = false;
        has_array_slice = false;
        cube_face_index = -1;
        array_slice_index = -1;
        
        if (width_cube > 0) {
            target_type = GL_TEXTURE_CUBE_MAP;
            
            // For cube maps, determine which face from the layer parameter
            if (layer >= 0 && layer < 6) {
                cube_face_index = layer;
                has_cube_face = true;
                target_face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + cube_face_index;
                
                // Get dimensions safely
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
                glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_WIDTH, &width);
                err = glGetError();
                if (err != GL_NO_ERROR) width = width_cube; // Fall back to previously determined width
                
                glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_HEIGHT, &height);
                err = glGetError();
                if (err != GL_NO_ERROR) height = width_cube; // For cube maps, width == height
                
                glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
                GL_ERRORCHECK();
            }
        } else if (width_3d > 0) {
            target_type = GL_TEXTURE_3D;
            
            // Get 3D texture dimensions safely
            glBindTexture(GL_TEXTURE_3D, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_WIDTH, &width);
            err = glGetError();
            if (err != GL_NO_ERROR) width = width_3d;
            
            glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_HEIGHT, &height);
            err = glGetError();
            if (err != GL_NO_ERROR) height = 0;
            
            glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_DEPTH, &depth);
            err = glGetError();
            if (err != GL_NO_ERROR) depth = 0;
            
            glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
            GL_ERRORCHECK();
            
            if (layer >= 0 && depth > 0 && layer < depth) {
                has_array_slice = true;
                array_slice_index = layer;
            }
        } else if (width_2d_array > 0) {
            target_type = GL_TEXTURE_2D_ARRAY;
            
            // Get 2D array texture dimensions safely
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_WIDTH, &width);
            err = glGetError();
            if (err != GL_NO_ERROR) width = width_2d_array;
            
            glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_HEIGHT, &height);
            err = glGetError();
            if (err != GL_NO_ERROR) height = 0;
            
            glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_DEPTH, &depth);
            err = glGetError();
            if (err != GL_NO_ERROR) depth = 0;
            
            glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
            GL_ERRORCHECK();
            
            if (layer >= 0 && depth > 0 && layer < depth) {
                has_array_slice = true;
                array_slice_index = layer;
            }
        } else if (width_2d > 0) {
            target_type = GL_TEXTURE_2D;
            
            // Get 2D texture dimensions safely
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_WIDTH, &width);
            err = glGetError();
            if (err != GL_NO_ERROR) width = width_2d;
            
            glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_HEIGHT, &height);
            err = glGetError();
            if (err != GL_NO_ERROR) height = 0;
            
            glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
            GL_ERRORCHECK();
        }
        
        // Get format based on determined texture type safely
        GLint tex_format = GL_NONE;
        std::string tex_format_str = "UNKNOWN";
        std::string target_type_str = GLenumToString(target_type);
        
        if (target_type != GL_NONE) {
            if (target_type == GL_TEXTURE_CUBE_MAP && has_cube_face) {
                glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
                glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
                err = glGetError();
                if (err != GL_NO_ERROR) tex_format = GL_NONE;
                glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
                GL_ERRORCHECK();
            } else if (target_type == GL_TEXTURE_3D) {
                glBindTexture(GL_TEXTURE_3D, tex_id);
                glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
                err = glGetError();
                if (err != GL_NO_ERROR) tex_format = GL_NONE;
                glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
                GL_ERRORCHECK();
            } else if (target_type == GL_TEXTURE_2D_ARRAY) {
                glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
                glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
                err = glGetError();
                if (err != GL_NO_ERROR) tex_format = GL_NONE;
                glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
                GL_ERRORCHECK();
            } else if (target_type == GL_TEXTURE_2D) {
                glBindTexture(GL_TEXTURE_2D, tex_id);
                glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
                err = glGetError();
                if (err != GL_NO_ERROR) tex_format = GL_NONE;
                glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
                GL_ERRORCHECK();
            }
            
            if (tex_format != GL_NONE) {
                tex_format_str = GLenumToString(tex_format);
            }
        }
        
        // Detailed attachment info
        if (has_cube_face) {
            const char* face_names[] = {"POS_X", "NEG_X", "POS_Y", "NEG_Y", "POS_Z", "NEG_Z"};
            _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s id<%d> type<%s> fmt<%s> size<%dx%d> CUBE_FACE<%s> mip<%d>\n", 
                      a, attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                      width, height, face_names[cube_face_index], tex_level);
        } else if (has_array_slice) {
            _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s id<%d> type<%s> fmt<%s> size<%dx%d> ARRAY_SLICE<%d/%d> mip<%d>\n", 
                      a, attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                      width, height, array_slice_index, depth, tex_level);
        } else {
            _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s id<%d> type<%s> fmt<%s> size<%dx%d> mip<%d>\n", 
                      a, attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                      width, height, tex_level);
        }
      } else if (attached_obj_type != GL_NONE) {
        _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s\n", a, attachment_type.c_str());
      }
    } else {
      _colortext(NODES, WHI, BLK, "  CLR attached<%d> is %s\n", a, attachment_type.c_str());
    }
  }

  // get depth attachment
  GL_ERRORCHECK();
  glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
  GL_ERRORCHECK();
  attachment_type = GLenumToString(attached_obj_type);
  
  // Check for depth layer attachment (for 3D/array textures/cube faces)
  GLint depth_layer = -1;
  GL_ERRORCHECK();
  glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER, &depth_layer);
  GL_ERRORCHECK();
  
  bool depth_is_cube = false;
  bool depth_is_array = false;
  int depth_face_index = -1;
  
  if (attached_obj_type == GL_TEXTURE) {
    // get texture id
    GLint tex_id = 0;
    GL_ERRORCHECK();
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex_id);
    GL_ERRORCHECK();

    // Get texture level
    GLint tex_level = 0;
    GL_ERRORCHECK();
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL, &tex_level);
    GL_ERRORCHECK();
    
    // Save previous texture bindings
    GLint prev_tex_2d = 0, prev_tex_cube = 0, prev_tex_3d = 0, prev_tex_2d_array = 0;
    GL_ERRORCHECK();
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex_2d);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &prev_tex_cube);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &prev_tex_3d);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev_tex_2d_array);
    GL_ERRORCHECK();
    
    GLenum target_type = GL_NONE;
    GLenum target_face = GL_NONE;
    
    // Check if it's a cube map texture (check first as it's common for depth attachments)
    GL_ERRORCHECK();
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
    err = glGetError();
    GLint width_cube = 0;
    if (err == GL_NO_ERROR) {
        // Try each face to see if any respond
        for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; face++) {
            glGetTexLevelParameteriv(face, tex_level, GL_TEXTURE_WIDTH, &width_cube);
            err = glGetError();
            if (err == GL_NO_ERROR && width_cube > 0) {
                // Valid cubemap face found
                break;
            }
            width_cube = 0; // Reset if this face failed
        }
    }
    GL_ERRORCHECK();
    
    // Check if it's a 2D texture
    glBindTexture(GL_TEXTURE_2D, tex_id);
    err = glGetError();
    GLint width_2d = 0;
    if (err == GL_NO_ERROR) {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_WIDTH, &width_2d);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            width_2d = 0;
        }
    }
    GL_ERRORCHECK();
    
    // Check if it's a 3D texture
    glBindTexture(GL_TEXTURE_3D, tex_id);
    err = glGetError();
    GLint width_3d = 0;
    if (err == GL_NO_ERROR) {
        glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_WIDTH, &width_3d);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            width_3d = 0;
        }
    }
    GL_ERRORCHECK();
    
    // Check if it's a 2D array texture
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
    err = glGetError();
    GLint width_2d_array = 0;
    if (err == GL_NO_ERROR) {
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_WIDTH, &width_2d_array);
        err = glGetError();
        if (err != GL_NO_ERROR) {
            width_2d_array = 0;
        }
    }
    GL_ERRORCHECK();
    
    // Restore previous bindings
    glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
    glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
    glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
    GL_ERRORCHECK();
    
    // Determine texture type and get dimensions safely
    GLint width = 0, height = 0, depth = 0;
    std::string target_type_str = "UNKNOWN";
    
    if (width_cube > 0) {
        target_type = GL_TEXTURE_CUBE_MAP;
        target_type_str = "CUBE_MAP";
        
        // For cube maps, determine which face from the layer
        if (depth_layer >= 0 && depth_layer < 6) {
            depth_face_index = depth_layer;
            depth_is_cube = true;
            target_face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + depth_face_index;
            
            // Get dimensions safely
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
            glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_WIDTH, &width);
            err = glGetError();
            if (err != GL_NO_ERROR) width = width_cube;
            
            glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_HEIGHT, &height);
            err = glGetError();
            if (err != GL_NO_ERROR) height = width_cube; // For cube maps, width == height
            
            glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
            GL_ERRORCHECK();
        }
    } else if (width_3d > 0) {
        target_type = GL_TEXTURE_3D;
        target_type_str = "TEXTURE_3D";
        
        // Get 3D texture dimensions safely
        glBindTexture(GL_TEXTURE_3D, tex_id);
        glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_WIDTH, &width);
        err = glGetError();
        if (err != GL_NO_ERROR) width = width_3d;
        
        glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_HEIGHT, &height);
        err = glGetError();
        if (err != GL_NO_ERROR) height = 0;
        
        glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_DEPTH, &depth);
        err = glGetError();
        if (err != GL_NO_ERROR) depth = 0;
        
        glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
        GL_ERRORCHECK();
        
        if (depth_layer >= 0 && depth > 0 && depth_layer < depth) {
            depth_is_array = true;
        }
    } else if (width_2d_array > 0) {
        target_type = GL_TEXTURE_2D_ARRAY;
        target_type_str = "TEXTURE_2D_ARRAY";
        
        // Get 2D array texture dimensions safely
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_WIDTH, &width);
        err = glGetError();
        if (err != GL_NO_ERROR) width = width_2d_array;
        
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_HEIGHT, &height);
        err = glGetError();
        if (err != GL_NO_ERROR) height = 0;
        
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_DEPTH, &depth);
        err = glGetError();
        if (err != GL_NO_ERROR) depth = 0;
        
        glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
        GL_ERRORCHECK();
        
        if (depth_layer >= 0 && depth > 0 && depth_layer < depth) {
            depth_is_array = true;
        }
    } else if (width_2d > 0) {
        target_type = GL_TEXTURE_2D;
        target_type_str = "TEXTURE_2D";
        
        // Get 2D texture dimensions safely
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_WIDTH, &width);
        err = glGetError();
        if (err != GL_NO_ERROR) width = width_2d;
        
        glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_HEIGHT, &height);
        err = glGetError();
        if (err != GL_NO_ERROR) height = 0;
        
        glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
        GL_ERRORCHECK();
    }
    
    // Get format based on determined texture type safely
    GLint tex_format = GL_NONE;
    std::string tex_format_str = "UNKNOWN";
    
    if (target_type != GL_NONE) {
        if (target_type == GL_TEXTURE_CUBE_MAP && depth_is_cube) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id);
            glGetTexLevelParameteriv(target_face, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
            err = glGetError();
            if (err != GL_NO_ERROR) tex_format = GL_NONE;
            glBindTexture(GL_TEXTURE_CUBE_MAP, prev_tex_cube);
            GL_ERRORCHECK();
        } else if (target_type == GL_TEXTURE_3D) {
            glBindTexture(GL_TEXTURE_3D, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_3D, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
            err = glGetError();
            if (err != GL_NO_ERROR) tex_format = GL_NONE;
            glBindTexture(GL_TEXTURE_3D, prev_tex_3d);
            GL_ERRORCHECK();
        } else if (target_type == GL_TEXTURE_2D_ARRAY) {
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
            err = glGetError();
            if (err != GL_NO_ERROR) tex_format = GL_NONE;
            glBindTexture(GL_TEXTURE_2D_ARRAY, prev_tex_2d_array);
            GL_ERRORCHECK();
        } else if (target_type == GL_TEXTURE_2D) {
            glBindTexture(GL_TEXTURE_2D, tex_id);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, tex_level, GL_TEXTURE_INTERNAL_FORMAT, &tex_format);
            err = glGetError();
            if (err != GL_NO_ERROR) tex_format = GL_NONE;
            glBindTexture(GL_TEXTURE_2D, prev_tex_2d);
            GL_ERRORCHECK();
        }
        
        if (tex_format != GL_NONE) {
            tex_format_str = GLenumToString(tex_format);
        }
    }
    
    // Display depth attachment info based on type
    if (depth_is_cube) {
        const char* face_names[] = {"POS_X", "NEG_X", "POS_Y", "NEG_Y", "POS_Z", "NEG_Z"};
        _colortext(NODES, WHI, BLK, "  DEPTH attached is %s id<%d> type<%s> fmt<%s> size<%dx%d> CUBE_FACE<%s> mip<%d>\n", 
                 attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                 width, height, face_names[depth_face_index], tex_level);
    } else if (depth_is_array) {
        _colortext(NODES, WHI, BLK, "  DEPTH attached is %s id<%d> type<%s> fmt<%s> size<%dx%d> ARRAY_SLICE<%d/%d> mip<%d>\n", 
                 attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                 width, height, depth_layer, depth, tex_level);
    } else if (target_type != GL_NONE) {
        _colortext(NODES, WHI, BLK, "  DEPTH attached is %s id<%d> type<%s> fmt<%s> size<%dx%d> mip<%d>\n", 
                 attachment_type.c_str(), tex_id, target_type_str.c_str(), tex_format_str.c_str(), 
                 width, height, tex_level);
    } else {
        _colortext(NODES, WHI, BLK, "  DEPTH attached is %s id<%d> type<UNKNOWN>\n", 
                 attachment_type.c_str(), tex_id);
    }
  } else if (attached_obj_type != GL_NONE) {
    _colortext(NODES, WHI, BLK, "  DEPTH attached is %s\n", attachment_type.c_str());
  }
  
  // Check for stencil attachment
  GL_ERRORCHECK();
  glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attached_obj_type);
  err = glGetError();
  if (err == GL_NO_ERROR) {
    attachment_type = GLenumToString(attached_obj_type);
    if (attached_obj_type != GL_NONE) {
      _colortext(NODES, WHI, BLK, "  STENCIL attached is %s\n", attachment_type.c_str());
    }
  }
  GL_ERRORCHECK();
  
  // Check for any error states if framebuffer is incomplete
  if (!complete) {
    std::string error_str;
    switch (status) {
      case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        error_str = "INCOMPLETE_ATTACHMENT";
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        error_str = "INCOMPLETE_MISSING_ATTACHMENT";
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        error_str = "INCOMPLETE_DRAW_BUFFER";
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        error_str = "INCOMPLETE_READ_BUFFER";
        break;
      case GL_FRAMEBUFFER_UNSUPPORTED:
        error_str = "UNSUPPORTED";
        break;
      case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        error_str = "INCOMPLETE_MULTISAMPLE";
        break;
      default:
        error_str = "UNKNOWN_ERROR";
    }
    _colortext(NODES, RED, BLK, "  ERROR: %s\n", error_str.c_str());
  }

  _node_framebuffer = vbox({
      text("Framebuffer State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
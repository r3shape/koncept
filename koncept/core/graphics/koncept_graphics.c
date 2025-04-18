#include <koncept/core/graphics/koncept_graphics.h>

_koncept_graphics_api* _graphics_api = NULL;

// GENERAL GRAPHICS API
KC_Shader _create_shader_impl(cstr vertex, cstr fragment) {
    u32 link = 0;
    u32 compile = 0;

    KC_Shader shader = {.uniforms = corex->dsx.array.create_hash_array(32)};
    if (!shader.uniforms) {
        // error: failed to allocate uniform hashmap!
        return (KC_Shader){0};
    }

    shader.program = _graphics_api->gl.create_program();
    i32 v_Shader = _graphics_api->gl.create_shader(KC_VERTEX_SHADER);
    i32 f_Shader = _graphics_api->gl.create_shader(KC_FRAGMENT_SHADER);

    _graphics_api->gl.shader_source(v_Shader, 1, &vertex, NULL);
    _graphics_api->gl.compile_shader(v_Shader);
    _graphics_api->gl.get_shaderiv(v_Shader, KC_COMPILE_STATUS, &compile);
    if (!compile) {
        // error: failed to compile vertex-shader!
        printf("failed to compile vertex shader!\n");
        corex->dsx.array.destroy_hash_array(shader.uniforms);
        return (KC_Shader){0};
    }
    
    _graphics_api->gl.shader_source(f_Shader, 1, &fragment, NULL);
    _graphics_api->gl.compile_shader(f_Shader);
    _graphics_api->gl.get_shaderiv(f_Shader, KC_COMPILE_STATUS, &compile);
    if (!compile) {
        // error: failed to compile fragment-shader!
        printf("failed to compile fragment shader!\n");
        corex->dsx.array.destroy_hash_array(shader.uniforms);
        return (KC_Shader){0};
    }
    
    _graphics_api->gl.attach_shader(shader.program, v_Shader);
    _graphics_api->gl.attach_shader(shader.program, f_Shader);
    _graphics_api->gl.link_program(shader.program);
    _graphics_api->gl.get_programiv(shader.program, KC_LINK_STATUS, &link);
    if (!link) {
        // error: failed to link shader program!
        printf("failed to link shader!\n");
        corex->dsx.array.destroy_hash_array(shader.uniforms);
        return (KC_Shader){0};
    }

    _graphics_api->gl.delete_shader(v_Shader);
    _graphics_api->gl.delete_shader(f_Shader);
    return shader;
}

void _destroy_shader_impl(KC_Shader* shader) {
    corex->dsx.array.destroy_hash_array(shader->uniforms);
    _graphics_api->gl.delete_program(shader->program);
}

u8 _set_uniform_impl(KC_Shader* shader, KC_Uniform* uniform) {
    if (!shader || !uniform) return COREX_FALSE;  // error: null ptr!
    if (!uniform->name || !uniform->value || uniform->type < 0 || uniform->type >= KC_UNIFORM_TYPES) {
        return COREX_FALSE;  // error: uniform value error!
    }
    uniform->location = _graphics_api->gl.get_uniform_location(shader->program, uniform->name);
    return corex->dsx.array.put_hash_array(shader->uniforms, uniform->name, uniform);
}

u8 _send_uniform_impl(KC_Shader* shader, str name) {
    if (!shader || !name) return COREX_FALSE;    // error: null ptr!
    
    KC_Uniform* uniform = corex->dsx.array.get_hash_array(shader->uniforms, name);
    if (!uniform) return COREX_FALSE;  // error: failed to find uniform in hashmap!

    switch (uniform->type) {
        case KC_UNIFORM_NONE: break;
        case KC_UNIFORM_TYPES: break;
        case KC_UNIFORM_FLOAT: _graphics_api->gl.uniform1f(uniform->location, *(f32*)uniform->value); break;
        case KC_UNIFORM_VEC2: _graphics_api->gl.uniform2fv(uniform->location, 1, (f32*)uniform->value); break;
        case KC_UNIFORM_VEC3: _graphics_api->gl.uniform3fv(uniform->location, 1, (f32*)uniform->value); break;
        case KC_UNIFORM_VEC4: _graphics_api->gl.uniform4fv(uniform->location, 1, (f32*)uniform->value); break;
        case KC_UNIFORM_MAT4: _graphics_api->gl.uniform_matrix4fv(uniform->location, 1, 0, (f32*)uniform->value); break;
        default: break;
    }
    return COREX_TRUE;
}


KC_Vertex_Data _create_vertex_data_impl(f32 *vertices, u32 vertexCount, u32 *indices, u32 indexCount, u8 attrs) {
    if (!_graphics_api || !_graphics_api->gl.init) return (KC_Vertex_Data){0}; // error: null ptr!

    if ((attrs & ~((1 << KC_VERTEX_ATTRIBS) - 1)) != 0 || !vertices) {
        return (KC_Vertex_Data){0};
    }

    KC_Vertex_Data vertexData;

    // calculate stride and offsets dynamically
    u32 stride = 0;
    u32 offsets[KC_VERTEX_ATTRIBS] = {0};
    const u32 attr_sizes[KC_VERTEX_ATTRIBS] = {
        3, // KC_LOCATION_ATTR:  vec3
        3, // KC_COLOR_ATTR:     vec3
        2, // KC_TCOORD_ATTR:    vec2
        3  // KC_NORMAL_ATTR:    vec3
    };

    for (int i = 0; i < KC_VERTEX_ATTRIBS; i++) {
        if ((attrs & (1 << i)) != 0) {
            // accumulate stride for enabled vertex attributes
            offsets[i] = stride;
            stride += attr_sizes[i];
        }
    }

    _graphics_api->gl.gen_vertex_arrays(1, &vertexData.vao);
    _graphics_api->gl.gen_buffers(1, &vertexData.vbo);

    _graphics_api->gl.bind_vertex_array(vertexData.vao);
    _graphics_api->gl.bind_buffer(KC_ARRAY_BUFFER, vertexData.vbo);
    
    size_t vertex_data_size = vertexCount * (stride * sizeof(f32));
    _graphics_api->gl.buffer_data(KC_ARRAY_BUFFER, vertex_data_size, vertices, KC_STATIC_DRAW);

    // generate EBO if indices are provided
    if (indexCount > 0 && indices) {
        _graphics_api->gl.gen_buffers(1, &vertexData.ebo);

        _graphics_api->gl.bind_buffer(KC_ELEMENT_ARRAY_BUFFER, vertexData.ebo);
        
        size_t index_data_size = indexCount * sizeof(u32);
        _graphics_api->gl.buffer_data(KC_ELEMENT_ARRAY_BUFFER, index_data_size, indices, KC_STATIC_DRAW);
        
        vertexData.indices = indices;
        vertexData.indexCount = indexCount;
    } else {
        vertexData.ebo = 0;
        vertexData.indices = NULL;
        vertexData.indexCount = 0;
    }

    // configure vertex attributes
    for (int i = 0; i < KC_VERTEX_ATTRIBS/2; i++) {
        if ((attrs & (1 << i)) != 0) {
            _graphics_api->gl.vertex_attrib_pointer(
                i, 
                attr_sizes[i], 
                GL_FLOAT, 
                GL_FALSE, 
                stride * sizeof(f32), 
                (void*)(offsets[i] * sizeof(f32))
            );
            _graphics_api->gl.enable_vertex_attrib_array(i);
        }
    }

    _graphics_api->gl.bind_buffer(KC_ARRAY_BUFFER, 0);
    _graphics_api->gl.bind_vertex_array(0);

    vertexData.attrs = attrs;
    vertexData.vertices = vertices;
    vertexData.vertexCount = vertexCount;

    return vertexData;
}

void _destroy_vertex_data_impl(KC_Vertex_Data *vertexData) {
    if (!_graphics_api || !_graphics_api->gl.init) return;  // error: null ptr!

    vertexData->attrs = 0;
    
    _graphics_api->gl.delete_buffers(1, &vertexData->vbo);
    vertexData->vbo = 0;
    
    _graphics_api->gl.delete_buffers(1, &vertexData->ebo);
    vertexData->ebo = 0;
    
    _graphics_api->gl.delete_vertex_arrays(1, &vertexData->vao);
    vertexData->vao = 0;
    
    vertexData->vertices = NULL;
    vertexData->vertexCount = 0;
    vertexData->indices = NULL;
    vertexData->indexCount = 0;
}


#define STB_IMAGE_IMPLEMENTATION
#include <stb/include/stb_image.h>

KC_Texture _create_texture2D_impl(char* path, KC_Texture_Format format) {
KC_Texture texture = { .id = 0, .width = 0, .height = 0, .channels = 0, .path = NULL, .raw = NULL };
    if (!path) return texture;

    texture.path = path;
    stbi_set_flip_vertically_on_load(COREX_TRUE);
    texture.raw = stbi_load(path, &texture.width, &texture.height, &texture.channels, 0);
    if (!texture.raw) return texture;    // error: failed to allocate raw data buffer!

    _graphics_api->gl.gen_textures(1, &texture.id);
    _graphics_api->gl.bind_texture(GL_TEXTURE_2D, texture.id);

    // set texture wrapping options
    _graphics_api->gl.tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // x axis
    _graphics_api->gl.tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // y axis

    // set texture filtering options (scaling up/down)
    _graphics_api->gl.tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    _graphics_api->gl.tex_parameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // generate the texture
    _graphics_api->gl.tex_image2D(GL_TEXTURE_2D, 0, format, texture.width, texture.height, 0, format, GL_UNSIGNED_BYTE, texture.raw);
    _graphics_api->gl.generate_mipmap(GL_TEXTURE_2D);

    return texture;
}

void _destroy_texture2D_impl(KC_Texture* texture) {
    texture->width = 0;
    texture->height = 0;
    texture->channels = 0;
    texture ->path = NULL;
    _graphics_api->gl.delete_textures(1, &texture->id);
    stbi_image_free(texture->raw);
    texture->id = 0;
}


// PIPELINE API
void _push_pipeline_impl(KC_Render_Call* call) {
    if (!_graphics_api->pipeline.init) return;  // error: render pipeline not initialized!
    corex->dsx.array.push_array(_graphics_api->pipeline.calls, call);
}

void _flush_pipeline_impl(void) {
    if (!_graphics_api->pipeline.init) return;  // error: render pipeline not initialized!

    _graphics_api->gl.clear_color(
        _graphics_api->pipeline.clear_color.x,
        _graphics_api->pipeline.clear_color.y,
        _graphics_api->pipeline.clear_color.z,
        _graphics_api->pipeline.clear_color.w
    ); _graphics_api->gl.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Array_Head head = corex->dsx.array.get_array_head(_graphics_api->pipeline.calls);
    KC_Shader* shader = _graphics_api->pipeline.shader;
    COREX_FORI(0, head.count, 1) {
        KC_Render_Call call;
        corex->dsx.array.pull_array(_graphics_api->pipeline.calls, 0, &call);
        
        if (call.shader) {
            if (shader) {
                if (shader->program != call.shader->program) shader = call.shader;
            }
        } if (!shader) continue;  // error: no global shader set!
        
        if (call.model) _graphics_api->set_uniform(shader,
            &(KC_Uniform){
            .type = KC_UNIFORM_MAT4, 
            .name = "u_model",
            .value = &call.model->m
        });
        
        if (_graphics_api->camera.init) {
            _graphics_api->set_uniform(shader,
                &(KC_Uniform){
                .type = KC_UNIFORM_MAT4, 
                .name = "u_view",
                .value = &_graphics_api->camera.view.m
            });
            _graphics_api->set_uniform(shader,
                &(KC_Uniform){
                .type = KC_UNIFORM_VEC3, 
                .name = "u_view_location",
                .value = &_graphics_api->camera.eye
            });
        }

        _graphics_api->set_uniform(shader,
            &(KC_Uniform){
            .type = KC_UNIFORM_MAT4, 
            .name = "u_proj",
            .value = &_graphics_api->pipeline.proj.m
        });
        
        if (call.uniform_count && call.uniform_count <= (16 - shader->uniforms->meta.count)) {
            COREX_FORI(0, call.uniform_count, 1) {
                if (!call.uniforms[i].name || !call.uniforms[i].value) break;
                _graphics_api->set_uniform(shader, &call.uniforms[i]);
            }
        }

        if (_graphics_api->pipeline.mode != call.mode) _graphics_api->pipeline.mode = call.mode;
        _graphics_api->gl.use_program(shader->program);
        
        KC_Uniform** uniforms = (KC_Uniform**)corex->dsx.array.get_hash_array_values(shader->uniforms);
        COREX_FORI(0, shader->uniforms->meta.count, 1) {
            _graphics_api->send_uniform(shader, (str)uniforms[i]->name);
        }
        corex->dsx.array.destroy_array(uniforms);

        if (call.texture && call.texture->id > 0) _graphics_api->gl.bind_texture(GL_TEXTURE_2D, call.texture->id);
        _graphics_api->gl.bind_vertex_array(call.vertex->vao);
        if (call.type == KC_RENDER_ARRAYS) {
            _graphics_api->gl.draw_arrays(call.mode, 0, call.vertex->vertexCount);
        } else if (call.type == KC_RENDER_ELEMENTS) {
            _graphics_api->gl.draw_elements(call.mode, call.vertex->indexCount, GL_UNSIGNED_INT, NULL);
        }
        _graphics_api->gl.bind_texture(GL_TEXTURE_2D, 0);
        _graphics_api->gl.bind_vertex_array(0);
        _graphics_api->gl.use_program(0);
    }
}

u8 _init_pipeline_impl(KC_Render_Mode mode, KC_Shader* shader, Mat4 proj) {
    if (_graphics_api->pipeline.init) return COREX_TRUE; // redundant call: graphics API already initialized!

    if (mode >= KC_DRAW_MODES) return COREX_FALSE;  // error: value error
    
    _graphics_api->pipeline.calls = corex->dsx.array.create_array(sizeof(KC_Render_Call), 1024);
    if (!_graphics_api->pipeline.calls) return COREX_FALSE;    // error: out of memory!

    _graphics_api->pipeline.init = COREX_TRUE;
    _graphics_api->pipeline.mode = mode;
    _graphics_api->pipeline.proj = proj;
    _graphics_api->pipeline.shader = shader;
    _graphics_api->pipeline.clear_color = (Vec4){0.14, 0.16, 0.18, 1.0};

    return COREX_TRUE;
}


// CAMERA API
u8 _init_camera_impl(Vec3 eye, Vec3 center, Vec3 up) {
    if (!_graphics_api->pipeline.init) return COREX_FALSE;
    
    _graphics_api->camera.sensitivity = 0.1;
    _graphics_api->camera.speed = 0.1;
    _graphics_api->camera.roll = 0.0;
    _graphics_api->camera.yaw = -90.0;
    _graphics_api->camera.pitch = 0.0;
    _graphics_api->camera.eye = eye;
    _graphics_api->camera.center = center;
    _graphics_api->camera.direction = corex->mathx.vec.norm3(corex->mathx.vec.sub3(eye, center));
    _graphics_api->camera.right = corex->mathx.vec.norm3(corex->mathx.vec.cross3(up, _graphics_api->camera.direction));
    _graphics_api->camera.up = corex->mathx.vec.cross3(_graphics_api->camera.direction, _graphics_api->camera.right);
    
    // update the view matrix with camera vectors
    _graphics_api->camera.view = corex->mathx.mat.lookat(
        _graphics_api->camera.eye,
        corex->mathx.vec.add3(_graphics_api->camera.eye, _graphics_api->camera.direction),
        _graphics_api->camera.up
    );
    
    _graphics_api->camera.init = COREX_TRUE;
    return COREX_TRUE;
}

void _rotate_camera_impl( f32 dx, f32 dy) {
    _graphics_api->camera.yaw += dx * _graphics_api->camera.sensitivity;
    _graphics_api->camera.pitch += dy * _graphics_api->camera.sensitivity;
    
    _graphics_api->camera.yaw = COREX_WRAPF(_graphics_api->camera.yaw, 360.0);
    _graphics_api->camera.pitch = COREX_CLAMP(_graphics_api->camera.pitch, -89.0, 89.0);
}

void _translate_camera_impl(i8 x, i8 y, i8 z) {
    if (x > 0) {
        _graphics_api->camera.eye = corex->mathx.vec.add3(_graphics_api->camera.eye, corex->mathx.vec.scale3(corex->mathx.vec.norm3(corex->mathx.vec.cross3(
            _graphics_api->camera.direction,
            _graphics_api->camera.up
        )), _graphics_api->camera.speed));
    }
    else if (x < 0) {
        _graphics_api->camera.eye = corex->mathx.vec.sub3(_graphics_api->camera.eye, corex->mathx.vec.scale3(corex->mathx.vec.norm3(corex->mathx.vec.cross3(
            _graphics_api->camera.direction,
            _graphics_api->camera.up
        )), _graphics_api->camera.speed));
    }

    if (y > 0) {
        _graphics_api->camera.eye = corex->mathx.vec.add3(
            _graphics_api->camera.eye,
            corex->mathx.vec.scale3(_graphics_api->camera.up, _graphics_api->camera.speed)
        );
    }
    else if (y < 0) {
        _graphics_api->camera.eye = corex->mathx.vec.sub3(
            _graphics_api->camera.eye,
            corex->mathx.vec.scale3(_graphics_api->camera.up, _graphics_api->camera.speed)
        );
    }
    
    if (z > 0) {
        _graphics_api->camera.eye = corex->mathx.vec.add3(
            _graphics_api->camera.eye,
            corex->mathx.vec.scale3(_graphics_api->camera.direction, _graphics_api->camera.speed)
        );
    }
    else if (z < 0) {
        _graphics_api->camera.eye = corex->mathx.vec.sub3(
            _graphics_api->camera.eye,
            corex->mathx.vec.scale3(_graphics_api->camera.direction, _graphics_api->camera.speed)
        );
    }
}

void _update_camera_impl(void) {
    _graphics_api->camera.direction = corex->mathx.vec.norm3(corex->mathx.vec.vec3(
        cosf(corex->mathx.scalar.radians(_graphics_api->camera.yaw)) * cosf(corex->mathx.scalar.radians(_graphics_api->camera.pitch)),
        sinf(corex->mathx.scalar.radians(_graphics_api->camera.pitch)),
        sinf(corex->mathx.scalar.radians(_graphics_api->camera.yaw)) * cosf(corex->mathx.scalar.radians(_graphics_api->camera.pitch))
    ));
    _graphics_api->camera.right = corex->mathx.vec.norm3(corex->mathx.vec.cross3(_graphics_api->camera.direction, corex->mathx.vec.vec3(0, 1.0, 0)));
    _graphics_api->camera.up = corex->mathx.vec.norm3(corex->mathx.vec.cross3(_graphics_api->camera.right, _graphics_api->camera.direction));

    _graphics_api->camera.view = corex->mathx.mat.lookat(
        _graphics_api->camera.eye,
        _graphics_api->camera.direction,
        _graphics_api->camera.up
    );
}


// GRAPHICAL UTILITIES
void _toggle_wireframe_impl(u8 toggle) {
    if (toggle) {
        _graphics_api->gl.polygon_mode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        _graphics_api->gl.polygon_mode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

// API SETUP & CONFIGURATION
#if defined(KC_PLATFORM_WINDOWS)
#include <Windows.h>
#include <windowsx.h>
static void *_get_gl_fn(const char *name) {
    void *proc = (void*)wglGetProcAddress(name);
    if (proc == NULL || (proc == (void*)0x1) || (proc == (void*)0x2) || (proc == (void*)0x3) || (proc == (void*)-1)) {
        proc = (void*)GetProcAddress(LoadLibrary("opengl32.dll"), name);
    }; return proc;
}
#endif

u8 _init_gl_impl(_koncept_graphics_api* api) {
    if (!api) return COREX_FALSE;   // error: null ptr!

    struct gl_func {
        void **function;
        const char *name;
    } functions[] = {
        // BUFFER FUNCTIONS
        {(void**)&api->gl.gen_buffers, "glGenBuffers"},
        {(void**)&api->gl.bind_buffer, "glBindBuffer"},
        {(void**)&api->gl.buffer_data, "glBufferData"},
        {(void**)&api->gl.map_buffer, "glMapBuffer"},
        {(void**)&api->gl.unmap_buffer, "glUnmapBuffer"},
        {(void**)&api->gl.buffer_sub_data, "glBufferSubData"},
        {(void**)&api->gl.get_buffer_parameteriv, "glGetBufferParameteriv"},
        {(void**)&api->gl.delete_buffers, "glDeleteBuffers"},

        // VERTEX ATTRIBUTE FUNCTIONS
        {(void**)&api->gl.enable_vertex_attrib_array, "glEnableVertexAttribArray"},
        {(void**)&api->gl.disable_vertex_attrib_array, "glDisableVertexAttribArray"},
        {(void**)&api->gl.vertex_attrib_pointer, "glVertexAttribPointer"},
        {(void**)&api->gl.bind_vertex_array, "glBindVertexArray"},
        {(void**)&api->gl.gen_vertex_arrays, "glGenVertexArrays"},
        {(void**)&api->gl.delete_vertex_arrays, "glDeleteVertexArrays"},

        // SHADER FUNCTIONS
        {(void**)&api->gl.create_program, "glCreateProgram"},
        {(void**)&api->gl.use_program, "glUseProgram"},
        {(void**)&api->gl.link_program, "glLinkProgram"},
        {(void**)&api->gl.delete_program, "glDeleteProgram"},
        {(void**)&api->gl.create_shader, "glCreateShader"},
        {(void**)&api->gl.compile_shader, "glCompileShader"},
        {(void**)&api->gl.attach_shader, "glAttachShader"},
        {(void**)&api->gl.detach_shader, "glDetachShader"},
        {(void**)&api->gl.delete_shader, "glDeleteShader"},
        {(void**)&api->gl.shader_source, "glShaderSource"},
        {(void**)&api->gl.get_shaderiv, "glGetShaderiv"},
        {(void**)&api->gl.get_shader_info_log, "glGetShaderInfoLog"},
        {(void**)&api->gl.get_programiv, "glGetProgramiv"},
        {(void**)&api->gl.get_program_info_log, "glGetProgramInfoLog"},
        {(void**)&api->gl.get_uniform_location, "glGetUniformLocation"},
        
        {(void**)&api->gl.uniform1f, "glUniform1f"},
        {(void**)&api->gl.uniform2fv, "glUniform2fv"},
        {(void**)&api->gl.uniform3fv, "glUniform3fv"},
        {(void**)&api->gl.uniform4fv, "glUniform4fv"},
        {(void**)&api->gl.uniform_matrix4fv, "glUniformMatrix4fv"},

        // TEXTURE FUNCTIONS
        {(void**)&api->gl.gen_textures, "glGenTextures"},
        {(void**)&api->gl.bind_texture, "glBindTexture"},
        {(void**)&api->gl.tex_parameteri, "glTexParameteri"},
        {(void**)&api->gl.tex_image2D, "glTexImage2D"},
        {(void**)&api->gl.active_texture, "glActiveTexture"},
        {(void**)&api->gl.delete_textures, "glDeleteTextures"},
        {(void**)&api->gl.generate_mipmap, "glGenerateMipmap"},
        {(void**)&api->gl.generate_texture_mipmap, "glGenerateTextureMipmap"},

        // FRAMEBUFFER FUNCTIONS
        {(void**)&api->gl.gen_framebuffers, "glGenFramebuffers"},
        {(void**)&api->gl.bind_framebuffer, "glBindFramebuffer"},
        {(void**)&api->gl.framebuffer_texture2d, "glFramebufferTexture2D"},
        {(void**)&api->gl.delete_framebuffers, "glDeleteFramebuffers"},
        {(void**)&api->gl.check_framebuffer_status, "glCheckFramebufferStatus"},

        // DRAWING FUNCTIONS
        {(void**)&api->gl.clear, "glClear"},
        {(void**)&api->gl.clear_color, "glClearColor"},
        {(void**)&api->gl.draw_arrays, "glDrawArrays"},
        {(void**)&api->gl.draw_elements, "glDrawElements"},
        {(void**)&api->gl.draw_elements_base_vertex, "glDrawElementsBaseVertex"},

        // STATE MANAGEMENT FUNCTIONS
        {(void**)&api->gl.enable, "glEnable"},
        {(void**)&api->gl.disable, "glDisable"},
        {(void**)&api->gl.blend_func, "glBlendFunc"},
        {(void**)&api->gl.cull_face, "glCullFace"},
        {(void**)&api->gl.depth_func, "glDepthFunc"},
        {(void**)&api->gl.viewport, "glViewport"},
        {(void**)&api->gl.polygon_mode, "glPolygonMode"},

        // UTILITY FUNCTIONS
        {(void**)&api->gl.get_error, "glGetError"},
        {(void**)&api->gl.get_string, "glGetString"}
    };

    for (size_t i = 0; i < sizeof(functions) / sizeof(functions[0]); ++i) {
        *functions[i].function = _get_gl_fn(functions[i].name);
        if (!*functions[i].function) {
            printf("Failed to load GL function: %s\n", functions[i].name);
        }
    }

    api->gl.init = COREX_TRUE;
    api->gl.enable(GL_DEPTH_TEST);
    return COREX_TRUE;
}


u8 _koncept_init_graphics(_koncept_graphics_api* api) {
    if (!api) return COREX_FALSE;

    api->init_pipeline = _init_pipeline_impl;
    api->push_pipeline = _push_pipeline_impl;
    api->flush_pipeline = _flush_pipeline_impl;

    api->init_camera = _init_camera_impl;
    api->rotate_camera = _rotate_camera_impl;
    api->translate_camera = _translate_camera_impl;
    api->update_camera = _update_camera_impl;

    api->create_shader = _create_shader_impl;
    api->destroy_shader = _destroy_shader_impl;

    api->set_uniform = _set_uniform_impl;
    api->send_uniform = _send_uniform_impl;
    
    api->create_vertex_data = _create_vertex_data_impl;
    api->destroy_vertex_data = _destroy_vertex_data_impl;

    api->create_texture2D = _create_texture2D_impl;
    api->destroy_texture2D = _destroy_texture2D_impl;

    api->gl.init = COREX_FALSE;
    api->init_gl = _init_gl_impl;

    api->toggle_wireframe = _toggle_wireframe_impl;

    _graphics_api = api;
    return COREX_TRUE;
}

u8 _koncept_cleanup_graphics(_koncept_graphics_api* api) {
    if (!api) return COREX_FALSE;
    
    if (api->pipeline.calls) corex->dsx.array.destroy_array(api->pipeline.calls);
    
    api->init_pipeline = NULL;
    api->push_pipeline = NULL;
    api->flush_pipeline = NULL;

    api->init_camera = NULL;
    api->rotate_camera = NULL;
    api->translate_camera = NULL;
    api->update_camera = NULL;

    api->create_shader = NULL;
    api->destroy_shader = NULL;

    api->set_uniform = NULL;
    api->send_uniform = NULL;

    api->create_vertex_data = NULL;
    api->destroy_vertex_data = NULL;
    
    api->create_texture2D = NULL;
    api->destroy_texture2D = NULL;

    api->init_gl = NULL;
    api->toggle_wireframe = NULL;

    _graphics_api = NULL;

    return COREX_TRUE;
}

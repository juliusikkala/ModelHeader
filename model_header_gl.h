#ifndef MODELHEADER_GL_H
#define MODELHEADER_GL_H

#include <GL/glew.h>
#include <stddef.h>
#define MODELHEADER_POS 1
#define MODELHEADER_NORMAL 2
#define MODELHEADER_UV0 3

static inline void model_header_gl_load_impl(
    float* vertices,
    unsigned vertex_stride,
    unsigned vertex_count,
    unsigned* indices,
    unsigned index_count,
    GLuint* vbo,
    GLuint* ibo
){
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float)*vertex_stride*vertex_count,
        vertices,
        GL_STATIC_DRAW
    );

    glGenBuffers(1, ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        sizeof(unsigned)*index_count,
        indices,
        GL_STATIC_DRAW
    );
}

#define model_header_gl_load(model, vbo, ibo) \
    model_header_gl_load_impl( \
        model ## _vertices, \
        model ## _vertex_stride, \
        model ## _vertex_count, \
        model ## _indices, \
        model ## _index_count, \
        vbo, \
        ibo \
    )

static inline void model_header_gl_set_vertex_attribs_impl(
    unsigned vertex_stride,
    int position_offset,
    int normal_offset,
    int uv0_offset,
    GLuint* locations
){
    static GLuint default_locations[] = {
        MODELHEADER_POS, 0,
        MODELHEADER_NORMAL, 1,
        MODELHEADER_UV0, 2,
        0
    };
    if(!locations) locations = default_locations;
    while(*locations != 0)
    {
        long long offset = -1;
        int size = 0;
        switch(locations[0])
        {
        case MODELHEADER_POS:
            offset = position_offset;
            size = 3;
            break;
        case MODELHEADER_NORMAL:
            offset = normal_offset;
            size = 3;
            break;
        case MODELHEADER_UV0:
            offset = uv0_offset;
            size = 2;
            break;
        }
        if(offset == -1) continue;
        glVertexAttribPointer(
            locations[1],
            size,
            GL_FLOAT,
            GL_FALSE,
            vertex_stride,
            (const GLvoid*)offset
        );
        glEnableVertexAttribArray(locations[1]);
        locations += 2;
    }
}

#define model_header_gl_set_vertex_attribs(model, locations) \
    model_header_gl_set_vertex_attribs_impl( \
        model ## _vertex_stride, \
        model ## _position_offset, \
        model ## _normal_offset, \
        model ## _uv0_offset, \
        locations \
    )

#ifndef MODELHEADER_DISABLE_VAO

static inline void model_header_gl_load_vao_impl(
    float* vertices,
    unsigned vertex_stride,
    unsigned vertex_count,
    unsigned* indices,
    unsigned index_count,
    int position_offset,
    int normal_offset,
    int uv0_offset,
    GLuint* vbo,
    GLuint* ibo,
    GLuint* vao,
    GLuint* locations
){
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    model_header_gl_load_impl(
        vertices,
        vertex_stride,
        vertex_count,
        indices,
        index_count,
        vbo,
        ibo
    );

    model_header_gl_set_vertex_attribs_impl(
        vertex_stride,
        position_offset,
        normal_offset,
        uv0_offset,
        locations
    );

    glBindVertexArray(0);
}

#define model_header_gl_load_vao(model, vbo, ibo, vao, locations) \
    model_header_gl_load_vao_impl( \
        model ## _vertices, \
        model ## _vertex_stride, \
        model ## _vertex_count, \
        model ## _indices, \
        model ## _index_count, \
        model ## _position_offset, \
        model ## _normal_offset, \
        model ## _uv0_offset, \
        vbo, \
        ibo, \
        vao, \
        locations \
    )
#endif

#endif

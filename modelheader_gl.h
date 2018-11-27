/*
The MIT License (MIT)

Copyright (c) 2018 Julius Ikkala

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef MODELHEADER_GL_H
#define MODELHEADER_GL_H

#include <stddef.h>
#define MODELHEADER_ATTRIB_END 0
#define MODELHEADER_POS 1
#define MODELHEADER_NORMAL 2
#define MODELHEADER_UV0 3

static inline void modelheader_gl_load_impl(
    const float* vertices,
    unsigned vertex_stride,
    unsigned vertex_count,
    const unsigned* indices,
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

#define modelheader_gl_load(model, vbo, ibo) \
    modelheader_gl_load_impl( \
        model ## _vertices, \
        model ## _vertex_stride, \
        model ## _vertex_count, \
        model ## _indices, \
        model ## _index_count, \
        vbo, \
        ibo \
    )

static inline void modelheader_gl_set_vertex_attribs_impl(
    unsigned vertex_stride,
    int position_offset,
    int normal_offset,
    int uv0_offset,
    const GLuint* locations
){
    static const GLuint default_locations[] = {
        MODELHEADER_POS, 0,
        MODELHEADER_NORMAL, 1,
        MODELHEADER_UV0, 2,
        MODELHEADER_ATTRIB_END
    };
    if(!locations) locations = default_locations;
    while(*locations != MODELHEADER_ATTRIB_END)
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

#define modelheader_gl_set_vertex_attribs(model, locations) \
    modelheader_gl_set_vertex_attribs_impl( \
        model ## _vertex_stride, \
        model ## _position_offset, \
        model ## _normal_offset, \
        model ## _uv0_offset, \
        locations \
    )

#ifndef MODELHEADER_DISABLE_VAO

static inline void modelheader_gl_load_vao_impl(
    const float* vertices,
    unsigned vertex_stride,
    unsigned vertex_count,
    const unsigned* indices,
    unsigned index_count,
    int position_offset,
    int normal_offset,
    int uv0_offset,
    GLuint* vbo,
    GLuint* ibo,
    GLuint* vao,
    const GLuint* locations
){
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    modelheader_gl_load_impl(
        vertices,
        vertex_stride,
        vertex_count,
        indices,
        index_count,
        vbo,
        ibo
    );

    modelheader_gl_set_vertex_attribs_impl(
        vertex_stride,
        position_offset,
        normal_offset,
        uv0_offset,
        locations
    );

    glBindVertexArray(0);
}

#define modelheader_gl_load_vao(model, vbo, ibo, vao, locations) \
    modelheader_gl_load_vao_impl( \
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

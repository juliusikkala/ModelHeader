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
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <vector>
#include <map>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

void print_help(const char* name)
{
    std::cerr
        << "Usage: " << name << " [-p] [-dnt] [-m] [-n name_prefix] model_file"
        << std::endl
        << "-p disables pre-transformed primitives." << std::endl
        << "-m disables material, mesh and node information." << std::endl
        << "-d deletes parts of vertex data. 'n' removes normals, "
        << "'t' removes UV coordinates." << std::endl
        << "-n sets the default name prefix for the model." << std::endl;
}

struct
{
    std::string input_file;
    std::string name_prefix;
    std::string uppercase_name_prefix;
    bool pretransform = true;
    bool delete_normal = false;
    bool delete_uv = false;
    bool disable_info = false;
} options;

bool parse_args(char** argv)
{
    const char* name = *argv++;
    bool skip_flags = false;
    unsigned parameter_count = 0;
    while(*argv)
    {
        const char* arg = *argv;
        if(!skip_flags && arg[0] == '-')
        {
            if(arg[1] == '-')
            {
                if(arg[2] == 0) skip_flags = true;
                else if(!strcmp(arg+2, "help")) goto fail;
                else if(!strcmp(arg+2, "no-pretransform"))
                {
                    options.pretransform = false;
                }
                else
                {
                    std::cerr << "Unknown long flag " << arg+2 << std::endl;
                    goto fail;
                }
            }
            else if(arg[1] == 'p' && arg[2] == 0)
            {
                options.pretransform = false;
            }
            else if(arg[1] == 'm' && arg[2] == 0)
            {
                options.disable_info = true;
            }
            else if(arg[1] == 'n' && arg[2] == 0)
            {
                argv++;
                if(!*argv)
                {
                    std::cerr << "Missing name prefix" << std::endl;
                    goto fail;
                }
                options.name_prefix = *argv;
            }
            else if(arg[1] == 'd')
            {
                for(unsigned i = 2; arg[i] != 0; ++i)
                {
                    switch(arg[i]) 
                    {
                    case 'n':
                        options.delete_normal = true;
                        break;
                    case 't':
                        options.delete_uv = true;
                        break;
                    default:
                        std::cerr << "Unknown vertex attribute \""
                            << arg[i] << "\"" << std::endl;
                        goto fail;
                    }
                }
            }
            else
            {
                std::cerr << "Unknown flag " << arg+1 << std::endl;
                goto fail;
            }
        }
        else
        {
            if(parameter_count == 0)
            {
                options.input_file = arg;
                parameter_count++;
            }
            else
            {
                std::cerr << "Too many parameters." << std::endl;
                goto fail;
            }
        }
        argv++;
    }
    if(parameter_count == 0) goto fail;
    return true;
fail:
    print_help(name);
    return false;
}

std::string escape_string(const std::string& str)
{
    std::stringstream out;
    out << "\"";
    for(char c: str)
    {
        switch(c)
        {
        case '\"':
        case '\\':
            out << '\\';
            /* Fallthrough intentional*/
        default:
            out << c;
        }
    }
    out << "\"";
    return out.str();
}

void write_preamble()
{
    std::cout <<
        "/* Automatically generated header from file \"" << options.input_file
        << "\" */\n"
        "#ifndef MODELHEADER_MODEL_" << options.uppercase_name_prefix << "_H\n"
        "#define MODELHEADER_MODEL_" << options.uppercase_name_prefix << "_H\n"
        << (options.disable_info ? "" : "#include <stddef.h>\n") <<
        "#ifndef MODELHEADER_TYPES_DECLARED\n"
        "#define MODELHEADER_TYPES_DECLARED\n"
        "#if __cplusplus >= 201103L\n"
        "#define MODELHEADER_CONST constexpr const\n"
        "#else\n"
        "#define MODELHEADER_CONST const\n"
        "#endif\n";

    if(!options.disable_info)
    {
        std::cout << "\n"
            "struct modelheader_material\n"
            "{\n"
            "    const char* name;\n"
            "    const char* albedo_texture;\n"
            "    float albedo_factor[3];\n"
            "};\n"
            "\n"
            "struct modelheader_mesh\n"
            "{\n"
            "    const char* name;\n"
            "    const struct modelheader_material* material;\n"
            "    unsigned start_index;\n"
            "    unsigned size;\n"
            "};\n"
            "\n"
            "struct modelheader_node\n"
            "{\n"
            "    const struct modelheader_mesh* const * meshes;\n"
            "    unsigned mesh_count;\n"
            "\n"
            "    const struct modelheader_node* parent;\n"
            "    const struct modelheader_node* const* children;\n"
            "    unsigned child_count;\n"
            "\n"
            "    float transform[16];\n"
            "};\n\n";
    }
    std::cout << "#endif\n\n";
}

void write_prologue()
{
    std::cout <<
        "#endif\n";
}

void write_node(
    unsigned& node_count,
    const std::map<unsigned, unsigned>& mesh_key,
    const std::map<aiNode*, unsigned>& node_key,
    aiNode* node,
    std::stringstream& nodes,
    std::stringstream& private_declaration,
    std::stringstream& private_content
){
    if(!node) return;

    unsigned index = node_count++;

    nodes << "        {";

    if(node->mNumMeshes > 0)
    {
        private_declaration
            << "    const struct modelheader_mesh* const meshes_"
            << index << "[" << node->mNumMeshes << "];\n";

        private_content << "    {\n";
        for(unsigned i = 0; i < node->mNumMeshes; ++i)
        {
            private_content
                << "        &" << options.name_prefix << "_meshes["
                << mesh_key.at(node->mMeshes[i]) << "],\n";
        }

        private_content << "    },\n";

        nodes
            << options.name_prefix << "_private_data.meshes_" << index << ", "
            << node->mNumMeshes << ", ";
    }
    else
    {
        nodes << "NULL, 0, ";
    }

    if(node->mParent)
        nodes
            << "&" << options.name_prefix << "_private_data.nodes["
            << node_key.at(node->mParent) << "], ";
    else nodes << "NULL, ";

    if(node->mNumChildren > 0)
    {
        private_declaration
            << "    const struct modelheader_node* const children_"
            << index << "[" << node->mNumChildren << "];\n";

        private_content << "    {\n";
        for(unsigned i = 0; i < node->mNumChildren; ++i)
        {
            private_content
                << "        &" << options.name_prefix << "_private_data.nodes["
                << node_key.at(node->mChildren[i]) << "],\n";
        }

        private_content << "    },\n";
        nodes
            << options.name_prefix << "_private_data.children_" << index << ", "
            << node->mNumChildren << ", ";
    }
    else
    {
        nodes << "NULL, 0, ";
    }

    nodes << "{";
    for(unsigned i = 0; i < 4*4; ++i)
    {
        nodes << node->mTransformation[i/4][i%4] << ",";
    }
    nodes << "}},\n";

    for(unsigned i = 0; i < node->mNumChildren; ++i)
    {
        write_node(
            node_count,
            mesh_key,
            node_key,
            node->mChildren[i],
            nodes,
            private_declaration,
            private_content
        );
    }
}

void construct_node_key(
    unsigned& counter,
    aiNode* node,
    std::map<aiNode*, unsigned>& node_key
){
    if(!node) return;
    node_key[node] = counter++;
    for(unsigned i = 0; i < node->mNumChildren; ++i)
    {
        construct_node_key(counter, node->mChildren[i], node_key);
    }
}

void write_scene(const aiScene* scene)
{
    std::stringstream vertices;
    std::stringstream indices;
    std::stringstream materials;
    std::stringstream meshes;
    std::stringstream nodes;
    std::stringstream private_declaration;
    std::stringstream private_content;

    unsigned vertex_count = 0;
    unsigned vertex_stride = 0;
    unsigned index_count = 0;
    unsigned material_count = 0;
    unsigned mesh_count = 0;
    unsigned node_count = 0;
    int position_offset = -1;
    int normal_offset = -1;
    int uv0_offset = -1;
    bool position_present = false;
    bool normal_present = false;
    bool uv0_present = false;
    std::map<unsigned, unsigned> mesh_key;
    std::map<aiNode*, unsigned> node_key;

    vertices
        << "static MODELHEADER_CONST float "
        << options.name_prefix << "_vertices[] = {\n    ";
    indices
        << "static MODELHEADER_CONST unsigned "
        << options.name_prefix << "_indices[] = {\n    ";
    materials
        << "static MODELHEADER_CONST struct modelheader_material "
        << options.name_prefix << "_materials[] = {\n";
    meshes
        << "static MODELHEADER_CONST struct modelheader_mesh "
        << options.name_prefix << "_meshes[] = {\n";
    private_declaration
        << "static MODELHEADER_CONST struct {\n";
    private_content
        << "} " << options.name_prefix << "_private_data = {\n";
    nodes << "    {\n";

    /* Material pass */
    for(unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* inmat = scene->mMaterials[i];
        aiString name("Unnamed material");
        aiString albedo_texture("");
        aiColor3D albedo_factor(1.0f,1.0f,1.0f);

        inmat->Get(AI_MATKEY_NAME, name);
        inmat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 0), albedo_texture);
        inmat->Get(AI_MATKEY_COLOR_DIFFUSE, albedo_factor);

        materials
            << "    {" << escape_string(name.C_Str()) << ", "
            << (albedo_texture.length == 0 ? "NULL" : escape_string(albedo_texture.C_Str()))
            << ", {" << albedo_factor.r << ", " << albedo_factor.g << ", "
            << albedo_factor.b << "}},\n";
        material_count++;
    }

    /* Vertex format pre-pass */
    for(unsigned i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh* inmesh = scene->mMeshes[i];
        position_present |= inmesh->HasPositions();
        normal_present |= inmesh->HasNormals();
        uv0_present |= inmesh->HasTextureCoords(0);
    }
    normal_present = normal_present && !options.delete_normal;
    uv0_present = uv0_present && !options.delete_uv;

    if(position_present)
    {
        position_offset = vertex_stride;
        vertex_stride += 3;
    }

    if(normal_present)
    {
        normal_offset = vertex_stride;
        vertex_stride += 3;
    }

    if(uv0_present)
    {
        uv0_offset = vertex_stride;
        vertex_stride += 2;
    }

    /* Actual vertex/index writing pass */
    for(unsigned i = 0; i < scene->mNumMeshes; ++i)
    {
        unsigned start_index = index_count;
        unsigned start_vertex = vertex_count;
        unsigned size = 0;

        aiMesh* inmesh = scene->mMeshes[i];
        if(!inmesh->HasFaces())
        {
            std::cerr << "Mesh " << inmesh->mName.C_Str()
                      << " has no faces, skipping..." << std::endl;
            continue;
        }
        mesh_key[i] = mesh_count++;

        /* Add indices */
        for(unsigned j = 0; j < inmesh->mNumFaces; ++j)
        {
            aiFace* face = inmesh->mFaces + j;
            index_count+=3;
            size+=3;
            indices
                << start_vertex + face->mIndices[0] << ","
                << start_vertex + face->mIndices[1] << ","
                << start_vertex + face->mIndices[2] << ",";
        }

        /* Add vertices */
        for(unsigned j = 0; j < inmesh->mNumVertices; ++j)
        {
            vertex_count++;
            if(position_present)
            {
                aiVector3D p(0);
                if(inmesh->HasPositions())
                    p = inmesh->mVertices[j];
                vertices << p.x << "," << p.y << "," << p.z << ",";
            }

            if(normal_present)
            {
                aiVector3D n(0);
                if(inmesh->HasNormals())
                    n = inmesh->mNormals[j];
                vertices << n.x << "," << n.y << "," << n.z << ",";
            }

            if(uv0_present)
            {
                aiVector3D uv(0);
                if(inmesh->HasTextureCoords(0))
                    uv = inmesh->mTextureCoords[0][j];
                vertices << uv.x << "," << uv.y << ",";
            }
        }

        /* Add this mesh */
        meshes
            << "    {" << escape_string(inmesh->mName.C_Str()) << ", &"
            << options.name_prefix << "_materials["
            << inmesh->mMaterialIndex << "], "
            << start_index << ", " << size << "},\n";
    }

    construct_node_key(
        node_count,
        scene->mRootNode,
        node_key
    );

    node_count = 0;

    write_node(
        node_count,
        mesh_key,
        node_key,
        scene->mRootNode,
        nodes,
        private_declaration,
        private_content
    );

    vertices << "\n};\n";
    indices << "\n};\n";
    materials << "};\n";
    meshes << "};\n";
    nodes << "    }\n";
    private_declaration
        << "    const struct modelheader_node nodes[" << node_count << "];\n";
    private_content << nodes.str() <<  "};\n";

    std::cout << vertices.str() << "\n" << indices.str() << "\n";

    if(!options.disable_info)
    {
        std::cout
            << materials.str() << "\n"
            << meshes.str() << "\n"
            << private_declaration.str()
            << private_content.str() << "\n"
            << "static MODELHEADER_CONST modelheader_node* "
            << options.name_prefix << "_nodes = "
            << options.name_prefix << "_private_data.nodes;\n\n";
    }

    std::cout
        << "#define " << options.name_prefix
        << "_vertex_stride " << vertex_stride << "\n"
        << "#define " << options.name_prefix
        << "_vertex_count " << vertex_count << "\n"
        << "#define " << options.name_prefix
        << "_index_count " << index_count << "\n"
        << "#define " << options.name_prefix
        << "_position_offset " << position_offset << "\n"
        << "#define " << options.name_prefix
        << "_normal_offset " << normal_offset << "\n"
        << "#define " << options.name_prefix
        << "_uv0_offset " << uv0_offset << "\n";
    if(!options.disable_info)
    {
        std::cout
            << "#define " << options.name_prefix
            << "_material_count " << material_count << "\n"
            << "#define " << options.name_prefix
            << "_mesh_count " << mesh_count << "\n"
            << "#define " << options.name_prefix
            << "_node_count " << node_count << "\n";
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    if(!parse_args(argv)) return 1;
    if(options.name_prefix == "")
    {
        size_t start = options.input_file.find_last_of("/\\");
        if(start == std::string::npos) start = 0;
        else start++;
        options.name_prefix = options.input_file.substr(
            start, options.input_file.find('.', start)
        );
        std::transform(
            options.name_prefix.begin(),
            options.name_prefix.end(),
            options.name_prefix.begin(),
            [](char c){
                c = tolower(c);
                if(isspace(c) || ispunct(c)) c = '_';
                return c;
            }
        );
        options.name_prefix.erase(
            std::remove_if(
                options.name_prefix.begin(),
                options.name_prefix.end(),
                [](char c){ return !isalnum(c) && c != '_'; }
            ),
            options.name_prefix.end()
        );
    }

    options.uppercase_name_prefix = options.name_prefix;
    std::transform(
        options.name_prefix.begin(),
        options.name_prefix.end(),
        options.uppercase_name_prefix.begin(),
        toupper
    );

    Assimp::Importer importer;
    importer.SetPropertyInteger(
        AI_CONFIG_PP_RVC_FLAGS,
        aiComponent_COLORS |
        aiComponent_BONEWEIGHTS |
        aiComponent_ANIMATIONS |
        aiComponent_LIGHTS |
        aiComponent_CAMERAS
    );

    auto flags =
        aiProcessPreset_TargetRealtime_MaxQuality |
        aiProcess_FlipUVs |
        aiProcess_RemoveComponent;
    if(options.pretransform) flags |= aiProcess_PreTransformVertices;

    const aiScene* scene = importer.ReadFile(options.input_file, flags);

    if(!scene)
    {
        std::cerr << "Failed to open file " << options.input_file << std::endl;
        return 1;
    }

    write_preamble();
    write_scene(scene);
    write_prologue();

    return 0;
}

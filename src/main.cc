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
        << "Usage: " << name << " [-p] [-n name_prefix] model_file" << std::endl
        << "-p disables pre-transformed primitives." << std::endl
        << "-n sets the default name prefix for the model." << std::endl;
}

struct
{
    std::string input_file;
    std::string name_prefix;
    std::string uppercase_name_prefix;
    bool pretransform = true;
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
        "#ifndef MODELHEADER_TYPES_DECLARED\n"
        "#define MODELHEADER_TYPES_DECLARED\n"
        "\n"
        "struct modelheader_mesh\n"
        "{\n"
        "    const char* name;\n"
        "    unsigned start_index;\n"
        "    unsigned size;\n"
        "};\n"
        "\n"
        "struct modelheader_node\n"
        "{\n"
        "    struct modelheader_mesh** meshes;\n"
        "    size_t mesh_count;\n"
        "\n"
        "    struct modelheader_node* parent;\n"
        "    struct modelheader_node** children;\n"
        "    size_t child_count;\n"
        "\n"
        "    float transform[16];\n"
        "};\n"
        "\n"
        "#endif\n\n";
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
    std::stringstream& private_content,
    bool content_first = true
){
    if(!node) return;
    if(node_count != 0) nodes << "\n";

    unsigned index = node_count++;

    nodes << "    {";

    if(node->mNumMeshes > 0)
    {
        private_declaration
            << "    struct modelheader_mesh* meshes_"
            << index << "[" << node->mNumMeshes << "];\n";

        if(!content_first) private_content << "\n";
        else content_first = false;

        private_content << "    {\n";
        for(unsigned i = 0; i < node->mNumMeshes; ++i)
        {
            if(i != 0) private_content << "\n";
            private_content
                << "        &" << options.name_prefix << "_meshes["
                << mesh_key.at(node->mMeshes[i]) << "],";
        }

        private_content.seekp(-1,private_content.cur);
        private_content << "\n    },";

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
            << "&" << options.name_prefix << "_nodes["
            << node_key.at(node->mParent) << "], ";
    else nodes << "NULL, ";

    if(node->mNumChildren > 0)
    {
        private_declaration
            << "    struct modelheader_node* children_"
            << index << "[" << node->mNumChildren << "];\n";

        if(!content_first) private_content << "\n";
        else content_first = false;

        private_content << "    {\n";
        for(unsigned i = 0; i < node->mNumChildren; ++i)
        {
            if(i != 0) private_content << "\n";
            private_content
                << "        &" << options.name_prefix << "_nodes["
                << node_key.at(node->mChildren[i]) << "],";
        }

        private_content.seekp(-1,private_content.cur);
        private_content << "\n    },";
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
        nodes << node->mTransformation[i/4][i%4];
        if(i != 4*4-1) nodes << ",";
    }
    nodes << "}},";

    for(unsigned i = 0; i < node->mNumChildren; ++i)
    {
        write_node(
            node_count,
            mesh_key,
            node_key,
            node->mChildren[i],
            nodes,
            private_declaration,
            private_content,
            content_first
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
    std::stringstream meshes;
    std::stringstream nodes;
    std::stringstream private_declaration;
    std::stringstream private_content;

    unsigned vertex_count = 0;
    unsigned vertex_stride = 0;
    unsigned index_count = 0;
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
        << "float " << options.name_prefix << "_vertices[] = {\n    ";
    indices
        << "unsigned " << options.name_prefix << "_indices[] = {\n    ";
    meshes
        << "struct modelheader_mesh "
        << options.name_prefix << "_meshes[] = {\n";
    private_declaration
        << "extern struct modelheader_node " << options.name_prefix
        << "_nodes[];\n\n"
        << "struct {\n";
    private_content
        << "} " << options.name_prefix << "_private_data = {\n";
    nodes
        << "struct modelheader_node "
        << options.name_prefix << "_nodes[] = {\n";

    /* Vertex format pre-pass */
    for(unsigned i = 0; i < scene->mNumMeshes; ++i)
    {
        aiMesh* inmesh = scene->mMeshes[i];
        position_present |= inmesh->HasPositions();
        normal_present |= inmesh->HasNormals();
        uv0_present |= inmesh->HasTextureCoords(0);
    }

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
        if(i != 0) meshes << "\n";
        meshes
            << "    {" << escape_string(inmesh->mName.C_Str()) << ", "
            << start_index << ", " << size << "},";
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

    vertices.seekp(-1,vertices.cur);
    vertices << "\n};\n";
    indices.seekp(-1,indices.cur);
    indices << "\n};\n";
    meshes.seekp(-1,meshes.cur);
    meshes << "\n};\n";
    nodes.seekp(-1,nodes.cur);
    nodes << "\n};\n";
    private_content.seekp(-1,private_content.cur);
    private_content << "\n};\n";

    std::cout
        << vertices.str() << "\n" << indices.str() << "\n"
        << meshes.str() << "\n"
        << private_declaration.str()
        << private_content.str() << "\n"
        << nodes.str() << "\n"
        << "#define " << options.name_prefix
        << "_vertex_stride " << vertex_stride << "\n"
        << "#define " << options.name_prefix
        << "_vertex_count " << vertex_count << "\n"
        << "#define " << options.name_prefix
        << "_index_count " << index_count << "\n"
        << "#define " << options.name_prefix
        << "_mesh_count " << mesh_count << "\n"
        << "#define " << options.name_prefix
        << "_node_count " << node_count << "\n"
        << "#define " << options.name_prefix
        << "_position_offset " << position_offset << "\n"
        << "#define " << options.name_prefix
        << "_normal_offset " << normal_offset << "\n"
        << "#define " << options.name_prefix
        << "_uv0_offset " << uv0_offset << "\n";
}

int main(int argc, char** argv)
{
    (void)argc;
    if(!parse_args(argv)) return 1;
    if(options.name_prefix == "")
    {
        size_t start = options.input_file.find_last_of("/\\");
        if(start == std::string::npos) start = 0;
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
        options.uppercase_name_prefix = options.name_prefix;
        std::transform(
            options.name_prefix.begin(),
            options.name_prefix.end(),
            options.uppercase_name_prefix.begin(),
            toupper
        );
    }


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

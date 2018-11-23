#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
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

void write_preamble()
{
    std::cout <<
        "/* Automatically generated header from file \"" << options.input_file
        << "\" */\n"
        "#ifndef MODELHEADER_MODEL_" << options.uppercase_name_prefix << "_H\n"
        "#define MODELHEADER_MODEL_" << options.uppercase_name_prefix << "_H\n"
        "#ifndef MODELHEADER_TYPES_DECLARED\n"
        "#define MODELHEADER_TYPES_DECLARED\n"
        "struct modelheader_primitive\n"
        "{\n"
        "    const char* name;\n"
        "    unsigned start_index;\n"
        "    unsigned size;\n"
        "\n"
        "    struct modelheader_primitive* parent;\n"
        "    struct modelheader_primitive** children;\n"
        "    unsigned child_count;\n"
        "    float transform[16];\n"
        "};\n"
        "#endif\n\n";
}

void write_prologue()
{
    std::cout <<
        "#endif\n";
}

void write_scene(const aiScene* scene)
{
    std::stringstream vertices;
    std::stringstream indices;
    std::stringstream primitives;

    unsigned vertex_count = 0;
    unsigned vertex_stride = 0;
    unsigned index_count = 0;
    unsigned primitive_count = 0;
    unsigned position_offset = 0;
    unsigned normal_offset = 0;
    unsigned uv0_offset = 0;

    vertices << "float " << options.name_prefix << "_vertices[] = {\n";
    indices << "unsigned " << options.name_prefix << "_indices[] = {\n";
    primitives
        << "struct modelheader_primitive "
        << options.name_prefix << "_primitives[] = {\n";

    /* TODO loop scene */

    vertices << "};\n";
    indices << "};\n";
    primitives << "};\n";

    std::cout
        << vertices.str() << "\n" << indices.str() << "\n"
        << primitives.str() << "\n"
        << "#define " << options.name_prefix
        << "_vertex_stride " << vertex_stride << "\n"
        << "#define " << options.name_prefix
        << "_vertex_count " << vertex_count << "\n"
        << "#define " << options.name_prefix
        << "_index_count " << index_count << "\n"
        << "#define " << options.name_prefix
        << "_primitive_count " << primitive_count << "\n"
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

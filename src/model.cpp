#include "model.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <algorithm>

Triangle::Triangle()
{
    // default this. to prevent potential garbage
    for (int i=0; i<3; i++)
    {
        vertex[i] = glm::vec4(0.0f,0.0f,0.0f,1.0f);
        normal[i] = glm::vec4(1.0f,0.0f,0.0f,0.0f);
        color[i]  = glm::vec4(0.0f,0.0f,0.0f,1.0f);
        texcoord[i] = glm::vec2(0.0f, 0.0f);
        material_index[i] = 0;
    }
    face_normal = glm::vec4(1.0f,0.0f,0.0f,0.0f);
}

Material::Material()
{
    // default this. to prevent potential garbage
    ambient  = glm::vec3(1.0f,1.0f,1.0f);
    diffuse  = glm::vec3(1.0f,1.0f,1.0f);
    specular = glm::vec3(1.0f,1.0f,1.0f);
    shine    = 1.0f;
}

Model::Model()
{
    num_triangles = 0;
    num_materials = 0;

    triangles = NULL;
    materials = NULL;
    has_texture = false;

    const float maxval = std::numeric_limits<float>::max();
    const float minval = -maxval;

    bbox_min = glm::vec3(maxval,maxval,maxval);
    bbox_max = glm::vec3(minval,minval,minval);
}

Model::~Model()
{
    if (triangles)
        delete[] triangles;

    if (materials)
        delete[] materials;
}

void Model::read_from_file(const char *FileName)
{
    int i;
    char ch;

    FILE* fp = fopen(FileName,"r");
    if (fp==NULL)
    {
        printf("ERROR: unable to open TriObj [%s]!\n",FileName);
        exit(1);
    }

    fscanf(fp, "%c", &ch);
    while(ch!= '\n') // skip the first line - object's name
        fscanf(fp, "%c", &ch);

    fscanf(fp,"# triangles = %d\n",     &num_triangles); // read # of triangles
    fscanf(fp,"Material count = %d\n",  &num_materials); // read material count

    //sanity checks
    if (num_triangles < 1)
    {
        printf("ERROR: TriObj [%s] has an invalid number of triangles!\n",FileName);
        exit(1);
    }
    if (num_materials < 1)
    {
        printf("ERROR: TriObj [%s] has an invalid number of materials!\n",FileName);
        exit(1);
    }

    printf ("Reading in %s (%d materials). . .\n", FileName, num_materials);
    materials = new Material[num_materials];

    for (i=0; i<num_materials; i++) // read materials
    {
        fscanf(fp, "ambient color %f %f %f\n",  &(materials[i].ambient.x), &(materials[i].ambient.y), &(materials[i].ambient.z));
        fscanf(fp, "diffuse color %f %f %f\n",  &(materials[i].diffuse.x), &(materials[i].diffuse.y), &(materials[i].diffuse.z));
        fscanf(fp, "specular color %f %f %f\n", &(materials[i].specular.x), &(materials[i].specular.y), &(materials[i].specular.z));
        fscanf(fp, "material shine %f\n", &(materials[i].shine));
    }

    {
        char texture_flag[4] = "NO";
        fscanf(fp, "Texture = %3s\n", texture_flag);
        has_texture = (strcmp(texture_flag, "YES") == 0);
    }

    fscanf(fp, "%c", &ch);
    while(ch!= '\n') // skip documentation line
        fscanf(fp, "%c", &ch);

    printf ("Reading in %s (%d triangles). . .\n", FileName, num_triangles);
    triangles = new Triangle[num_triangles];

    for (i=0; i<num_triangles; i++) // read triangles
    {
        char line[256];
        int consumed;
        float s, t;

        if (!fgets(line, sizeof(line), fp))
        {
            printf("ERROR: unexpected end of file while reading triangle %d v0\n", i);
            exit(1);
        }
        if (sscanf(line, "v0 %f %f %f %f %f %f %d %n", &(triangles[i].vertex[0].x), &(triangles[i].vertex[0].y), &(triangles[i].vertex[0].z), &(triangles[i].normal[0].x), &(triangles[i].normal[0].y), &(triangles[i].normal[0].z), &(triangles[i].material_index[0]), &consumed) < 7)
        {
            printf("ERROR: malformed triangle %d v0 line\n", i);
            exit(1);
        }
        if (has_texture)
        {
            if (sscanf(line + consumed, "%f %f", &s, &t) != 2)
            {
                printf("ERROR: missing texture coordinates on triangle %d v0\n", i);
                exit(1);
            }
            triangles[i].texcoord[0] = glm::vec2(s, t);
        }
        else
        {
            triangles[i].texcoord[0] = glm::vec2(0.0f, 0.0f);
        }

        if (!fgets(line, sizeof(line), fp))
        {
            printf("ERROR: unexpected end of file while reading triangle %d v1\n", i);
            exit(1);
        }
        if (sscanf(line, "v1 %f %f %f %f %f %f %d %n", &(triangles[i].vertex[1].x), &(triangles[i].vertex[1].y), &(triangles[i].vertex[1].z), &(triangles[i].normal[1].x), &(triangles[i].normal[1].y), &(triangles[i].normal[1].z), &(triangles[i].material_index[1]), &consumed) < 7)
        {
            printf("ERROR: malformed triangle %d v1 line\n", i);
            exit(1);
        }
        if (has_texture)
        {
            if (sscanf(line + consumed, "%f %f", &s, &t) != 2)
            {
                printf("ERROR: missing texture coordinates on triangle %d v1\n", i);
                exit(1);
            }
            triangles[i].texcoord[1] = glm::vec2(s, t);
        }
        else
        {
            triangles[i].texcoord[1] = glm::vec2(0.0f, 0.0f);
        }

        if (!fgets(line, sizeof(line), fp))
        {
            printf("ERROR: unexpected end of file while reading triangle %d v2\n", i);
            exit(1);
        }
        if (sscanf(line, "v2 %f %f %f %f %f %f %d %n", &(triangles[i].vertex[2].x), &(triangles[i].vertex[2].y), &(triangles[i].vertex[2].z), &(triangles[i].normal[2].x), &(triangles[i].normal[2].y), &(triangles[i].normal[2].z), &(triangles[i].material_index[2]), &consumed) < 7)
        {
            printf("ERROR: malformed triangle %d v2 line\n", i);
            exit(1);
        }
        if (has_texture)
        {
            if (sscanf(line + consumed, "%f %f", &s, &t) != 2)
            {
                printf("ERROR: missing texture coordinates on triangle %d v2\n", i);
                exit(1);
            }
            triangles[i].texcoord[2] = glm::vec2(s, t);
        }
        else
        {
            triangles[i].texcoord[2] = glm::vec2(0.0f, 0.0f);
        }

        fscanf(fp, "face normal %f %f %f\n",    &(triangles[i].face_normal.x), &(triangles[i].face_normal.y), &(triangles[i].face_normal.z));

        //hmm.
        triangles[i].color[0].x = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[0]].diffuse.x));
        triangles[i].color[0].y = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[0]].diffuse.y));
        triangles[i].color[0].z = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[0]].diffuse.z));

        triangles[i].color[1].x = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[1]].diffuse.x));
        triangles[i].color[1].y = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[1]].diffuse.y));
        triangles[i].color[1].z = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[1]].diffuse.z));

        triangles[i].color[2].x = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[2]].diffuse.x));
        triangles[i].color[2].y = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[2]].diffuse.y));
        triangles[i].color[2].z = (float)(unsigned char)(int)(255*(materials[triangles[i].material_index[2]].diffuse.z));

        //bbox calculations go here
        for (int j = 0; j < 3; j++)
        {
            bbox_min.x = std::min(bbox_min.x, triangles[i].vertex[j].x);
            bbox_min.y = std::min(bbox_min.y, triangles[i].vertex[j].y);
            bbox_min.z = std::min(bbox_min.z, triangles[i].vertex[j].z);

            bbox_max.x = std::max(bbox_max.x, triangles[i].vertex[j].x);
            bbox_max.y = std::max(bbox_max.y, triangles[i].vertex[j].y);
            bbox_max.z = std::max(bbox_max.z, triangles[i].vertex[j].z);
        }
    }

    fclose(fp);
}

void Model::write_to_file(const char *FileName)
{
    const char* filesufix = ".in";
    std::string combined = std::string(FileName) + filesufix; //why yes I did include <iostream> exclusively for this one operation why do you ask
    const char* result = combined.c_str();
    FILE* fp = fopen(result, "w");
    if (fp == NULL)
    {
        printf("ERROR: unable to open file for writing [%s]!\n", result);
        return;
    }

    // Write header
    fprintf(fp, "Model Export\n");
    fprintf(fp, "# triangles = %d\n", num_triangles);
    fprintf(fp, "Material count = %d\n", num_materials);

    // Write materials
    for (int i = 0; i < num_materials; i++)
    {
        fprintf(fp, "ambient color %f %f %f\n", materials[i].ambient.x, materials[i].ambient.y, materials[i].ambient.z);
        fprintf(fp, "diffuse color %f %f %f\n", materials[i].diffuse.x, materials[i].diffuse.y, materials[i].diffuse.z);
        fprintf(fp, "specular color %f %f %f\n", materials[i].specular.x, materials[i].specular.y, materials[i].specular.z);
        fprintf(fp, "material shine %f\n", materials[i].shine);
    }

    // Write texture flag
    fprintf(fp, "Texture = %s\n", has_texture ? "YES" : "NO");
    fprintf(fp, "End of materials\n");

    // Write triangles
    for (int i = 0; i < num_triangles; i++)
    {
        if (has_texture)
        {
            fprintf(fp, "v0 %f %f %f %f %f %f %d %f %f\n",
                triangles[i].vertex[0].x, triangles[i].vertex[0].y, triangles[i].vertex[0].z,
                triangles[i].normal[0].x, triangles[i].normal[0].y, triangles[i].normal[0].z,
                triangles[i].material_index[0],
                triangles[i].texcoord[0].x, triangles[i].texcoord[0].y);
            fprintf(fp, "v1 %f %f %f %f %f %f %d %f %f\n",
                triangles[i].vertex[1].x, triangles[i].vertex[1].y, triangles[i].vertex[1].z,
                triangles[i].normal[1].x, triangles[i].normal[1].y, triangles[i].normal[1].z,
                triangles[i].material_index[1],
                triangles[i].texcoord[1].x, triangles[i].texcoord[1].y);
            fprintf(fp, "v2 %f %f %f %f %f %f %d %f %f\n",
                triangles[i].vertex[2].x, triangles[i].vertex[2].y, triangles[i].vertex[2].z,
                triangles[i].normal[2].x, triangles[i].normal[2].y, triangles[i].normal[2].z,
                triangles[i].material_index[2],
                triangles[i].texcoord[2].x, triangles[i].texcoord[2].y);
        }
        else
        {
            fprintf(fp, "v0 %f %f %f %f %f %f %d\n",
                triangles[i].vertex[0].x, triangles[i].vertex[0].y, triangles[i].vertex[0].z,
                triangles[i].normal[0].x, triangles[i].normal[0].y, triangles[i].normal[0].z,
                triangles[i].material_index[0]);
            fprintf(fp, "v1 %f %f %f %f %f %f %d\n",
                triangles[i].vertex[1].x, triangles[i].vertex[1].y, triangles[i].vertex[1].z,
                triangles[i].normal[1].x, triangles[i].normal[1].y, triangles[i].normal[1].z,
                triangles[i].material_index[1]);
            fprintf(fp, "v2 %f %f %f %f %f %f %d\n",
                triangles[i].vertex[2].x, triangles[i].vertex[2].y, triangles[i].vertex[2].z,
                triangles[i].normal[2].x, triangles[i].normal[2].y, triangles[i].normal[2].z,
                triangles[i].material_index[2]);
        }
        fprintf(fp, "face normal %f %f %f\n", triangles[i].face_normal.x, triangles[i].face_normal.y, triangles[i].face_normal.z);
    }

    fclose(fp);
    printf("Model saved to %s\n", result);
}


void Model::tris_to_arrays(float* &vert, float* &norm, float* &tex)
{
    vert = new float[9*num_triangles];
    norm = new float[9*num_triangles];
    tex = nullptr;
    if (has_texture)
        tex = new float[6 * num_triangles]; // 3 verts * 2 coords

    for (int i=0; i<num_triangles; i++)
    {
        // vertex coordinates
        (vert)[9*i]     = triangles[i].vertex[0].x;
        (vert)[9*i + 1] = triangles[i].vertex[0].y;
        (vert)[9*i + 2] = triangles[i].vertex[0].z;
        (vert)[9*i + 3] = triangles[i].vertex[1].x;
        (vert)[9*i + 4] = triangles[i].vertex[1].y;
        (vert)[9*i + 5] = triangles[i].vertex[1].z;
        (vert)[9*i + 6] = triangles[i].vertex[2].x;
        (vert)[9*i + 7] = triangles[i].vertex[2].y;
        (vert)[9*i + 8] = triangles[i].vertex[2].z;
        // vertex normal coordinates
        (norm)[9*i]     = triangles[i].normal[0].x;
        (norm)[9*i + 1] = triangles[i].normal[0].y;
        (norm)[9*i + 2] = triangles[i].normal[0].z;
        (norm)[9*i + 3] = triangles[i].normal[1].x;
        (norm)[9*i + 4] = triangles[i].normal[1].y;
        (norm)[9*i + 5] = triangles[i].normal[1].z;
        (norm)[9*i + 6] = triangles[i].normal[2].x;
        (norm)[9*i + 7] = triangles[i].normal[2].y;
        (norm)[9*i + 8] = triangles[i].normal[2].z;
        // texture coordinates (s,t) per vertex if present
        if (has_texture && tex)
        {
            // each triangle contributes 6 floats: s0,t0,s1,t1,s2,t2
            tex[6*i + 0] = triangles[i].texcoord[0].x;
            tex[6*i + 1] = triangles[i].texcoord[0].y;
            tex[6*i + 2] = triangles[i].texcoord[1].x;
            tex[6*i + 3] = triangles[i].texcoord[1].y;
            tex[6*i + 4] = triangles[i].texcoord[2].x;
            tex[6*i + 5] = triangles[i].texcoord[2].y;
        }
    }
}

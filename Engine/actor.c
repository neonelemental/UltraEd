#include <nusys.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "upng.h"
#include "actor.h"
#include "utilities.h"

actor *load_model(void *data_start, void *data_end, double positionX, double positionY, double positionZ,
    double rotX, double rotY, double rotZ, double angle, double scaleX, double scaleY, double scaleZ, 
    double centerX, double centerY, double centerZ, double radius,
    double extentX, double extentY, double extentZ, enum colliderType collider)
{
    return load_model_with_texture(data_start, data_end,
        NULL, NULL, 0, 0, positionX, positionY, positionZ, rotX, rotY, rotZ, angle, scaleX, scaleY, scaleZ, 
        centerX, centerY, centerZ, radius, extentX, extentY, extentZ, collider);
}

actor *load_model_with_texture(void *data_start, void *data_end, void *texture_start, void *texture_end,
    int textureWidth, int textureHeight, double positionX, double positionY, double positionZ, double rotX, 
    double rotY, double rotZ, double angle, double scaleX, double scaleY, double scaleZ, 
    double centerX, double centerY, double centerZ, double radius,
    double extentX, double extentY, double extentZ, enum colliderType collider)
{
    unsigned char data_buffer[200000];
    unsigned char texture_buffer[20000];
    int data_size = data_end - data_start;
    int texture_size = texture_end - texture_start;
    actor *new_model;

    // Transfer from ROM the model mesh data and texture.
    rom_2_ram(data_start, data_buffer, data_size);
    rom_2_ram(texture_start, texture_buffer, texture_size);

    new_model = (actor*)malloc(sizeof(actor));
    new_model->mesh = (mesh*)malloc(sizeof(mesh));
    new_model->position = (vector3*)malloc(sizeof(vector3));
    new_model->rotationAxis = (vector3*)malloc(sizeof(vector3));
    new_model->scale = (vector3*)malloc(sizeof(vector3));
    new_model->visible = 1;
    new_model->type = Model;
    new_model->collider = collider;
    new_model->texture = NULL;
    new_model->textureWidth = textureWidth;
    new_model->textureHeight = textureHeight;

    new_model->center = (vector3*)malloc(sizeof(vector3));
    new_model->center->x = centerX;
    new_model->center->y = centerY;
    new_model->center->z = centerZ;
    new_model->radius = radius;

    new_model->extents = (vector3 *)malloc(sizeof(vector3));
    new_model->extents->x = extentX;
    new_model->extents->y = extentY;
    new_model->extents->z = extentZ;

    // Read how many vertices for this mesh.
    int vertex_count = 0;
    char *line = (char*)strtok(data_buffer, "\n");
    sscanf(line, "%i", &vertex_count);
    new_model->mesh->vertices = (Vtx*)malloc(vertex_count * sizeof(Vtx));
    new_model->mesh->vertex_count = vertex_count;

    // Gather all of the X, Y, and Z vertex info.
    for (int i = 0; i < vertex_count; i++)
    {
        double x, y, z, r, g, b, a, s, t;
        line = (char*)strtok(NULL, "\n");
        sscanf(line, "%lf %lf %lf %lf %lf %lf %lf %lf %lf", &x, &y, &z, &r, &g, &b, &a, &s, &t);

        new_model->mesh->vertices[i].v.ob[0] = x * scaleX * 1000;
        new_model->mesh->vertices[i].v.ob[1] = y * scaleY * 1000;
        new_model->mesh->vertices[i].v.ob[2] = -z * scaleZ * 1000;
        new_model->mesh->vertices[i].v.flag = 0;
        new_model->mesh->vertices[i].v.tc[0] = (int)(s * textureWidth) << 5;
        new_model->mesh->vertices[i].v.tc[1] = (int)(t * textureHeight) << 5;
        new_model->mesh->vertices[i].v.cn[0] = r * 255;
        new_model->mesh->vertices[i].v.cn[1] = g * 255;
        new_model->mesh->vertices[i].v.cn[2] = b * 255;
        new_model->mesh->vertices[i].v.cn[3] = a * 255;
    }

    // Entire axis can't be zero or it won't render.
    if (rotX == 0.0 && rotY == 0.0 && rotZ == 0.0) rotZ = 1;

    new_model->position->x = positionX;
    new_model->position->y = positionY;
    new_model->position->z = -positionZ;
    new_model->scale->x = 0.001;
    new_model->scale->y = 0.001;
    new_model->scale->z = 0.001;
    new_model->rotationAxis->x = rotX;
    new_model->rotationAxis->y = rotY;
    new_model->rotationAxis->z = -rotZ;
    new_model->rotationAngle = -angle;

    // Load in the png texture data.
    upng_t *png = upng_new_from_bytes(texture_buffer, texture_size);
    if (png != NULL)
    {
        upng_decode(png);
        if (upng_get_error(png) == UPNG_EOK)
        {
            // Convert texture data from 24bpp to 16bpp in RGB5551 format.
            new_model->texture = image_24_to_16(upng_get_buffer(png), textureWidth, textureHeight);
        }
        upng_free(png);
    }

    return new_model;
}

void model_draw(actor *model, Gfx **display_list)
{
    if (!model->visible) return;

    guTranslate(&model->transform.translation, model->position->x,
        model->position->y, model->position->z);

    guRotate(&model->transform.rotation, model->rotationAngle,
        model->rotationAxis->x, model->rotationAxis->y, model->rotationAxis->z);

    guScale(&model->transform.scale, model->scale->x,
        model->scale->y, model->scale->z);

    gSPMatrix((*display_list)++, OS_K0_TO_PHYSICAL(&model->transform.translation),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_PUSH);

    gSPMatrix((*display_list)++, OS_K0_TO_PHYSICAL(&model->transform.rotation),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

    gSPMatrix((*display_list)++, OS_K0_TO_PHYSICAL(&model->transform.scale),
        G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH);

    gDPPipeSync((*display_list)++);

    gDPSetCycleType((*display_list)++, G_CYC_1CYCLE);
    gDPSetRenderMode((*display_list)++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gSPClearGeometryMode((*display_list)++, 0xFFFFFFFF);
    gSPSetGeometryMode((*display_list)++, G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER | G_CULL_FRONT);

    if (model->texture != NULL)
    {
        gSPTexture((*display_list)++, 0xffff, 0xffff, 0, G_TX_RENDERTILE, G_ON);
        gDPSetTextureFilter((*display_list)++, G_TF_BILERP);
        gDPSetTexturePersp((*display_list)++, G_TP_PERSP);
        gDPSetCombineMode((*display_list)++, G_CC_MODULATERGB, G_CC_MODULATERGB);
        gDPLoadTextureBlock((*display_list)++, model->texture, G_IM_FMT_RGBA, G_IM_SIZ_16b, 
            model->textureWidth, model->textureHeight, 0,
            G_TX_WRAP, G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    }
    else
    {
        gDPSetCombineMode((*display_list)++, G_CC_SHADE, G_CC_SHADE);
    }

    int offset = 0;
    int remaining_vertices = model->mesh->vertex_count;

    // Send vertex data in batches of 30.
    while (remaining_vertices >= 30)
    {
        gSPVertex((*display_list)++, &(model->mesh->vertices[offset]), 30, 0);
        gDPPipeSync((*display_list)++);

        for (int i = 0; i < 30 / 3; i++)
        {
            gSP1Triangle((*display_list)++, i * 3, i * 3 + 1, i * 3 + 2, 0);
        }

        remaining_vertices -= 30;
        offset += 30;
    }

    // Process last remaining vertices.
    if (remaining_vertices > 0)
    {
        gSPVertex((*display_list)++, &(model->mesh->vertices[offset]), remaining_vertices, 0);
        gDPPipeSync((*display_list)++);

        for (int i = 0; i < remaining_vertices / 3; i++)
        {
            gSP1Triangle((*display_list)++, i * 3, i * 3 + 1, i * 3 + 2, 0);
        }
    }

    gSPPopMatrix((*display_list)++, G_MTX_MODELVIEW);
}

actor *create_camera(double positionX, double positionY, double positionZ,
    double rotX, double rotY, double rotZ, double angle, 
    double centerX, double centerY, double centerZ, double radius,
    double extentX, double extentY, double extentZ, enum colliderType collider)
{
    actor *camera = (actor*)malloc(sizeof(actor));
    camera->position = (vector3*)malloc(sizeof(vector3));
    camera->rotationAxis = (vector3*)malloc(sizeof(vector3));
    camera->visible = 1;
    camera->type = Camera;
    camera->collider = collider;

    camera->center = (vector3*)malloc(sizeof(vector3));
    camera->center->x = centerX;
    camera->center->y = centerY;
    camera->center->z = centerZ;
    camera->radius = radius;

    camera->extents = (vector3 *)malloc(sizeof(vector3));
    camera->extents->x = extentX;
    camera->extents->y = extentY;
    camera->extents->z = extentZ;

    if (rotX == 0.0 && rotY == 0.0 && rotZ == 0.0) rotZ = 1;
    camera->position->x = positionX;
    camera->position->y = positionY;
    camera->position->z = positionZ;
    camera->rotationAxis->x = rotX;
    camera->rotationAxis->y = rotY;
    camera->rotationAxis->z = rotZ;
    camera->rotationAngle = angle;

    return camera;
}

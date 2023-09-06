#include <limits>
#include "platform/win32.h"
#include "core/camera.h"
#include "core/graphics.h"

void ClearZbuffer(int width, int height, float* zbuffer, float* shadowbuffer)
{
    for (int i = width * height; --i; ) {
        zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
    }
}

void ClearFramebuffer(int width, int height, unsigned char* framebuffer)
{
    memset(framebuffer, 50, width * height * 4);
}

void UpdateMatrix(Camera &camera)
{
    ModelView = lookat(camera.eye, camera.target, camera.up);
    Projection = projection(-1.f/(camera.eye-camera.target).norm());
}

int main(int argc, char** argv) 
{
    model = new Model("obj/pbr/backpack.obj");
    Camera camera(eye, target, up, (float)(width) / height);
    ModelView = lookat(eye, target, up);
    Projection = projection(-1.f/(eye-target).norm());
    Viewport = viewport(width/8, height/8, width*3/4, height*3/4);
    light_dir = proj<3>(Projection*ModelView*embed<4>(light_dir, 0.f));

    window_init(width, height, L"Renderers");
 
    DrawMode dm = Triangle; 

    DepthShader depthShader;
    PBRShader shader;
    light_dir_depth.normalize();
    Matrix depthM, normalM;
    std::vector<int> face;
    Vec3f pts[3];
    // render loop

    depth.clear();

    ClearZbuffer(width, height, zbuffer, shadowbuffer);
    ClearFramebuffer(width, height, image.buffer());

    switch (dm) {
    case Point:
        DrawPoint(200, 200, image, red);
        break;
    case Line:

        DrawLineBresenham(80, 40, 200, 200, image, red);
        break;
    case WireFrame:
    {
        for (int i = 0; i < model->nfaces(); i++) {
            face = model->face(i);
            for (int j = 0; j < 3; j++) {
                pts[j] = proj<3>(Viewport * Projection * ModelView *
                    embed<4>(model->vert(face[j])));
            }
            DrawWireframe(pts, image, red);
        }
    }
    break;
    case Triangle:
    {
        {
            lookat(light_dir_depth, camera.target, camera.up);
            projection(0);
            viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);

            for (int i = 0; i < model->nfaces(); i++) {
                for (int j = 0; j < 3; j++) {
                    depthShader.vertex(i, j);
                }
                DrawTriangle(depthShader.varying_tri, depthShader, depth, shadowbuffer, camera);
            }
        }
        depthM = Projection * ModelView;
        { 
            lookat(camera.eye, camera.target, camera.up);
            projection(-1.f / ((camera.eye - camera.target).norm()));
            viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);

            Mshadow = depthM * (Projection * ModelView).invert();
            normalM = (Projection * ModelView).invert_transpose();

            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    NormalMatrix[i][j] = normalM[i][j];

            for (int i = 0; i < model->nfaces(); i++) {
                for (int j = 0; j < 3; j++) {
                    shader.vertex(i, j);
                }
                DrawTriangle(shader.varying_tri, shader, image, zbuffer, camera);
            }
        }
    }
    break;
    }

    depth.flip_vertically();
    depth.write_tga_file("../depth.tga");
    image.flip_vertically();
    image.write_tga_file("../output.tga");

    while(!window->is_close){
        // send framebuffer to window to display
        window_draw(image.buffer());
        //msg_dispatch();
    }

    image.write_tga_file("../output.tga");
    window_destroy();
    delete model;
    delete [] zbuffer;
    delete [] shadowbuffer;
    return 0;
}
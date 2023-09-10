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
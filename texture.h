#ifndef __TEXTURE_H__
#define ___TEXTURE_H__
#include "geometry.h"
#include "tgaimage.h"

class TGAImage;

class Texture
{
public:
    Texture(const char* file_name);
    ~Texture();

    TGAColor get_color(Vec2f uv);
    Vec2i get_coordinate(Vec2f uv);
    Vec3f get_normal(Vec2f uv);
    float get_specular(Vec2f uv);

private:
    TGAImage* img;
};

#endif

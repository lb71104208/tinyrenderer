#include "texture.h"

#include "tgaimage.h"

Texture::Texture(const char* file_name)
{
    img = new TGAImage();
    img->read_tga_file(file_name);
    img->flip_vertically();
}

Texture::~Texture()
{
    delete img;
    img = nullptr;
}

TGAColor Texture::get_color(Vec2f uv)
{
    Vec2i coord = get_coordinate(uv);
    return img->get(coord.x, coord.y);
}

Vec2i Texture::get_coordinate(Vec2f uv)
{
    return Vec2i(img->get_width() * uv.x, img->get_height() * uv.y);
}

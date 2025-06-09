#include <complex>
#define NOMINMAX
#include <Windows.h>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"
#include "texture.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0,   255);
const TGAColor blue = TGAColor(0, 0, 255,   255);

Model *model = nullptr;
Texture *texture = nullptr;

const int width  = 800;
const int height = 800;
const int depth  = 255;

Vec3f camera(0,0,3);
Vec3f light_dir = Vec3f(1,-1,1).normalize();
Vec3f eye(1,1,3);
Vec3f center(0,0,0);
Vec3f up(0,1,0);

void line(int x0, int y0, int x1, int y1, TGAImage &image, const TGAColor &color)
{
	bool steep = false;
	if (std::abs(x0 - x1) < std::abs(y0 - y1))
	{
		std::swap(x0, y0);
		std::swap(x1, y1);
		steep = true;
	}
	if (x0 > x1)
	{
		std::swap(x0, x1);
		std::swap(y0, y1);
	}
	int dx = x1 - x0;
	int dy = y1 - y0;
	int derror2 = std::abs(dy) *2;
	int error2 = 0;
	int y = y0;
	for (int x = x0; x <= x1; x++)
	{
		if (steep)
		{
			image.set(y, x, color);
		}
		else
		{
			image.set(x, y, color);
		}
		error2 += derror2;
		if (error2 > dx)
		{
			y += (y1 > y0 ? 1 : -1);
			error2 -= dx*2;
		}
	}
}

void line(Vec2i v0, Vec2i v1, TGAImage &image, const TGAColor &color)
{
	line(v0.x, v0.y, v1.x, v1.y, image, color);
}

void triangle(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, const TGAColor &color)
{
	if (t0.y==t1.y && t0.y==t2.y) return; // I dont care about degenerate triangles 
	// sort the vertices, t0, t1, t2 lower−to−upper (bubblesort yay!) 
	if (t0.y>t1.y) std::swap(t0, t1); 
	if (t0.y>t2.y) std::swap(t0, t2); 
	if (t1.y>t2.y) std::swap(t1, t2); 
	int total_height = t2.y-t0.y; 
	for (int i=0; i<total_height; i++) { 
		bool second_half = i>t1.y-t0.y || t1.y==t0.y; 
		int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y; 
		float alpha = (float)i/total_height; 
		float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height; // be careful: with above conditions no division by zero here 
		Vec2i A =               t0 + (t2-t0)*alpha; 
		Vec2i B = second_half ? t1 + (t2-t1)*beta : t0 + (t1-t0)*beta; 
		if (A.x>B.x) std::swap(A, B); 
		for (int j=A.x; j<=B.x; j++) { 
			image.set(j, t0.y+i, color); // attention, due to int casts t0.y+i != A.y 
		} 
	} 
}

void rasterize(Vec2i p0, Vec2i p1, TGAImage &image, const TGAColor &color, int ybuffer[])
{
	if (p0.x > p1.x)
	{
		std::swap(p0, p1);
	}
	for (int x = p0.x; x <= p1.x; x++)
	{
		float t = (x - p0.x)/(float)(p1.x - p0.x);
		int y = p0.y * (1 - t) + p1.y * t;
		if (ybuffer[x] < y)
		{
			ybuffer[x] = y;
			image.set(x, 8, color);
		}
	}
}

Vec3f world2screen(Vec3f v) {
	return Vec3f(int((v.x+1.)*width/2.+.5), int((v.y+1.)*height/2.+.5), v.z);
}

struct GouraudShader : IShader
{
	Vec3f varying_intensity;

	virtual Vec4f vertex(int iface, int nthvert)
	{
		Vec4f gl_vertex = embed<4>(model->vert(iface, nthvert));
		gl_vertex = Viewport*Projection*ModelView*gl_vertex;
		varying_intensity[nthvert] = model->normal(iface, nthvert) * light_dir;
		return gl_vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor& color) override
	{
		float intensity = bar * varying_intensity;
		color = TGAColor(255, 255, 255) * intensity;
		return false;
	}
};

int main(int argc, char** argv) {
	if (argc == 2)
	{
		model = new Model(argv[1]);
	}
	else
	{
		model = new Model("obj/african_head.obj");
	}

	lookat(eye, center, up);
	projection(-1.f/(eye - center).norm());
	viewport(width/8, height/8, width*3/4, height*3/4);
	light_dir.normalize();
	
	TGAImage image(width, height, TGAImage::RGB);
	TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

	texture = new Texture("obj/african_head_diffuse.tga");

	GouraudShader shader;
	for (int i = 0; i<model->nfaces(); i++)
	{
		Vec4f screen_coords[3];
		for (int j = 0; j<3; j++)
		{
			screen_coords[j] = shader.vertex(i, j);
		}
		triangle(screen_coords, shader, image, zbuffer);
	}
	
	image.  flip_vertically(); // to place the origin in the bottom left corner of the image
	zbuffer.flip_vertically();
	image.  write_tga_file("output.tga");
	zbuffer.write_tga_file("zbuffer.tga");
	
	delete model;
	return 0;
}


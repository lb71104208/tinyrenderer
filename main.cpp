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
Texture *texture_normal = nullptr;
Texture *texture_specular = nullptr;

const int width  = 800;
const int height = 800;
const int depth  = 255;

Vec3f camera(0,0,3);
Vec3f light_dir = Vec3f(1,1,1).normalize();
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

struct Shader : public IShader {
	mat<2,3,float> varying_uv;  // triangle uv coordinates, written by the vertex shader, read by the fragment shader
	mat<3,3,float> varying_nrm; // normal per vertex to be interpolated by FS
	mat<4,3, float> varying_tri;
	mat<3,3,float> ndc_tri;

	virtual Vec4f vertex(int iface, int nthvert) {
		varying_uv.set_col(nthvert, model->uv(iface, nthvert));
		varying_nrm.set_col(nthvert, proj<3>((Projection*ModelView).invert_transpose()*embed<4>(model->normal(iface, nthvert), 0.f)));
		Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
		varying_tri.set_col(nthvert, gl_Vertex);
		ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
		return gl_Vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor &color) {
		Vec3f bn = (varying_nrm*bar).normalize();
		Vec2f uv = varying_uv*bar;

		mat<3,3,float> A;
		A[0] = ndc_tri.col(1) - ndc_tri.col(0);
		A[1] = ndc_tri.col(2) - ndc_tri.col(0);
		A[2] = bn;

		mat<3,3,float> AI = A.invert();
		Vec3f i = AI * Vec3f(varying_uv[0][1] - varying_uv[0][0], varying_uv[0][2] - varying_uv[0][0], 0);
		Vec3f j = AI * Vec3f(varying_uv[1][1] - varying_uv[1][0], varying_uv[1][2] - varying_uv[1][0], 0);

		mat<3,3,float> B;
		B.set_col(0, i.normalize());
		B.set_col(1, j.normalize());
		B.set_col(2, bn);

		Vec3f n = (B*texture_normal->get_normal(uv)).normalize();
		
		float diff = std::max(0.f, n*light_dir);
		color = texture->get_color(uv)*diff;
		return false;
	}
};

int main(int argc, char** argv) {
	if (2>argc) {
		std::cerr << "Usage: " << argv[0] << " obj name" << std::endl;
		return 1;
	}

	lookat(eye, center, up);
	projection(-1.f/(eye - center).norm());
	viewport(width/8, height/8, width*3/4, height*3/4);
	light_dir.normalize();
	
	TGAImage image(width, height, TGAImage::RGB);
	float *zbuffer = new float[width*height];
	for (int i=width*height; i--; zbuffer[i] = -std::numeric_limits<float>::max());
	
	lookat(eye, center, up);
	viewport(width/8, height/8, width*3/4, height*3/4);
	projection(-1.f/(eye-center).norm());
	light_dir = proj<3>((Projection*ModelView*embed<4>(light_dir, 0.f))).normalize();

	for (int m=1; m<argc; m++) {
		model = new Model((std::string("obj/") + argv[m] + ".obj").c_str());
		texture = new Texture((std::string("obj/") + argv[m] + "_diffuse.tga").c_str());
		texture_normal = new Texture((std::string("obj/") + argv[m] + "_nm_tangent.tga").c_str());
		texture_specular = new Texture((std::string("obj/") + argv[m] + "_spec.tga").c_str());
		Shader shader;
		for (int i=0; i<model->nfaces(); i++) {
			for (int j=0; j<3; j++) {
				shader.vertex(i, j);
			}
			triangle(shader.varying_tri, shader, image, zbuffer);
		}
		delete model;
		delete texture;
		delete texture_normal;
		delete texture_specular;
	}
	
	image.flip_vertically(); // to place the origin in the bottom left corner of the image
	image.write_tga_file("output.tga");
	
	return 0;
}


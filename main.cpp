#define _USE_MATH_DEFINES
#include <complex>
#define NOMINMAX
#include <Windows.h>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0,   255);
const TGAColor blue = TGAColor(0, 0, 255,   255);

Model *model = nullptr;
float *shadowbuffer = NULL;

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
	mat<4,4,float> uniform_M;   //  Projection*ModelView
	mat<4,4,float> uniform_MIT; // (Projection*ModelView).invert_transpose()
	mat<4,4,float> uniform_Mshadow; // transform framebuffer screen coordinates to shadowbuffer screen coordinates

	Shader(Matrix M, Matrix MIT, Matrix MS) : uniform_M(M), uniform_MIT(MIT), uniform_Mshadow(MS), varying_uv(), varying_tri() {}

	virtual Vec4f vertex(int iface, int nthvert) {
		varying_uv.set_col(nthvert, model->uv(iface, nthvert));
		varying_nrm.set_col(nthvert, proj<3>((Projection*ModelView).invert_transpose()*embed<4>(model->normal(iface, nthvert), 0.f)));
		Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
		varying_tri.set_col(nthvert, gl_Vertex);
		ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
		return gl_Vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor &color) {
		// tangent nm map
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

		Vec3f n = (B*model->normal(uv)).normalize();

		// specular map
		Vec3f l = proj<3>(ModelView * embed<4>(light_dir)).normalize();
		Vec3f r = (n*(n * l * 2.f) - l).normalize();
		float spec = pow(std::max(r.z, 0.0f), model->specular(uv));

		// shadow map
		Vec4f screen_coord = Viewport*embed<4>(ndc_tri*bar);
		Vec4f sb_p = uniform_Mshadow*screen_coord; // corresponding point in the shadow buffer
		sb_p = sb_p/sb_p[3];
		int idx = int(sb_p[0]) + int(sb_p[1])*width; // index in the shadowbuffer array
		float shadow = .3+.7*(shadowbuffer[idx]<sb_p[2]+43.34);

		// final color (ambient + color*shadow*(diff + spec))
		float diff = std::max(0.f, n*l);
		color = model->diffuse(uv);
		for (int i = 0; i < 3; i++)
		{
			color[i] = std::min<float>(5 + color[i]*shadow*(1.2*diff + 0.6*spec), 255);
		}
		return false;
	}
};

struct DepthShader : public IShader {
	mat<3,3,float> varying_tri;

	DepthShader() : varying_tri() {}

	virtual Vec4f vertex(int iface, int nthvert) {
		Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
		gl_Vertex = Viewport*Projection*ModelView*gl_Vertex;          // transform it to screen coordinates
		varying_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
		return gl_Vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor &color) {
		Vec3f p = varying_tri*bar;
		color = TGAColor(255, 255, 255)*(p.z/depth);
		return false;
	}
};

struct ZShader : public IShader {
	mat<4,3,float> varying_tri;

	ZShader() : varying_tri() {}

	virtual Vec4f vertex(int iface, int nthvert) {
		Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
		varying_tri.set_col(nthvert, gl_Vertex);
		return gl_Vertex;
	}

	virtual bool fragment(Vec3f bar, TGAColor &color) {
		color = TGAColor(0, 0, 0);
		return false;
	}
};

float max_elevation_angle(float *zbuffer, Vec2f p, Vec2f dir) {
	float maxangle = 0;
	for (float t=0.; t<1000.; t+=1.) {
		Vec2f cur = p + dir*t;
		if (cur.x>=width || cur.y>=height || cur.x<0 || cur.y<0) return maxangle;

		float distance = (p-cur).norm();
		if (distance < 1.f) continue;
		float elevation = zbuffer[int(cur.x)+int(cur.y)*width]-zbuffer[int(p.x)+int(p.y)*width];
		maxangle = std::max(maxangle, atanf(elevation/distance));
	}
	return maxangle;
}

int main(int argc, char** argv) {
	if (2>argc) {
		std::cerr << "Usage: " << argv[0] << "obj/model.obj" << std::endl;
		return 1;
	}
	float *zbuffer = new float[width*height];
	shadowbuffer   = new float[width*height];
	for (int i=width*height; --i; ) {
		zbuffer[i] = shadowbuffer[i] = -std::numeric_limits<float>::max();
	}
	
	model = new Model(argv[1]);

	{ // rendering the shadow buffer
		TGAImage depth(width, height, TGAImage::RGB);
		lookat(light_dir, center, up);
		viewport(width/8, height/8, width*3/4, height*3/4);
		projection(0);

		DepthShader depthshader;
		Vec4f screen_coords[3];
		for (int i=0; i<model->nfaces(); i++) {
			for (int j=0; j<3; j++) {
				screen_coords[j] = depthshader.vertex(i, j);
			}
			triangle(screen_coords, depthshader, depth, shadowbuffer);
		}
		depth.flip_vertically(); // to place the origin in the bottom left corner of the image
		depth.write_tga_file("depth.tga");
	}

	Matrix M = Viewport*Projection*ModelView;
	
	{
		TGAImage frame(width, height, TGAImage::RGB);
		lookat(eye, center, up);
		viewport(width/8, height/8, width*3/4, height*3/4);
		projection(-1.f/(eye-center).norm());
		
		ZShader zshader;
		for (int i=0; i<model->nfaces(); i++) {
			for (int j=0; j<3; j++) {
				zshader.vertex(i, j);
			}
			triangle(zshader.varying_tri, zshader, frame, zbuffer);
		}
		
		Shader shader(ModelView, (Projection*ModelView).invert_transpose(), M*(Viewport*Projection*ModelView).invert());
		for (int i=0; i<model->nfaces(); i++) {
			for (int j=0; j<3; j++) {
				shader.vertex(i, j);
			}
			triangle(shader.varying_tri, shader, frame, zbuffer);
		}

		// SSAO
		for (int x=0; x<width; x++) {
			for (int y=0; y<height; y++) {
				if (zbuffer[x+y*width] < -1e5) continue;
				float total = 0;
				for (float a=0; a<M_PI*2-1e-4; a += M_PI/4) {
					total += M_PI/2 - max_elevation_angle(zbuffer, Vec2f(x, y), Vec2f(cos(a), sin(a)));
				}
				total /= (M_PI/2)*8;
				total = pow(total, 100.f);
				frame.set(x, y, frame.get(x, y) * total);
			}
		}
	
		frame.flip_vertically();
		frame.write_tga_file("framebuffer.tga");
	}
	
	delete [] zbuffer;
	delete [] shadowbuffer;
	delete model;
	return 0;
}


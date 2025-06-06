#include <complex>
#define NOMINMAX
#include <Windows.h>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "texture.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0,   255);
const TGAColor blue = TGAColor(0, 0, 255,   255);
Model *model = nullptr;
const int width  = 800;
const int height = 800;
const int depth  = 255;

Vec3f camera(0,0,3);
Vec3f light_dir = Vec3f(1,-1,1).normalize();
Vec3f eye(1,1,3);
Vec3f center(0,0,0);

Matrix viewport(int x, int y, int w, int h) {
	Matrix m = Matrix::identity(4);
	m[0][3] = x+w/2.f;
	m[1][3] = y+h/2.f;
	m[2][3] = depth/2.f;

	m[0][0] = w/2.f;
	m[1][1] = h/2.f;
	m[2][2] = depth/2.f;
	return m;
}

Matrix lookat(Vec3f eye, Vec3f center, Vec3f up)
{
	Vec3f z = (eye - center).normalize();
	Vec3f x = (up ^ z).normalize();
	Vec3f y = (z ^ x).normalize();
	Matrix modelview = Matrix::identity(4);
	for (int i=0; i<3; i++)
	{
		modelview[0][i] = x[i];
		modelview[1][i] = y[i];
		modelview[2][i] = z[i];
		modelview[i][3] = -center[i];
	}
	return modelview;
}

Vec3f m2v(Matrix m) {
	return Vec3f(m[0][0]/m[3][0], m[1][0]/m[3][0], m[2][0]/m[3][0]);
}

Matrix v2m(Vec3f v) {
	Matrix m = Matrix::identity(4);
	m[0][0] = v.x;
	m[1][0] = v.y;
	m[2][0] = v.z;
	m[3][0] = 1.f;
	return m;
}

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

Vec3f barycentric(Vec2i *pts, Vec2i p)
{
	Vec3f vec1(pts[2][0] - pts[0][0], pts[1][0] - pts[0][0], pts[0][0] - p[0]);
	Vec3f vec2(pts[2][1] - pts[0][1], pts[1][1] - pts[0][1], pts[0][1] - p[1]);
	Vec3f c = vec1^ vec2;
	if (std::abs(c.z) < 1) return Vec3f(-1,1,1);
	return Vec3f(1.f - (c.x + c.y)/c.z, c.y/c.z, c.x/c.z); 
}

Vec3f barycentric(Vec3f A, Vec3f B, Vec3f C, Vec3f p)
{
	Vec3f s[2];
	for (int i=2; i--; ) {
		s[i][0] = C[i]-A[i];
		s[i][1] = B[i]-A[i];
		s[i][2] = A[i]-p[i];
	}
	Vec3f u = s[0] ^ s[1];
	if (std::abs(u[2])>1e-2) // dont forget that u[2] is integer. If it is zero then triangle ABC is degenerate
		return Vec3f(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
	return Vec3f(-1,1,1); // in this case generate negative coordinates, it will be thrown away by the rasterizator
}

void triangle(Vec2i *pts, TGAImage &image, const TGAColor &color)
{
	Vec2i bboxmin(image.get_width() - 1, image.get_height() - 1);
	Vec2i bboxmax(0, 0);
	Vec2i clamp(image.get_width() - 1, image.get_height() - 1);
	for (int i = 0; i < 3; i++)
	{
		bboxmin.x = std::max(0, std::min(bboxmin.x, pts[i].x));
		bboxmin.y = std::max(0, std::min(bboxmin.y, pts[i].y));

		bboxmax.x = std::min(clamp.x, std::max(bboxmax.x, pts[i].x));
		bboxmax.y = std::min(clamp.y, std::max(bboxmax.y, pts[i].y));
	}
	Vec2i p;
	for (p.x = bboxmin.x; p.x <= bboxmax.x; p.x++)
	{
		for (p.y = bboxmin.y; p.y <= bboxmax.y; p.y++)
		{
			Vec3f bc_coordinates = barycentric(pts, p);
			if (bc_coordinates.x < 0 || bc_coordinates.y < 0 || bc_coordinates.z < 0) continue;
			image.set(p.x, p.y, color);
		}
	}
}

void triangle(Vec3f *pts, Vec2f *uvs, int *zbuffer, TGAImage &image, float intensity, Texture &texture) {
	Vec2f bboxmin( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
	Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
	Vec2f clamp(image.get_width()-1, image.get_height()-1);
	for (int i=0; i<3; i++) {
		for (int j=0; j<2; j++) {
			bboxmin[j] = std::max(0.f,      std::min(bboxmin[j], pts[i][j]));
			bboxmax[j] = std::min(clamp[j], std::max(bboxmax[j], pts[i][j]));
		}
	}
	Vec3f P;
	for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) {
		for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) {
			Vec3f bc_screen  = barycentric(pts[0], pts[1], pts[2], P);
			if (bc_screen.x<0 || bc_screen.y<0 || bc_screen.z<0) continue;
			P.z = 0;
			Vec2f puv(0.f, 0.f);
			for (int i=0; i<3; i++)
			{
				P.z += pts[i][2]*bc_screen[i];
				puv.x += uvs[i][0]*bc_screen[i];
				puv.y += uvs[i][1]*bc_screen[i];
			}
			if (zbuffer[int(P.x+P.y*width)]<P.z) {
				zbuffer[int(P.x+P.y*width)] = P.z;
				TGAColor color = texture.get_color(puv);
				image.set(P.x, P.y, TGAColor(color.bgra[0] *intensity, color.bgra[1]*intensity, color.bgra[2]*intensity, 255));
			}
		}
	}
}

void triangle(Vec3i t0, Vec3i t1, Vec3i t2, Vec2f uv0, Vec2f uv1, Vec2f uv2,
	float it0, float it1, float it2, TGAImage &image, float intensity, int *zbuffer, Texture &texture) {
	if (t0.y==t1.y && t0.y==t2.y) return; // i dont care about degenerate triangles
	if (t0.y>t1.y) { std::swap(t0, t1); std::swap(uv0, uv1); std::swap(it0, it1); }
	if (t0.y>t2.y) { std::swap(t0, t2); std::swap(uv0, uv2); std::swap(it0, it2); }
	if (t1.y>t2.y) { std::swap(t1, t2); std::swap(uv1, uv2); std::swap(it1, it2); }

	int total_height = t2.y-t0.y;
	for (int i=0; i<total_height; i++) {
		bool second_half = i>t1.y-t0.y || t1.y==t0.y;
		int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y;
		float alpha = (float)i/total_height;
		float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height; // be careful: with above conditions no division by zero here
		Vec3i A   =               t0  + Vec3f(t2-t0  )*alpha;
		Vec3i B   = second_half ? t1  + Vec3f(t2-t1  )*beta : t0  + Vec3f(t1-t0  )*beta;
		Vec2f uvA =               uv0 +      (uv2-uv0)*alpha;
		Vec2f uvB = second_half ? uv1 +      (uv2-uv1)*beta : uv0 +      (uv1-uv0)*beta;
		float itA =			      it0 +      (it2-it0)*alpha;
		float itB = second_half ? it1 +      (it2-it1)*beta : it0 +      (it1-it0)*beta;
		if (A.x>B.x) { std::swap(A, B); std::swap(uvA, uvB); std::swap(itA, itB);}
		for (int j=A.x; j<=B.x; j++) {
			float phi = B.x==A.x ? 1. : (float)(j-A.x)/(float)(B.x-A.x);
			Vec3i   P = Vec3f(A) + Vec3f(B-A)*phi;
			Vec2f uvP =     uvA +   (uvB-uvA)*phi;
			float itP  = itA + (itB-itA)*phi;
			int idx = P.x+P.y*width;
			if (zbuffer[idx]<P.z) {
				zbuffer[idx] = P.z;
				TGAColor color = texture.get_color(uvP);
				image.set(P.x, P.y, color * itP);
			}
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

int main(int argc, char** argv) {
	if (argc == 2)
	{
		model = new Model(argv[1]);
	}
	else
	{
		model = new Model("obj/african_head.obj");
	}
	
	TGAImage image(width, height, TGAImage::RGB);

	Texture texture("obj/african_head_diffuse.tga");

	int *zbuffer = new int[width*height];
	for (int i=width*height; i--; zbuffer[i] = std::numeric_limits<int>::min());

	Matrix Projection = Matrix::identity(4);
	Projection[3][2] = -1.f/(eye - center).norm();
	Matrix ViewPort   = viewport(width/8, height/8, width*3/4, height*3/4);
	Matrix ModelView = lookat(eye,center, Vec3f(0,1,0));

	for (int i=0; i<model->nfaces(); i++) { 
		std::vector<Vec3i> face = model->face(i);
		Vec3f vert[3];
		Vec3f screen_coords[3];
		Vec2f uv[3];
		Vec3f normal[3];
		for (int j=0; j<3; j++) {
			vert[j] = model->vert(face[j][0]);
			//printf("vert[j].z = %f\n", vert[j].z);
			//screen_coords[j] = world2screen(vert[j]);
			screen_coords[j] = m2v(ViewPort*Projection*ModelView*v2m(vert[j]));
			//printf("screen_coord[j].z = %f\n", pts[j].z);
			uv[j] = model->uv(face[j][1]);
			normal[j] = model->normal(face[j][2]);
		}
		triangle(screen_coords[0], screen_coords[1], screen_coords[2], uv[0], uv[1], uv[2],
				normal[0] * light_dir, normal[1]* light_dir, normal[2]* light_dir, image, 0, zbuffer, texture);
		// Vec3f n = (vert[2]-vert[0])^(vert[1]-vert[0]); 
		// n.normalize(); 
		// float intensity = n*light_dir; 
		// if (intensity>0) {
		// 	//triangle(screen_coords, uv, zbuffer, image, intensity, texture);
		// 	triangle(screen_coords[0], screen_coords[1], screen_coords[2], uv[0], uv[1], uv[2],
		// 		normal[0] * light_dir, normal[1]* light_dir, normal[2]* light_dir, image, intensity, zbuffer, texture);
		// }
	}

	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("output.tga");

	delete model;
	return 0;
}


#include <complex>
#define NOMINMAX
#include <Windows.h>

#include "tgaimage.h"
#include "model.h"
#include "geometry.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0, 255, 0,   0);
Model *model = nullptr;
const int width = 400;
const int height = 400;

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

Vec3f baycentric(Vec2i *pts, Vec2i p)
{
	Vec3f vec1(pts[2][0] - pts[0][0], pts[1][0] - pts[0][0], pts[0][0] - p[0]);
	Vec3f vec2(pts[2][1] - pts[0][1], pts[1][1] - pts[0][1], pts[0][1] - p[1]);
	Vec3f cross = vec1 ^ vec2;
	if (std::abs(cross.z) < 1) return Vec3f(-1,1,1);
	return Vec3f(1.f - (cross.x + cross.y)/cross.z, cross.y/cross.z, cross.x/cross.z); 
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
			Vec3f bc_coordinates = baycentric(pts, p);
			if (bc_coordinates.x < 0 || bc_coordinates.y < 0 || bc_coordinates.z < 0) continue;
			image.set(p.x, p.y, color);
		}
	}
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

	Vec3f light_dir(0,0,-1); // define light_dir

	for (int i=0; i<model->nfaces(); i++) { 
		std::vector<int> face = model->face(i); 
		Vec2i screen_coords[3]; 
		Vec3f world_coords[3]; 
		for (int j=0; j<3; j++) { 
			Vec3f v = model->vert(face[j]); 
			screen_coords[j] = Vec2i((v.x+1.)*width/2., (v.y+1.)*height/2.); 
			world_coords[j]  = v; 
		} 
		Vec3f n = (world_coords[2]-world_coords[0])^(world_coords[1]-world_coords[0]); 
		n.normalize(); 
		float intensity = n*light_dir; 
		if (intensity>0) { 
			triangle(screen_coords[0], screen_coords[1], screen_coords[2], image, TGAColor(intensity*255, intensity*255, intensity*255, 255)); 
		}
	}

	// for (int i=0; i<model->nfaces(); i++)
	// {
	// 	std::vector<int> face = model->face(i);
	// 	for (int j = 0; j < 3; j++)
	// 	{
	// 		Vec3f v0 = model->vert(face[j]);
	// 		Vec3f v1 = model->vert(face[(j + 1) % 3]);
	// 		int x0 = (v0.x+1.)*width/2.; 
	// 		int y0 = (v0.y+1.)*height/2.; 
	// 		int x1 = (v1.x+1.)*width/2.; 
	// 		int y1 = (v1.y+1.)*height/2.; 
	// 		line(x0, y0, x1, y1, image, white);
	// 	}
	// }

	// Vec2i t0[3] = {Vec2i(10, 70),   Vec2i(50, 160),  Vec2i(70, 80)}; 
	// Vec2i t1[3] = {Vec2i(180, 50),  Vec2i(150, 1),   Vec2i(70, 180)}; 
	// Vec2i t2[3] = {Vec2i(180, 150), Vec2i(120, 160), Vec2i(130, 180)}; 
	// triangle(t0[0], t0[1], t0[2], image, red); 
	// triangle(t1[0], t1[1], t1[2], image, white); 
	// triangle(t2[0], t2[1], t2[2], image, green);

	// Vec2i pts[3] = {Vec2i(10,10), Vec2i(100, 30), Vec2i(190, 160)}; 
	// triangle(pts, image, red);
	
	image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
	image.write_tga_file("output.tga");
	delete model;
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HBITMAP hBitmap = nullptr;
	static TGAImage image;

	switch (uMsg) {
	case WM_CREATE:
		{
			HDC hdc = GetDC(hwnd);
			// if (image.Load("your_image.tga")) {  // 把"your_image.tga"换成你的文件路径
			// 	hBitmap = CreateBitmapFromPixels(hdc, image);
			// }
			ReleaseDC(hwnd, hdc);
		}
		return 0;
	case WM_PAINT:
		{
			// PAINTSTRUCT ps;
			// HDC hdc = BeginPaint(hwnd, &ps);
			// if (hBitmap) {
			// 	HDC memDC = CreateCompatibleDC(hdc);
			// 	HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, hBitmap);
			// 	BitBlt(hdc, 0, 0, image.width, image.height, memDC, 0, 0, SRCCOPY);
			// 	SelectObject(memDC, oldBmp);
			// 	DeleteDC(memDC);
			// }
			// EndPaint(hwnd, &ps);
			return 0;
		}
	case WM_DESTROY:
		if (hBitmap) DeleteObject(hBitmap);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
// 	const wchar_t* CLASS_NAME = L"TGAWindowClass";
//
// 	WNDCLASS wc = { };
// 	wc.lpfnWndProc = WndProc;
// 	wc.hInstance = hInstance;
// 	wc.lpszClassName = CLASS_NAME;
//
// 	RegisterClass(&wc);
//
// 	HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Display TGA Image", 
// 		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
//
// 	if (!hwnd) return 0;
//
// 	ShowWindow(hwnd, nCmdShow);
//
// 	MSG msg = { };
// 	while (GetMessage(&msg, nullptr, 0, 0)) {
// 		TranslateMessage(&msg);
// 		DispatchMessage(&msg);
// 	}
// 	return 0;
// }


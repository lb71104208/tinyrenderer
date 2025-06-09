#ifndef __MODEL_H__
#define __MODEL_H__

#include <vector>
#include "geometry.h"

class Model {
private:
	std::vector<Vec3f> normals_;
	std::vector<Vec2f> uvs_;
	std::vector<Vec3f> verts_;
	std::vector<std::vector<Vec3i> > faces_;
public:
	Model(const char *filename);
	~Model();
	int nverts();
	int nfaces();
	int nnormals();
	int nuvs();
	Vec3f vert(int i);
	Vec3f vert(int f, int i);
	std::vector<Vec3i> face(int idx);
	Vec3f normal(int i);
	Vec3f normal(int f, int i);
	Vec2f uv(int i);
	Vec2f uv(int f, int i);
};

#endif //__MODEL_H__

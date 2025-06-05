#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include "model.h"

Model::Model(const char *filename) : verts_(), faces_() {
    std::ifstream in;
    in.open (filename, std::ifstream::in);
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) {
        std::getline(in, line);
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) {
            iss >> trash;
            Vec3f v;
            for (int i=0;i<3;i++) iss >> v[i];
            verts_.push_back(v);
        }
        else if (!line.compare(0, 3, "vn "))
        {
            iss >> trash >> trash;
            Vec3f n;
            for (int i=0;i<3;i++) iss >> n[i];
            normals_.push_back(n);
        }
        else if (!line.compare(0, 3, "vt "))
        {
            iss >> trash >> trash;
            Vec2f t;
            for (int i=0;i<2;i++) iss >> t[i];
            uvs_.push_back(t);
        }
        else if (!line.compare(0, 2, "f ")) {
            std::vector<Vec3i> f;
            int idx;
            iss >> trash;
            Vec3i vert_data;
            while (iss >> vert_data[0] >> trash >> vert_data[1] >> trash >> vert_data[2]) {
                for (int i=0; i<3; i++) vert_data[i]--; // in wavefront obj all indices start at 1, not zero
                f.push_back(vert_data);
            }
            faces_.push_back(f);
        }
    }
    std::cerr << "# v# " << verts_.size() << " f# "  << faces_.size() << " vt# " << uvs_.size() << " vn# " << normals_.size() << std::endl;
}

Model::~Model() {
}

int Model::nverts() {
    return (int)verts_.size();
}

int Model::nfaces() {
    return (int)faces_.size();
}

int Model::nnormals()
{
    return (int)normals_.size();
}

int Model::nuvs()
{
    return (int)uvs_.size();
}

std::vector<Vec3i> Model::face(int idx) {
    return faces_[idx];
}

Vec3f Model::normal(int i)
{
    return normals_[i].normalize();
}

Vec2f Model::uv(int i)
{
    return uvs_[i];
}

Vec3f Model::vert(int i) {
    return verts_[i];
}


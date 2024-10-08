#include "Object3d.h"
#include "Object3dbasic.h"

void Object3d::Initialize(Object3dBasic* object3dBasic)
{
	object3dBasic_ = object3dBasic;

	modelData_ = LoadObjFile("resources", "plane.obj");
}

Object3d::ModelData Object3d::LoadObjFile(const std::string& directoryPath, const std::string& fileName)
{
	ModelData modelData;
	VertexData triangleVertices[3];
	std::vector<Vector4> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	std::string line;

	std::ifstream file(directoryPath + "/" + fileName);
	assert(file.is_open());

	while (std::getline(file, line))
	{
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);

		} else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);

		} else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);

		} else if (identifier == "f") {

			for (int32_t facevertex = 0; facevertex < 3; facevertex++) {
				std::string vertexDefiniton;
				s >> vertexDefiniton;

				std::istringstream v(vertexDefiniton);
				uint32_t elementIndices[3];

				for (int32_t element = 0; element < 3; element++) {
					std::string index;
					std::getline(v, index, '/');
					elementIndices[element] = std::stoi(index);
				}

				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];

				position.z *= -1.0f;
				normal.z *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;

				triangleVertices[facevertex] = { position, texcoord, normal };

			}

			// 三角形の頂点データを追加
			modelData.vertices.push_back(triangleVertices[2]);
			modelData.vertices.push_back(triangleVertices[1]);
			modelData.vertices.push_back(triangleVertices[0]);

		} else if (identifier == "mtllib") {
			std::string mtlFileName;
			s >> mtlFileName;
			modelData.material = LoadMtlFile(directoryPath, mtlFileName);
		}
	}

	return modelData;
}

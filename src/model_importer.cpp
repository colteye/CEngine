#include "model_importer.h"

#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace {
struct FaceVertex {
	size_t vertex = 0;
	size_t uv = 0;
	size_t normal = 0;
};

bool ParseFaceVertex(const std::string& token, FaceVertex& face_vertex)
{
	std::stringstream stream(token);
	int vertex = 0;
	int uv = 0;
	int normal = 0;
	char slash_a = '\0';
	char slash_b = '\0';

	if (!(stream >> vertex >> slash_a >> uv >> slash_b >> normal) || slash_a != '/' || slash_b != '/')
	{
		return false;
	}

	if (vertex <= 0 || uv <= 0 || normal <= 0)
	{
		return false;
	}

	face_vertex.vertex = static_cast<size_t>(vertex - 1);
	face_vertex.uv = static_cast<size_t>(uv - 1);
	face_vertex.normal = static_cast<size_t>(normal - 1);
	return true;
}

bool FaceVertexInRange(const FaceVertex& face_vertex, size_t vertex_count, size_t uv_count, size_t normal_count)
{
	return face_vertex.vertex < vertex_count &&
		face_vertex.uv < uv_count &&
		face_vertex.normal < normal_count;
}

glm::vec3 CalculateTangent(const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
	const glm::vec2& uv0, const glm::vec2& uv1, const glm::vec2& uv2)
{
	const glm::vec3 delta_pos_1 = v1 - v0;
	const glm::vec3 delta_pos_2 = v2 - v0;
	const glm::vec2 delta_uv_1 = uv1 - uv0;
	const glm::vec2 delta_uv_2 = uv2 - uv0;
	const float denominator = delta_uv_1.x * delta_uv_2.y - delta_uv_1.y * delta_uv_2.x;

	if (std::abs(denominator) <= std::numeric_limits<float>::epsilon())
	{
		return glm::vec3(0.0f);
	}

	return (delta_pos_1 * delta_uv_2.y - delta_pos_2 * delta_uv_1.y) / denominator;
}
} // namespace

Model ModelImporter::ImportOBJ(const std::string& model_p,
	const std::unordered_map<std::string, Material*>& in_mats, bool auto_register)
{
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;

	temp_vertices.reserve(4096);
	temp_uvs.reserve(4096);
	temp_normals.reserve(4096);

	std::vector<glm::vec3> out_vertices;
	std::vector<glm::vec2> out_uvs;
	std::vector<glm::vec3> out_normals;
	std::vector<glm::vec3> out_tangents;

	out_vertices.reserve(4096);
	out_uvs.reserve(4096);
	out_normals.reserve(4096);
	out_tangents.reserve(4096);

	std::unordered_map<std::string, Material*> out_mats;
	std::unordered_map<std::string, Mesh> out_meshes;

	std::ifstream model_file;
	model_file.open(model_p);


	if (!model_file.is_open())
	{
		std::cout << "Impossible to open model file: " << model_p << "\n";
		return Model(out_meshes, out_mats, auto_register);
	}

	std::string line;
	std::string current_mesh_name;
	std::string current_mat_name;

	auto flush_mesh_data = [&]() {
		if (current_mesh_name.empty() || current_mat_name.empty() || out_vertices.empty())
		{
			out_vertices.clear();
			out_uvs.clear();
			out_normals.clear();
			out_tangents.clear();
			return;
		}

		const auto material_it = in_mats.find(current_mat_name);
		if (material_it == in_mats.end() || material_it->second == nullptr)
		{
			std::cout << "Skipping OBJ material with no registered Material: " << current_mat_name << "\n";
		}
		else
		{
			out_mats[current_mat_name] = material_it->second;
			MeshData mesh_data;
			mesh_data.vertices = std::move(out_vertices);
			mesh_data.uvs = std::move(out_uvs);
			mesh_data.normals = std::move(out_normals);
			mesh_data.tangents = std::move(out_tangents);
			out_meshes[current_mesh_name].AddMaterialMeshData(material_it->second, std::move(mesh_data));
		}

		out_vertices.clear();
		out_uvs.clear();
		out_normals.clear();
		out_tangents.clear();
	};

	while (std::getline(model_file, line))
	{
		if (line.empty() || line.at(0) == '#')
		{
			continue;
		}

		if (line.rfind("vt ", 0) == 0)
		{
			std::stringstream line_stream(line.substr(3));

			glm::vec2 uv;
			line_stream >> uv.x >> uv.y;
			temp_uvs.push_back(uv);
		}
		else if (line.rfind("vn ", 0) == 0)
		{
			std::stringstream line_stream(line.substr(3));

			glm::vec3 normal;
			line_stream >> normal.x >> normal.y >> normal.z;
			temp_normals.push_back(normal);
		}
		else if (line.rfind("v ", 0) == 0)
		{
			std::stringstream line_stream(line.substr(2));

			glm::vec3 vertex;
			line_stream >> vertex.x >> vertex.y >> vertex.z;
			temp_vertices.push_back(vertex);
		}
		else if (line.rfind("f ", 0) == 0)
		{
			if (current_mesh_name.empty())
			{
				current_mesh_name = "default";
				out_meshes[current_mesh_name] = Mesh();
			}

			std::stringstream face_stream(line.substr(2));
			std::array<std::string, 3> tokens;
			std::string extra_token;
			face_stream >> tokens[0] >> tokens[1] >> tokens[2] >> extra_token;

			if (tokens[0].empty() || tokens[1].empty() || tokens[2].empty() || !extra_token.empty())
			{
				std::cout << "Skipping unsupported OBJ face: " << line << "\n";
				continue;
			}

			std::array<FaceVertex, 3> face_vertices;
			bool face_is_supported = true;
			for (size_t index = 0; index < tokens.size(); ++index)
			{
				if (!ParseFaceVertex(tokens[index], face_vertices[index]) ||
					!FaceVertexInRange(face_vertices[index], temp_vertices.size(), temp_uvs.size(), temp_normals.size()))
				{
					std::cout << "Skipping unsupported OBJ face: " << line << "\n";
					face_is_supported = false;
					break;
				}
			}
			if (!face_is_supported)
			{
				continue;
			}

			const glm::vec3& v0 = temp_vertices[face_vertices[0].vertex];
			const glm::vec3& v1 = temp_vertices[face_vertices[1].vertex];
			const glm::vec3& v2 = temp_vertices[face_vertices[2].vertex];
			const glm::vec2& uv0 = temp_uvs[face_vertices[0].uv];
			const glm::vec2& uv1 = temp_uvs[face_vertices[1].uv];
			const glm::vec2& uv2 = temp_uvs[face_vertices[2].uv];
			const glm::vec3 tangent = CalculateTangent(v0, v1, v2, uv0, uv1, uv2);

			for (const FaceVertex& face_vertex : face_vertices)
			{
				out_vertices.push_back(temp_vertices[face_vertex.vertex]);
				out_uvs.push_back(temp_uvs[face_vertex.uv]);
				out_normals.push_back(temp_normals[face_vertex.normal]);
				out_tangents.push_back(tangent);
			}

		}
		else if (line.rfind("o ", 0) == 0)
		{
			flush_mesh_data();

			// Instantiate new mesh.
			// Get name after "o ".
			std::string mesh_name = line.substr(2);
			out_meshes[mesh_name] = Mesh();
			current_mesh_name = mesh_name;
		}
		else if (line.rfind("usemtl ", 0) == 0)
		{
			if (current_mesh_name.empty())
			{
				current_mesh_name = "default";
				out_meshes[current_mesh_name] = Mesh();
			}

			flush_mesh_data();

			// Get name after "usemtl ".
			std::string mat_name = line.substr(7);
			current_mat_name = mat_name;
		}
	}
	// If done, time to export!
	flush_mesh_data();

	model_file.close();
	return Model(std::move(out_meshes), std::move(out_mats), auto_register);
}

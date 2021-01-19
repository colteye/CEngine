#include "model_importer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <unordered_map>

Model ModelImporter::ImportOBJ(std::string model_p, std::unordered_map<std::string, Material*> in_mats)
{
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;

	std::vector<glm::vec3> out_vertices;
	std::vector<glm::vec2> out_uvs;
	std::vector<glm::vec3> out_normals;
	std::vector<glm::vec3> out_tangents;

	//std::vector<Material*> out_mats;
	//std::vector<Mesh> out_meshes;

	std::unordered_map<std::string, Material*> out_mats;
	std::unordered_map<std::string, Mesh> out_meshes;

	std::ifstream model_file;
	model_file.open(model_p);


	if (!model_file.is_open())
	{
		std::cout << "Impossible to open the file !\n";
	}

	std::string line;
	std::string current_mesh_name = "";
	std::string current_mat_name = "";

	// Remove offset for face indexes when loading in data.
	int vert_offset = 0;
	int norm_offset = 0;
	int uv_offset = 0;

	while (std::getline(model_file, line))
	{
		if (line.at(0) == 'v')
		{
			if (line.at(1) == 't')
			{
				line.erase(0, 2);
				std::stringstream line_stream(line);

				glm::vec2 uv;
				line_stream >> uv.x >> uv.y;
				temp_uvs.push_back(uv);
			}
			else if (line.at(1) == 'n')
			{
				line.erase(0, 2);
				std::stringstream line_stream(line);

				glm::vec3 normal;
				line_stream >> normal.x >> normal.y >> normal.z;
				temp_normals.push_back(normal);
			}
			else
			{
				line.erase(0, 1);
				std::stringstream line_stream(line);

				glm::vec3 vertex;
				line_stream >> vertex.x >> vertex.y >> vertex.z;
				temp_vertices.push_back(vertex);
			}
		}
		else if (line.at(0) == 'f')
		{
			line.erase(0, 1);
			const int indexes_size = 9;
			unsigned int indexes[indexes_size];

			std::regex re("[^\/ ]+");
			//the '-1' is what makes the regex split (-1 := what was not matched)
			std::sregex_token_iterator first{ line.begin(), line.end(), re }, last;
			std::vector<std::string> tokens{ first, last };

			if (tokens.size() != indexes_size)
			{
				std::cout << "File can't be read by this simple parser: Try exporting with other options\n";
			}

			for (int i = 0; i < indexes_size; i+=3)
			{
				indexes[i] = std::atoi(tokens[i].c_str()) - 1;
				indexes[i + 1] = std::atoi(tokens[i + 1].c_str()) - 1;
				indexes[i + 2] = std::atoi(tokens[i + 2].c_str()) - 1;

				out_vertices.push_back(temp_vertices[indexes[i] - vert_offset]);
				out_uvs.push_back(temp_uvs[indexes[i + 1] - uv_offset]);
				out_normals.push_back(temp_normals[indexes[i + 2] - norm_offset]);
			}

			// Shortcuts for vertices
			glm::vec3 & v0 = temp_vertices[indexes[0] - vert_offset];
			glm::vec3 & v1 = temp_vertices[indexes[3] - vert_offset];
			glm::vec3 & v2 = temp_vertices[indexes[6] - vert_offset];

			// Shortcuts for UVs
			glm::vec2 & uv0 = temp_uvs[indexes[1] - uv_offset];
			glm::vec2 & uv1 = temp_uvs[indexes[4] - uv_offset];
			glm::vec2 & uv2 = temp_uvs[indexes[7] - uv_offset];

			// Edges of the triangle : position delta
			glm::vec3 deltaPos1 = v1 - v0;
			glm::vec3 deltaPos2 = v2 - v0;

			// UV delta
			glm::vec2 deltaUV1 = uv1 - uv0;
			glm::vec2 deltaUV2 = uv2 - uv0;

			float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
			glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;

			out_tangents.push_back(tangent);
			out_tangents.push_back(tangent);
			out_tangents.push_back(tangent);
		}
		else if (line.at(0) == 'o')
		{
			if (current_mesh_name != "")
			{
				// If going to next mesh, time to export!
				out_meshes[current_mesh_name].addMaterialMeshData(out_mats[current_mat_name], out_vertices, out_uvs, out_normals, out_tangents);

				vert_offset += temp_vertices.size();
				uv_offset += temp_uvs.size();
				norm_offset += temp_normals.size();

				// Clear arrays, time to add new data sequentially.
				temp_vertices.clear();
				temp_uvs.clear();
				temp_normals.clear();

				out_vertices.clear();
				out_normals.clear();
				out_uvs.clear();
				out_tangents.clear();
			}

			// Instantiate new mesh.
			// Get name after "o ".
			std::string mesh_name = line.substr(2);
			out_meshes[mesh_name] = Mesh();
			current_mesh_name = mesh_name;
		}
		else if (line.find("usemtl") != std::string::npos)
		{
			if (current_mat_name != "")
			{
				// If going to next material, time to export!
				out_meshes[current_mesh_name].addMaterialMeshData(out_mats[current_mat_name], out_vertices, out_uvs, out_normals, out_tangents);

				out_vertices.clear();
				out_uvs.clear();
				out_normals.clear();
				out_tangents.clear();
			}

			// Get name after "usemtl ".
			std::string mat_name = line.substr(7);
			out_mats[mat_name] = in_mats[mat_name];
			current_mat_name = mat_name;
		}
	}
	// If done, time to export!
	out_meshes[current_mesh_name].addMaterialMeshData(out_mats[current_mat_name], out_vertices, out_uvs, out_normals, out_tangents);

	model_file.close();
	return Model(out_meshes, out_mats);
}
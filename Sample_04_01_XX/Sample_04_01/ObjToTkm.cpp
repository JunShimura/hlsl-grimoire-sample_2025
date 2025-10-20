#include "stdafx.h"
#include "ObjToTkm.h"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <limits> // 追加: std::numeric_limits を使うため

// 簡易 MTL 情報格納
struct MtlInfo {
	std::string albedo;     // map_Kd
	std::string normal;     // map_Bump / bump
	std::string specular;   // map_Ks
	std::string reflection; // map_refl / refl (best-effort)
	std::string refraction; // map_refr / refr (best-effort)
};

static std::string GetDirectory(const std::string& path)
{
	auto pos = path.find_last_of("/\\");
	if (pos == std::string::npos) return std::string();
	return path.substr(0, pos + 1);
}

static void Trim(std::string& s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	size_t b = s.find_last_not_of(" \t\r\n");
	if (a == std::string::npos) { s.clear(); return; }
	s = s.substr(a, b - a + 1);
}

static void ParseMtlFile(const std::string& mtlPath, std::unordered_map<std::string, MtlInfo>& outMtls)
{
	std::ifstream ifs(mtlPath);
	if (!ifs.is_open()) return;

	std::string line;
	std::string current;
	auto dir = GetDirectory(mtlPath);

	while (std::getline(ifs, line)) {
		Trim(line);
		if (line.empty()) continue;
		if (line[0] == '#') continue;
		std::istringstream iss(line);
		std::string key;
		iss >> key;
		if (key == "newmtl") {
			iss >> current;
			outMtls.emplace(current, MtlInfo());
		}
		else if (!current.empty()) {
			std::string val;
			if (key == "map_Kd") {
				std::getline(iss, val);
				Trim(val);
				// store filename (keep relative path if provided)
				outMtls[current].albedo = val;
			}
			else if (key == "map_Bump" || key == "bump" || key == "map_bump") {
				std::getline(iss, val);
				Trim(val);
				outMtls[current].normal = val;
			}
			else if (key == "map_Ks") {
				std::getline(iss, val);
				Trim(val);
				outMtls[current].specular = val;
			}
			else if (key == "map_refl" || key == "refl" || key == "map_refllection") {
				std::getline(iss, val);
				Trim(val);
				outMtls[current].reflection = val;
			}
			else if (key == "map_refr" || key == "refract" || key == "map_refract") {
				std::getline(iss, val);
				Trim(val);
				outMtls[current].refraction = val;
			}
			// 他のキーは無視（必要なら拡張）
		}
	}
}

// objPath に対する相対パス解決（mtl に相対パス記述されている場合を考慮）
static std::string ResolveRelative(const std::string& baseDir, const std::string& ref)
{
	if (ref.empty()) return std::string();
	// if ref is absolute or contains drive, return as-is
	if (ref.size() > 1 && (ref[1] == ':' || ref[0] == '/' || ref[0] == '\\')) return ref;
	return baseDir + ref;
}

bool ConvertObjToTkm(const char* objFilePath, const char* outTkmFilePath)
{
	std::ifstream ifs(objFilePath);
	if (!ifs.is_open()) {
		char buf[512]; sprintf(buf, "Unable to open the OBJ file: %s", objFilePath);
		MessageBoxA(nullptr, buf, "error", MB_OK);
		return false;
	}

	std::vector<std::array<float, 3>> positions;
	std::vector<std::array<float, 3>> normals;
	std::vector<std::array<float, 2>> uvs;

	// vertex pool and map (token->index)
	std::unordered_map<std::string, uint32_t> vertMap;
	struct OutVertex { float pos[3]; float normal[3]; float uv[2]; };
	std::vector<OutVertex> outVertices;

	// materials usage
	std::string currentMaterial = "default";
	std::vector<std::string> materialOrder; // order of used materials
	std::unordered_map<std::string, std::vector<uint32_t>> indicesPerMaterial;

	std::unordered_map<std::string, MtlInfo> mtls;

	std::string objDir = GetDirectory(objFilePath);

	std::string line;
	// first pass to collect mtllibs if any (we'll parse mtl files when encountered)
	std::vector<std::string> mtlFiles;
	while (std::getline(ifs, line)) {
		Trim(line);
		if (line.empty()) continue;
		if (line[0] == '#') continue;
		std::istringstream iss(line);
		std::string key; iss >> key;
		if (key == "mtllib") {
			std::string mtlname; iss >> mtlname;
			std::string resolved = ResolveRelative(objDir, mtlname);
			mtlFiles.push_back(resolved);
		}
	}
	// parse all mtllibs
	for (auto& mtlp : mtlFiles) {
		ParseMtlFile(mtlp, mtls);
	}

	// rewind obj stream to parse geometry
	ifs.clear();
	ifs.seekg(0, std::ios::beg);

	while (std::getline(ifs, line)) {
		Trim(line);
		if (line.empty()) continue;
		if (line[0] == '#') continue;
		std::istringstream iss(line);
		std::string key; iss >> key;
		if (key == "v") {
			float x, y, z; iss >> x >> y >> z;
			positions.push_back({ x,y,z });
		}
		else if (key == "vt") {
			float u, v; iss >> u >> v;
			uvs.push_back({ u,v });
		}
		else if (key == "vn") {
			float nx, ny, nz; iss >> nx >> ny >> nz;
			normals.push_back({ nx,ny,nz });
		}
		else if (key == "usemtl") {
			iss >> currentMaterial;
			if (indicesPerMaterial.find(currentMaterial) == indicesPerMaterial.end()) {
				indicesPerMaterial.emplace(currentMaterial, std::vector<uint32_t>());
				materialOrder.push_back(currentMaterial);
			}
		}
		else if (key == "f") {
			std::vector<std::string> tokens;
			std::string tok;
			while (iss >> tok) tokens.push_back(tok);
			if (tokens.size() < 3) continue;
			// ensure material present
			if (indicesPerMaterial.find(currentMaterial) == indicesPerMaterial.end()) {
				indicesPerMaterial.emplace(currentMaterial, std::vector<uint32_t>());
				materialOrder.push_back(currentMaterial);
			}
			// triangulate fan
			for (size_t i = 1; i + 1 < tokens.size(); ++i) {
				std::string tri[3] = { tokens[0], tokens[i], tokens[i + 1] };
				for (int k = 0; k < 3; ++k) {
					const std::string& t = tri[k];
					auto it = vertMap.find(t);
					if (it != vertMap.end()) {
						indicesPerMaterial[currentMaterial].push_back(it->second);
					}
					else {
						// parse indices: v/vt/vn or v//vn or v/vt
						int vi = -1, vti = -1, vni = -1;
						size_t s1 = t.find('/');
						if (s1 == std::string::npos) {
							vi = std::stoi(t) - 1;
						}
						else {
							vi = std::stoi(t.substr(0, s1)) - 1;
							size_t s2 = t.find('/', s1 + 1);
							if (s2 == std::string::npos) {
								vti = std::stoi(t.substr(s1 + 1)) - 1;
							}
							else {
								if (s2 == s1 + 1) {
									vni = std::stoi(t.substr(s2 + 1)) - 1;
								}
								else {
									vti = std::stoi(t.substr(s1 + 1, s2 - s1 - 1)) - 1;
									vni = std::stoi(t.substr(s2 + 1)) - 1;
								}
							}
						}
						OutVertex ov{};
						if (vi >= 0 && vi < (int)positions.size()) {
							ov.pos[0] = positions[vi][0];
							ov.pos[1] = positions[vi][1];
							ov.pos[2] = positions[vi][2];
						}
						else { ov.pos[0] = ov.pos[1] = ov.pos[2] = 0.0f; }
						// normals are optional; TkmFile::Load currently zeroes normals and rebuilds them,
						// but we still copy if provided.
						if (vni >= 0 && vni < (int)normals.size()) {
							ov.normal[0] = normals[vni][0];
							ov.normal[1] = normals[vni][1];
							ov.normal[2] = normals[vni][2];
						}
						else { ov.normal[0] = ov.normal[1] = ov.normal[2] = 0.0f; }
						if (vti >= 0 && vti < (int)uvs.size()) {
							ov.uv[0] = uvs[vti][0];
							ov.uv[1] = uvs[vti][1];
						}
						else { ov.uv[0] = ov.uv[1] = 0.0f; }

						uint32_t newIndex = static_cast<uint32_t>(outVertices.size());
						outVertices.push_back(ov);
						vertMap.insert({ t, newIndex });
						indicesPerMaterial[currentMaterial].push_back(newIndex);
					}
				}
			}
		}
		else if (key == "mtllib") {
			// already parsed above; skip
		}
	}

	if (outVertices.empty() || materialOrder.empty()) {
		// If no material used, create default material and all indices into it
		if (materialOrder.empty()) {
			materialOrder.push_back("default");
			indicesPerMaterial.emplace("default", std::vector<uint32_t>());
			// If faces were parsed but materialOrder empty (rare), try to put indices into default:
			// If any indices exist in other maps we move them.
			// (but above we ensure some material key created when faces parsed)
		}
	}

	// Prepare writing TKM binary
	bool use16 = (outVertices.size() <= 0xffff);

	FILE* fp = fopen(outTkmFilePath, "wb");
	if (!fp) {
		MessageBoxA(nullptr, "Failed to open output tkm file.", "ERROR", MB_OK);
		return false;
	}

	// --- header ---
	// version(uint8_t), isFlat(uint8_t), numMeshParts(uint16_t)
	uint8_t version = 100;
	uint8_t isFlat = 0;
	uint16_t numMeshParts = 1;
	fwrite(&version, sizeof(version), 1, fp);
	fwrite(&isFlat, sizeof(isFlat), 1, fp);
	fwrite(&numMeshParts, sizeof(numMeshParts), 1, fp);

	// SMeshePartsHeader: numMaterial(uint32_t), numVertex(uint32_t), indexSize(uint8_t), pad[3]
	uint32_t numMaterial = static_cast<uint32_t>(materialOrder.size());
	uint32_t numVertex = static_cast<uint32_t>(outVertices.size());
	uint8_t indexSize = use16 ? 2 : 4;
	uint8_t pad[3] = { 0,0,0 };
	fwrite(&numMaterial, sizeof(numMaterial), 1, fp);
	fwrite(&numVertex, sizeof(numVertex), 1, fp);
	fwrite(&indexSize, sizeof(indexSize), 1, fp);
	fwrite(pad, sizeof(pad), 1, fp);

	// For each material: write 5 texture file names (uint32 len, then string with null)
	for (auto& matName : materialOrder) {
		MtlInfo mi;
		auto it = mtls.find(matName);
		if (it != mtls.end()) mi = it->second;
		// entry order: albedo, normal, specular, reflection, refraction
		auto writeTex = [&](const std::string& s) {
			if (s.empty()) {
				uint32_t l = 0; fwrite(&l, sizeof(l), 1, fp);
			}
			else {
				// leave as provided in MTL (may contain extension). Tkm loader will replace extension to dds.
				std::string tex = s;
				// If path is relative, keep only filename (TkmFile::BuildMaterial replaces filename in tkm path)
				size_t pos = tex.find_last_of("/\\");
				if (pos != std::string::npos) tex = tex.substr(pos + 1);
				uint32_t l = static_cast<uint32_t>(tex.size());
				fwrite(&l, sizeof(l), 1, fp);
				fwrite(tex.c_str(), l + 1, 1, fp); // include null
			}
			};
		writeTex(mi.albedo);
		writeTex(mi.normal);
		writeTex(mi.specular);
		writeTex(mi.reflection);
		writeTex(mi.refraction);
	}

	// Write vertex data: tkmFileFormat::SVertex { pos[3], normal[3], uv[2], weights[4], indices[4] }
	for (const auto& v : outVertices) {
		fwrite(v.pos, sizeof(float), 3, fp);
		fwrite(v.normal, sizeof(float), 3, fp);
		fwrite(v.uv, sizeof(float), 2, fp);
		float weights[4] = { 0,0,0,0 };
		fwrite(weights, sizeof(float), 4, fp);
		int16_t boneIdx[4] = { -1, -1, -1, -1 };
		fwrite(boneIdx, sizeof(int16_t), 4, fp);
	}

	// For each material: write numPolygon (int) and indices (1-based stored)
	for (auto& matName : materialOrder) {
		auto& idxs = indicesPerMaterial[matName];
		int numPolygon = static_cast<int>(idxs.size() / 3);
		fwrite(&numPolygon, sizeof(numPolygon), 1, fp);
		if (use16) {
			for (auto id : idxs) {
				uint16_t v = static_cast<uint16_t>(id + 1);
				fwrite(&v, sizeof(v), 1, fp);
			}
		}
		else {
			for (auto id : idxs) {
				uint32_t v = static_cast<uint32_t>(id + 1);
				fwrite(&v, sizeof(v), 1, fp);
			}
		}
	}

	fclose(fp);
	return true;
}
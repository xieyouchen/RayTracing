#pragma once
#include <vector>

struct Material
{
	float Roughness;
	glm::vec3 Albedo;

	glm::vec3 EmissionColor{ 0.0f };
	float EmissionPower = 0.0f;

	glm::vec3 GetEmission() const { return EmissionColor * EmissionPower; }
};

struct Sphere {
	glm::vec3 Position;
	float Radius;

	int MaterialIndex;
};


struct Scene 
{
	std::vector<Sphere> Spheres;
	std::vector<Material> Materials;
};
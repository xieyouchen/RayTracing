#include "Renderer.h"
#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Ray.h"

#include <iostream>

#include <execution>

namespace Utils
{
	static uint32_t ConvertToUint4(glm::vec4& color)
	{
		float r = (float)color.r * 255.0f;
		float g = (float)color.g * 255.0f;
		float b = (float)color.b * 255.0f;
		float a = (float)color.a * 255.0f;

		return uint32_t(a) << 24 | uint32_t(b) << 16 | uint32_t(g) << 8 | uint32_t(r);

	}

	static uint32_t PCG_Hash(uint32_t& seed) {
		uint32_t state = seed * 747796405u + 2891336453u;
		uint32_t word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
		return (word >> 22u) ^ word;
	}

	static float RandomFloat(uint32_t& seed) {
		seed = PCG_Hash(seed);
		return (float)seed / (float)std::numeric_limits<uint32_t>::max();
	}

	static glm::vec3 InUnitSphere(uint32_t& seed)
	{
		return glm::normalize(glm::vec3(
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f,
			RandomFloat(seed) * 2.0f - 1.0f
		));
	}
}



void Renderer::OnResize(uint32_t width, uint32_t height)
{

	if (m_FinalImage) {
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;
		m_FinalImage->Resize(width, height);
	}
	else {
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);
	}

	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImgHorizontalIter.resize(width);
	m_ImgVerticalIter.resize(height);
	for (uint32_t i = 0; i < width; i++)
		m_ImgHorizontalIter[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImgVerticalIter[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveCamera = &camera;
	m_ActiveScene = &scene;

	if (m_FrameIndex == 1) {
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));
	}

#define MT 1
#if MT
	std::for_each(std::execution::par, m_ImgVerticalIter.begin(), m_ImgVerticalIter.end(),
		[this](uint32_t j) {
			std::for_each(std::execution::par, m_ImgHorizontalIter.begin(), m_ImgHorizontalIter.end(),
				[this, j](uint32_t i) {
					glm::vec4 color = PerPixel(i, j);
					color = glm::clamp(color, 0.0f, 1.0f);

					m_AccumulationData[i + j * m_FinalImage->GetWidth()] += color;
					glm::vec4 accumulatedColor = m_AccumulationData[i + j * m_FinalImage->GetWidth()] / (float)m_FrameIndex;
					m_ImageData[i + j * m_FinalImage->GetWidth()] = Utils::ConvertToUint4(accumulatedColor);
				});
		});
#else
	for (uint32_t j = 0; j < m_FinalImage->GetHeight(); j++) {
		for (uint32_t i = 0; i < m_FinalImage->GetWidth(); i++) {
			glm::vec4 color = PerPixel(i, j);
			color = glm::clamp(color, 0.0f, 1.0f);

			m_AccumulationData[i + j * m_FinalImage->GetWidth()] += color;
			glm::vec4 accumulatedColor = m_AccumulationData[i + j * m_FinalImage->GetWidth()] / (float)m_FrameIndex;
			m_ImageData[i + j * m_FinalImage->GetWidth()] = ConvertToUint4(accumulatedColor);
		}
	}

#endif

	if (GetSettings().Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;


	m_FinalImage->SetData(m_ImageData);
}

glm::vec4 Renderer::PerPixel(uint32_t i, uint32_t j)
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[i + j * m_FinalImage->GetWidth()];

	glm::vec3 light = { 0.0f, 0.0f ,0.0f };

	int bounds = 5;
	glm::vec3 damping(1.0f);

	uint32_t seed = i + j * m_FinalImage->GetWidth();
	seed *= m_FrameIndex;

	for (int k = 0; k < bounds; k++) {
		seed += bounds;

		HitPayload payload = TraceRay(ray);

		if (payload.HitDistance < 0.0f) {
			//glm::vec3 skycolor = glm::vec3(0.6f, 0.8f, 1.0f);
			//light += skycolor * damping;
			break;
		}

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		damping *= material.Albedo;
		light += material.GetEmission();


		ray.Origin = payload.Position + payload.Normal * 0.0001f;
		//ray.Direction = glm::reflect(ray.Direction, payload.Normal 
		//	+ material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
		if (m_Settings.SlowDown)
			ray.Direction = glm::normalize(payload.Normal + Walnut::Random::InUnitSphere());
		else 
			ray.Direction = glm::normalize(payload.Normal + Utils::InUnitSphere(seed));

	} 

	return glm::vec4(light, 1.0f);
}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{

	if(m_ActiveScene->Spheres.size() == 0)
		return Miss(ray);

	float hitDistance = FLT_MAX;
	int cloestSphere = -1;
	for (size_t i = 0; i < m_ActiveScene->Spheres.size(); i++) {
		const Sphere& sphere = m_ActiveScene->Spheres[i];

		float radius = sphere.Radius;

		glm::vec3 origin = ray.Origin - sphere.Position;

		float A = glm::dot(ray.Direction, ray.Direction);
		float B = 2.0f * glm::dot(origin, ray.Direction);
		float C = glm::dot(origin, origin) - radius * radius;

		// discriminant = b^2 - 4ac
		float discriminant = B * B - 4.0f * A * C;
		if (discriminant < 0.0f) {
			continue;
		}

		// t = (-b +- sqrt(discriminant)) / 2a
		float t0 = (-B - glm::sqrt(discriminant)) / (2.0f * A);

		if (t0 > 0.0f && hitDistance > t0) {
			hitDistance = t0;
			cloestSphere = i;
		}
	}

	if (cloestSphere < 0.0f) {
		return Miss(ray);
	}

	return ClosestHit(ray, hitDistance, cloestSphere);

}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, uint32_t objectIndex)
{
	HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& sphere = m_ActiveScene->Spheres[objectIndex];
	glm::vec3 origin = ray.Origin - sphere.Position;

	payload.Position = origin + hitDistance * ray.Direction;

	payload.Normal = glm::normalize(payload.Position);

	payload.Position += sphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	HitPayload payload;
	payload.HitDistance = -1.0f;
	
	return payload;
}



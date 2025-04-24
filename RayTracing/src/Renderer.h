#pragma once

#include "Walnut/Image.h"

#include <memory>
#include "glm/glm.hpp"
#include "Camera.h"
#include "Ray.h"
#include "Scene.h"

class Renderer {
public:
	struct Settings {
		bool Accumulate = true;
		bool SlowDown = true;
	};
public:
	Renderer() = default;

	struct HitPayload {
		glm::vec3 Position;
		glm::vec3 Normal;
		float HitDistance;

		size_t ObjectIndex;
	};

	void OnResize(uint32_t width, uint32_t height);

	void Render(const Scene& scene, const Camera& camera);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }

	glm::vec4 TraceRay(const Scene& scene, const Ray& ray);

	glm::vec4 PerPixel(uint32_t i, uint32_t j); // input: pixel  output: color

	HitPayload TraceRay(const Ray& ray);
	HitPayload ClosestHit(const Ray& ray, float hitDistance, uint32_t objectIndex);
	HitPayload Miss(const Ray& ray);


	void ResetFrameIndex() { 
		m_FrameIndex = 1; 
	}

	Settings& GetSettings() { return m_Settings; }
	int m_FrameIndex = 1;

private:
	std::shared_ptr<Walnut::Image> m_FinalImage;
	uint32_t* m_ImageData = nullptr;
	glm::vec4* m_AccumulationData = nullptr;
	const Camera* m_ActiveCamera = nullptr;
	const Scene* m_ActiveScene = nullptr;

	Settings m_Settings;

	std::vector<uint32_t> m_ImgHorizontalIter;
	std::vector<uint32_t> m_ImgVerticalIter;
};

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"
#include "Walnut/Random.h"
#include "Walnut/Timer.h"
#include "Renderer.h"
#include "Camera.h"

#include <glm/gtc/type_ptr.hpp>

using namespace Walnut;

class ExampleLayer : public Walnut::Layer
{
public:
	ExampleLayer()
		: m_Camera(45.0f, 0.1f, 100.0f) 
	{
		{
			Sphere sphere;
			sphere.Radius = 1.0f;
			sphere.Position = { 0.0f, 0.0f, 0.0f };
			sphere.MaterialIndex = 0;

			m_Scene.Spheres.push_back(sphere);
		}

		{
			Sphere sphere;
			sphere.Radius = 100.0f;
			sphere.Position = { 0.0f, -101.0f, 0.0f };
			sphere.MaterialIndex = 1;

			m_Scene.Spheres.push_back(sphere);
		}

		{
			Sphere sphere;
			sphere.Radius = 1.0f;
			sphere.Position = { 2.2f, 0.0f, 0.0f };
			sphere.MaterialIndex = 2;

			m_Scene.Spheres.push_back(sphere);
		}

		{
			Material& pinkMaterial = m_Scene.Materials.emplace_back();
			pinkMaterial.Albedo = { 1.0f, 0.0f, 1.0f };
			pinkMaterial.Roughness = 0.42f;
		}

		{
			Material& blueMaterial = m_Scene.Materials.emplace_back();
			blueMaterial.Albedo = { 0.2f, 0.3f, 1.0f };
			blueMaterial.Roughness = 0.05f;
		}

		{
			Material& orangeMaterial = m_Scene.Materials.emplace_back();
			orangeMaterial.Albedo = { 0.8f, 0.5f, 0.2f };
			orangeMaterial.Roughness = 0.42f;
			orangeMaterial.EmissionColor = orangeMaterial.Albedo;
			orangeMaterial.EmissionPower = 2.0f;
		}


	}

	virtual void OnUpdate(float ts) override {
		if (m_Camera.OnUpdate(ts)) {
			m_Renderer.m_FrameIndex = 1;
		}
	}

	virtual void OnUIRender() override
	{
		ImGui::Begin("Sphere");
		for (size_t i = 0; i < m_Scene.Spheres.size(); i++) {
			ImGui::PushID(i);
			Sphere& sphere = m_Scene.Spheres[i];
			ImGui::DragFloat("Radius", &sphere.Radius, 0.1f);
			ImGui::DragFloat3("Position", glm::value_ptr(sphere.Position), 0.1f);
			ImGui::DragInt("Material", &sphere.MaterialIndex, 0.5f, 0, m_Scene.Materials.size()-1);
			ImGui::Separator();
			
			ImGui::PopID();
		}

		for (size_t i = 0; i < m_Scene.Materials.size(); i++) {
			ImGui::PushID(i);
			ImGui::ColorEdit3("Albedo", glm::value_ptr(m_Scene.Materials[i].Albedo), 0.1f);
			ImGui::DragFloat("Roughness", &m_Scene.Materials[i].Roughness, 0.01f, 0.0f, 1.0f);
			ImGui::ColorEdit3("EmissionColor", glm::value_ptr(m_Scene.Materials[i].EmissionColor), 0.1f);
			ImGui::DragFloat("EmissionPower", &m_Scene.Materials[i].EmissionPower, 0.05f, 0.0f, FLT_MAX);


			ImGui::Separator();

			ImGui::PopID();
		}

		ImGui::End();

		ImGui::Begin("Hello");
		ImGui::Text("Last frame time: %.3f ms", m_LastFrameTime);
		ImGui::Text("now fps: %.1f", 1000.0f / m_LastFrameTime);

		ImGui::Checkbox("Accumulate", &m_Renderer.GetSettings().Accumulate);
		ImGui::Checkbox("SlowDown", &m_Renderer.GetSettings().SlowDown);

		if (ImGui::Button("Reset")) {
			m_Renderer.ResetFrameIndex();
		}

		ImGui::End();

		//ImGui::ShowDemoWindow();

		ImGui::Begin("Viewport");
		
		m_ViewportWidth  = ImGui::GetContentRegionAvail().x;
		m_ViewportHeight = ImGui::GetContentRegionAvail().y;

		auto image = m_Renderer.GetFinalImage();
		
		if (image) {
			ImGui::Image(image->GetDescriptorSet(), { (float)image->GetWidth(), (float)image->GetHeight()}
			, ImVec2(0, 1), ImVec2(1,0));
		}
		
		ImGui::End();



		Render();
	}


	void Render() {
		//if(m_ImageData)
		//	delete[] m_ImageData;
		Timer timer;

		m_Renderer.OnResize(m_ViewportWidth, m_ViewportHeight);

		m_Camera.OnResize(m_ViewportWidth, m_ViewportHeight);
		m_Renderer.Render(m_Scene, m_Camera);

		m_LastFrameTime = timer.ElapsedMillis();
	}

private:
	uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
	uint32_t* m_ImageData = nullptr;
	float m_LastFrameTime = 0.0f;
	Renderer m_Renderer;
	Scene m_Scene;
	Camera m_Camera;
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Walnut Example";

	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}
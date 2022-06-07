/**
 * @file		engine_pipeline_raytracing.h
 * @brief	A pipeline for doing simple ray tracing on GPU
 *
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */
#pragma once



/**
 * @brief Basic ray tracing.
 */
class ENG_API PipelineRayTracing : public Eng::Pipeline
{
//////////
public: //
//////////

   /**
    * Per-triangle data. This struct must be aligned for OpenGL std430.
    */
   __declspec(align(16)) struct TriangleStruct 
   {      
      glm::vec4 v[3]; // 4 * 4 * 3 -> 48 bytes
      glm::vec4 n[3]; // 4 * 4 * 3 -> 48 bytes -> 96 bytes
      glm::vec2 u[3]; // 4 * 2 * 3 -> 24 bytes -> 120 bytes
      uint32_t matId; // 4 bytes -> 124 bytes
      uint32_t _pad;  // 4 bytes -> 128 bytes
   };


   /**
    * Per-light data. This struct must be aligned for OpenGL std430.
    */
   __declspec(align(16)) struct LightStruct
   {
      glm::vec4 position;   
      glm::vec4 color;
   };


   /**
    * Bounding sphere data. This struct must be aligned for OpenGL std430.
    */
   __declspec(align(16)) struct BSphereStruct 
   {      
      glm::vec4 position;      
      float radius;
      uint32_t firstTriangle;
      uint32_t nrOfTriangles;
      uint32_t _pad;
   };

   /** 
    * Material data. This struct must be aligned for OpenGL std430.
    */
   __declspec(align(16)) struct MaterialStruct {
      glm::vec4 albedo;    // 16b
      glm::vec4 emission;  // + 16b -> 32b
      float metalness;     // + 4b  -> 36b
      float roughness;     // + 4b  -> 40b

      uint64_t albedoTexHandle;     // + 8b -> 48b
      uint64_t metalnessTexHandle;  // + 8b -> 56b
      uint64_t normalTexHandle;     // + 8b -> 64b
   };

   struct RayStruct {
      glm::vec3 worldPos;
      glm::vec2 screenPos;
      glm::vec3 rayDir;
      glm::vec3 color;

      float distance;
   };
   

   // Const/dest:
	PipelineRayTracing();      
	PipelineRayTracing(PipelineRayTracing &&other);
   PipelineRayTracing(PipelineRayTracing const&) = delete;   
   virtual ~PipelineRayTracing(); 

   // Get/set:
   const Eng::Texture &getColorBuffer() const;

   // Data preparation:
   bool migrate(const Eng::List &list);

   // Rendering methods:
   // bool render(uint32_t value = 0, void *data = nullptr) const = delete;
   bool render(const Eng::Camera &camera, const Eng::List &list);
   
   // Managed:
   bool init() override;
   bool free() override;


/////////////
protected: //
/////////////

   // Reserved:
   struct Reserved;           
   std::unique_ptr<Reserved> reserved;			

   // Const/dest:
   PipelineRayTracing(const std::string &name);
};








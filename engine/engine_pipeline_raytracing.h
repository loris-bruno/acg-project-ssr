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
      uint32_t _pad; // 4 bytes -> 128
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
      glm::vec4 albedo;
      glm::vec4 emission;
      float metalness;
      float roughness;

      uint64_t albedoTexHandle;
      uint64_t metalnessTexHandle;
      uint64_t roughnessTexHandle;
   };


   /**
    * Ray data. Utility to calculate size of SSBO.
    */
   struct RayStruct {
      glm::vec3 position;
      glm::vec3 normal;
      glm::vec3 albedo;
      float metalness;
      float roughness;

      glm::vec3 rayDir;
      int32_t next;
   };


   /**
    * Workgroup count. Utility to calculate size of SSBO.
    */
   struct DispatchIndirectCommand
   {
      uint32_t  num_groups_x;
      uint32_t  num_groups_y;
      uint32_t  num_groups_z;
   };
   

   // Const/dest:
   PipelineRayTracing();      
   PipelineRayTracing(PipelineRayTracing &&other);
   PipelineRayTracing(PipelineRayTracing const&) = delete;   
   virtual ~PipelineRayTracing(); 

   // Data preparation:
   bool migrate(const Eng::List &list);

   // Rendering methods:
   // bool render(uint32_t value = 0, void *data = nullptr) const = delete;
   bool render(const Eng::Camera& camera, const Eng::List& list, const Eng::PipelineGeometry& geometryPipe, uint32_t nrOfBounces);
   
   // Managed:
   bool init() override;
   bool free() override;

   static const int MAX_BOUNCES = 4;

/////////////
protected: //
/////////////

   // Reserved:
   struct Reserved;           
   std::unique_ptr<Reserved> reserved;			

   // Const/dest:
   PipelineRayTracing(const std::string &name);
};








/**
 * @file		engine_pipeline_raytracing.cpp 
 * @brief	A pipeline for doing simple ray tracing on GPU
 *
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */



//////////////
// #INCLUDE //
//////////////

   // Main include:
   #include "engine.h"

   // OGL:      
   #include <GL/glew.h>
   #include <GLFW/glfw3.h>



/////////////
// SHADERS //
/////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline vertex shader.
 */
static const std::string pipeline_cs = R"(
#version 460 core
#extension GL_ARB_gpu_shader_int64 : enable

// This is the (hard-coded) workgroup size:
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;



/////////////
// #DEFINE //
/////////////

   #define K_EPSILON     1e-4f                // Tolerance around zero   
   #define FLT_MAX       3.402823466e+38f     // Max float value
   #define NR_OF_BOUNCES 3                    // Number of bounces
   // #define CULLING                            // Back face culling enabled when defined


struct DispatchIndirectCommand 
{    
   uint  num_groups_x;
   uint  num_groups_y;
   uint  num_groups_z;
};
   
layout(shared, binding=5) buffer DispatchIndirectCommandData
{     
   DispatchIndirectCommand cmd;
};

///////////////
// TRIANGLES //
///////////////

struct TriangleStruct 
{    
   vec4 v[3];
   vec4 n[3];
   vec2 u[3];
   uint matId;
};
   
layout(std430, binding=0) buffer SceneData
{     
   TriangleStruct triangle[];     
};


//////////////////////
// BOUNDING SPHERES //
//////////////////////

struct BSphereStruct 
{    
   vec4 position;
   float radius;
   uint firstTriangle;
   uint nrOfTriangles;
   uint _pad;
};

layout(std430, binding=1) buffer BSphereData
{     
   BSphereStruct bsphere[];     
};



///////////////
// MATERIALS //
///////////////

struct MaterialStruct 
{
   vec4 albedo;
   vec4 emission;
   float metalness;
   float roughness;

   uint64_t albedoTexHandle;
   uint64_t metalnessTexHandle;
   uint64_t roughnessTexHandle;
};

layout(std430, binding=2) buffer MaterialData
{
   MaterialStruct materials[];
};  


//////////////
// RAY DATA //
//////////////

struct RayStruct {

   vec3 position;
   vec3 normal;
   vec3 albedo;
   float metalness;
   float roughness;

   vec3 rayDir;
   int next;
};

layout(shared, binding=3) buffer RayData
{
   RayStruct rayData[];
};

layout (binding = 4, offset = 0) uniform atomic_uint counter;


///////////////////
// LOCAL STRUCTS //
///////////////////

/**
 * Structure for modeling a ray. 
 */
struct Ray
{ 
   vec3 origin;    // Ray origin point
   vec3 dir;       // Normalized ray direction  
};


/**
 * Structure reporting information about the collision. 
 */
struct HitInfo 
{  
   unsigned int triangle;  // Triangle index (within the triangle[] array)
   float t, u, v;          // Triangle barycentric coords
   vec3 albedo;            // Triangle albedo
   float metalness;        // Triangle metalness
   float roughness;        // Triangle roughness
   vec3 collisionPoint;    // Triangle's coords at collision point
   vec3 normal;            // Triangle's normal at collision point
   vec3 faceNormal;        // Triangle's face normal
};



////////////
// IN/OUT //
////////////

// Uniforms:
uniform uint nrOfBSpheres;
uniform uint nrOfBounces;

///////////////
// FUNCTIONS //
///////////////

/**
 * Ray-sphere intersection.
 * param ray input ray
 * param center sphere center coords
 * param radius sphere radius size
 * param t output collision distance
 * return true on collision, false otherwise
 */
bool intersectSphere(const Ray ray, 
                     const vec3 center, const float radius,
                     out float t)
{ 
   float t0, t1; // solutions for t if the ray intersects 

   // Geometric solution:
   vec3 L = center - ray.origin; 
   float tca = dot(L, ray.dir); 
   //if (tca < 0.0f) return false; // the sphere is behind the ray origin
   float d2 = dot(L, L) - tca * tca; 
   if (d2 > (radius * radius)) 
      return false; 
   float thc = sqrt((radius * radius) - d2); 
   t0 = tca - thc; 
   t1 = tca + thc; 

   if (t0 > t1) 
   {
      float _t = t0;
      t0 = t1;
      t1 = _t;
   }
 
   if (t0 < 0.0f) 
   { 
      t0 = t1; // if t0 is negative, let's use t1 instead 
      if (t0 < 0.0f) 
         return false; // both t0 and t1 are negative 
   } 
 
   t = t0;  
   return true; 
} 


/**
 * Ray-triangle intersection.
 * param ray current ray
 * param v0 first triangle vertex
 * param v1 second triangle vertex
 * param v2 third triangle vertex
 * param t output collision distance
 * param u output barycentric coordinate u
 * param v output barycentric coordinate v
 */
bool intersectTriangle(const Ray ray, 
                       const vec3 v0, const vec3 v1, const vec3 v2, 
                       out float t, out float u, out float v) 
{ 
   vec3 v0v1 = v1 - v0; 
   vec3 v0v2 = v2 - v0; 
   vec3 pvec = cross(ray.dir, v0v2); 
   float det = dot(v0v1, pvec);    

#ifdef CULLING 
    // if the determinant is negative the triangle is backfacing
    // if the determinant is close to 0, the ray misses the triangle    
    if (det < K_EPSILON) 
      return false; 
#else 
    // ray and triangle are parallel if det is close to 0    
    if (abs(det) < K_EPSILON)
      return false;     
#endif 
    float invDet = 1.0f / det; 
 
    vec3 tvec = ray.origin - v0; 
    u = dot(tvec, pvec) * invDet; 
    if (u < 0.0f || u > 1.0f)     
      return false; 
 
    vec3 qvec = cross(tvec, v0v1); 
    v = dot(ray.dir, qvec) * invDet; 
    if (v < 0.0f || ((u + v) > 1.0f))     
      return false; 
 
    t = dot(v0v2, qvec) * invDet;  
    return (t > 0.0f) ? true : false;     
}


/**
 * Main intersection method
 * param ray current ray  
 * param info collision information (output)
 * return true when the ray intersects a triangle, false otherwise
 */
bool intersect(const Ray ray, out HitInfo info)
{  
   float dist;
   info.triangle = 999999; // Special value for "no triangle"
   info.t = FLT_MAX;         

   for (uint b = 0; b < nrOfBSpheres; b++)
      if (intersectSphere(ray, bsphere[b].position.xyz, bsphere[b].radius, dist)) 
      {
         float t, u, v;
         for (uint i = bsphere[b].firstTriangle; i < bsphere[b].firstTriangle + bsphere[b].nrOfTriangles; i++) 
            if (intersectTriangle(ray, triangle[i].v[0].xyz, triangle[i].v[1].xyz, triangle[i].v[2].xyz, t, u, v)) 
               if (t < info.t && i != info.triangle)
               {  
                  info.triangle = i;   
                  info.t = t;
                  info.u = u;
                  info.v = v;
                  vec2 uv = triangle[i].u[1] * u + triangle[i].u[2] * v + (1.0f - u - v) * triangle[i].u[0];
                  info.albedo = texture(sampler2D(materials[triangle[i].matId].albedoTexHandle), uv).rgb;
                  info.metalness = texture(sampler2D(materials[triangle[i].matId].metalnessTexHandle), uv).r;
                  info.roughness = texture(sampler2D(materials[triangle[i].matId].roughnessTexHandle), uv).r;
         }
      }

   // Compute final values:
   if (info.triangle != 999999)
   {  
      info.collisionPoint = ray.origin + info.t * ray.dir;      
      info.normal = normalize(info.u * triangle[info.triangle].n[1].xyz + info.v * triangle[info.triangle].n[2].xyz + (1.0f - info.u - info.v) * triangle[info.triangle].n[0].xyz);      
      if (dot(info.normal, -ray.dir.xyz) < 0.0f) // Coll. from inside
         info.normal = -info.normal;
      
      // Compute face normal:
      vec3 v0v1 = triangle[info.triangle].v[1].xyz - triangle[info.triangle].v[0].xyz;
      vec3 v0v2 = triangle[info.triangle].v[2].xyz - triangle[info.triangle].v[0].xyz;
      info.faceNormal = normalize(cross(v0v1, v0v2));
   }
            
   // Done:
   return info.triangle != 999999;
}


/**
 * Ray casting function for tracing a (recursive) ray within the scene.
 * param ray primary ray
 * return color of the pixel's ray
 */
void rayCasting(Ray ray, uint index, uint nrOfRays)
{
   HitInfo hit;   
   vec4 outputColor = vec4(0.0f);
   vec4 throughput = vec4(1.0f);

   vec3 oldHitNormal = vec3(0.0f);

   for (unsigned int c = 0; c < nrOfBounces; c++)
      if (intersect(ray, hit))
      {
         // get and increase counter
         uint newIndex = atomicCounterIncrement(counter);
         //uint newIndex = index + nrOfRays;
         rayData[index].next = int(newIndex);
         index = newIndex;

         rayData[index].position = hit.collisionPoint.xyz;
         rayData[index].normal = hit.normal.xyz;
         rayData[index].albedo = hit.albedo.rgb;
         rayData[index].metalness = hit.metalness;
         rayData[index].roughness = hit.roughness;
         rayData[index].rayDir = reflect(ray.dir, hit.normal.xyz);
         rayData[index].next = -1;

         
         // Update next ray:
        ray.origin = rayData[index].position.xyz + hit.faceNormal.xyz * (2.0f * K_EPSILON);
        ray.dir = reflect(ray.dir, rayData[index].normal.xyz);
        oldHitNormal = rayData[index].normal.xyz;

      }
}



//////////
// MAIN //
//////////

void main()
{   

   // Ray data index
   uint index = gl_GlobalInvocationID.x;
   uint nrOfRays = atomicCounter(counter);

   // Avoid out of range values:
   if (index >= nrOfRays)
      return;

   // Secondary ray casting:
   Ray ray; 
   ray.origin = rayData[index].position;
   ray.dir = rayData[index].rayDir;    

   // Ray casting:
   rayCasting(ray, index, nrOfRays);
})";



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief PipelineRayTracing reserved structure.
 */
struct Eng::PipelineRayTracing::Reserved
{  
   Eng::Shader cs;   
   Eng::Program program;         
   Eng::Ssbo triangles;       ///< List of triangles in world coords
   Eng::Ssbo bspheres;        ///< List of bounding spheres in world coords
   Eng::Ssbo materials;       ///< List of materials
   Eng::Ssbo rayData;         ///< List of ray data, populated 

   // Scene-specific:
   uint32_t nrOfTriangles;
   uint32_t nrOfMeshes;
   uint32_t nrOfMaterials;


   /**
    * Constructor. 
    */
   Reserved() : nrOfTriangles{ 0 }, nrOfMeshes{ 0 }, nrOfMaterials{ 0 }
   {}
};



//////////////////////////////////////
// BODY OF CLASS PipelineRayTracing //
//////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::PipelineRayTracing::PipelineRayTracing() : reserved(std::make_unique<Eng::PipelineRayTracing::Reserved>())
{	
   ENG_LOG_DETAIL("[+]");      
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::PipelineRayTracing::PipelineRayTracing(const std::string &name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineRayTracing::Reserved>())
{	   
   ENG_LOG_DETAIL("[+]");   
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor. 
 */
ENG_API Eng::PipelineRayTracing::PipelineRayTracing(PipelineRayTracing &&other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{  
   ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::PipelineRayTracing::~PipelineRayTracing()
{	
   ENG_LOG_DETAIL("[-]");
   if (this->isInitialized())
      free();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes this pipeline. 
 * @return TF
 */
bool ENG_API Eng::PipelineRayTracing::init()
{
   // Already initialized?
   if (this->Eng::Managed::init() == false)
      return false;
   if (!this->isDirty())
      return false;

   // Build:
   reserved->cs.load(Eng::Shader::Type::compute, pipeline_cs);   
   if (reserved->program.build({ reserved->cs }) == false)
   {
      ENG_LOG_ERROR("Unable to build RayTracing program");
      return false;
   }
   this->setProgram(reserved->program);   

   
   // Create SSBO and counter for ray data:
   Eng::Base& eng = Eng::Base::getInstance();
   int width = eng.getWindowSize().x;
   int height = eng.getWindowSize().y;
   reserved->rayData.create(sizeof(RayStruct) * width * height, nullptr);

   
   // Done: 
   this->setDirty(false);
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineRayTracing::free()
{
   if (this->Eng::Managed::free() == false)
      return false;

   // Done:   
   return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Migrates the data from a standard list into RT-specific structures. 
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineRayTracing::migrate(const Eng::List &list)
{
   // Safety net:
   if (list == Eng::List::empty)
   {
      ENG_LOG_ERROR("Invalid params");
      return false;
   }


   ////////////////////////////////////////
   // 1st pass: count elems and fill lights
   uint32_t nrOfVertices = 0;
   uint32_t nrOfFaces = 0;   
   uint32_t nrOfLights = list.getNrOfLights();   
   uint32_t nrOfRenderables = list.getNrOfRenderableElems();
   uint32_t nrOfMeshes = nrOfRenderables - nrOfLights;
   uint32_t nrOfMaterials = nrOfRenderables;
   
   for (uint32_t c = nrOfLights; c < nrOfRenderables; c++)
   {
      const Eng::List::RenderableElem &re = list.getRenderableElem(c);
      const Eng::Mesh &mesh = dynamic_cast<const Eng::Mesh &>(re.reference.get());
      const Eng::Vbo &vbo = mesh.getVbo();
      const Eng::Ebo &ebo = mesh.getEbo();
      nrOfVertices += vbo.getNrOfVertices();
      nrOfFaces += ebo.getNrOfFaces();
   }

   // ENG_LOG_DEBUG("Tot. nr. of faces . . :  %u", nrOfFaces);
   // ENG_LOG_DEBUG("Tot. nr. of vertices  :  %u", nrOfVertices);   


   /////////////////////////
   // 2nd pass: fill buffers
   std::vector<Eng::PipelineRayTracing::TriangleStruct> allTriangles(nrOfFaces);
   std::vector<Eng::PipelineRayTracing::BSphereStruct> allBSpheres(nrOfMeshes);
   std::vector<Eng::PipelineRayTracing::MaterialStruct> allMaterials(nrOfMaterials);
   nrOfFaces = 0; // Reset counter

   for (uint32_t c = 0; c < nrOfRenderables; c++)
   {
      const Eng::List::RenderableElem &re = list.getRenderableElem(c);

      // Get renderable world matrices:
      glm::mat4 modelMat = re.matrix;
      glm::mat3 normalMat = glm::inverseTranspose(re.matrix);

      // Lights:
      if (c < nrOfLights)
      {
             
      }
      
      // Meshes (and bounding spheres):
      else
      {
         const Eng::Mesh &mesh = dynamic_cast<const Eng::Mesh &>(re.reference.get());
         
         // Read VBO back:
         const Eng::Vbo &vbo = mesh.getVbo();
         std::vector<Eng::Vbo::VertexData> vData(vbo.getNrOfVertices());

         glBindBuffer(GL_ARRAY_BUFFER, vbo.getOglHandle());
         glGetBufferSubData(GL_ARRAY_BUFFER, 0, vbo.getNrOfVertices() * sizeof(Eng::Vbo::VertexData), vData.data());         

         // Read EBO back:
         const Eng::Ebo &ebo = mesh.getEbo();
         std::vector<Eng::Ebo::FaceData> fData(ebo.getNrOfFaces());

         glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo.getOglHandle());
         glGetBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, ebo.getNrOfFaces() * sizeof(Eng::Ebo::FaceData), fData.data());
         
         // ENG_LOG_DEBUG("Object: %s, data: %s, face: %u, %u, %u", mesh.getName().c_str(), glm::to_string(vData[0].vertex).c_str(), fData[0].a, fData[0].b, fData[0].c);

         // Bounding sphere:
         Eng::PipelineRayTracing::BSphereStruct s;
         s.firstTriangle = nrOfFaces;
         s.nrOfTriangles = ebo.getNrOfFaces();
         s.radius = mesh.getRadius();
         s.position = modelMat[3];
         allBSpheres[c - nrOfLights] = s;

         const Eng::Material& material = mesh.getMaterial();

         Eng::PipelineRayTracing::MaterialStruct m;
         m.albedo = glm::vec4(material.getAlbedo(), 1.f);
         m.emission = glm::vec4(material.getEmission(), 1.f);
         m.metalness = material.getMetalness();
         m.roughness = material.getRoughness();

         m.albedoTexHandle = material.getTexture(Eng::Texture::Type::albedo).getOglBindlessHandle();
         m.metalnessTexHandle = material.getTexture(Eng::Texture::Type::metalness).getOglBindlessHandle();
         m.roughnessTexHandle = material.getTexture(Eng::Texture::Type::roughness).getOglBindlessHandle();
         allMaterials[c - nrOfLights] = m;

         // Copy faces and vertices into the std::vector:
         for (uint32_t f = 0; f < ebo.getNrOfFaces(); f++)
         {
            Eng::PipelineRayTracing::TriangleStruct t;
            t.matId = c - nrOfLights;

            // First vertex:
            t.v[0] = modelMat * glm::vec4(vData[fData[f].a].vertex, 1.0f);
            t.n[0] = glm::vec4(normalMat * glm::vec3(glm::unpackSnorm3x10_1x2(vData[fData[f].a].normal)), 1.0f);
            t.u[0] = glm::vec2(glm::unpackHalf2x16(vData[fData[f].a].uv));

            // Second vertex:
            t.v[1] = modelMat * glm::vec4(vData[fData[f].b].vertex, 1.0f);
            t.n[1] = glm::vec4(normalMat * glm::vec3(glm::unpackSnorm3x10_1x2(vData[fData[f].b].normal)), 1.0f);
            t.u[1] = glm::vec2(glm::unpackHalf2x16(vData[fData[f].b].uv));

            // Third vertex:
            t.v[2] = modelMat * glm::vec4(vData[fData[f].c].vertex, 1.0f);
            t.n[2] = glm::vec4(normalMat * glm::vec3(glm::unpackSnorm3x10_1x2(vData[fData[f].c].normal)), 1.0f);
            t.u[2] = glm::vec2(glm::unpackHalf2x16(vData[fData[f].c].uv));

            allTriangles[nrOfFaces] = t;
            nrOfFaces++;
         }
      }
   }


   ////////////////////////////
   // 3rd: copy data into SSBOs
   reserved->triangles.create(nrOfFaces * sizeof(Eng::PipelineRayTracing::TriangleStruct), allTriangles.data());
   reserved->bspheres.create(nrOfMeshes * sizeof(Eng::PipelineRayTracing::BSphereStruct), allBSpheres.data());
   reserved->materials.create(nrOfMaterials * sizeof(Eng::PipelineRayTracing::MaterialStruct), allMaterials.data());

   // Done:
   reserved->nrOfTriangles = nrOfFaces;
   reserved->nrOfMeshes = nrOfMeshes;
   reserved->nrOfMaterials = nrOfMaterials;
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Main rendering method for the pipeline.  
 * @param camera view camera
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineRayTracing::render(const Eng::Camera &camera, const Eng::List &list, const Eng::PipelineGeometry &geometryPipe, uint32_t nrOfBounces)
{	
   // Safety net:
   if (camera == Eng::Camera::empty || list == Eng::List::empty)
   {
      ENG_LOG_ERROR("Invalid params");
      return false;
   }

   if (nrOfBounces == 0)
      return true;

   // Just to update the cache
   this->Eng::Pipeline::render(list); 

   // Lazy-loading:
   if (this->isDirty())
      if (!this->init())
      {
         ENG_LOG_ERROR("Unable to render (initialization failed)");
         return false;
      } 

   // Apply program:
   Eng::Program &program = getProgram();
   if (program == Eng::Program::empty)
   {
      ENG_LOG_ERROR("Invalid program");
      return false;
   }   
   program.render();     

   // Bindings:
   reserved->triangles.render(0);
   reserved->bspheres.render(1);
   reserved->materials.render(2);
   geometryPipe.getRayBuffer().render(3);
   geometryPipe.getRayBufferCounter().render(4);
   geometryPipe.getWorkgroupCount().render(5);

   // Uniforms:
   program.setUInt("nrOfBSpheres", reserved->nrOfMeshes);
   program.setUInt("nrOfBounces", nrOfBounces);

   // Execute:
   program.computeIndirect(geometryPipe.getWorkgroupCount().getOglHandle());
   program.wait();

   //uint32_t rayBufferSize;
   //geometryPipe.getRayBufferCounter().read(&rayBufferSize);
   //std::string out = "Ray-triangle intersections: ";
   //out += std::to_string(rayBufferSize-nrOfRays);
   //ENG_LOG_DEBUG(out.c_str());

   // Done:   
   return true;
}

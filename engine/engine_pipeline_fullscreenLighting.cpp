/**
 * @file		engine_pipeline_fullscreen2d.cpp 
 * @brief	A pipeline for rendering a texture to the fullscreen in 2D
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
static const std::string pipeline_vs = R"(
#version 460 core

// Out:
out vec2 uv;

void main()
{   
   float x = -1.0f + float((gl_VertexID & 1) << 2);
   float y = -1.0f + float((gl_VertexID & 2) << 1);
   
   uv.x = (x + 1.0f) * 0.5f;
   uv.y = (y + 1.0f) * 0.5f;
   
   gl_Position = vec4(x, y, 1.0f, 1.0f);
})";


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline fragment shader.
 */
static const std::string pipeline_fs = R"(
#version 460 core
#extension GL_ARB_bindless_texture : require
   
// In:   
in vec2 uv;
   
// Out:
out vec4 outFragment;

// Uniform:
layout (bindless_sampler) uniform sampler2D texture0;
layout (bindless_sampler) uniform sampler2D texture1;
layout (bindless_sampler) uniform sampler2D texture2;
layout (bindless_sampler) uniform isampler2D texture3;
uniform sampler2D shadowMaps[4];

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

layout(shared, binding=4) buffer RayData
{
   RayStruct rayData[];
};

// Uniforms:
uniform vec3 camPos;          // Camera position in World-Space

struct LightData {
   vec3 position;
   vec3 color;
   mat4 matrix;
};

uniform uint nrOfLights;
uniform LightData lightData[4];

const float PI = 3.14159265359;


/**
 * Computes the amount of shadow for a given fragment.
 * @param fragPosLightSpace frament coords in light space
 * @return shadow intensity
 */
float shadowAmount(vec4 fragPosLightSpace, int lightNum)
{
   // From "clip" to "ndc" coords:
   vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
   // Transform to the [0,1] range:
   projCoords = projCoords * 0.5f + 0.5f;
   
   // Get closest depth in the shadow map:
   float closestDepth = texture(shadowMaps[lightNum], projCoords.xy).r;    

   // Check whether current fragment is in shadow:
   return projCoords.z > closestDepth  ? 1.0f : 0.0f;   
}  


/**
 * Computes the light distribution.
 * @param Normal     frament normal in world space
 * @param Half       frament half-vector in world space
 * @param roughness  frament roughness
 * @return light distribution
 */
float DistributionGGX(vec3 normal, vec3 halfvector, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(normal, halfvector), 0.0);
    float NdotH2 = NdotH*NdotH;	
    float num    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;	
    return num / denom;
}


/**
 * Computes the Fresnel coefficient.
 * @param cosTheta   Angle between normal and view vector
 * @param F0         Initial Fresnel-coefficient
 * @return Fresnel coefficient
 */
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


/**
 * Computes the light distribution.
 * @param Normal     frament normal in world space
 * @param Viewvector frament view-vector in world space
 * @param alpha      roughness of sorts
 * @return A value for this points self-shadowing
 */
float evaluateGeometry(vec3 N, vec3 V, float alpha) 
{
   float k_direct = pow((alpha + 1.0f), 2.0f) / 8.0f;
   return dot(N, V) / (dot(N, V) * (1.0f - k_direct) + k_direct);
}

vec3 computeFinalLighting(vec3 viewPos, vec4 pixPos, vec3 pixNorm, vec3 pixAlbedo, float pixMetal, float pixRough) {
   vec3 color = pixAlbedo * .1f;       // hardcoded ambient term
   vec3 viewDir = normalize(viewPos - pixPos.xyz);

   if (dot(pixNorm, viewDir) < 0.0f){
      return color;
   }

   for(int i = 0; i < nrOfLights; i++) {
      vec3 lightDir = normalize(lightData[i].position.xyz - pixPos.xyz);
      vec3 halfVector = normalize(lightDir + viewDir);

      float cosTheta = max(dot(pixNorm, viewDir), .0f);
      vec3 F0 = mix(vec3(.04f), pixAlbedo, pixMetal);
      vec3  F = fresnelSchlick(cosTheta, F0);
      float G = evaluateGeometry(pixNorm, viewDir, pixRough);
      float D = DistributionGGX(pixNorm, halfVector, pixRough);

      float distance = length(pixPos.xyz - lightData[i].position.xyz);
      float attenuation = 1000.f / (distance * distance);
      vec3 radiance = lightData[i].color * attenuation;

      vec3 kD = (vec3(1.f) - F) * (1.f - pixMetal);
      vec3 lighting = kD * pixAlbedo / PI;
      vec3 specular = D * G * F;
      float denum = 4 * max(dot(pixNorm, viewDir), 0.f) * max(dot(pixNorm, lightDir), 0.f) + .0001f;
   
      specular /= denum;
   
      lighting += specular;
      lighting *= radiance *  max(dot(pixNorm, lightDir), 0.f);
      lighting = pow(lighting, vec3(1.f/2.2f));
      
      float shadow = shadowAmount(lightData[i].matrix * pixPos, i);

      color += lighting * (1.f - shadow);
   }
   
   return color;
}

vec3 computeRaycastedLighting(RayStruct ray) {
   vec3 color = vec3(.0f);
   vec3 viewPos = camPos;
   uint nrOfBounces = 0;
   while(ray.next != -1) {
      vec3 color = ray.albedo * .1f;       // hardcoded ambient term
      vec3 viewDir = normalize(viewPos - ray.position);

      for(int i = 0; i < nrOfLights; i++) {
         vec3 lightDir = normalize(lightData[i].position.xyz - ray.position);
         vec3 halfVector = normalize(lightDir + viewDir);

         float cosTheta = max(dot(ray.normal, viewDir), .0f);
         vec3 F0 = mix(vec3(.04f), ray.albedo, ray.metalness);
         vec3  F = fresnelSchlick(cosTheta, F0);

         float distance = length(ray.position - lightData[i].position.xyz);
         float attenuation = 1000.f / (distance * distance);
         vec3 radiance = lightData[i].color * attenuation;

         vec3 kD = (vec3(1.f) - F) * (1.f - ray.metalness);
         vec3 lighting = kD * ray.albedo / PI;
         lighting *= radiance *  max(dot(ray.normal, lightDir), 0.f);
         lighting = pow(lighting, vec3(1.f/2.2f));
      
         float shadow = shadowAmount(lightData[i].matrix * vec4(ray.position, 1.f), i);

         nrOfBounces++;
         color += (lighting * (1.f - shadow)) / nrOfBounces;
      }
      viewPos = ray.position;
      ray = rayData[ray.next];
   }
   
   color += computeFinalLighting(viewPos, vec4(ray.position, 1.0f), ray.normal, ray.albedo, ray.metalness, ray.roughness) / nrOfBounces;
   
   return color;
}

void main()
{
   // Texture lookup:
   vec4 pixWorldPos     = texture(texture0, uv);
   vec4 pixWorldNormal  = texture(texture1, uv);
   vec4 pixMaterial     = texture(texture2, uv);
   int rayId            = texture(texture3, uv).r;

   vec3 color = vec3(0.0f);
   
   if(rayId == -1 || rayData[rayId].next == -1) {
      color = computeFinalLighting(camPos, pixWorldPos, pixWorldNormal.xyz, pixMaterial.rgb, pixWorldNormal.w, pixMaterial.w);
    } else {
     color = computeRaycastedLighting(rayData[rayId]);
    }
   /*
   if(rayId == -1)
      color = vec3(1.f, 1.f, 1.f);
   else if(rayData[rayId].next == -1)
      color = vec3(1.f, 0.f, 0.f);
   else if(rayData[rayData[rayId].next].next == -1)
      color = vec3(0.f, 1.f, 0.f);
   else if(rayData[rayData[rayData[rayId].next].next].next == -1)
      color = vec3(0.f, 0.f, 1.f);
   else
      color = vec3(1.f, 1.f, 0.f);
   */
   outFragment = vec4(color.rgb, 1.0f);
}
)";



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief PipelineFullscreen2D reserved structure.
 */
struct Eng::PipelineFullscreenLighting::Reserved
{  
   Eng::Shader vs;
   Eng::Shader fs;
   Eng::Program program;      
   Eng::Vao vao;  ///< Dummy VAO, always required by context profiles

   /**
    * Constructor. 
    */
   Reserved()
   {}
};



////////////////////////////////////////
// BODY OF CLASS PipelineFullscreen2D //
////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::PipelineFullscreenLighting::PipelineFullscreenLighting() : reserved(std::make_unique<Eng::PipelineFullscreenLighting::Reserved>())
{	
   ENG_LOG_DETAIL("[+]");      
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::PipelineFullscreenLighting::PipelineFullscreenLighting(const std::string &name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineFullscreenLighting::Reserved>())
{	   
   ENG_LOG_DETAIL("[+]");   
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor. 
 */
ENG_API Eng::PipelineFullscreenLighting::PipelineFullscreenLighting(PipelineFullscreenLighting&&other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{  
   ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::PipelineFullscreenLighting::~PipelineFullscreenLighting()
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
bool ENG_API Eng::PipelineFullscreenLighting::init()
{
   // Already initialized?
   if (this->Eng::Managed::init() == false)
      return false;
   if (!this->isDirty())
      return false;

   // Build:
   reserved->vs.load(Eng::Shader::Type::vertex, pipeline_vs);
   reserved->fs.load(Eng::Shader::Type::fragment, pipeline_fs);   
   if (reserved->program.build({ reserved->vs, reserved->fs }) == false)
   {
      ENG_LOG_ERROR("Unable to build fullscreen2D program");
      return false;
   }
   this->setProgram(reserved->program);   

   // Init dummy VAO:
   if (reserved->vao.init() == false)
   {
      ENG_LOG_ERROR("Unable to init VAO for fullscreen2D");
      return false;
   }

   glm::ivec2 windowSize = Eng::Base::getInstance().getWindowSize();

   // Done: 
   this->setDirty(false);
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineFullscreenLighting::free()
{
   if (this->Eng::Managed::free() == false)
      return false;

   // Done:   
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Main rendering method for the pipeline.  
 * @param camera view camera
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineFullscreenLighting::render(const Eng::PipelineGeometry& geometries, const Eng::PipelineShadowMapping& shadowmap, const Eng::PipelineRayTracing& raytracing, const Eng::List &list)
{	
   // Safety net:
   if (geometries.getPositionBuffer() == Eng::Texture::empty || geometries.getNormalBuffer() == Eng::Texture::empty || geometries.getMaterialBuffer() == Eng::Texture::empty || list == Eng::List::empty)
   {
      ENG_LOG_ERROR("Invalid params");
      return false;
   }

   // Just to update the cache
   this->Eng::Pipeline::render(list); 

   // Lazy-loading:
   if (this->isDirty())
      if (!this->init())
      {
         ENG_LOG_ERROR("Unable to render (initialization failedl)");
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
   geometries.getPositionBuffer().render(0);
   geometries.getNormalBuffer().render(1);
   geometries.getMaterialBuffer().render(2);
   geometries.getRayBufferIndexTexture().render(3);
   geometries.getRayBuffer().render(4);

   GLuint64 handles[4];

   for (int i = 0; i < shadowmap.getShadowMapCount(); i++) {
      handles[i] = shadowmap.getShadowMaps()[i].getOglBindlessHandle();
   }
   program.setUInt64Array("shadowMaps", handles, shadowmap.getShadowMapCount());
   

   glm::mat4 camMat = Eng::Camera::getCached().getMatrix();
   float x = camMat[3][0];
   float y = camMat[3][1];
   float z = camMat[3][2];
   glm::vec3 camPos = glm::vec3(x, y, z);

   program.setVec3("camPos", camPos);

   // copy light data
   for (int i = 0; i < list.getNrOfLights(); i++) {
      const Eng::List::RenderableElem& lightRe = list.getRenderableElem(i);

      if (dynamic_cast<const Eng::Light&>(lightRe.reference.get()) == Eng::Light::empty) {
         ENG_LOG_ERROR("Invalid params");
         return false;
      }

      const Eng::Light& light = dynamic_cast<const Eng::Light&>(lightRe.reference.get());

      glm::mat4 lightMatrix = light.getMatrix();
      x = lightMatrix[3][0];
      y = lightMatrix[3][1];
      z = lightMatrix[3][2];
      glm::vec3 lightPos = glm::vec3(x, y, z);

      program.setVec3(std::string("lightData[") + std::to_string(i) + std::string("].position"), lightPos);
      program.setVec3(std::string("lightData[") + std::to_string(i) + std::string("].color"), light.getColor());

      glm::mat4 lpm = light.getProjMatrix();
      glm::mat4 lvm = glm::inverse(lightMatrix);
      glm::mat4 lightFinalMatrix = lpm * lvm; // lvm; // To convert from eye coords into light space
      program.setMat4(std::string("lightData[") + std::to_string(i) + std::string("].matrix"), lightFinalMatrix);
   }

   program.setUInt("nrOfLights", list.getNrOfLights());

   Eng::Base &eng = Eng::Base::getInstance();
   Eng::Fbo::reset(eng.getWindowSize().x, eng.getWindowSize().y);   

   // Smart trick:   
   reserved->vao.render();
   glDrawArrays(GL_TRIANGLES, 0, 3);

   // Done:   
   return true;
}

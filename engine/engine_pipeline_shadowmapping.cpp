/**
 * @file		engine_pipeline_shadowmapping.cpp 
 * @brief	A pipeline for generating planar shadow maps
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
 
// Per-vertex data from VBOs:
layout(location = 0) in vec3 a_vertex;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

// Uniforms:
uniform mat4 modelMat;        // Transformation per obejct
uniform mat4 viewMat;         // Transformation into camera space
uniform mat4 projectionMat;   // Projection

void main()
{   
   vec4 position = modelMat * vec4(a_vertex, 1.0f);
   vec4 tmp = viewMat * position;
   gl_Position = projectionMat * tmp;
})";


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Default pipeline fragment shader.
 */
static const std::string pipeline_fs = R"(
#version 460 core

void main()
{
})";



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief PipelineShadowMapping reserved structure.
 */
struct Eng::PipelineShadowMapping::Reserved
{  
   #define MAX_LIGHTS 4

   Eng::Shader vs;
   Eng::Shader fs;
   Eng::Program program;
   Eng::Texture depthMaps[MAX_LIGHTS];
   Eng::Fbo fbo;

   uint32_t shadowMapCount;


   /**
    * Constructor. 
    */
   Reserved() : shadowMapCount{ 0 }
   {}
};



/////////////////////////////////////////
// BODY OF CLASS PipelineShadowMapping //
/////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping() : reserved(std::make_unique<Eng::PipelineShadowMapping::Reserved>())
{	
   ENG_LOG_DETAIL("[+]");      
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping(const std::string &name) : Eng::Pipeline(name), reserved(std::make_unique<Eng::PipelineShadowMapping::Reserved>())
{	   
   ENG_LOG_DETAIL("[+]");   
   this->setProgram(reserved->program);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor. 
 */
ENG_API Eng::PipelineShadowMapping::PipelineShadowMapping(PipelineShadowMapping &&other) : Eng::Pipeline(std::move(other)), reserved(std::move(other.reserved))
{  
   ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::PipelineShadowMapping::~PipelineShadowMapping()
{	
   ENG_LOG_DETAIL("[-]");
   if (this->isInitialized())
      free();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets number of shadow maps stored.
 * @return number of shadow maps stored
 */
const uint32_t ENG_API Eng::PipelineShadowMapping::getShadowMapCount() const
{
   return reserved->shadowMapCount;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets shadow map texture array reference.
 * @return shadow map texture array reference
 */
const Eng::Texture ENG_API *Eng::PipelineShadowMapping::getShadowMaps() const
{	
   return reserved->depthMaps;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes this pipeline with 1 shadowmap.
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::init()
{
   return this->init(1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes this pipeline. 
 * @param nrOfLights number of lights to render shadowmaps for
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::init(int nrOfLights)
{
   // Already initialized?
   if (this->Eng::Managed::init() == false)
      return false;
   if (!this->isDirty())
      return false;
   if (nrOfLights <= 0)
      return false;

   // Build:
   reserved->vs.load(Eng::Shader::Type::vertex, pipeline_vs);
   reserved->fs.load(Eng::Shader::Type::fragment, pipeline_fs);   
   if (reserved->program.build({ reserved->vs, reserved->fs }) == false)
   {
      ENG_LOG_ERROR("Unable to build shadow mapping program");
      return false;
   }
   this->setProgram(reserved->program);

   // Depth map:
   for (int i = 0; i < nrOfLights; i++) {
      if (reserved->depthMaps[i].create(depthTextureSize, depthTextureSize, Eng::Texture::Format::depth) == false)
      {
         ENG_LOG_ERROR("Unable to init depth map");
         return false;
      }
      reserved->shadowMapCount += 1;
   }


   // Done: 
   this->setDirty(false);
   return true;
}

bool ENG_API Eng::PipelineShadowMapping::attachDepthTexture(int lightNumber) {

   // Depth FBO:
   if (!reserved->fbo.attachTexture(reserved->depthMaps[lightNumber])) {
      ENG_LOG_ERROR((std::string("Unable to attach depth texture for light #") + std::to_string(lightNumber)).c_str());
   }
   if (reserved->fbo.validate() == false)
   {
      ENG_LOG_ERROR("Unable to init depth FBO");
      return false;
   }
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases this pipeline.
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::free()
{
   if (this->Eng::Managed::free() == false)
      return false;

   // Done:   
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Main rendering method for the pipeline.  
 * @param lightRe light renderable element
 * @param list list of renderables
 * @return TF
 */
bool ENG_API Eng::PipelineShadowMapping::render(const Eng::List &list)
{	
   // Safety net:
   if (list == Eng::List::empty)
   {
      ENG_LOG_ERROR("Invalid params");
      return false;
   }

   // Just to update the cache
   this->Eng::Pipeline::render(list); 

   // Lazy-loading:
   if (this->isDirty())
      if (!this->init(list.getNrOfLights()))
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

   // Render one light at time:
   for (int i = 0; i < list.getNrOfLights(); i++) {
      
      const Eng::List::RenderableElem& lightRe = list.getRenderableElem(i);
      
      if (dynamic_cast<const Eng::Light&>(lightRe.reference.get()) == Eng::Light::empty) {
         ENG_LOG_ERROR("Invalid params");
         return false;
      }

      const Eng::Light& light = dynamic_cast<const Eng::Light&>(lightRe.reference.get());

      program.render();
      program.setMat4("projectionMat", light.getProjMatrix());

      // Bind FBO and change OpenGL settings:
      if (!this->attachDepthTexture(i)) {
         ENG_LOG_ERROR("Cannot attach depth texture");
         return false;
      }
      reserved->fbo.render();
      glClear(GL_DEPTH_BUFFER_BIT);
      glColorMask(0, 0, 0, 0);
      //glClearDepth(0.5);
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);

      // Light source is the camera:
      glm::mat4 viewMatrix = glm::inverse(lightRe.matrix);

      // Render meshes:   
      list.render(viewMatrix, Eng::List::Pass::meshes);

      // Redo OpenGL settings:
      glCullFace(GL_BACK);
      glDisable(GL_CULL_FACE);
      glColorMask(1, 1, 1, 1);
   }
      
   Eng::Base &eng = Eng::Base::getInstance();
   Eng::Fbo::reset(eng.getWindowSize().x, eng.getWindowSize().y);   
  
   // Done:   
   return true;
}

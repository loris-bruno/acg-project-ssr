/**
 * @file		engine_mesh.cpp
 * @brief	Geometric mesh
 *
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */



 //////////////
 // #INCLUDE //
 //////////////

    // Main include:
#include "engine.h"

// GLM:
#include <glm/gtc/packing.hpp>  

// OGL:      
#include <GL/glew.h>
#include <GLFW/glfw3.h>



////////////
// STATIC //
////////////

   // Special values:
Eng::Mesh Eng::Mesh::empty("[empty]");



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief Mesh class reserved structure.
 */
struct Eng::Mesh::Reserved
{
   // Buffers:
   Eng::Vao vao;
   Eng::Vbo vbo;
   Eng::Ebo ebo;

   // Material:
   std::reference_wrapper<const Eng::Material> material;

   // Bounding volumes:
   float radius;


   /**
    * Constructor
    */
   Reserved() : material{ Eng::Material::empty }, radius { 1.0f }
   {}
};



////////////////////////
// BODY OF CLASS Mesh //
////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::Mesh::Mesh() : reserved(std::make_unique<Eng::Mesh::Reserved>())
{
   ENG_LOG_DETAIL("[+]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name mesh name
 */
ENG_API Eng::Mesh::Mesh(const std::string& name) : Eng::Node(name), reserved(std::make_unique<Eng::Mesh::Reserved>())
{
   ENG_LOG_DETAIL("[+]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor.
 */
ENG_API Eng::Mesh::Mesh(Mesh&& other) : Eng::Node(std::move(other)), reserved(std::move(other.reserved))
{
   ENG_LOG_DETAIL("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::Mesh::~Mesh()
{
   ENG_LOG_DETAIL("[-]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Sets material.
 * @param mat material
 * @return TF
 */
bool ENG_API Eng::Mesh::setMaterial(const Eng::Material& mat)
{
   reserved->material = mat;

   // Done:
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets material.
 * @return used material or Material::empty if not set
 */
const Eng::Material ENG_API& Eng::Mesh::getMaterial() const
{
   return reserved->material;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets a reference to the used VBO.
 * @return reference to used VBO
 */
const Eng::Vbo ENG_API& Eng::Mesh::getVbo() const
{
   return reserved->vbo;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets a reference to the used EBO.
 * @return reference to used EBO
 */
const Eng::Ebo ENG_API& Eng::Mesh::getEbo() const
{
   return reserved->ebo;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Gets the bounding sphere radius for this mesh.
 * @return bounding sphere radius
 */
const float ENG_API Eng::Mesh::getRadius() const
{
   return reserved->radius;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Loads the specific information of a given object. In its base class, this function loads the file version chunk.
 * @param serializer serial data
 * @param data optional pointer
 * @return TF
 */
uint32_t ENG_API Eng::Mesh::loadChunk(Eng::Serializer& serial, void* data)
{
   // Chunk header
   uint32_t chunkId;
   serial.deserialize(&chunkId, sizeof(uint32_t));
   if (chunkId != static_cast<uint32_t>(Ovo::ChunkId::mesh))
   {
      ENG_LOG_ERROR("Invalid chunk ID found");
      return 0;
   }
   uint32_t chunkSize;
   serial.deserialize(&chunkSize, sizeof(uint32_t));

   // Node properties:       
   std::string name;
   serial.deserialize(name);
   this->setName(name);

   glm::mat4 matrix;
   serial.deserialize(matrix);
   this->setMatrix(matrix);

   uint32_t nrOfChildren;
   serial.deserialize(nrOfChildren);

   std::string target;
   serial.deserialize(target);

   // Data:
   uint8_t subtype;
   serial.deserialize(subtype);

   std::string materialName;
   serial.deserialize(materialName);
   std::reference_wrapper<const Eng::Material> mat = Eng::Material::empty;
   mat = dynamic_cast<Eng::Material&>(Eng::Container::getInstance().find(materialName));
   this->setMaterial(mat);

   float radius;
   serial.deserialize(radius);

   glm::vec3 bboxMin;
   serial.deserialize(bboxMin);

   glm::vec3 bboxMax;
   serial.deserialize(bboxMax);

   uint8_t hasPhysics;
   serial.deserialize(hasPhysics);
   if (hasPhysics)
   {
      ENG_LOG_ERROR("Physics section not supported");
      return 0;
   }

   uint32_t nrOfLods;
   serial.deserialize(nrOfLods);

   for (uint32_t curLod = 0; curLod < nrOfLods; curLod++)
   {
      uint32_t nrOfVertices;
      serial.deserialize(nrOfVertices);

      uint32_t nrOfFaces;
      serial.deserialize(nrOfFaces);

      ENG_LOG_PLAIN("LOD: %u, v: %u, f: %u", curLod + 1, nrOfVertices, nrOfFaces);

      std::vector<Eng::Vbo::VertexData> allVertices(nrOfVertices);
      serial.deserialize(allVertices.data(), nrOfVertices * sizeof(Eng::Vbo::VertexData));

      std::vector<Eng::Ebo::FaceData> allFaces(nrOfFaces);
      serial.deserialize(allFaces.data(), nrOfFaces * sizeof(Eng::Ebo::FaceData));

      // Store only first LOD for now:
      if (curLod == 0)
      {
         reserved->vao.init();
         reserved->vao.render();

         reserved->vbo.create(nrOfVertices, allVertices.data());
         reserved->ebo.create(nrOfFaces, allFaces.data());
      }
   }

   // Done:      
   return nrOfChildren;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Rendering method.
 * @param value generic value
 * @param data generic pointer to any kind of data
 * @return TF
 */
bool ENG_API Eng::Mesh::render(uint32_t value, void* data) const
{
   Eng::Program& program = dynamic_cast<Eng::Program&>(Eng::Program::getCached());

   Eng::List::RenderableElemInfo* info = (Eng::List::RenderableElemInfo*)data;

   program.setMat4("modelMat", info->objMatrix);
   program.setMat4("viewMat", info->camMatrix);
   program.setMat3("normalMat", glm::inverseTranspose(glm::mat3(info->objMatrix)));

   reserved->material.get().render();

   reserved->vao.render();
   glDrawElements(GL_TRIANGLES, reserved->ebo.getNrOfFaces() * 3, GL_UNSIGNED_INT, nullptr);

   // Done:
   return true;
}

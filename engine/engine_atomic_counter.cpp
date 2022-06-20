/**
 * @file		engine_atomic_counter.cpp
 * @brief	OpenGL Atomic Counter (VBO)
 *
 * @author	Alessandro Ferrari (alessandro.ferrari@supsi.ch), (C) SUPSI
 */



 //////////////
 // #INCLUDE //
 //////////////

    // Main include:
#include "engine.h"

// OGL:      
#include <GL/glew.h>
#include <GLFW/glfw3.h>



////////////
// STATIC //
////////////

   // Special values:
Eng::AtomicCounter Eng::AtomicCounter::empty("[empty]");



/////////////////////////
// RESERVED STRUCTURES //
/////////////////////////

/**
 * @brief AtomicCounter reserved structure.
 */
struct Eng::AtomicCounter::Reserved
{
   GLuint oglId;           ///< OpenGL shader ID
   uint64_t size;          ///< Size in bytes


   /**
    * Constructor.
    */
   Reserved() : oglId{ 0 }, size{ 0 }
   {}
};



////////////////////////
// BODY OF CLASS AtomicCounter //
////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor.
 */
ENG_API Eng::AtomicCounter::AtomicCounter() : reserved(std::make_unique<Eng::AtomicCounter::Reserved>())
{
   ENG_LOG_DEBUG("[+]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Constructor with name.
 * @param name node name
 */
ENG_API Eng::AtomicCounter::AtomicCounter(const std::string& name) : Eng::Object(name), reserved(std::make_unique<Eng::AtomicCounter::Reserved>())
{
   ENG_LOG_DEBUG("[+]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Move constructor.
 */
ENG_API Eng::AtomicCounter::AtomicCounter(AtomicCounter&& other) : Eng::Object(std::move(other)), reserved(std::move(other.reserved))
{
   ENG_LOG_DEBUG("[M]");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Destructor.
 */
ENG_API Eng::AtomicCounter::~AtomicCounter()
{
   ENG_LOG_DEBUG("[-]");
   if (reserved)
      this->free();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Return the GLuint buffer ID.
 * @return buffer ID or 0 if not valid
 */
uint32_t ENG_API Eng::AtomicCounter::getOglHandle() const
{
   return reserved->oglId;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Return the size in bytes of the buffer.
 * @return size in bytes
 */
uint64_t ENG_API Eng::AtomicCounter::getSize() const
{
   return reserved->size;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Initializes an OpenGL AtomicCounter.
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::init()
{
   if (this->Eng::Managed::init() == false)
      return false;

   // Free buffer if already stored:
   if (reserved->oglId)
   {
      glDeleteBuffers(1, &reserved->oglId);
      reserved->oglId = 0;
      reserved->size = 0;
   }

   // Create it:		    
   glGenBuffers(1, &reserved->oglId);

   // Done:   
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Releases an OpenGL AtomicCounter.
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::free()
{
   if (this->Eng::Managed::free() == false)
      return false;

   // Free AtomicCounter if stored:
   if (reserved->oglId)
   {
      glDeleteBuffers(1, &reserved->oglId);
      reserved->oglId = 0;
      reserved->size = 0;
   }

   // Done:   
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Create buffer by allocating the required storage.
 * @param size size in bytes
 * @param data pointer to the data to copy into the buffer
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::create(uint64_t size)
{
   // Release, if already used:
   if (this->isInitialized())
      this->free();

   // Init buffer:
   if (!this->isInitialized())
      this->init();

   // Fill it:		              
   const GLuint oglId = this->getOglHandle();
   glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, oglId);
   glBufferData(GL_ATOMIC_COUNTER_BUFFER, size, NULL, GL_DYNAMIC_DRAW);

   // Done:
   reserved->size = size;
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Maps this AtomicCounter for direct C-sided operations.
 * @param mapping kind of mapping (use enums)
 * @return pointer to the mapped area or nullptr on error
 */
void ENG_API* Eng::AtomicCounter::map(Eng::AtomicCounter::Mapping mapping)
{
   GLint bufMask = 0;

   // Bind buffer and map:   
   glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, reserved->oglId); // <-- rendering to base 0 by default!
   switch (mapping)
   {
   case Mapping::read: bufMask = GL_MAP_READ_BIT; break;
   case Mapping::write: bufMask = GL_MAP_WRITE_BIT; break;
   }
   bufMask |= GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
   return glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, reserved->size, bufMask);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Unmap this AtomicCounter.
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::unmap()
{
   glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);

   // Done:
   return true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Reset this AtomicCounter.
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::reset() const
{

   GLuint* tmp;
   glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, reserved->oglId);
   tmp = (GLuint*) glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, reserved->size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
   memset(tmp, 0, reserved->size);
   glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
   // Done:
   return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Read the value of this AtomicCounter.
 * @param data pointer to host memory data will be read back into.
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::read(void* data) const
{
   GLuint* tmp;
   glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, reserved->oglId);
   tmp = (GLuint*)glMapBufferRange(GL_ATOMIC_COUNTER_BUFFER, 0, reserved->size, GL_MAP_READ_BIT);
   memcpy(data, tmp, reserved->size);
   glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
   // Done:
   return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Read the value of this AtomicCounter.
 * @param data pointer to host memory data will be read back into.
 * @return TF
 */
void ENG_API Eng::AtomicCounter::wait()
{
   glMemoryBarrier(GL_ATOMIC_COUNTER_BARRIER_BIT);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Rendering method.
 * @param value generic value
 * @param data generic pointer to any kind of data
 * @return TF
 */
bool ENG_API Eng::AtomicCounter::render(uint32_t value, void* data) const
{
   glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, value, reserved->oglId);

   // Done:
   return true;
}

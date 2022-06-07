/**
 * @file		engine_atomic_counter.h
 * @brief	OpenGL Atomic Counter
 *
 * @author	Achille Peternier (achille.peternier@supsi.ch), Alessandro Ferrari (alessandro.ferrari@supsi.ch), (C) SUPSI
 */
#pragma once

 /**
  * @brief Class for modeling an Atomic Counter.
  */

class ENG_API AtomicCounter final : public Eng::Object, public Eng::Managed
{
//////////
public: //
//////////

   // Special values:
   static AtomicCounter empty;


   /**
    * @brief Types of mapping.
    */
   enum class Mapping : uint32_t
   {
      read,
      write
   };


   // Const/dest:
   AtomicCounter();
   AtomicCounter(AtomicCounter&& other);
   AtomicCounter(AtomicCounter const&) = delete;
   ~AtomicCounter();

   // Get/set:   
   uint64_t getSize() const;
   uint32_t getOglHandle() const;

   // Data:
   bool create(uint64_t size);
   void* map(Mapping mapping);
   bool unmap();
   bool reset();
   bool read(void* data);

   // Rendering methods:   
   bool render(uint32_t value = 0, void* data = nullptr) const;

   // Managed:
   bool init() override;
   bool free() override;


///////////
private: //
///////////

   // Reserved:
   struct Reserved;
   std::unique_ptr<Reserved> reserved;

   // Const/dest:
   AtomicCounter(const std::string& name);

};


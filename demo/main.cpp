/**
 * @file		main.cpp
 * @brief	Engine usage example
 * 
 * @author	Achille Peternier (achille.peternier@supsi.ch), (C) SUPSI
 */



//////////////
// #INCLUDE //
//////////////

   // Main engine header:
   #include "engine.h"

   // C/C++:
   #include <iostream>



//////////   
// VARS //
//////////

   // Mouse status:
   double oldMouseX, oldMouseY;
   float rotX, rotY;
   bool mouseBR, mouseBL;
   float transZ = 50.0f;

   // Pipelines:
   Eng::PipelineDefault dfltPipe;
   Eng::PipelineShadowMapping shadowPipe;
   Eng::PipelineGeometry geometryPipe;
   Eng::PipelineFullscreen2D full2dPipe;
   Eng::PipelineFullscreenLighting lightingPipe;
   Eng::PipelineRayTracing raytracingPipe;

   float roughnessThreshold = 0.25f;

///////////////
// CALLBACKS //
///////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse cursor callback.
 * @param mouseX updated mouse X coordinate
 * @param mouseY updated mouse Y coordinate
 */
void mouseCursorCallback(double mouseX, double mouseY)
{
   // ENG_LOG_DEBUG("x: %.1f, y: %.1f", mouseX, mouseY);
   float deltaY = (float) (mouseX - oldMouseX);
   oldMouseX = mouseX;
   if (mouseBR)
      rotY += deltaY;

   float deltaX = (float) (mouseY - oldMouseY);
   oldMouseY = mouseY;
   if (mouseBR)
      rotX += deltaX;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse button callback.
 * @param button mouse button id
 * @param action action
 * @param mods modifiers
 */
void mouseButtonCallback(int button, int action, int mods)
{
   // ENG_LOG_DEBUG("button: %d, action: %d, mods: %d", button, action, mods);
   switch (button)
   {
      case 0: mouseBL = (bool) action; break;
      case 1: mouseBR = (bool) action; break;
   }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Mouse scroll callback.
 * @param scrollX updated mouse scroll X coordinate
 * @param scrollY updated mouse scroll Y coordinate
 */
void mouseScrollCallback(double scrollX, double scrollY)
{
   // ENG_LOG_DEBUG("x: %.1f, y: %.1f", scrollX, scrollY);
   transZ -= (float) scrollY;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Keyboard callback.
 * @param key key code
 * @param scancode key scan code
 * @param action action
 * @param mods modifiers
 */
void keyboardCallback(int key, int scancode, int action, int mods)
{
    #define GLFW_KEY_DOWN      264
    #define GLFW_KEY_UP        265 
   // ENG_LOG_DEBUG("key: %d, scancode: %d, action: %d, mods: %d", key, scancode, action, mods);
    if (action == 0)
        return;
   switch (key)
   {
       case GLFW_KEY_UP:
           if (roughnessThreshold < 1.0f) roughnessThreshold += 0.05f; 
           
           break;
       case GLFW_KEY_DOWN: 
           if (roughnessThreshold > 0.0f) roughnessThreshold -= 0.05f; 
           break;

   }
   std::string outstring = "\n\nRoughness threshold: ";
   outstring += std::to_string(roughnessThreshold);
   outstring += "\n\n";
   ENG_LOG_DEBUG(outstring.c_str());
}


//////////
// MAIN //
//////////

/**
 * Application entry point.
 * @param argc number of command-line arguments passed
 * @param argv array containing up to argc passed arguments
 * @return error code (0 on success, error code otherwise)
 */
int main(int argc, char *argv[])
{
   // Credits:
   std::cout << "Engine demo, A. Peternier (C) SUPSI" << std::endl;
   std::cout << std::endl;

   // Init engine:
   Eng::Base &eng = Eng::Base::getInstance();
   eng.init();

   // Register callbacks:
   eng.setMouseCursorCallback(mouseCursorCallback);
   eng.setMouseButtonCallback(mouseButtonCallback);
   eng.setMouseScrollCallback(mouseScrollCallback);
   eng.setKeyboardCallback(keyboardCallback);

   std::string outstring = "screen x: ";
   outstring += std::to_string(eng.getWindowSize().x);
   outstring += ", screen y: ";
   outstring += std::to_string(eng.getWindowSize().y);
   ENG_LOG_DEBUG(outstring.c_str());

   /////////////////
   // Loading scene:   
   Eng::Ovo ovo; 
   std::reference_wrapper<Eng::Node> root = ovo.load("simpler3dScene.ovo");
   std::cout << "Scene graph:\n" << root.get().getTreeAsString() << std::endl;
   
   // Get light ref:
   dynamic_cast<Eng::Light&>(Eng::Container::getInstance().find("Omni001")).setProjMatrix(glm::perspective(glm::radians(75.f), 1.0f, .1f, 100.f)); // Perspective projection
   dynamic_cast<Eng::Light&>(Eng::Container::getInstance().find("Omni002")).setProjMatrix(glm::perspective(glm::radians(150.f), 1.0f, .1f, 100.f));
   
   Eng::Light& light2 = dynamic_cast<Eng::Light&>(Eng::Container::getInstance().find("Omni002"));
   light2.setCutoff(75.f);
   light2.setSubtype(1);


   // Get torus knot ref:
   std::reference_wrapper<Eng::Mesh> tknot = dynamic_cast<Eng::Mesh &>(Eng::Container::getInstance().find("Torus Knot001"));   

   // Rendering elements:
   Eng::List list;
   Eng::Camera camera;
   camera.setProjMatrix(glm::perspective(glm::radians(45.0f), eng.getWindowSize().x / (float) eng.getWindowSize().y, 1.0f, 1000.0f));

   /////////////
   // Main loop:
   std::cout << "Entering main loop..." << std::endl;      
   while (eng.processEvents())
   {      
      // Update viewpoint:
      glm::mat4 tmp = glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-rotY), { 0.0f, 1.0f, 0.0f }), glm::radians(-rotX), { 1.0f, 0.0f, 0.0f });
      tmp = tmp * glm::mat4(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, transZ)));
      camera.setMatrix(tmp);

      // Animate torus knot:      
      tknot.get().setMatrix(glm::rotate(tknot.get().getMatrix(), glm::radians(0.5f), glm::vec3(0.0f, 1.0f, 0.0f)));

      // Update list:
      list.reset();
      list.process(root);
      
      // Clear last frame:
      eng.clear();

      // Render shadow maps:
      shadowPipe.render(list);

      // Render geometry buffer:
      camera.render();
      glm::mat4 viewMatrix = glm::inverse(camera.getWorldMatrix());

      uint64_t start = Eng::Timer::getInstance().getCounter();
      geometryPipe.render(viewMatrix, list, roughnessThreshold);
      uint64_t end = Eng::Timer::getInstance().getCounter();
      double time = Eng::Timer::getInstance().getCounterDiff(start, end);
      std::string out = "Geometry pipeline time: ";
      out += std::to_string(time);
      out += "ms";
      ENG_LOG_DEBUG(out.c_str());

      start = Eng::Timer::getInstance().getCounter();
      raytracingPipe.migrate(list);
      end = Eng::Timer::getInstance().getCounter();
      time = Eng::Timer::getInstance().getCounterDiff(start, end);
      out = "Raytracing migrate time: ";
      out += std::to_string(time);
      out += "ms";
      ENG_LOG_DEBUG(out.c_str());
      
      start = Eng::Timer::getInstance().getCounter();
      raytracingPipe.render(camera, list, geometryPipe);
      end = Eng::Timer::getInstance().getCounter();
      time = Eng::Timer::getInstance().getCounterDiff(start, end);
      out = "Raytracing pipeline time: ";
      out += std::to_string(time);
      out += "ms";
      ENG_LOG_DEBUG(out.c_str());

      ////dfltPipe.render(camera, list);
      //
      ///// Uncomment the following line for displaying the shadow map:
      ////full2dPipe.render(shadowPipe.getShadowMaps()[0], list);

      ///// Uncomment the following line to display content of the GBuffer (select the exact texture to be displayed here):
      ///// options: getNormalBuffer(), getPositionBuffer(), getMaterialBuffer()
      ////full2dPipe.render(dfltPipe.getGeometryPipeline().getPositionBuffer(), list);
      ////full2dPipe.render(dfltPipe.getGeometryPipeline().getNormalBuffer(), list);
      ////full2dPipe.render(dfltPipe.getGeometryPipeline().getMaterialBuffer(), list);


      /// Visualize the shaded scene by drawing a fullscreen quad
      start = Eng::Timer::getInstance().getCounter();
      lightingPipe.render(geometryPipe, shadowPipe, raytracingPipe, list);
      end = Eng::Timer::getInstance().getCounter();
      time = Eng::Timer::getInstance().getCounterDiff(start, end);
      out = "Shading pipeline time: ";
      out += std::to_string(time);
      out += "ms";
      ENG_LOG_DEBUG(out.c_str());


      eng.swap();
   }
   std::cout << "Leaving main loop..." << std::endl;

   // Release engine:
   eng.free();

   // Done:
   std::cout << "[application terminated]" << std::endl;
   return 0;
}

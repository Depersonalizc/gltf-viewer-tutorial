#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>
#include <random>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "utils/cameras.hpp"
#include "utils/gltf.hpp"
#include "utils/images.hpp"

#include <stb_image_write.h>
#include <tiny_gltf.h>

float lerp(float a, float b, float f) {
  return a + f * (b - a);
}

void keyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, 1);
  }
}

int ViewerApplication::run()
{
  initPrograms();
  initUniforms();

  tinygltf::Model model;
  // Load the glTF file
  if (!loadGltfFile(model)) {
    return -1;
  }

  // Creation of texture object
  const auto textureObjects = createTextureObjects(model);

  // Default white texture
  GLuint whiteTexture = createDefaultTexture();

  // Creation of Buffer Objects
  const auto bufferObjects = createBufferObjects(model);

  // Creation of Vertex Array Objects
  std::vector<VaoRange> meshIndexToVaoRange;
  const auto vertexArrayObjects = createVertexArrayObjects(model, bufferObjects, meshIndexToVaoRange);

  // Compute scene bounds and get min and max of bounding box
  glm::vec3 bboxMin, bboxMax;
  computeSceneBounds(model, bboxMin, bboxMax);
  glm::vec3 diag = bboxMax - bboxMin;

  // Build projection matrix
  auto maxDistance = glm::length(diag); // Use scene bounds to compute the maxDistance
  maxDistance = maxDistance > 0.f ? maxDistance : 100.f;
  const auto projMatrix = glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  // Implement a new CameraController model and use it instead. > Done
  // Propose the choice from the GUI > Done
  std::unique_ptr<CameraController> cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.01f);
  if (m_hasUserCamera) {
    cameraController->setCamera(m_userCamera);
  } else {
    // Use scene bounds to compute a better default camera
    const auto center = 0.5f * (bboxMax + bboxMin);
    const auto up = glm::vec3(0, 1, 0);
    const auto eye = diag.z > 0 ? center + diag : center + 2.f * glm::cross(diag, up);
    cameraController->setCamera(Camera{eye, center, up});
  }

  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);

  // Init light parameters
  glm::vec3 lightDirection(1.f);
  glm::vec3 lightIntensity(3.f);
  auto occlusionStrength = 0u;

  // Bolleans to toggle features in GUI
  bool useBaseColor = true;
  bool useMetallicRoughnessTexture = true;
  bool useEmissive = true;
  bool useOcclusionMap = true;
  m_useSSAO = true;

  const auto bindMaterial = [&](const auto materialIndex) {
    float baseColorFactor[] = {1.f, 1.f, 1.f, 1.f};
    
    // Material binding
    if (materialIndex >= 0) {
      // Get Material
      const auto &material = model.materials[materialIndex];

      // Default Base Color
      auto baseColorTex = whiteTexture;
      // Default Metallic Roughness
      auto metallicRoughnessTex = 0u;
      auto metallicFactor = 0;
      auto roughnessFactor = 0;
      // Default Emissive Texture
      auto emissiveTex = 0u;
      float emissiveFactor[] = {1.f, 1.f, 1.f};
      // Default Occlusion
      auto occlusionTex = 0u;
      
      // Base color texture
      if (useBaseColor && material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
        baseColorTex = textureObjects[material.pbrMetallicRoughness.baseColorTexture.index];
        baseColorFactor[0] = (float) material.pbrMetallicRoughness.baseColorFactor[0];
        baseColorFactor[1] = (float) material.pbrMetallicRoughness.baseColorFactor[1];
        baseColorFactor[2] = (float) material.pbrMetallicRoughness.baseColorFactor[2];
        baseColorFactor[3] = (float) material.pbrMetallicRoughness.baseColorFactor[3];
      }

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, baseColorTex);
      glUniform1i(m_uBaseColorTextureLocation, 0);
      glUniform4f(m_uBaseColorFactorLocation,
        baseColorFactor[0],
        baseColorFactor[1],
        baseColorFactor[2],
        baseColorFactor[3]);

      // Metallic / Roughness texture
      if (useMetallicRoughnessTexture && material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0) {
        metallicRoughnessTex = textureObjects[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
        metallicFactor = material.pbrMetallicRoughness.metallicFactor;
        roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
      }

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, metallicRoughnessTex);
      glUniform1i(m_uMetallicRoughnessTextureLocation, 1);
      glUniform1f(m_uMetallicFactorLocation, metallicFactor);
      glUniform1f(m_uRoughnessFactorLocation, roughnessFactor);

      // EmissiveTexture
      if (useEmissive && material.emissiveTexture.index >= 0) {
        emissiveTex = textureObjects[material.emissiveTexture.index];
        emissiveFactor[0] = (float) material.emissiveFactor[0];
        emissiveFactor[1] = (float) material.emissiveFactor[1];
        emissiveFactor[2] = (float) material.emissiveFactor[2];
      }

      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_2D, emissiveTex);
      glUniform1i(m_uEmissiveTextureLocation, 2);
      glUniform3f(m_uEmissiveFactorLocation, 
        emissiveFactor[0],
        emissiveFactor[1],
        emissiveFactor[2]);

      // OcclusionTexture
      glActiveTexture(GL_TEXTURE3);
      if (useOcclusionMap && material.occlusionTexture.index >= 0) {
        occlusionTex = textureObjects[material.occlusionTexture.index];
        occlusionStrength = material.occlusionTexture.strength;
        glBindTexture(GL_TEXTURE_2D, occlusionTex);
      } else {
        glBindTexture(GL_TEXTURE_2D, whiteTexture);
      }
      glUniform1i(m_uOcclusionTextureLocation, 3);

      return;
    }

    // Set default white texture if no material
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, whiteTexture);
    glUniform1i(m_uBaseColorTextureLocation, 0);
    glUniform4f(m_uBaseColorFactorLocation, 
      baseColorFactor[0], 
      baseColorFactor[1], 
      baseColorFactor[2], 
      baseColorFactor[3]);
  };

  // Lambda function to draw the scene
  const auto drawScene = [&](const Camera &camera) {
    glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto viewMatrix = camera.getViewMatrix();

    // if (m_uLightDirectionLocation >= 0) {
    //   const auto lightDirectionInViewSpace =
    //     glm::normalize(glm::vec3(viewMatrix * glm::vec4(lightDirection, 0.)));
    //   glUniform3f(m_uLightDirectionLocation, lightDirectionInViewSpace[0],
    //     lightDirectionInViewSpace[1], lightDirectionInViewSpace[2]);
    // }

    // if (m_uLightIntensityLocation >= 0) {
    //   glUniform3f(m_uLightIntensityLocation, lightIntensity[0], lightIntensity[1],
    //     lightIntensity[2]);
    // }

    // The recursive function that should draw a node
    // We use a std::function because a simple lambda cannot be recursive
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix) {
          // The drawNode function
          const auto &node = model.nodes[nodeIdx];
          glm::mat4 modelMatrix = getLocalToWorldMatrix(node, parentMatrix);

          // If the node is a mesh (and not a camera or light)
          if (node.mesh >= 0) {
            const auto &modelViewMatrix = viewMatrix * modelMatrix;
            const auto &modelViewProjectionMatrix = projMatrix * modelViewMatrix;
            const auto &normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));

            glUniformMatrix4fv(m_modelViewMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewMatrix));
            glUniformMatrix4fv(m_modelViewProjMatrixLocation, 1, GL_FALSE, glm::value_ptr(modelViewProjectionMatrix));
            glUniformMatrix4fv(m_normalMatrixLocation, 1, GL_FALSE, glm::value_ptr(normalMatrix));

            const auto &mesh = model.meshes[node.mesh];
            const auto &vaoRange = meshIndexToVaoRange[node.mesh];
            for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
              const auto &primitive = mesh.primitives[primIdx];
              bindMaterial(primitive.material);
              auto const &vao = vertexArrayObjects[vaoRange.begin + primIdx];
              glBindVertexArray(vao);
              if (primitive.indices >= 0) {
                const auto &accessor = model.accessors[primitive.indices];
                const auto &bufferView = model.bufferViews[accessor.bufferView];
                const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
                glDrawElements(primitive.mode, GLsizei(accessor.count), accessor.componentType, (const GLvoid *)byteOffset);
              } else {
                const auto accessorIdx = (*begin(primitive.attributes)).second;
                const auto &accessor = model.accessors[accessorIdx];
                glDrawArrays(primitive.mode, 0, GLsizei(accessor.count));
              }
            }
          }

          // Draw children
          for (const auto childNodeIdx : node.children) {
            drawNode(childNodeIdx, modelMatrix);
          }
        };

    // Draw the scene referenced by gltf file
    if (model.defaultScene >= 0) {
      // Draw all nodes
      for (const auto nodeIdx : model.scenes[model.defaultScene].nodes) {
        drawNode(nodeIdx, glm::mat4(1));
      }
    }
    glBindVertexArray(0);
  };

  // Render to image
  if (!m_OutputPath.empty()) {
    std::clog << "Saving..." << std::endl;
    const auto numComponents = 3; // RGB
    std::vector<unsigned char> pixels(m_nWindowWidth * m_nWindowHeight * numComponents); // Store the image
    renderToImage(m_nWindowWidth, m_nWindowHeight, numComponents, pixels.data(), [&]() {
      drawScene(cameraController->getCamera());
    });
    flipImageYAxis(m_nWindowWidth, m_nWindowHeight, 3, pixels.data()); // Flip the image
    const auto strPath = m_OutputPath.string();
    stbi_write_png(strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, pixels.data(), 0); // Write the image to a png file
    std::clog << "Done." << std::endl;
    return 0; // Prevent the render loop to run
  }

  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount) {
    const auto seconds = glfwGetTime();

    const auto camera = cameraController->getCamera();

    // 1. Geometry Pass
    // Draw the scene in the GBuffers
    m_geometryProgram.use();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_GBufferFBO);
    drawScene(camera);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    if (m_useSSAO) {
      // 2. SSAO Pass
      // Use G-buffer to render SSAO texture
      m_ssaoProgram.use();
      glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_GBufferTextures[GPosition]);
        glUniform1i(m_uGPositionLocation, GPosition);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_GBufferTextures[GNormal]);
        glUniform1i(m_uGNormalLocation, GNormal);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
        glUniform1i(m_uNoiseTexLocation, 2);

        // Send kernel + projection
        glUniform3fv(m_uSamplesLocation, 64, glm::value_ptr(m_ssaoKernel[0]));
        glUniformMatrix4fv(m_uProjectionLocation, 1, GL_FALSE, glm::value_ptr(projMatrix));

        glUniform1i(m_uKernelSizeLocation, m_ssaoKernelSize);
        glUniform1f(m_uRadiusLocation, m_ssaoRadius);
        glUniform1f(m_uBiasLocation, m_ssaoBias);
        glUniform1f(m_uSSAOIntensityLocation, m_ssaoIntensity);

        renderTriangle();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      // 3. Blur SSAO texture to remove noise
      // ------------------------------------
      m_ssaoBlurProgram.use();
      glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
          glClear(GL_COLOR_BUFFER_BIT);
          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, m_ssaoColorBuffer);
          glUniform1i(m_uSSAOInputLocation, 0);
          renderTriangle();
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(0.f, 0.f, 0.f, 1.f);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    if (m_CurrentlyDisplayed == GBufferTextureCount) { // Beauty
      // 4. Shading pass
      // ------------------------------------
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
        m_shadingProgram.use();

        // Set lights uniforms (uLightDirection uLightIntensity and uOcclusionStrength)
        const auto viewMatrix = camera.getViewMatrix();
        if (m_uLightDirectionLocation >= 0) {
          const auto lightDirectionInViewSpace =
              glm::normalize(glm::vec3(viewMatrix * glm::vec4(lightDirection, 0.)));
          glUniform3f(m_uLightDirectionLocation, lightDirectionInViewSpace[0],
              lightDirectionInViewSpace[1], lightDirectionInViewSpace[2]);
        }

        if (m_uLightIntensityLocation >= 0) {
          glUniform3f(m_uLightIntensityLocation, lightIntensity[0], lightIntensity[1],
              lightIntensity[2]);
        }

        glUniform1f(m_uOcclusionStrengthLocation, occlusionStrength);

        // Binding des textures du GBuffer sur différentes texture units (de 0 à 4 inclut)
        // Set des uniforms correspondant aux textures du GBuffer (chacune avec
        // l'indice de la texture unit sur laquelle la texture correspondante est
        // bindée)
        for (int32_t i = GPosition; i < GDepth; ++i) {
          glActiveTexture(GL_TEXTURE0 + i);
          glBindTexture(GL_TEXTURE_2D, m_GBufferTextures[i]);
          glUniform1i(m_uGBufferSamplerLocations[i], i);
        }

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, m_ssaoColorBufferBlur);
        glUniform1i(m_uSSAOLocation, 5);

        glUniform1f(m_uBloomThresholdLocation, m_bloomThreshold);

        renderTriangle();

      glBindFramebuffer(GL_FRAMEBUFFER, 0);

      // HDR buffer blit
      // glBindFramebuffer(GL_READ_FRAMEBUFFER, m_hdrFBO);
      // glReadBuffer(GL_COLOR_ATTACHMENT1);
      // glBlitFramebuffer(0, 0, m_nWindowWidth, m_nWindowHeight, 0, 0,
      //     m_nWindowWidth, m_nWindowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
      // glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

      // 5. Blur bright fragments with two-pass Gaussian Blur 
      // --------------------------------------------------
      if (m_useBloom) {
        glCopyImageSubData(
          m_colorBuffers[1], GL_TEXTURE_2D, 0, 0, 0, 0,
          m_pingpongBuffer[0], GL_TEXTURE_2D, 0, 0, 0, 0,
          m_nWindowWidth, m_nWindowHeight, 1
        );

        bool horizontal = true, first_iteration = true;
        m_blurProgram.use();
        glUniform1i(m_uBlurMaxLodLocation, m_maxLod);

        for (unsigned int i = 0; i < 2 * m_bloomQuality; i++)
        {
          glUniform1i(m_uBlurHorizontalLocation, horizontal);

          glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[horizontal]);

          glActiveTexture(GL_TEXTURE0);
          glBindTexture(GL_TEXTURE_2D, m_pingpongBuffer[!horizontal]);  // Bind texture of other framebuffer
          glUniform1i(m_uBlurImageLocation, 0);

          renderTriangle();
          glGenerateMipmap(GL_TEXTURE_2D);

          horizontal = !horizontal;
        }
        
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
      }

      // 6. Final pass, combine Scene color + blur (Additive blending)
      // ----------------------------------------------------------------------
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      m_bloomProgram.use();

      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_colorBuffers[0]);
      glUniform1i(m_uSceneLocation, 0);

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, m_pingpongBuffer[0]);
      glUniform1i(m_uBloomBlurLocation, 1);

      glUniform1i(m_uUseBloomLocation, m_useBloom);
      glUniform1f(m_uBloomIntensityLocation, m_bloomIntensity);
      glUniform3f(m_uBloomTintLocation, m_bloomTint[0], m_bloomTint[1], m_bloomTint[2]);
      glUniform1f(m_uExposureLocation, m_exposure);
      glUniform1f(m_uShowBloomOnlyLocation, m_showBloomOnly);

      renderTriangle();

    } else if (m_CurrentlyDisplayed == GDepth) {

      m_displayDepthProgram.use();
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, m_GBufferTextures[GDepth]);
      glUniform1i(m_uGDisplayDepthLocation, 0);

      renderTriangle();

    } else {

      // GBuffer blit
      glBindFramebuffer(GL_READ_FRAMEBUFFER, m_GBufferFBO);
      glReadBuffer(GL_COLOR_ATTACHMENT0 + m_CurrentlyDisplayed);
      glBlitFramebuffer(0, 0, m_nWindowWidth, m_nWindowHeight, 0, 0,
          m_nWindowWidth, m_nWindowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
      glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    }

    // GUI code:
    imguiNewFrame();

    {
      ImGui::Begin("GUI");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
          1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
            camera.eye().z);
        ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
            camera.center().y, camera.center().z);
        ImGui::Text(
            "up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

        ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
            camera.front().z);
        ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
            camera.left().z);

        if (ImGui::Button("CLI camera args to clipboard")) {
          std::stringstream ss;
          ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
             << camera.eye().z << "," << camera.center().x << ","
             << camera.center().y << "," << camera.center().z << ","
             << camera.up().x << "," << camera.up().y << "," << camera.up().z;
          const auto str = ss.str();
          glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
        }

        // Radio buttons to switch camera type
        static int cameraControllerType = 0;
        if (ImGui::RadioButton("Trackball", &cameraControllerType, 0)) {
          cameraController = std::make_unique<TrackballCameraController>(m_GLFWHandle.window(), 0.01f);
          cameraController->setCamera(camera);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("FirstPerson", &cameraControllerType, 1)) {
          cameraController = std::make_unique<FirstPersonCameraController>(m_GLFWHandle.window(), 5.f * maxDistance);
          cameraController->setCamera(camera);
        }
      }

      if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float theta = 0.0f;
        static float phi = 0.0f;
        static bool lightFromCamera = true;
        ImGui::Checkbox("Light from camera", &lightFromCamera);
        if (lightFromCamera) {
          lightDirection = -camera.front();
        } else {
          if (ImGui::SliderFloat("Theta", &theta, 0.f, glm::pi<float>()) ||
              ImGui::SliderFloat("Phi", &phi, 0, 2.f * glm::pi<float>())) {
            lightDirection = glm::vec3(
              glm::sin(theta) * glm::cos(phi),
              glm::cos(theta),
              glm::sin(theta) * glm::sin(phi)
            );
          }
        }

        static glm::vec3 lightColor(1.f, 1.f, 1.f);
        static float lightIntensityFactor = 3.f;
        if (ImGui::ColorEdit3("Light Color", (float *)&lightColor) ||
            ImGui::SliderFloat("Ligth Intensity", &lightIntensityFactor, 0.f, 10.f)) {
          lightIntensity = lightColor * lightIntensityFactor;
        }
        ImGui::SliderFloat("Exposure", &m_exposure, 0.f, 2.f);
      }

      if (ImGui::CollapsingHeader("Toggle Textures")) {
        ImGui::Checkbox("Base Color", &useBaseColor);
        ImGui::Checkbox("Metallic / Roughness", &useMetallicRoughnessTexture);
        ImGui::Checkbox("Emissive Texture", &useEmissive);
        ImGui::Checkbox("Occlusion Map", &useOcclusionMap);
      }

      if (ImGui::CollapsingHeader("Deferred Shading - GBuffers")) {
        for (int32_t i = GPosition; i <= GBufferTextureCount; ++i) {
          if (ImGui::RadioButton(m_GBufferTexNames[i], m_CurrentlyDisplayed == i))
            m_CurrentlyDisplayed = GBufferTextureType(i);
        }
      }

      if (ImGui::CollapsingHeader("SSAO")) {
        ImGui::Checkbox("Enable SSAO", &m_useSSAO);
        if (m_useSSAO) {
          ImGui::SliderInt("Kernel Size", &m_ssaoKernelSize, 1, 64);
          ImGui::SliderFloat("Radius", &m_ssaoRadius, 0.f, 5.f);
          ImGui::SliderFloat("Bias", &m_ssaoBias, 0.f, 1.f);
          ImGui::SliderFloat("Intensity", &m_ssaoIntensity, 0.f, 10.f);
        }
      }

      if (ImGui::CollapsingHeader("Bloom")) {
        ImGui::Checkbox("Enable Bloom", &m_useBloom);
        if (m_useBloom) {
          ImGui::Checkbox("Show Bloom only", &m_showBloomOnly);
          ImGui::SliderInt("Quality", &m_bloomQuality, 0, 10);
          ImGui::SliderInt("Radius (MaxLOD)", &m_maxLod, 0, 7);
          ImGui::SliderFloat("Bloom Threshold", &m_bloomThreshold, 0.f, 3.f);
          ImGui::ColorEdit3("Bloom Tint", (float *)&m_bloomTint);
          ImGui::SliderFloat("Bloom Intensity", &m_bloomIntensity, 0.f, 10.f);
        }
      }

      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController->update(float(ellapsedTime));
    }

    m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  // Clean up allocated GL data
  glDeleteBuffers(GLsizei(bufferObjects.size()), bufferObjects.data());
  glDeleteVertexArrays(GLsizei(vertexArrayObjects.size()), vertexArrayObjects.data());
  glDeleteTextures(GLsizei(textureObjects.size()), textureObjects.data());
  glDeleteTextures(1, &whiteTexture);

  return 0;
}

bool ViewerApplication::loadGltfFile(tinygltf::Model & model) {
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());

  if (!warn.empty()) {
    printf("Warn: %s\n", warn.c_str());
  }

  if (!err.empty()) {
    printf("Err: %s\n", err.c_str());
  }

  if (!ret) {
    printf("Failed to parse glTF\n");
    return false;
  }

  return true;
}

std::vector<GLuint> ViewerApplication::createBufferObjects(const tinygltf::Model &model) {
  std::vector<GLuint> bufferObjects(model.buffers.size(), 0);

  glGenBuffers(GLsizei(model.buffers.size()), bufferObjects.data());
  for (size_t i = 0; i < model.buffers.size(); ++i) {
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
    glBufferStorage(GL_ARRAY_BUFFER, model.buffers[i].data.size(), // Assume a Buffer has a data member variable of type std::vector
      model.buffers[i].data.data(), 0);
  }
  glBindBuffer(GL_ARRAY_BUFFER, 0); // Cleanup the binding point after the loop

  return bufferObjects;
}

std::vector<GLuint> ViewerApplication::createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange) {
  std::vector<GLuint> vertexArrayObjects;

  // For each mesh of model we keep its range of VAOs
  meshIndexToVaoRange.resize(model.meshes.size());

  const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
  const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
  const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;

  std::map<std::string, GLuint> attributes;
  attributes.insert(std::make_pair("POSITION", VERTEX_ATTRIB_POSITION_IDX));
  attributes.insert(std::make_pair("NORMAL", VERTEX_ATTRIB_NORMAL_IDX));
  attributes.insert(std::make_pair("TEXCOORD_0", VERTEX_ATTRIB_TEXCOORD0_IDX));

  for (size_t i = 0; i < model.meshes.size(); ++i) {
    const auto &mesh = model.meshes[i];
    const auto vaoOffset = vertexArrayObjects.size();
    vertexArrayObjects.resize(vaoOffset + mesh.primitives.size());

    auto &vaoRange = meshIndexToVaoRange[i];
    vaoRange.begin = GLsizei(vaoOffset); // Range for this mesh will be at the end of vertexArrayObjects
    vaoRange.count = GLsizei(mesh.primitives.size()); // One VAO for each primitive
    glGenVertexArrays(vaoRange.count, &vertexArrayObjects[vaoRange.begin]);

    for (size_t pIdx = 0; pIdx < mesh.primitives.size(); ++pIdx) {
      const auto vao = vertexArrayObjects[vaoRange.begin + pIdx];
      const auto &primitive = mesh.primitives[pIdx];
      glBindVertexArray(vao);

      // Loop over POSITION, NORMAL, TEXCOORD_0
      for (auto attribute : attributes) {
        const auto iterator = primitive.attributes.find(attribute.first); // for example attribute.first = "POSITION"
        if (iterator != end(primitive.attributes)) {
          const auto accessorIdx = (*iterator).second;
          const auto &accessor = model.accessors[accessorIdx];
          const auto &bufferView = model.bufferViews[accessor.bufferView];
          const auto bufferIdx = bufferView.buffer;

          glEnableVertexAttribArray(attribute.second);

          assert(GL_ARRAY_BUFFER == bufferView.target);

          // Theorically we could also use bufferView.target, but it is safer
          // Here it is important to know that the next call
          // (glVertexAttribPointer) use what is currently bound
          glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[bufferIdx]);

          // tinygltf converts strings type like "VEC3, "VEC2" to the number of
          // components, stored in accessor.type
          const auto byteOffset = accessor.byteOffset + bufferView.byteOffset;
          glVertexAttribPointer(attribute.second, accessor.type,
              accessor.componentType, GL_FALSE, GLsizei(bufferView.byteStride),
              (const GLvoid *)byteOffset);
        }
      }

      if (primitive.indices >= 0) {
        const auto accessorIdx = primitive.indices;
        const auto &accessor = model.accessors[accessorIdx];
        const auto &bufferView = model.bufferViews[accessor.bufferView];
        const auto bufferIdx = bufferView.buffer;

        assert(GL_ELEMENT_ARRAY_BUFFER == bufferView.target);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[bufferIdx]);
      }
    }
  }
  glBindVertexArray(0); // Unbind VAO

  std::clog << "Number of VAOs: " << vertexArrayObjects.size() << std::endl;

  return vertexArrayObjects;
}

std::vector<GLuint> ViewerApplication::createTextureObjects(const tinygltf::Model &model) const {
  std::vector<GLuint> textureObjects(model.textures.size(), 0);

  tinygltf::Sampler defaultSampler;
  defaultSampler.minFilter = GL_LINEAR;
  defaultSampler.magFilter = GL_LINEAR;
  defaultSampler.wrapS = GL_REPEAT;
  defaultSampler.wrapT = GL_REPEAT;
  defaultSampler.wrapR = GL_REPEAT;

  glActiveTexture(GL_TEXTURE0);
  glGenTextures(GLsizei(model.textures.size()), textureObjects.data());
  for (size_t i = 0; i < model.textures.size(); ++i) {
    // Assume a texture object has been created and bound to GL_TEXTURE_2D
    const auto &texture = model.textures[i]; // get i-th texture
    assert(texture.source >= 0); // ensure a source image is present
    const auto &image = model.images[texture.source]; // get the image

    glBindTexture(GL_TEXTURE_2D, textureObjects[i]); // Bind to target GL_TEXTURE_2D

    // fill the texture object with the data from the image
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
            GL_RGBA, image.pixel_type, image.image.data());

    const auto &sampler = texture.sampler >= 0 ? model.samplers[texture.sampler] : defaultSampler;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, sampler.wrapR);

    if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
      sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
      sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
      sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR) {
      glGenerateMipmap(GL_TEXTURE_2D);
    }

  }

  glBindTexture(GL_TEXTURE_2D, 0);

  return textureObjects;
}

GLuint ViewerApplication::createDefaultTexture() const {
  GLuint whiteTexture;

  float white[] = {1, 1, 1, 1};
  glGenTextures(1, &whiteTexture);
  glBindTexture(GL_TEXTURE_2D, whiteTexture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
          GL_RGBA, GL_FLOAT, &white);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

  glBindTexture(GL_TEXTURE_2D, 0);

  return whiteTexture;
}

void ViewerApplication::initPrograms() {
  // Forward rendering program
  m_forwardProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_vertexShader,
    m_ShadersRootPath / m_AppName / m_fragmentShader
  });

  // Geometry pass program
  m_geometryProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_geometryPassVSShader,
    m_ShadersRootPath / m_AppName / m_geometryPassFSShader
  });

  // Shading pass program
  m_shadingProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_shadingPassVSShader,
    m_ShadersRootPath / m_AppName / m_shadingPassFSShader
  });

  // SSAO pass program
  m_ssaoProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_ssaoPassVSShader,
    m_ShadersRootPath / m_AppName / m_ssaoPassFSShader
  });

  // SSAO blur program
  m_ssaoBlurProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_ssaoPassVSShader,
    m_ShadersRootPath / m_AppName / m_ssaoBlurFSShader
  });

  // Display depth program
  m_displayDepthProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_ssaoPassVSShader,
    m_ShadersRootPath / m_AppName / m_displayDepthFSShader
  });

  // Blur program (for bloom)
  m_blurProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_blurVSShader,
    m_ShadersRootPath / m_AppName / m_blurFSShader
  });

  // Bloom final program (final pass)
  m_bloomProgram = compileProgram({
    m_ShadersRootPath / m_AppName / m_bloomVSShader,
    m_ShadersRootPath / m_AppName / m_bloomFSShader
  });
}

void ViewerApplication::initUniforms() {
  // Geometry pass uniforms
  m_modelViewProjMatrixLocation = glGetUniformLocation(m_geometryProgram.glId(), "uModelViewProjMatrix");
  m_modelViewMatrixLocation = glGetUniformLocation(m_geometryProgram.glId(), "uModelViewMatrix");
  m_normalMatrixLocation = glGetUniformLocation(m_geometryProgram.glId(), "uNormalMatrix");
  m_uBaseColorTextureLocation = glGetUniformLocation(m_geometryProgram.glId(), "uBaseColorTexture");
  m_uBaseColorFactorLocation = glGetUniformLocation(m_geometryProgram.glId(), "uBaseColorFactor");
  m_uMetallicFactorLocation = glGetUniformLocation(m_geometryProgram.glId(), "uMetallicFactor");
  m_uRoughnessFactorLocation = glGetUniformLocation(m_geometryProgram.glId(), "uRoughnessFactor");
  m_uMetallicRoughnessTextureLocation = glGetUniformLocation(m_geometryProgram.glId(), "uMetallicRoughnessTexture");
  m_uEmissiveTextureLocation = glGetUniformLocation(m_geometryProgram.glId(), "uEmissiveTexture");
  m_uEmissiveFactorLocation = glGetUniformLocation(m_geometryProgram.glId(), "uEmissiveFactor");
  m_uOcclusionTextureLocation = glGetUniformLocation(m_geometryProgram.glId(), "uOcclusionTexture");

  // Shading pass uniforms
  m_uLightDirectionLocation = glGetUniformLocation(m_shadingProgram.glId(), "uLightDirection");
  m_uLightIntensityLocation = glGetUniformLocation(m_shadingProgram.glId(), "uLightIntensity");
  m_uOcclusionStrengthLocation = glGetUniformLocation(m_shadingProgram.glId(), "uOcclusionStrength");
  m_uSSAOLocation = glGetUniformLocation(m_shadingProgram.glId(), "uSSAO");
  m_uBloomThresholdLocation = glGetUniformLocation(m_shadingProgram.glId(), "uBloomThreshold");
  m_uGBufferSamplerLocations[GPosition] = glGetUniformLocation(m_shadingProgram.glId(), "uGPosition");
  m_uGBufferSamplerLocations[GNormal] = glGetUniformLocation(m_shadingProgram.glId(), "uGNormal");
  m_uGBufferSamplerLocations[GDiffuse] = glGetUniformLocation(m_shadingProgram.glId(), "uGDiffuse");
  m_uGBufferSamplerLocations[GMetalRoughness] = glGetUniformLocation(m_shadingProgram.glId(), "uGMetalRoughness");
  m_uGBufferSamplerLocations[GEmissive] = glGetUniformLocation(m_shadingProgram.glId(), "uGEmissive");

  // SSAO Uniforms
  m_uGPositionLocation = glGetUniformLocation(m_ssaoProgram.glId(), "gPosition");
  m_uGNormalLocation = glGetUniformLocation(m_ssaoProgram.glId(), "gNormal");
  m_uNoiseTexLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uNoiseTex");
  m_uSamplesLocation = glGetUniformLocation(m_ssaoProgram.glId(), "samples");
  m_uProjectionLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uProjection");
  m_uKernelSizeLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uKernelSize");
  m_uRadiusLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uRadius");
  m_uBiasLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uBias");
  m_uSSAOIntensityLocation = glGetUniformLocation(m_ssaoProgram.glId(), "uIntensity");

  // SSAO Blur Uniforms
  m_uSSAOInputLocation = glGetUniformLocation(m_ssaoBlurProgram.glId(), "ssaoInput");

  // Display Depth Uniforms
  m_uGDisplayDepthLocation = glGetUniformLocation(m_ssaoBlurProgram.glId(), "uGDepth");

  // Bloom Blur Uniforms
  m_uBlurHorizontalLocation = glGetUniformLocation(m_blurProgram.glId(), "uHorizontal");
  m_uBlurImageLocation = glGetUniformLocation(m_blurProgram.glId(), "uImage");
  m_uBlurMaxLodLocation = glGetUniformLocation(m_blurProgram.glId(), "uMaxLod");

  // Final Bloom Uniforms
  m_uSceneLocation = glGetUniformLocation(m_bloomProgram.glId(), "uScene");
  m_uBloomBlurLocation = glGetUniformLocation(m_bloomProgram.glId(), "uBloomBlur");
  m_uUseBloomLocation = glGetUniformLocation(m_bloomProgram.glId(), "uUseBloom");
  m_uBloomIntensityLocation = glGetUniformLocation(m_bloomProgram.glId(), "uBloomIntensity");
  m_uBloomTintLocation = glGetUniformLocation(m_bloomProgram.glId(), "uBloomTint");
  m_uExposureLocation = glGetUniformLocation(m_bloomProgram.glId(), "uExposure");
  m_uShowBloomOnlyLocation = glGetUniformLocation(m_bloomProgram.glId(), "uShowBloomOnly");
}

void ViewerApplication::initTriangle() {
  glGenBuffers(1, &m_TriangleVBO);
  glBindBuffer(GL_ARRAY_BUFFER, m_TriangleVBO);

  GLfloat data[] = {
    // Positions  // Texture coords
    -1.f, -1.f,   0.f,  0.f, 
     3.f, -1.f,   2.f,  0.f,
    -1.f,  3.f,   0.f,  2.f
  };
  glBufferStorage(GL_ARRAY_BUFFER, sizeof(data), data, 0);

  glGenVertexArrays(1, &m_TriangleVAO);
  glBindVertexArray(m_TriangleVAO);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) 0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *) (2 * sizeof(float)));

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

// Drawing the triangle covering the all screen
void ViewerApplication::renderTriangle() const {
  glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBindVertexArray(m_TriangleVAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

// Init GBuffers
void ViewerApplication::initGBuffers() {
  glGenTextures(GBufferTextureCount, m_GBufferTextures);

  for (int32_t i = GPosition; i < GBufferTextureCount; ++i) {
    glBindTexture(GL_TEXTURE_2D, m_GBufferTextures[i]);
    glTexStorage2D(GL_TEXTURE_2D, 1, m_GBufferTextureFormat[i], m_nWindowWidth,
        m_nWindowHeight);
  }

  glGenFramebuffers(1, &m_GBufferFBO);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_GBufferFBO);
  for (int32_t i = GPosition; i < GDepth; ++i) {
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
        GL_TEXTURE_2D, m_GBufferTextures[i], 0);
  }
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
      GL_TEXTURE_2D, m_GBufferTextures[GDepth], 0);

  // We will write into 5 textures from the fragment shader
  GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
      GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4};
  glDrawBuffers(5, drawBuffers);

  GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);

  if (status != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "FBO error, status: " << status << std::endl;
    throw std::runtime_error("FBO error");
  }

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

// Init SSAO Texures and Framebuffers
void ViewerApplication::initSSAO() {
  // Generate sample kernel
  // ----------------------
  std::uniform_real_distribution<GLfloat> randomFloats(
      0.0, 1.0); // generates random floats between 0.0 and 1.0
  std::default_random_engine generator;
  for (unsigned int i = 0; i < 64; ++i) {
    glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0,
        randomFloats(generator) * 2.0 - 1.0, randomFloats(generator));
    sample = glm::normalize(sample);
    sample *= randomFloats(generator);
    float scale = float(i) / 64.0;

    // scale samples s.t. they're more aligned to center of kernel
    scale = lerp(0.1f, 1.0f, scale * scale);
    sample *= scale;
    m_ssaoKernel.push_back(sample);
  }

  // generate noise texture
  // ----------------------
  std::vector<glm::vec3> ssaoNoise;
  for (unsigned int i = 0; i < 16; i++) {
    glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0,
        randomFloats(generator) * 2.0 - 1.0,
        0.0f); // rotate around z-axis (in tangent space)
    ssaoNoise.push_back(noise);
  }
  m_noiseTexture;
  glGenTextures(1, &m_noiseTexture);
  glBindTexture(GL_TEXTURE_2D, m_noiseTexture);
  glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGB32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // Also create framebuffer to hold SSAO processing stage
  // -----------------------------------------------------
  glGenFramebuffers(1, &m_ssaoFBO);
  glGenFramebuffers(1, &m_ssaoBlurFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoFBO);

  // SSAO color buffer
  glGenTextures(1, &m_ssaoColorBuffer);
  glBindTexture(GL_TEXTURE_2D, m_ssaoColorBuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_nWindowWidth, m_nWindowHeight, 0,
      GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ssaoColorBuffer, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "SSAO Framebuffer not complete!" << std::endl;

  // And blur stage
  glBindFramebuffer(GL_FRAMEBUFFER, m_ssaoBlurFBO);
  glGenTextures(1, &m_ssaoColorBufferBlur);
  glBindTexture(GL_TEXTURE_2D, m_ssaoColorBufferBlur);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, m_nWindowWidth, m_nWindowHeight, 0,
      GL_RGB, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
      m_ssaoColorBufferBlur, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Init Bloom Texures and Framebuffers
void ViewerApplication::initBloom() {
  // Set up floating point framebuffer to render scene to
  glGenFramebuffers(1, &m_hdrFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
  glGenTextures(2, m_colorBuffers);
  for (unsigned int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, m_colorBuffers[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_nWindowWidth, m_nWindowHeight, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 7);
    glGenerateMipmap(GL_TEXTURE_2D);
    // Attach texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
        GL_TEXTURE_2D, m_colorBuffers[i], 0);
  }

  unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);

  // Finally check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Framebuffer not complete!" << std::endl;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // PingPong
  glGenFramebuffers(2, m_pingpongFBO);
  glGenTextures(2, m_pingpongBuffer);
  for (unsigned int i = 0; i < 2; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_pingpongFBO[i]);
    glBindTexture(GL_TEXTURE_2D, m_pingpongBuffer[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, m_nWindowWidth, m_nWindowHeight, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 7);
    glGenerateMipmap(GL_TEXTURE_2D);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_pingpongBuffer[i], 0);
    // Also check if framebuffers are complete (no need for depth buffer)
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "Framebuffer not complete!" << std::endl;
  }
}

ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
    uint32_t height, const fs::path &gltfFile,
    const std::vector<float> &lookatArgs, const std::string &vertexShader,
    const std::string &fragmentShader, const fs::path &output) :
    m_nWindowWidth(width),
    m_nWindowHeight(height),
    m_AppPath{appPath},
    m_AppName{m_AppPath.stem().string()},
    m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
    m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
    m_gltfFilePath{gltfFile},
    m_OutputPath{output}
{
  if (!lookatArgs.empty()) {
    m_hasUserCamera = true;
    m_userCamera =
        Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
            glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
            glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
  }

  if (!vertexShader.empty()) {
    m_vertexShader = vertexShader;
  }

  if (!fragmentShader.empty()) {
    m_fragmentShader = fragmentShader;
  }

  ImGui::GetIO().IniFilename =
      m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
                                  // positions in this file

  glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

  printGLVersion();

  initGBuffers();
  initSSAO();
  initBloom();
  initTriangle();
}

#pragma once

#include "utils/GLFWHandle.hpp"
#include "utils/cameras.hpp"
#include "utils/filesystem.hpp"
#include "utils/shaders.hpp"
#include <tiny_gltf.h>

class ViewerApplication
{
public:
  ViewerApplication(const fs::path &appPath, uint32_t width, uint32_t height,
      const fs::path &gltfFile, const std::vector<float> &lookatArgs,
      const std::string &vertexShader, const std::string &fragmentShader,
      const fs::path &output);

  int run();


private:
  // A range of indices in a vector containing Vertex Array Objects
  struct VaoRange
  {
    GLsizei begin; // Index of first element in vertexArrayObjects
    GLsizei count; // Number of elements in range
  };

  GLsizei m_nWindowWidth = 1280;
  GLsizei m_nWindowHeight = 720;

  bool loadGltfFile(tinygltf::Model &model);
  std::vector<GLuint> createBufferObjects(const tinygltf::Model &model);
  std::vector<GLuint> createVertexArrayObjects(const tinygltf::Model &model, const std::vector<GLuint> &bufferObjects, std::vector<VaoRange> &meshIndexToVaoRange);
  std::vector<GLuint> createTextureObjects(const tinygltf::Model &model) const;
  GLuint createDefaultTexture() const;

  const fs::path m_AppPath;
  const std::string m_AppName;
  const fs::path m_ShadersRootPath;

  fs::path m_gltfFilePath;

  std::string m_vertexShader = "geometryPass.vs.glsl";
  std::string m_fragmentShader = "geometryPass.fs.glsl";
  std::string m_geometryPassVSShader = "geometryPass.vs.glsl";
  std::string m_geometryPassFSShader = "geometryPass.fs.glsl";
  std::string m_shadingPassVSShader = "shadingPass.vs.glsl";
  std::string m_shadingPassFSShader = "shadingPass.fs.glsl";
  std::string m_ssaoPassVSShader = "ssao.vs.glsl";
  std::string m_ssaoPassFSShader = "ssao.fs.glsl";
  std::string m_ssaoBlurFSShader = "ssaoBlur.fs.glsl";
  std::string m_displayDepthFSShader = "displayDepth.fs.glsl";
  std::string m_blurVSShader = "blur.vs.glsl";
  std::string m_blurFSShader = "blur.fs.glsl";
  std::string m_bloomVSShader = "bloom.vs.glsl";
  std::string m_bloomFSShader = "bloom.fs.glsl";

  bool m_hasUserCamera = false;
  Camera m_userCamera;

  fs::path m_OutputPath;

  enum GBufferTextureType {
    GPosition = 0,
    GNormal,
    GDiffuse,
    GMetalRoughness,
    GEmissive,
    GDepth, // On doit créer une texture de depth mais on écrit pas
            // directement dedans dans le FS. OpenGL le fait pour nous
            // (et l'utilise).
    GBufferTextureCount
  };

  const char * m_GBufferTexNames[GBufferTextureCount + 1] = { "Position", "Normal", "Diffuse", "Occlusion / Metal / Roughness", "Emissive", "Depth", "Beauty" }; // Tricks, since we cant blit depth, we use its value to draw the result of the shading pass
  const GLenum m_GBufferTextureFormat[GBufferTextureCount] = { GL_RGB32F, GL_RGB32F, GL_RGB32F, GL_RGB32F, GL_RGB32F, GL_DEPTH_COMPONENT32F };

  GLuint m_GBufferTextures[GBufferTextureCount];
  GLuint m_GBufferFBO;
  GBufferTextureType m_CurrentlyDisplayed = GBufferTextureCount; // Beauty

  // Triangle covering the whole screen, for the shading pass:
  GLuint m_TriangleVBO = 0;
  GLuint m_TriangleVAO = 0;

  // Order is important here, see comment below
  const std::string m_ImGuiIniFilename;
  // Last to be initialized, first to be destroyed:
  GLFWHandle m_GLFWHandle{int(m_nWindowWidth), int(m_nWindowHeight),
      "glTF Viewer",
      m_OutputPath.empty()}; // show the window only if m_OutputPath is empty
  /*
    ! THE ORDER OF DECLARATION OF MEMBER VARIABLES IS IMPORTANT !
    - m_ImGuiIniFilename.c_str() will be used by ImGUI in ImGui::Shutdown, which
    will be called in destructor of m_GLFWHandle. So we must declare
    m_ImGuiIniFilename before m_GLFWHandle so that m_ImGuiIniFilename
    destructor is called after.
    - m_GLFWHandle must be declared before the creation of any object managing
    OpenGL resources (e.g. GLProgram, GLShader) because it is responsible for
    the creation of a GLFW windows and thus a GL context which must exists
    before most of OpenGL function calls.
  */

  // GL Programs
  GLProgram m_shadingProgram;
  GLProgram m_forwardProgram;
  GLProgram m_geometryProgram;
  GLProgram m_ssaoProgram;
  GLProgram m_ssaoBlurProgram;
  GLProgram m_displayDepthProgram;
  GLProgram m_blurProgram;
  GLProgram m_bloomProgram;

  // Geometry Pass Uniforms Locations
  GLint m_modelViewProjMatrixLocation;
  GLint m_modelViewMatrixLocation;
  GLint m_normalMatrixLocation;
  GLint m_uBaseColorTextureLocation;
  GLint m_uBaseColorFactorLocation;
  GLint m_uMetallicFactorLocation;
  GLint m_uRoughnessFactorLocation;
  GLint m_uMetallicRoughnessTextureLocation;
  GLint m_uEmissiveTextureLocation;
  GLint m_uEmissiveFactorLocation;
  GLint m_uOcclusionTextureLocation;

  // Shading Pass Uniforms Locations
  GLint m_uLightDirectionLocation;
  GLint m_uLightIntensityLocation;
  GLint m_uOcclusionStrengthLocation;
  GLint m_uSSAOLocation;
  GLint m_uGBufferSamplerLocations[GDepth];

  // SSAO Pass Uniforms Locations
  GLint m_uGPositionLocation;
  GLint m_uGNormalLocation;
  GLint m_uNoiseTexLocation;
  GLint m_uProjectionLocation;
  GLint m_uSamplesLocation;
  GLint m_uKernelSizeLocation;
  GLint m_uRadiusLocation;
  GLint m_uBiasLocation;
  GLint m_uSSAOIntensityLocation;
  GLint m_uBloomThresholdLocation;

  // SSAO Blur Uniforms Locations
  GLint m_uSSAOInputLocation;

  // Display Depth Uniforms Locations
  GLint m_uGDisplayDepthLocation;

  // Bloom Blur Uniforms Locations
  GLint m_uBlurHorizontalLocation;
  GLint m_uBlurImageLocation;
  GLint m_uBlurWeightLocation;
  GLint m_uBlurMaxLodLocation;

  // Bloom Final Uniforms Locations
  GLint m_uSceneLocation;
  GLint m_uBloomBlurLocation;
  GLint m_uUseBloomLocation;
  GLint m_uBloomIntensityLocation;
  GLint m_uBloomTintLocation;
  GLint m_uExposureLocation;

  void initPrograms();
  void initUniforms();
  void initTriangle();
  void renderTriangle() const;
  void initGBuffers();
  void initSSAO();
  void initBloom();

  // Init SSAO
  unsigned int m_ssaoFBO, m_ssaoBlurFBO;
  unsigned int m_noiseTexture;
  std::vector<glm::vec3> m_ssaoKernel;
  unsigned int m_ssaoColorBuffer, m_ssaoColorBufferBlur;

  // Init Bloom
  unsigned int m_hdrFBO;
  unsigned int m_colorBuffers[2];
  unsigned int m_pingpongFBO[2];
  unsigned int m_pingpongBuffer[2];

  // SSAO parameters
  bool m_useSSAO = true;
  int m_ssaoKernelSize = 32;
  float m_ssaoRadius = 0.5f;
  float m_ssaoBias = 0.001f;
  float m_ssaoIntensity = 3.f;

  // Bloom parameters
  bool m_useBloom = true;
  int m_bloomQuality = 2;
  int m_maxLod = 4;
  float m_bloomThreshold = 1.f;
  float m_bloomIntensity = 2.5f;
  glm::vec3 m_bloomTint = glm::vec3(1.f, 1.f, 1.f);
  float m_exposure = 1.f;
};

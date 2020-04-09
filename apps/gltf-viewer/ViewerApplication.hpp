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

  GLuint m_GBufferTextures[GBufferTextureCount];
  GLuint m_GBufferFBO;
  GBufferTextureType m_CurrentlyDisplayed = GDiffuse;

  // Triangle covering the whole screen, for the shading pass:
  GLuint m_TriangleVBO = 0;
  GLuint m_TriangleVAO = 0;

  // Shading pass uniforms
  GLint m_uGBufferSamplerLocations[GDepth];

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

  GLint m_uLightDirectionLocation;
  GLint m_uLightIntensityLocation;
  GLint m_uOcclusionStrengthLocation;

  void initPrograms();
  void initUniforms();
  void initTriangle();

};

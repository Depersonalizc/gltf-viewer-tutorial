# glTF Viewer Tutorial
> Florian Torres

[glTF Viewer tutorial link](https://celeborn2bealive.github.io/openglnoel/docs/gltf-viewer-01-intro-01-intro)

__How to run the program :__

```shell
git clone https://github.com/torresf/gltf-viewer-tutorial.git
cd gltf-viewer-tutorial
mkdir build
cd build
cmake ..
make -j
./bin/gltf-viewer viewer ../../PATH_TO_GLTF_MODEL/MODEL.gltf
```

__TODO / IDEAS__ :

- [x] Loading and drawing
- [x] Controlling the Camera
- [x] Directional Lighting
- [x] Physically Based Materials
- [x] Refacto white default texture
- [x] Emissive Material
- [x] Toggle Textures in GUI
- [x] Occlusion Mapping
- [x] Deferred Rendering
- [x] Post Processing - SSAO
- [x] Post Processing - Bloom
- [x] Fix loading multiple VAOs
- [ ] Normal mapping
- [ ] Shadow Mapping
- [ ] Point Light
- [ ] Multiple lights
- [ ] HDRi
- [ ] Motion blur
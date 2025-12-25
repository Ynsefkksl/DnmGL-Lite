#ifndef DnmGLLite
#define DnmGLLite

#define StorageBuffer(set_, binding_) layout(set = set_, binding = binding_) buffer
#define UniformBuffer(set_, binding_) layout(set = set_, binding = binding_) uniform
#define Texture2D(set_, binding_) layout(set = set_, binding = binding_) uniform sampler2D
#define PushConstant layout(push_constant) uniform

#endif
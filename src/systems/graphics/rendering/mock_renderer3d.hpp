// Mock renderer for headless runs (determinism_runner). Implements the full
// Renderer3D interface as no-ops so the graphics message queue can be drained
// without creating a window or an OpenGL context.

#ifndef _HPP_MOCK_RENDERER3D
#define _HPP_MOCK_RENDERER3D

#include "interface_renderer3d.hpp"

namespace blunted {

  class MockRenderer3D : public Renderer3D {

    public:
      MockRenderer3D() {
        view.x = 0;
        view.y = 0;
        view.width = 10;
        view.height = 10;
      }
      virtual ~MockRenderer3D() {}

      virtual void SwapBuffers() {}

      virtual void SetMatrix(const std::string &shaderUniformName, const Matrix4 &matrix) {}

      virtual void RenderOverlay2D(const std::vector<Overlay2DQueueEntry> &overlay2DQueue) {}
      virtual void RenderOverlay2D() {}
      virtual void RenderLights(std::deque<LightQueueEntry> &lightQueue, const Matrix4 &projectionMatrix, const Matrix4 &viewMatrix) {}

      // init & exit
      virtual bool CreateContext(int width, int height, int bpp, bool fullscreen) { return true; }
      virtual void SetContextSize(int width, int height, bool fullscreen) {}
      virtual void Exit() {}

      virtual int CreateView(float x_percent, float y_percent, float width_percent, float height_percent) { return 1; }
      virtual View &GetView(int viewID) { return view; }
      virtual void DeleteView(int viewID) {}

      // general
      virtual void SetCullingMode(e_CullingMode cullingMode) {}
      virtual void SetBlendingMode(e_BlendingMode blendingMode) {}
      virtual void SetDepthFunction(e_DepthFunction depthFunction) {}
      virtual void SetDepthTesting(bool OnOff) {}
      virtual void SetDepthMask(bool OnOff) {}
      virtual void SetBlendingFunction(e_BlendingFunction blendingFunction1, e_BlendingFunction blendingFunction2) {}

      virtual void ClearBuffer(const Vector3 &color, bool clearDepth, bool clearColor) {}

      virtual Matrix4 CreatePerspectiveMatrix(float aspectRatio, float nearCap = -1, float farCap = -1) { return Matrix4(); }
      virtual Matrix4 CreateOrthoMatrix(float left, float right, float bottom, float top, float nearCap = -1, float farCap = -1) { return Matrix4(); }

      // vertex buffers
      virtual VertexBufferID CreateVertexBuffer(float *vertices, unsigned int verticesDataSize, std::vector<unsigned int> indices, e_VertexBufferUsage usage) { VertexBufferID id; id.bufferID = 0; id.vertexArrayID = 0; return id; }
      virtual void UpdateVertexBuffer(VertexBufferID vertexBufferID, float *vertices, unsigned int verticesDataSize) {}
      virtual void DeleteVertexBuffer(VertexBufferID vertexBufferID) {}
      virtual void RenderVertexBuffer(const std::deque<VertexBufferQueueEntry> &vertexBufferQueue, e_RenderMode renderMode = e_RenderMode_Full) {}

      // textures
      virtual int CreateTexture(e_InternalPixelFormat internalPixelFormat, e_PixelFormat pixelFormat, int width, int height, bool alpha = false, bool repeat = true, bool mipmaps = true, bool filter = true, bool multisample = false, bool compareDepth = false) { return 1; }
      virtual void ResizeTexture(int textureID, SDL_Surface *source, e_InternalPixelFormat internalPixelFormat, e_PixelFormat pixelFormat, bool alpha = false, bool mipmaps = true) {}
      virtual void UpdateTexture(int textureID, SDL_Surface *source, bool alpha = false, bool mipmaps = true) {}
      virtual void DeleteTexture(int textureID) {}
      virtual void CopyFrameBufferToTexture(int textureID, int width, int height) {}
      virtual void BindTexture(int textureID) {}
      virtual void SetTextureUnit(int textureUnit) {}

      // frame buffer
      virtual int CreateFrameBuffer() { return 1; }
      virtual void DeleteFrameBuffer(int fbID) {}
      virtual void BindFrameBuffer(int fbID) {}
      virtual void SetFrameBufferRenderBuffer(e_TargetAttachment targetAttachment, int rbID) {}
      virtual void SetFrameBufferTexture2D(e_TargetAttachment targetAttachment, int texID) {}
      virtual bool CheckFrameBufferStatus() { return true; }
      virtual void SetFramebufferGammaCorrection(bool onOff) {}

      // render buffers
      virtual int CreateRenderBuffer() { return 1; }
      virtual void DeleteRenderBuffer(int rbID) {}
      virtual void BindRenderBuffer(int rbID) {}
      virtual void SetRenderBufferStorage(e_InternalPixelFormat internalPixelFormat, int width, int height) {}

      // render targets
      virtual void SetRenderTargets(std::vector<e_TargetAttachment> targetAttachments) {}

      // utility
      virtual void SetFOV(float angle) {}
      virtual void SetViewport(int x, int y, int width, int height) {}
      virtual void GetContextSize(int &width, int &height, int &bpp) { width = 1280; height = 720; bpp = 32; }
      virtual void SetPolygonOffset(float scale, float bias) {}

      // shaders
      virtual void LoadShader(const std::string &name, const std::string &filename) {}
      virtual void UseShader(const std::string &name) {}
      virtual void SetUniformInt(const std::string &shaderName, const std::string &varName, int value) {}
      virtual void SetUniformFloat(const std::string &shaderName, const std::string &varName, float value) {}
      virtual void SetUniformFloat2(const std::string &shaderName, const std::string &varName, float value1, float value2) {}
      virtual void SetUniformFloat3(const std::string &shaderName, const std::string &varName, float value1, float value2, float value3) {}
      virtual void SetUniformFloat3Array(const std::string &shaderName, const std::string &varName, int count, float *values) {}
      virtual void SetUniformMatrix4(const std::string &shaderName, const std::string &varName, const Matrix4 &mat) {}

      void operator()() {
        bool quit = false;
        while (!quit) {
          bool isMessage;
          boost::intrusive_ptr<Command> message = messageQueue.WaitForMessage(isMessage, 1);
          if (isMessage) {
            if (!message->Handle(this)) quit = true;
            message.reset();
          }
        }
      }

    protected:
      View view;

  };

}

#endif

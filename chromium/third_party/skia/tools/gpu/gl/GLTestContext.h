
/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef GLTestContext_DEFINED
#define GLTestContext_DEFINED

#include "gl/GrGLInterface.h"
#include "../private/SkGpuFenceSync.h"


namespace sk_gpu_test {
/**
 * Create an offscreen Oppengl context. Provides a GrGLInterface struct of function pointers for
 * the context. This class is intended for Skia's internal testing needs and not for general use.
 */
class GLTestContext : public SkNoncopyable {
public:
    virtual ~GLTestContext();

    bool isValid() const { return NULL != gl(); }

    const GrGLInterface *gl() const { return fGL.get(); }

    bool fenceSyncSupport() const { return fFenceSync != nullptr; }

    bool getMaxGpuFrameLag(int *maxFrameLag) const {
        if (!fFenceSync) {
            return false;
        }
        *maxFrameLag = kMaxFrameLag;
        return true;
    }

    void makeCurrent() const;

    /** Used for testing EGLImage integration. Take a GL_TEXTURE_2D and wraps it in an EGL Image */
    virtual GrEGLImage texture2DToEGLImage(GrGLuint /*texID*/) const { return 0; }

    virtual void destroyEGLImage(GrEGLImage) const { }

    /** Used for testing GL_TEXTURE_RECTANGLE integration. */
    GrGLint createTextureRectangle(int width, int height, GrGLenum internalFormat,
                                   GrGLenum externalFormat, GrGLenum externalType,
                                   GrGLvoid *data);

    /**
     * Used for testing EGLImage integration. Takes a EGLImage and wraps it in a
     * GL_TEXTURE_EXTERNAL_OES.
     */
    virtual GrGLuint eglImageToExternalTexture(GrEGLImage) const { return 0; }

    void swapBuffers();

    /**
     * The only purpose of this function it to provide a means of scheduling
     * work on the GPU (since all of the subclasses create primary buffers for
     * testing that are small and not meant to be rendered to the screen).
     *
     * If the platform supports fence sync (OpenGL 3.2+ or EGL_KHR_fence_sync),
     * this will not swap any buffers, but rather emulate triple buffer
     * synchronization using fences.
     *
     * Otherwise it will call the platform SwapBuffers method. This may or may
     * not perform some sort of synchronization, depending on whether the
     * drawing surface provided by the platform is double buffered.
     */
    void waitOnSyncOrSwap();

    /**
     * This notifies the context that we are deliberately testing abandoning
     * the context. It is useful for debugging contexts that would otherwise
     * test that GPU resources are properly deleted. It also allows a debugging
     * context to test that further GL calls are not made by Skia GPU code.
     */
    void testAbandon();

    /**
     * Creates a new GL context of the same type and makes the returned context current
     * (if not null).
     */
    virtual GLTestContext *createNew() const { return nullptr; }

    class GLFenceSync;  // SkGpuFenceSync implementation that uses the OpenGL functionality.

    /*
     * returns the fencesync object owned by this GLTestContext
     */
    SkGpuFenceSync *fenceSync() { return fFenceSync.get(); }

protected:
    GLTestContext();

    /*
     * Methods that sublcasses must call from their constructors and destructors.
     */
    void init(const GrGLInterface *, SkGpuFenceSync * = NULL);

    void teardown();

    /*
     * Operations that have a platform-dependent implementation.
     */
    virtual void onPlatformMakeCurrent() const = 0;

    virtual void onPlatformSwapBuffers() const = 0;

    virtual GrGLFuncPtr onPlatformGetProcAddress(const char *) const = 0;

private:
    enum {
        kMaxFrameLag = 3
    };

    SkAutoTDelete <SkGpuFenceSync> fFenceSync;
    SkPlatformGpuFence fFrameFences[kMaxFrameLag - 1];
    int fCurrentFenceIdx;

    /** Subclass provides the gl interface object if construction was
     *  successful. */
    SkAutoTUnref<const GrGLInterface> fGL;

    friend class GLFenceSync;  // For onPlatformGetProcAddress.
};


/** Creates platform-dependent GL context object.  The shareContext parameter is in an optional
 * context with which to share display lists. This should be a pointer to an GLTestContext created
 * with SkCreatePlatformGLTestContext.  NULL indicates that no sharing is to take place. Returns a valid
 * gl context object or NULL if such can not be created.
 * Note: If Skia embedder needs a custom GL context that sets up the GL interface, this function
 * should be implemented by the embedder. Otherwise, the default implementation for the platform
 * should be compiled in the library.
 */
GLTestContext* CreatePlatformGLTestContext(GrGLStandard forcedGpuAPI, GLTestContext *shareContext = nullptr);

}  // namespace sk_gpu_test
#endif

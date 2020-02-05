//
// Book:      OpenGL(R) ES 2.0 Programming Guide
// Authors:   Aaftab Munshi, Dan Ginsburg, Dave Shreiner
// ISBN-10:   0321502795
// ISBN-13:   9780321502797
// Publisher: Addison-Wesley Professional
// URLs:      http://safari.informit.com/9780321563835
//            http://www.opengles-book.com
//

// ESUtil.c
//
//    A utility library for OpenGL ES.  This library provides a
//    basic common framework for the example applications in the
//    OpenGL ES 2.0 Programming Guide.
//

///
//  Includes
//
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include "esUtil.h"

#include "common.h"
#include "drm-common.h"
#include "drm-legacy.h"

// DRM local variables
static struct Platform
{
    struct egl egl;   // TODO: This egl is basically an ESContext.  Need to think
    const struct gbm *gbm;  // actual object is a static variable in common.c
    const struct drm *drm;  // actual object is a static variable in drm-legacy.c
} platform;

///
//  WinCreate()
//
//      This function initializes the native DRM display and window for EGL
//
EGLBoolean WinCreate(ESContext *esContext, const char *title)
{
    const char *device = "/dev/dri/card1";
    platform.drm = init_drm_legacy(device, "", 0);
    if (!platform.drm) {
        printf("Failed to initialize DRM %s\n", device);
        return EGL_FALSE;
    }

    uint64_t modifier = DRM_FORMAT_MOD_LINEAR;
    uint32_t format = DRM_FORMAT_XRGB8888;

    printf("Try to init gbm with fd=%i, h=%i v=%i\n", platform.drm->fd, platform.drm->mode->hdisplay, platform.drm->mode->vdisplay);
    platform.gbm = init_gbm(platform.drm->fd, platform.drm->mode->hdisplay, platform.drm->mode->vdisplay, format, modifier);
	if (!platform.gbm) {
        printf("Failed to initialize GBM\n");
        return EGL_FALSE;
	}

    esContext->platformData = (void *) &platform;
	esContext->eglNativeDisplay = (EGLNativeDisplayType)platform.gbm->dev;
    esContext->eglNativeWindow = (EGLNativeWindowType)platform.gbm->surface;

    init_egl(&platform.egl, platform.gbm, 0);
    esContext->eglDisplay=platform.egl.display;
    esContext->eglContext=platform.egl.context;
    esContext->eglSurface=platform.egl.surface;

    return EGL_TRUE;
}


///
//  userInterrupt()
//
//
GLboolean userInterrupt(ESContext *esContext)
{
    //      TODO: spin off a thread that monitors the keyboard otherwise this will block
    /* C++ aircode
    char keypress;
    while (std::cin)
    {
        std::cin >> keypress;
        if (esContext->keyFunc != NULL)
        {
            esContext->keyFunc(esContext, keypress, 0, 0);
        }
    }
    */
    return GL_FALSE;
}


//////////////////////////////////////////////////////////////////
//
//  Public Functions
//
//

///
//  esInitContext()
//
//      Initialize ES utility context.  This must be called before calling any other
//      functions.
//
void ESUTIL_API esInitContext ( ESContext *esContext )
{
   if ( esContext != NULL )
   {
      memset( esContext, 0, sizeof( ESContext) );
   }
}


///
//  WinLoop()
//
//    Start the main loop for the OpenGL ES application
//

void WinLoop ( ESContext *esContext )
{
    struct timeval t1, t2;
    struct timezone tz;
    float deltatime;
    float totaltime = 0.0f;
    unsigned int frames = 0;

	fd_set fds;
	drmEventContext evctx = {
			.version = 2,
			.page_flip_handler = page_flip_handler,
	};
	struct gbm_bo *bo;
	struct drm_fb *fb;
	uint32_t i = 0;
	int ret;

    eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
	bo = gbm_surface_lock_front_buffer(platform.gbm->surface);
	if (!bo) {
		fprintf(stderr, "Failed to lock the front buffer\n");
		return;
	}

	fb = drm_fb_get_from_bo(bo);
	if (!fb) {
		fprintf(stderr, "Failed to get a new framebuffer from BO\n");
		return;
	}

	/* set mode: */
	ret = drmModeSetCrtc(platform.drm->fd, platform.drm->crtc_id, fb->fb_id, 0, 0,
			(uint32_t*)&platform.drm->connector_id, 1, platform.drm->mode);
	if (ret) {
		printf("failed to set mode: %s\n", strerror(errno));
		return;
	}

    gettimeofday ( &t1 , &tz );

    while(userInterrupt(esContext) == GL_FALSE)
    {
        gettimeofday(&t2, &tz);
        deltatime = (float)(t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) * 1e-6);
        t1 = t2;

		struct gbm_bo *next_bo;
		int waiting_for_flip = 1;

        if (esContext->updateFunc != NULL)
            esContext->updateFunc(esContext, deltatime);
        if (esContext->drawFunc != NULL)
            esContext->drawFunc(esContext);

		//egl.draw(i++);

        eglSwapBuffers(esContext->eglDisplay, esContext->eglSurface);
		next_bo = gbm_surface_lock_front_buffer(platform.gbm->surface);
		fb = drm_fb_get_from_bo(next_bo);
		if (!fb) {
			fprintf(stderr, "Failed to get a new framebuffer BO\n");
			return;
		}

		/*
		 * Here you could also update drm plane layers if you want
		 * hw composition
		 */

		ret = drmModePageFlip(platform.drm->fd, platform.drm->crtc_id, fb->fb_id,
				DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);
		if (ret) {
			printf("failed to queue page flip: %s\n", strerror(errno));
			return;
		}

		while (waiting_for_flip) {
			FD_ZERO(&fds);
			FD_SET(0, &fds);
			FD_SET(platform.drm->fd, &fds);

			ret = select(platform.drm->fd + 1, &fds, NULL, NULL, NULL);
			if (ret < 0) {
				printf("select err: %s\n", strerror(errno));
				return;
			} else if (ret == 0) {
				printf("select timeout!\n");
				return;
			} else if (FD_ISSET(0, &fds)) {
				printf("user interrupted!\n");
				return;
			}
			drmHandleEvent(platform.drm->fd, &evctx);
		} 

		/* release last buffer to render on again: */
		gbm_surface_release_buffer(platform.gbm->surface, bo);
		bo = next_bo;

        totaltime += deltatime;
        frames++;
        if (totaltime >  2.0f)
        {
            printf("%4d frames rendered in %1.4f seconds -> FPS=%3.4f\n", frames, totaltime, frames/totaltime);
            totaltime -= 2.0f;
            frames = 0;
        }
    }
}

///
//  Global extern.  The application must declare this function
//  that runs the application.
//
extern int esMain( ESContext *esContext );

///
//  main()
//
//      Main entrypoint for application
//
int main ( int argc, char *argv[] )
{
   ESContext esContext;
   
   memset ( &esContext, 0, sizeof( esContext ) );


   if ( esMain ( &esContext ) != GL_TRUE )
   {
       fprintf(stderr, "esMain unexpectedly returned GL_FALSE! KABOOM!\n");
        return 1;   
   }
 
   WinLoop ( &esContext );

   if ( esContext.shutdownFunc != NULL )
	   esContext.shutdownFunc ( &esContext );

   if ( esContext.userData != NULL )
	   free ( esContext.userData );

   return 0;
}

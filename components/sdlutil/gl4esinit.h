#ifndef OPENFALLOUT_COMPONENTS_SDLUTIL_GL4ESINIT_H
#define OPENFALLOUT_COMPONENTS_SDLUTIL_GL4ESINIT_H
#ifdef OPENFALLOUT_GL4ES_MANUAL_INIT
#include <SDL_video.h>

// Must be called once SDL video mode has been set,
// which creates a context.
//
// GL4ES can then query the context for features and extensions.
extern "C" void openmw_gl4es_init(SDL_Window* window);

#endif // OPENFALLOUT_GL4ES_MANUAL_INIT
#endif // OPENFALLOUT_COMPONENTS_SDLUTIL_GL4ESINIT_H

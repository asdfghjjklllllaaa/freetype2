/****************************************************************************
 *
 * svgtypes.h
 *
 *   TODO:
 *   The FreeType svg renderer internal types (specification).
 *
 * Copyright (C) 1996-2019 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */

#include <ft2build.h>
#include FT_INTERNAL_OBJECTS_H
#include FT_RENDER_H


  /* Function Pointer definitions for SVG_RendererHooks */
  typedef FT_Error (*SVG_Lib_Init)(); /* initialize the external lib */
  typedef FT_Error (*SVG_Lib_Free)(); /* destroy the external lib */

  typedef struct SVG_RendererHooks_
  {
    /* Api Hooks for OT-SVG Rendering */ 
    SVG_Lib_Init  svg_lib_init;
    SVG_Lib_Free  svg_lib_free;
  } SVG_RendererHooks;

  typedef struct SVG_RendererRec_
  {
    FT_RendererRec     root;   /* This inherits FT_RendererRec */
    FT_Bool            loaded;
    SVG_RendererHooks  hooks;  /* Holds out hooks to the outside library */
  } SVG_RendererRec;

  typedef struct SVG_RendererRec_*  SVG_Renderer;


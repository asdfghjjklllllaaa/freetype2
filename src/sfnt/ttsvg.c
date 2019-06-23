/****************************************************************************
 *
 * ttsvg.c
 *
 *   OpenType SVG Color (specification).
 *
 * Copyright (C) 2018-2019 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


  /**************************************************************************
   *
   * `SVG' table specification:
   *
   *    https://docs.microsoft.com/en-us/typography/opentype/spec/svg
   *
   */

#include <ft2build.h>
#include FT_INTERNAL_STREAM_H
#include FT_TRUETYPE_TAGS_H
#include FT_GZIP_H


#include "ttsvg.h"

  /* TODO: (OT-SVG) Decide whether to add documentation here or not */

  typedef struct Svg_
  {
    FT_UShort  version;           /* Table version (starting at 0)         */
    FT_UShort  num_entries;        /* Number of SVG document records       */
    /* Pointer to the starting of SVG Document List */
    FT_Byte*   svg_doc_list;      
    /* Memory that backs up SVG */
    void*     table;
    FT_ULong  table_size;
  } Svg;


  FT_LOCAL_DEF( FT_Error )
  tt_face_load_svg( TT_Face    face,
                    FT_Stream  stream )
  {
    FT_Error   error;
    FT_Memory  memory = face->root.memory;

    FT_ULong   table_size;
    FT_Byte*   table = NULL;
    FT_Byte*   p     = NULL;

    Svg*       svg   = NULL;

    FT_ULong  offsetToSVGDocumentList;


    error = face->goto_table( face, TTAG_SVG, stream, &table_size );
    if( error )
      goto NoSVG;

    if( FT_FRAME_EXTRACT( table_size, table ))
      goto NoSVG;
     
    /* Allocate the memory for the Svg object */
    if( FT_NEW( svg ) )
      goto NoSVG;

    p = table;
    svg->version =            FT_NEXT_USHORT( p );
    offsetToSVGDocumentList = FT_NEXT_ULONG( p );

    if( offsetToSVGDocumentList == 0 )
      goto InvalidTable;

    svg->svg_doc_list = (FT_Byte*)( table + offsetToSVGDocumentList );

    p = svg->svg_doc_list;
    svg->num_entries = FT_NEXT_USHORT( p );

    svg->table =      table;
    svg->table_size = table_size;

    face->svg = svg;

    return FT_Err_Ok;

  InvalidTable:
    error = FT_THROW( Invalid_Table );

  NoSVG:
    FT_FRAME_RELEASE( table );
    FT_FREE( svg );
    face->svg = NULL;

    return error;
  }

  FT_LOCAL_DEF( void )
  tt_face_free_svg( TT_Face  face )
  {
    FT_Memory  memory = face->root.memory;
    FT_Stream  stream = face->root.stream;

    Svg* svg = (Svg*) face->svg;
    if( svg )
    {
      FT_FRAME_RELEASE( svg->table );
      FT_FREE( svg );
    }   
  }

  FT_Error
  find_doc( FT_Byte*   stream,
            FT_UShort  num_entries,
            FT_UInt    glyph_index,
            FT_ULong   *doc_offset,
            FT_ULong   *doc_length  )
  {
    FT_Error   error;
    FT_UShort  start_glyph_id;
    FT_UShort  end_glyph_id;
    FT_ULong   cur_doc_offset;
    FT_ULong   cur_doc_length;

    FT_Bool    found = FALSE;
    FT_UInt    i     = 0;

    /* TODO: (OT-SVG) Convert to efficient search algorithm        */
    /* TODO: (OT-SVG) Use Frame Fields here instead of `FT_NEXT_*' */
    for ( i = 0; i < num_entries; i++)
    {
      start_glyph_id = FT_NEXT_USHORT( stream );
      end_glyph_id   = FT_NEXT_USHORT( stream );
      cur_doc_offset = FT_NEXT_ULONG( stream );
      cur_doc_length = FT_NEXT_ULONG( stream );

      if ( ( glyph_index >= start_glyph_id) &&
           ( glyph_index <= end_glyph_id  ) )
      {
        found = TRUE;
        *doc_offset = cur_doc_offset;
        *doc_length = cur_doc_length;
        break;
      }
    }
    if ( found != TRUE )
      error = FT_THROW( Invalid_Glyph_Index );
    else
      error = FT_Err_Ok;
    return error;
  }

  FT_LOCAL_DEF(FT_Error)
  tt_face_load_svg_doc( FT_GlyphSlot  glyph_,
                        FT_UInt       glyph_index )
  {

    TT_GlyphSlot glyph = (TT_GlyphSlot) glyph_;

    /* TODO: (OT-SVG) properly clean stuff here on errors */

    FT_Byte*   doc_list;             /* Pointer to the Svg Document List */
    FT_UShort  num_entries;          /* Total no of entires in doc list  */

    FT_ULong   doc_offset;
    FT_ULong   doc_length;

    FT_ULong   uncomp_size;
    FT_Byte*   uncomp_buffer;

    FT_Bool    is_gzip_encoded = FALSE;

    FT_Error   error  = FT_Err_Ok;
    TT_Face    face   = (TT_Face)glyph->root.face;
    FT_Memory  memory = face->root.memory;
    Svg*       svg    = face->svg;

    /* handle svg being 0x0 situation here */
    doc_list     = svg->svg_doc_list;
    num_entries  = FT_NEXT_USHORT( doc_list );

    error = find_doc( doc_list, num_entries, glyph_index,
                                &doc_offset, &doc_length );
    if ( error != FT_Err_Ok )
      return error;

    doc_list = svg->svg_doc_list;   /* Reset to so we can use it again */
    doc_list = (FT_Byte*)( doc_list + doc_offset );

    if( ( doc_list[0] == 0x1F ) && ( doc_list[1] == 0x8B )
                                && ( doc_list[2] == 0x08 ) )
    {
      is_gzip_encoded = TRUE;

      /* get the size of the orignal document. This helps in alotting the
       * buffer to accomodate the uncompressed version. The last 4 bytes
       * of the compressed document are equal to orignal_size modulo 2^32.
       * Since SVG docs will be lesser in size then 2^32, we can use this
       * accurately. The four bytes are stored in little-endian format.
       */
      uncomp_size = (FT_ULong)doc_list[doc_length - 1] << 24 |
                    (FT_ULong)doc_list[doc_length - 2] << 16 |
                    (FT_ULong)doc_list[doc_length - 3] << 8  |
                    (FT_ULong)doc_list[doc_length - 4];
      
      /* TODO: (OT-SVG) memory allocated here needs to be freed somewhere */
      uncomp_buffer = (FT_Byte*) memory->alloc(memory, uncomp_size);
      error = FT_Gzip_Uncompress( memory, uncomp_buffer, &uncomp_size,
                                          doc_list,      doc_length  );
      if ( error != FT_Err_Ok )
      {
        error = FT_THROW( Invalid_Table );
        return error;
      }

      glyph->svg_document        = uncomp_buffer;
      glyph->svg_document_length = uncomp_size;
      return FT_Err_Ok;
    }

    glyph->svg_document        = doc_list;
    glyph->svg_document_length = doc_length;
    return FT_Err_Ok;
  }

/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include <import/nitf.h>
#include <import/cgm.h>


NITF_BOOL addGraphicSegment(nitf_Record *record, nitf_Error *error)
{
    /* add a new graphics segment */
    nitf_GraphicSegment *graphic = nitf_Record_newGraphicSegment(record, error);
    
    if (!nitf_Field_setString(graphic->subheader->filePartType, "SY", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->graphicID, "TEST", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->name, " ", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->securityClass, "U", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->encrypted, "0", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->stype, "C", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setUint32(graphic->subheader->res1, 0, error))
        goto CATCH_ERROR;
    /* might want to calculate this... for now, hard code it */
    if (!nitf_Field_setUint32(graphic->subheader->displayLevel, 999, error))
        goto CATCH_ERROR;
    if (!nitf_Field_setUint32(graphic->subheader->attachmentLevel, 0, error))
        goto CATCH_ERROR;
    if (!nitf_Field_setUint32(graphic->subheader->location, 0, error))
        goto CATCH_ERROR;
    if (!nitf_Field_setUint32(graphic->subheader->bound1Loc, 0, error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->color, "C", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setString(graphic->subheader->bound2Loc, "0010000100", error))
        goto CATCH_ERROR;
    if (!nitf_Field_setUint32(graphic->subheader->res2, 0, error))
        goto CATCH_ERROR;
    
    return NITF_SUCCESS;
    
  CATCH_ERROR:
    return NITF_FAILURE;
}


cgm_Metafile* createCGM(nitf_Error *error)
{
    cgm_Metafile *mf = NULL;
    cgm_Element *element = NULL;
    cgm_TextElement *textElement = NULL;
    
    if (!(mf = cgm_Metafile_construct("TEXT", "TEXT", error)))
        goto CATCH_ERROR;
    
    if (!cgm_Metafile_createPicture(mf, "Text", error))
        goto CATCH_ERROR;
    
    if (!nitf_List_pushBack(mf->fontList, (NITF_DATA*)"Helvetica", error))
        goto CATCH_ERROR;
    if (!nitf_List_pushBack(mf->fontList, (NITF_DATA*)"Courier", error))
        goto CATCH_ERROR;
    
    if (!(element = cgm_TextElement_construct(error)))
        goto CATCH_ERROR;
    
    textElement = (cgm_TextElement*)element->data;
    
    if (!(textElement->attributes = cgm_TextAttributes_construct(error)))
        goto CATCH_ERROR;
    
    if (!(textElement->text = cgm_Text_construct("NITRO rocks!", error)))
        goto CATCH_ERROR;
    
    
    textElement->attributes->characterHeight = 14;
    textElement->attributes->textFontIndex = 2; /* courier */
    
    /* set the color of the text */
    textElement->attributes->textColor->r = 255;
    textElement->attributes->textColor->g = 0;
    textElement->attributes->textColor->b = 0;
    
    textElement->attributes->characterOrientation->x1 = 0;
    textElement->attributes->characterOrientation->y1 = 0;
    textElement->attributes->characterOrientation->x2 = 100;
    textElement->attributes->characterOrientation->y2 = 100;
    
    textElement->text->x = 10;
    textElement->text->y = 20;
    
    /* add the element to the picture body */
    if (!nitf_List_pushBack(mf->picture->body->elements,
            (NITF_DATA*)element, error))
        goto CATCH_ERROR;
    
    return mf;
    
  CATCH_ERROR:
    if (mf)
        cgm_Metafile_destruct(&mf);
    return NULL;
}

NITF_BOOL writeNITF(nitf_Record * record, nitf_IOHandle input,
        char *outFile, cgm_Metafile *lastCGM, nitf_Error *error)
{
    nitf_ListIterator iter;
    nitf_ListIterator end;
    int num;
    int numSegments;
    nitf_Writer *writer = NULL;
    
    /* open the output IO Handle */
    nitf_IOHandle output = nitf_IOHandle_create(outFile,
            NITF_ACCESS_WRITEONLY, NITF_CREATE, error);

    if (NITF_INVALID_HANDLE(output))
    {
        goto CATCH_ERROR;
    }

    writer = nitf_Writer_construct(error);
    if (!writer)
    {
        goto CATCH_ERROR;
    }
    
    /* prepare the writer with this record */
    if (!nitf_Writer_prepare(writer, record, output, error))
    {
        goto CATCH_ERROR;
    }

    /**************************************************************************/
    /* IMAGES                                                                 */
    /**************************************************************************/
    /* get the # of images from the field */
    if (!nitf_Field_get(record->header->numImages,
            &numSegments, NITF_CONV_INT, NITF_INT32_SZ, error))
    {
        nitf_Error_print(error, stderr, "nitf::Value::get() failed");
        numSegments = 0;
    }

    if (record->images)
    {
        iter = nitf_List_begin(record->images);
        end = nitf_List_end(record->images);
        
        num = 0;
        while(nitf_ListIterator_notEqualTo(&iter, &end))
        {
            nitf_WriteHandler *writeHandler = NULL;
            nitf_ImageSegment *segment =
                    (nitf_ImageSegment *) nitf_ListIterator_get(&iter);
            
            writeHandler = nitf_StreamIOWriteHandler_construct(input,
                    segment->imageOffset, segment->imageEnd - segment->imageOffset,
                    error);
            if (!writeHandler)
                goto CATCH_ERROR;
            
            /* attach the write handler */
            if (!nitf_Writer_setImageWriteHandler(writer, num,
                    writeHandler, error))
            {
                nitf_WriteHandler_destruct(&writeHandler);
                goto CATCH_ERROR;
            }
            
            nitf_ListIterator_increment(&iter);
            ++num;
        }
    }
    
    /**************************************************************************/
    /* GRAPHICS                                                               */
    /**************************************************************************/
    /* get the # of images from the field */
    if (!nitf_Field_get(record->header->numGraphics,
            &numSegments, NITF_CONV_INT, NITF_INT32_SZ, error))
    {
        nitf_Error_print(error, stderr, "nitf::Value::get() failed");
        numSegments = 0;
    }

    if (record->graphics)
    {
        iter = nitf_List_begin(record->graphics);
        end = nitf_List_end(record->graphics);
        
        num = 0;
        while(nitf_ListIterator_notEqualTo(&iter, &end))
        {
            nitf_WriteHandler *writeHandler = NULL;
            
            /* if this is the last graphic segment - this is the
             * graphic segment we created by hand, so we want provide our
             * own WriteHandler
             */
            if (num == numSegments - 1)
            {
                writeHandler = cgm_NITFWriteHandler_construct(lastCGM, error);
            }
            else
            {
                /* otherwise, this graphic existed previously */
                nitf_GraphicSegment *segment =
                        (nitf_GraphicSegment *) nitf_ListIterator_get(&iter);
                
                writeHandler = nitf_StreamIOWriteHandler_construct(input,
                        segment->offset, segment->end - segment->offset,
                        error);
            }
            
            if (!writeHandler)
                goto CATCH_ERROR;
            
            /* attach the write handler */
            if (!nitf_Writer_setGraphicWriteHandler(writer, num,
                    writeHandler, error))
            {
                nitf_WriteHandler_destruct(&writeHandler);
                goto CATCH_ERROR;
            }
            
            nitf_ListIterator_increment(&iter);
            ++num;
        }
    }
    
    /**************************************************************************/
    /* TEXTS                                                                  */
    /**************************************************************************/
    /* get the # of images from the field */
    if (!nitf_Field_get(record->header->numTexts,
            &numSegments, NITF_CONV_INT, NITF_INT32_SZ, error))
    {
        nitf_Error_print(error, stderr, "nitf::Value::get() failed");
        numSegments = 0;
    }

    if (record->texts)
    {
        iter = nitf_List_begin(record->texts);
        end = nitf_List_end(record->texts);
        
        num = 0;
        while(nitf_ListIterator_notEqualTo(&iter, &end))
        {
            nitf_WriteHandler *writeHandler = NULL;
            nitf_TextSegment *segment =
                    (nitf_TextSegment *) nitf_ListIterator_get(&iter);
            
            writeHandler = nitf_StreamIOWriteHandler_construct(input,
                    segment->offset, segment->end - segment->offset,
                    error);
            if (!writeHandler)
                goto CATCH_ERROR;
            
            /* attach the write handler */
            if (!nitf_Writer_setTextWriteHandler(writer, num,
                    writeHandler, error))
            {
                nitf_WriteHandler_destruct(&writeHandler);
                goto CATCH_ERROR;
            }
            
            nitf_ListIterator_increment(&iter);
            ++num;
        }
    }
    
    /**************************************************************************/
    /* DES                                                                    */
    /**************************************************************************/
    /* get the # of images from the field */
    if (!nitf_Field_get(record->header->numDataExtensions,
            &numSegments, NITF_CONV_INT, NITF_INT32_SZ, error))
    {
        nitf_Error_print(error, stderr, "nitf::Value::get() failed");
        numSegments = 0;
    }

    if (record->dataExtensions)
    {
        iter = nitf_List_begin(record->dataExtensions);
        end = nitf_List_end(record->dataExtensions);
        
        num = 0;
        while(nitf_ListIterator_notEqualTo(&iter, &end))
        {
            nitf_WriteHandler *writeHandler = NULL;
            nitf_DESegment *segment =
                    (nitf_DESegment *) nitf_ListIterator_get(&iter);
            
            writeHandler = nitf_StreamIOWriteHandler_construct(input,
                    segment->offset, segment->end - segment->offset,
                    error);
            if (!writeHandler)
                goto CATCH_ERROR;
            
            /* attach the write handler */
            if (!nitf_Writer_setDEWriteHandler(writer, num,
                    writeHandler, error))
            {
                nitf_WriteHandler_destruct(&writeHandler);
                goto CATCH_ERROR;
            }
            
            nitf_ListIterator_increment(&iter);
            ++num;
        }
    }
    
    if (!nitf_Writer_write(writer, error))
    {
        goto CATCH_ERROR;
    }
    
    nitf_IOHandle_close(output);
    nitf_Writer_destruct(&writer);
    return NITF_SUCCESS;
    
  CATCH_ERROR:    
    if (!NITF_INVALID_HANDLE(output))
        nitf_IOHandle_close(output);
    if (writer)
        nitf_Writer_destruct(&writer);
    return NITF_FAILURE;
}

int main(int argc, char **argv)
{
    nitf_Record *record = NULL;
    nitf_Reader *reader = NULL;
    cgm_Metafile *metafile = NULL;
    nitf_Error error;
    nitf_IOHandle io;
    
    if (argc != 3)
    {
        printf("Usage: %s <input-file> <output-file> \n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    reader = nitf_Reader_construct(&error);
    if (!reader)
    {
        nitf_Error_print(&error, stderr, "nitf::Reader::construct() failed");
        exit(EXIT_FAILURE);
    }

    /* open the input handle */
    io = nitf_IOHandle_create(argv[1],
                              NITF_ACCESS_READONLY,
                              NITF_OPEN_EXISTING, &error);

    /* check to make sure it is valid */
    if (NITF_INVALID_HANDLE(io))
    {
        nitf_Error_print(&error, stderr, "nitf::IOHandle::create() failed");
        exit(EXIT_FAILURE);
    }

    /*  read the file  */
    record = nitf_Reader_read(reader, io, &error);
    if (!record)
    {
        nitf_Error_print(&error, stderr, "nitf::Reader::read() failed");
        exit(EXIT_FAILURE);
    }
    
    /* add a graphic segment */
    addGraphicSegment(record, &error);
    
    metafile = createCGM(&error);
    
    if (!writeNITF(record, io, argv[2], metafile, &error))
    {
        nitf_Error_print(&error, stderr, "Write failed");
    }
    
    nitf_IOHandle_close(io);
    nitf_Reader_destruct(&reader);
    nitf_Record_destruct(&record);
    
    return 0;
}
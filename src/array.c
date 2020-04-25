/*
 * OCILIB - C Driver for Oracle (C Wrapper for Oracle OCI)
 *
 * Website: http://www.ocilib.net
 *
 * Copyright (c) 2007-2020 Vincent ROGIER <vince.rogier@ocilib.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "array.h"

#include "collection.h"
#include "date.h"
#include "file.h"
#include "helpers.h"
#include "interval.h"
#include "list.h"
#include "lob.h"
#include "macros.h"
#include "memory.h"
#include "number.h"
#include "object.h"
#include "reference.h"
#include "timestamp.h"

#define ARRAY_INIT(type, func)                                              \
    arr->tab_obj[i]                 = func;                                 \
    ((void **)(arr->mem_handle))[i] = ((type *) arr->tab_obj[i])->handle;   \


/* --------------------------------------------------------------------------------------------- *
 * ArrayFindAny
 * --------------------------------------------------------------------------------------------- */

boolean ArrayFindAny
(
    OCI_Array *arr,
    void     **handles
)
{
    return arr && (arr->tab_obj == handles || arr->mem_struct == handles);
}

/* --------------------------------------------------------------------------------------------- *
* ArrayFindObjects
* --------------------------------------------------------------------------------------------- */

boolean ArrayFindObjects
(
    OCI_Array *arr,
    void     **handles
)

{
    return arr && arr->tab_obj == handles;
}

/* --------------------------------------------------------------------------------------------- *
 * _ArrayInit
 * --------------------------------------------------------------------------------------------- */

boolean ArrayInitialize
(
    OCI_Array    *arr,
    OCI_TypeInfo *typinf
)
{
    for (unsigned int i = 0; i < arr->nb_elem; i++)
    {
        void *handle = NULL;
        void *data   = ((char*)arr->mem_struct) + (arr->struct_size * i);;

        if (OCI_CDT_DATETIME == arr->elem_type)
        {
            handle = &(((OCIDate *)(arr->mem_handle))[i]);
        }
        else if (IS_OCI_NUMBER(arr->elem_type, arr->elem_subtype))
        {
            handle = &(((OCINumber *)(arr->mem_handle))[i]);
        }
        else
        {
            handle = ((void **)(arr->mem_handle))[i];
        }

        arr->tab_obj[i] = data;

        ((OCI_Datatype *) data)->hstate = OCI_OBJECT_ALLOCATED_ARRAY;

        switch (arr->elem_type)
        {
            case OCI_CDT_NUMERIC:
            {
                if (OCI_NUM_NUMBER == arr->elem_subtype)
                {
                    arr->tab_obj[i] = NumberInitialize(arr->con,
                                                       (OCI_Number *)data,
                                                       (OCINumber *) handle);
                }
                break;
            }
            case OCI_CDT_DATETIME:
            {
                arr->tab_obj[i] =
                    DateInitialize(arr->con, (OCI_Date *)data,
                                   (OCIDate *) handle,
                                   FALSE, FALSE);
                break;
            }
            case OCI_CDT_LOB:
            {
                ARRAY_INIT(OCI_Lob,
                           LobInitialize(arr->con, (OCI_Lob *)data,
                                         (OCILobLocator *) handle,
                                         arr->elem_subtype))
                break;
            }
            case OCI_CDT_FILE:
            {
                ARRAY_INIT(OCI_File,
                           FileInitialize(arr->con, (OCI_File *)data,
                                          (OCILobLocator *) handle,
                                          arr->elem_subtype))
                break;
            }
            case OCI_CDT_TIMESTAMP:
            {
                ARRAY_INIT(OCI_Timestamp,
                           TimestampInitialize(arr->con, (OCI_Timestamp *)data,
                                               (OCIDateTime *) handle,
                                               arr->elem_subtype))
                break;
            }
            case OCI_CDT_INTERVAL:
            {
                ARRAY_INIT(OCI_Interval,
                           IntervalInitialize(arr->con, (OCI_Interval *)data,
                                              (OCIInterval *) handle,
                                              arr->elem_subtype))
                break;
            }
            case OCI_CDT_OBJECT:
            {
                ARRAY_INIT(OCI_Object,
                           ObjectInitialize(arr->con, (OCI_Object *)data,
                                            handle, typinf,
                                            NULL, -1, TRUE))
                break;
            }
            case OCI_CDT_COLLECTION:
            {
                ARRAY_INIT(OCI_Coll,
                           CollectionInitialize(arr->con, (OCI_Coll *)data,
                                                handle, typinf))
                break;
            }
            case OCI_CDT_REF:
            {
                ARRAY_INIT(OCI_Ref,
                           ReferenceInitialize(arr->con, typinf,
                                               (OCI_Ref *) data, handle))
                break;
            }
        }
    }

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- *
 * ArrayClose
 * --------------------------------------------------------------------------------------------- */

boolean ArrayDispose
(
    OCI_Array *arr
)
{
    CHECK(NULL == arr, FALSE)

    if (IS_OCILIB_OBJECT(arr->elem_type, arr->elem_subtype))
    {
        /* Cleanup OCILIB Objects */

        for (unsigned int i = 0; i < arr->nb_elem; i++)
        {
            FreeObjectFromType(arr->tab_obj[i], arr->elem_type);
        }
    }

    /* free OCI descriptors */

    if (OCI_UNKNOWN != arr->handle_type)
    {
        MemoryFreeDescriptorArray((dvoid **) arr->mem_handle,
                                  (ub4) arr->handle_type,
                                  (ub4) arr->nb_elem);
    }

    FREE(arr->mem_handle)
    FREE(arr->mem_struct)
    FREE(arr->tab_obj)

    return TRUE;
}

/* --------------------------------------------------------------------------------------------- *
 * ArrayCreate
 * --------------------------------------------------------------------------------------------- */

OCI_Array * ArrayCreate
(
    OCI_Connection *con,
    unsigned int    nb_elem,
    unsigned int    elem_type,
    unsigned int    elem_subtype,
    unsigned int    elem_size,
    unsigned int    struct_size,
    unsigned int    handle_type,
    OCI_TypeInfo   *typinf
)
{
    OCI_Array *arr = NULL;

    DECLARE_CTX(TRUE)
    CALL_CONTEXT_FROM_CON(con)

    /* create array object */

    arr = ListAppend(Env.arrs, sizeof(*arr));

    STATUS = (NULL != arr);

    if (STATUS)
    {
        arr->con          = con;
        arr->err          = con ? con->err : Env.err;
        arr->env          = con ? con->env : Env.env;
        arr->elem_type    = elem_type;
        arr->elem_subtype = elem_subtype;
        arr->elem_size    = elem_size;
        arr->nb_elem      = nb_elem;
        arr->struct_size  = struct_size;
        arr->handle_type  = handle_type;

        /* allocate buffers */

        if (IS_OCILIB_OBJECT(arr->elem_type, arr->elem_subtype))
        {
            ALLOC_DATA(OCI_IPC_VOID, arr->tab_obj, nb_elem)
        }

        ALLOC_BUFFER(OCI_IPC_VOID, arr->mem_handle, elem_size, nb_elem)
        ALLOC_BUFFER(OCI_IPC_VOID, arr->mem_struct, struct_size, nb_elem)

        /* allocate OCI handle descriptors */

        if (STATUS && handle_type != 0)
        {
            STATUS = MemoryAllocDescriptorArray((dvoid  *)arr->env,
                                                (dvoid **)arr->mem_handle,
                                                (ub4)handle_type,
                                                (ub4)nb_elem);
        }

        if (STATUS && arr->tab_obj && arr->mem_handle)
        {
            STATUS = ArrayInitialize(arr, typinf);
        }
    }

    /* check for failure */

    if (!STATUS)
    {
        ArrayDispose(arr);
        FREE(arr)
    }

    return arr;
}

/* --------------------------------------------------------------------------------------------- *
 * OCI_ArrayFreeFromHandles
 * --------------------------------------------------------------------------------------------- */

boolean ArrayFreeFromHandles
(
    void **handles
)
{
    boolean    res = FALSE;
    OCI_Array *arr = ListFind(Env.arrs, (POCI_LIST_FIND) ArrayFindAny, handles);

    if (arr)
    {
        res = ListRemove(Env.arrs, arr);
        ArrayDispose(arr);
        FREE(arr)
    }

    return res;
}

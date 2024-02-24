/*********************************************************************
* marcproclist.h                                                     *
*                                                                    *
*   PURPOSE                                                          *
*       Header file with prototypes for custom marcconv conversion   *
*       procedures.                                                  *
*                                                                    *
*                                               Author: Alan Meyer   *
*********************************************************************/

/* $Id: marcproclist.h,v 1.2 2000/10/05 14:27:59 meyer Exp $
 * $Log $
 */

#ifndef MARCPROCLIST_H
#define MARCPROCLIST_H

#include "marcconv.h"

/* Prototypes for specialized marcconv routines */

CM_STAT cmp_proc_000               (CM_PROC_PARMS *);
CM_STAT cmp_proc_010               (CM_PROC_PARMS *);
CM_STAT cmp_naco_010               (CM_PROC_PARMS *);
CM_STAT cmp_proc_022               (CM_PROC_PARMS *);
CM_STAT cmp_proc_035               (CM_PROC_PARMS *);
//CM_STAT cmp_proc_035_plus          (CM_PROC_PARMS *);
CM_STAT cmp_proc_035_remarc        (CM_PROC_PARMS *);
CM_STAT cmp_generic_035_processing (CM_PROC_PARMS *);
CM_STAT cmp_proc_041               (CM_PROC_PARMS *);
CM_STAT cmp_proc_066               (CM_PROC_PARMS *);
CM_STAT cmp_punc_245               (CM_PROC_PARMS *);
CM_STAT cmp_proc_659               (CM_PROC_PARMS *);
CM_STAT cmp_proc_76X               (CM_PROC_PARMS *);
CM_STAT cmp_proc_856               (CM_PROC_PARMS *);
CM_STAT cmp_proc_880               (CM_PROC_PARMS *);
CM_STAT cmp_proc_998               (CM_PROC_PARMS *);
CM_STAT cmp_isbn_13                (CM_PROC_PARMS *);
CM_STAT cmp_checkdups              (CM_PROC_PARMS *);
CM_STAT cmp_Mesh                   (CM_PROC_PARMS *);
CM_STAT cmp_checklen               (CM_PROC_PARMS *);
CM_STAT cmp_sortfields             (CM_PROC_PARMS *);
CM_STAT cmp_getUIfrom035           (CM_PROC_PARMS *);
CM_STAT cmp_naco_cleanup           (CM_PROC_PARMS *);

CM_STAT add_field_and_indics (CM_PROC_PARMS *pp, int field_id);
int get_isbn_length(unsigned char *tsrc2, size_t src_len);

#endif /* MARCPROCLIST_H */

/*********************************************************************
* marc.h                                                             *
*                                                                    *
*   PURPOSE                                                          *
*       External header file for National Library of Medicine        *
*       MaRC (Machine Readable Cataloging) format record             *
*       management routines.                                         *
*                                                                    *
*       Defines error codes and function prototypes.                 *
*                                                                    *
*                                               Author: Alan Meyer   *
*********************************************************************/

/* $Id: marc.h,v 1.5 2000/10/02 18:42:49 meyer Exp $
 * $Log: marc.h,v $
 * Revision 1.5  2000/10/02 18:42:49  meyer
 * Added prototype for marc_copy_field, plus two new error codes.
 *
 * Revision 1.4  1999/08/30 23:11:23  meyer
 * Added some error codes.
 *
 * Revision 1.3  1999/07/22 02:52:57  meyer
 * Added prototype for marc_rename_subfield()
 *
 * Revision 1.2  1998/12/31 21:12:57  meyer
 * Added a comment.
 *
 * Revision 1.1  1998/12/20 20:58:36  meyer
 * Initial revision
 *
 */

#ifndef MARC_H
#define MARC_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdlib.h>                 /* For size_t                   */
#include <stdio.h>
#ifdef MCK
#include <memcheck.h>
#endif

/*********************************************************************
* MARCP                                                              *
*   One of these is passed to each of the marc functions.            *
*                                                                    *
*   It is defined here as an incomplete type.  Calling programs      *
*   should never inspect its internals.                              *
*                                                                    *
*   The full definition appears in marcdefs.h, which external        *
*   programs should NOT include.                                     *
*********************************************************************/
typedef struct marcctl * MARCP;


/*********************************************************************
*   Constants                                                        *
*********************************************************************/
#define MARC_NO_ORDER            0  /* See marc_field_order()       */
#define MARC_NO_SUBFIELD       (-1) /* Subfield code when no sfs    */
#define MARC_SF_STRLEN       0xFFFF /* Use strlen in marc_add_subfd */


/*********************************************************************
*   Indicators - passed to marc_add_subfield() to modify indicators  *
*                                                                    *
*   Note: These are not the same as '1' or '2', which are valid      *
*         MARC variable subfield codes.                              *
*********************************************************************/
#define MARC_INDIC1              1  /* Represents first indicator   */
#define MARC_INDIC2              2  /* Second indicator             */


/*********************************************************************
*   Values from marc_ref() indicating symbolic id's or occurrences   *
*********************************************************************/
#define MARC_REF_CURRENT      9998  /* Current id or occurrence     */
#define MARC_REF_NEW          9999  /* New occurrence               */


/*********************************************************************
*   Errors                                                           *
*********************************************************************/
#define MARC_ERR_ALLOC      (-4001) /* Could not allocate memory    */
#define MARC_ERR_BAD_ORDER  (-4002) /* Invalid marc_field_order parm*/
#define MARC_ERR_CTLFIELD   (-4003) /* Internal err not a var field */
#define MARC_ERR_DIRALLOC   (-4004) /* Can't allocate more fields   */
#define MARC_ERR_DUPFFLD    (-4005) /* Duplicate fixed field        */
#define MARC_ERR_EMPTY_FLD  (-4006) /* Added field without subfields*/
#define MARC_ERR_END_TAG    (-4007) /* Corrupt end marcctl struct   */
#define MARC_ERR_FIELDID    (-4008) /* Field id out of legal range  */
#define MARC_ERR_FIELDLEN   (-4009) /* Field too large              */
#define MARC_ERR_INDICLEN   (-4010) /* Indicators must = 1 char each*/
#define MARC_ERR_MARCALLOC  (-4011) /* Can't get space for output   */
#define MARC_ERR_MAXFIELDS  (-4012) /* Too many fields              */
#define MARC_ERR_MAX_SFS    (-4013) /* Too many subfields in field  */
#define MARC_ERR_MBASEADDR  (-4014) /* Bad baseaddr "   "      "    */
#define MARC_ERR_MDIRCHARS  (-4015) /* Directory contains non-digits*/
#define MARC_ERR_MDIRENTRY  (-4016) /* Conflicting entries in dir.  */
#define MARC_ERR_MDIRTERM   (-4017) /* Missing directory term " " " */
#define MARC_ERR_MRECLEN    (-4018) /* Bad reclen in passed marcrec */
#define MARC_ERR_MRECTERM   (-4019) /* Missing rec term "  "   "    */
#define MARC_ERR_NO_AFIELD  (-4020) /* Adding sf to non-existent fld*/
#define MARC_ERR_RAWALLOC   (-4021) /* Can't get space for buffer   */
#define MARC_ERR_RECLEN     (-4022) /* Illegally large marc record  */
#define MARC_ERR_SFCODE     (-4023) /* SF code out of legal range   */
#define MARC_ERR_SFDIRALLOC (-4024) /* Can't allocate internal sfdir*/
#define MARC_ERR_START_TAG  (-4025) /* Corrupt start marcctl struct */
#define MARC_ERR_VARONLY    (-4026) /* Function only works on var fd*/
#define MARC_ERR_XFIELD     (-4027) /* Internal xcheck field error  */
#define MARC_ERR_XMARCSIZE  (-4028) /* Internal xcheck marcnuf error*/
#define MARC_ERR_XRAWSIZE   (-4029) /* Internal xcheck rawbuf error */
#define MARC_ERR_DUP_COLL   (-4030) /* Duplicate sf codes in set_col*/
#define MARC_ERR_NON_DUP    (-4031) /* Attempt to marc_dup non MARCP*/
#define MARC_ERR_READ_ONLY  (-4032) /* Can't add fields or sfs      */
#define MARC_ERR_NO_DFIELD  (-4033) /* Can't delete non-existent fld*/
#define MARC_ERR_NO_DELPOS  (-4034) /* No sf pos set before delete  */
#define MARC_ERR_DEL_INDIC  (-4035) /* Can't delete an indicator    */
#define MARC_ERR_BAD_INDIC  (-4036) /* Indicator must be 1 or 2     */
#define MARC_ERR_INDIC_VAL  (-4037) /* Indic value not printable chr*/
#define MARC_ERR_INDIC_FLD  (-4038) /* Indics only in var len fields*/
#define MARC_ERR_LENMATCH   (-4039) /* Dirlen sum mismatches lrecl  */
#define MARC_ERR_FIELDTAG   (-4040) /* Invalid ASCII field tag      */
#define MARC_ERR_BAD_REF    (-4041) /* Bad ref syntax in marc_ref() */
#define MARC_ERR_READ       (-4042) /* fread err in marc_read_rec() */
#define MARC_ERR_READ_BSIZE (-4043) /* Short buf in marc_read_rec() */
#define MARC_ERR_READ_LDR   (-4044) /* fread err in LDR in  "       */
#define MARC_ERR_READ_OFLOW (-4045) /* Still short buf in   "       */
#define MARC_ERR_READ_INC   (-4046) /* Incomplete last rec in "     */
#define MARC_ERR_READ_TERM  (-4047) /* No rec terminator in   "     */
#define MARC_ERR_WRITE_PACK (-4048) /* Can't pack record for write  */
#define MARC_ERR_WRITE      (-4049) /* fwrite err in marc_write_rec */
#define MARC_ERR_WRITE_TERM (-4050) /* No recterm on rec to write   */
#define MARC_ERR_POS_STACK  (-4051) /* Reached max marc_save_pos's  */
#define MARC_ERR_SAVE_POS   (-4052) /* Unbalanced save/restore      */
#define MARC_ERR_REN_FLDPOS (-4053) /* No current field to rename   */
#define MARC_ERR_REN_FLDTAG (-4054) /* Must rename field 1..999     */
#define MARC_ERR_SF_FLDPOS  (-4055) /* Can't pos to sf if no field  */
#define MARC_ERR_LEN_FLDPOS (-4056) /* Can't get fld len if no field*/
#define MARC_ERR_CUR_FIELD  (-4057) /* No current field to get      */
#define MARC_ERR_REF_FIELD  (-4058) /* Invalid field in marc_ref()  */
#define MARC_ERR_REF_FOCC   (-4059) /* Invalid field occ in ref()   */
#define MARC_ERR_REF_FIXLEN (-4060) /* Must have pos+len in ref()   */
#define MARC_ERR_REF_SFOCC  (-4061) /* Invalid sf occ in marc_ref() */
#define MARC_ERR_REF_PARTS  (-4062) /* Too many parts in marc_ref() */
#define MARC_ERR_REF_INDIC  (-4063) /* Invalid indicator marc_ref() */
#define MARC_ERR_BAD_CHAR   (-4064) /* Attempt to add invalid char  */
#define MARC_ERR_CHK_PARM   (-4065) /* Internal err marc_xcheck_data*/
#define MARC_ERR_SAME_REC   (-4066) /* Attempt to copy into same rec*/
#define MARC_ERR_DIFF_FLD   (-4067) /* Can't copy fld to diff field */


/*********************************************************************
*   Non-error return codes                                           *
*********************************************************************/
#define MARC_RET_NO_FIELD     4001  /* Could not find field         */
#define MARC_RET_NO_FLDOCC    4002  /* Found field but not right occ*/
#define MARC_RET_NO_SUBFIELD  4003  /* Could not find subfield      */
#define MARC_RET_NO_SUBFLDOCC 4004  /* Found sf but not right occ   */
#define MARC_RET_END_REC      4005  /* Can't position past rec end  */
#define MARC_RET_END_FIELD    4006  /* Can't position past field end*/
#define MARC_RET_FX_OFFSET    4007  /* No req. offset in fixed field*/
#define MARC_RET_FX_LENGTH    4008  /* No req. length in fixed field*/
#define MARC_RET_NO_POS       4009  /* No current field, can't save */
#define MARC_RET_NO_SAVE_POS  4010  /* Saved pos gone, can't restore*/


/*********************************************************************
*   Prototypes                                                       *
*********************************************************************/
int marc_init               (MARCP *);
int marc_free               (MARCP);
int marc_new                (MARCP);
int marc_old                (MARCP, unsigned char *);
int marc_dup                (MARCP, MARCP *);
int marc_add_field          (MARCP, int);
int marc_add_subfield       (MARCP, int, unsigned char *, size_t);
int marc_copy_field         (MARCP, MARCP, int);
int marc_get_record         (MARCP, unsigned char **, size_t *);
int marc_get_field          (MARCP, int, int, unsigned char **, size_t *);
int marc_get_subfield       (MARCP, int, int, unsigned char **, size_t *);
int marc_get_item           (MARCP, int, int, int, int, unsigned char **,
                             size_t *);
int marc_set_indic          (MARCP, int, unsigned char);
int marc_get_indic          (MARCP, int, unsigned char *);
int marc_pos_field          (MARCP, int, int *, unsigned char **, size_t *);
int marc_pos_subfield       (MARCP, int, int *, unsigned char **, size_t *);
int marc_next_field         (MARCP, int, int *, unsigned char **, size_t *,
                             int *);
int marc_next_subfield      (MARCP, int, int *, unsigned char **, size_t *,
                             int *);
int marc_next_item          (MARCP, int, int *, int *, unsigned char **,
                             size_t *, int *, int *);
int marc_del_field          (MARCP);
int marc_del_subfield       (MARCP);
int marc_field_sort         (MARCP, int);
int marc_field_order        (MARCP, int, int);
int marc_subfield_sort      (MARCP, int);
int marc_subfield_order     (MARCP, int, int);
int marc_set_collate        (MARCP, char *);
int marc_cur_field_count    (MARCP, int *);
int marc_cur_field_len      (MARCP, size_t *);
int marc_cur_subfield_count (MARCP, int *);
int marc_cur_field          (MARCP, int *, int *, int *);
int marc_cur_subfield       (MARCP, int *, int *, int *);
int marc_cur_item           (MARCP, int *, int *, int *, int *, int *, int *);
int marc_ref                (char *, int *, int *, int *, int *);
int marc_ok_subfield        (int);
int marc_save_pos           (MARCP);
int marc_restore_pos        (MARCP);
int marc_rename_field       (MARCP, int);
int marc_rename_subfield    (MARCP, int);
#ifdef DEBUG
int marc_read_rec           (FILE *, unsigned char *, size_t, int *);
#else
int marc_read_rec           (FILE *, unsigned char *, size_t);
#endif
int marc_write_rec          (FILE *, void *);

#ifdef  __cplusplus
}
#endif

#endif /* MARC_H */

/************************************************************************
* meshproc.c                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       MeSH processing procedures used in marcconv processing.         *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: meshproc.c,v 1.13 2000/09/12 19:37:45 meyer Exp $
 * $Log: meshproc.c,v $
 * Revision 1.13  2000/09/12 19:37:45  meyer
 * Changed mesh_combine() to not duplicate a field and replace the last
 * subfield if the replacement is identical to what's already there.
 *
 * Revision 1.12  2000/06/20 00:57:52  meyer
 * Added new handling for 651 or 655 when no 650 present.
 *
 * Revision 1.11  1999/07/22 03:04:28  meyer
 * Fix to KILL_RECORDS.
 * Fixed warnings found by Microsoft compiler
 *
 * Revision 1.10  1999/03/26 22:05:22  meyer
 * Changed language processing on 'und' and 'mul'.
 *
 * Revision 1.9  1999/03/18 18:49:39  meyer
 * Added mrule_non_dict() to make a dictionary type $v entry without
 * language recombinations in addition to the one with recombos.
 * This was a request from Marti S.
 *
 * Revision 1.8  1999/03/18 15:47:15  meyer
 * Fixed sort compare on subfield length, was sorting longest first.
 *
 * Revision 1.7  1999/03/09 00:40:58  meyer
 * Made age650 main headings accept subheadings other than other age650's.
 * Sorted 650's on output by indicator 1, then by strings.
 *
 * Revision 1.6  1999/03/08 16:12:56  meyer
 * Added 4th case to Age650 processing.
 *
 * Revision 1.5  1999/02/20 22:46:22  meyer
 * Fixed bug  - getting indic1 twice instead of 1 & 2.
 *
 * Revision 1.4  1999/02/18 02:54:52  meyer
 * Fixed bug in meshexcp.tbl sort.
 * Made dump of the tables part of the code.  Set MESHTEST=1 to get it.
 *
 * Revision 1.3  1999/02/10 23:00:51  meyer
 * Fixed bug in mesh_find_subfield and improved sorting.
 *
 * Revision 1.2  1999/02/09 02:38:46  meyer
 * Numerous fixes to the complex age650 processing.
 * Eliminated field level exception group count.  I needed to modify
 * exception groups on the fly and didn't want to maintain the count.
 *
 * Revision 1.1  1999/02/08 23:30:23  meyer
 * Initial revision
 *
 */

#include <string.h>
#include "marcproclist.h"

void *marc_alloc(int, int);

/************************************************************************
*                             Constants                                 *
************************************************************************/
#define MH_MAX_EXCP         200 /* Max allocation of exception headings*/
#define MH_MAX_HDGS         100 /* Max headings in one output record   */
#define MH_MAX_COMBOHDGS     20 /* Max headings converted to subfields */
#define MH_MAX_STRSIZE   0x8000 /* Size of a store_str string buffer   */
#define MH_MAX_SFS            8 /* Max subfields in one field          */
#define MH_MAX_LANGS        600 /* Max languages                       */
#define MH_NO_COMBOS          1 /* Don't recombine field with any sfs  */


/*----------------------------------------------------------------------\
| mh_grp                                                                |
|   List of valid groups.  Add to this if new ones identified.          |
\----------------------------------------------------------------------*/
typedef enum mh_grp {
    MH_GRP_NONE = 0,        /* Not an exception                        */
    MH_GRP_AGE,             /* Mesh age heading                        */
    MH_GRP_AGE_SOURCE,      /* Age heading used as source for combines */
    MH_GRP_AGE_TARGET,      /* Heading which combines with age headings*/
    MH_GRP_LAW,             /* Law and legislation headings & subheads */
    MH_GRP_LAW5,            /* Legislation in 655$a                    */
    MH_GRP_CASEREP,         /* Single heading "Case Report"            */
    MH_GRP_STAT,            /* Statistics headings & subheadings       */
    MH_GRP_STAT5,           /* Statistics in 655$a                     */
    MH_GRP_DICT,            /* Dictionaries and similar pub types      */
    MH_GRP_USMED,           /* Medicare, medicaid, etc.                */
    MH_GRP_USMED1           /* US in 651 has special treatment on USMed*/
} MH_GRP;


/*----------------------------------------------------------------------\
| mh_chk                                                                |
|   What we do with exception groups when adding subfields.             |
\----------------------------------------------------------------------*/
typedef enum mh_chk {
    MH_CHK_NONE = 0,        /* Ignore exception group membership       */
    MH_CHK_PLUS,            /* Require membership in group             */
    MH_CHK_MINUS            /* Disallow membership in group            */
} MH_CHK;


/*----------------------------------------------------------------------\
| mh_disp                                                               |
|   What we'll do with a particular field.                              |
\----------------------------------------------------------------------*/
typedef enum mh_disp {
    MH_DSP_NONE = 0,        /* Nothing decided about this field yet    */
    MH_DSP_OUTPUT,          /* Field will be written to output record  */
    MH_DSP_COMBINE,         /* Field is only used in a combination     */
    MH_DSP_COMPLETE,        /* Record has been output, done processing */
    MH_DSP_ERROR            /* Bad field.  Don't process it.           */
} MH_DISP;


/************************************************************************
*                             Structures                                *
************************************************************************/

/*----------------------------------------------------------------------\
| mh_strbuf                                                             |
|   Control structure for a string buffer.                              |
|   Buffer grows as needed, but no deletions (very simple).             |
\----------------------------------------------------------------------*/
typedef struct mh_strbuf {
    char   *bufp;       /* Ptr to buffer                               */
    size_t buflen;      /* Amount of space, not necessarily all used   */
    char   *nextp;      /* Ptr within bufp to place to put next        */
} MH_STRBUF;


/*----------------------------------------------------------------------\
| mh_excp                                                               |
|   Table of MeSH headings for which exception processing is required.  |
|   Table is sorted by heading for binary searching.                    |
\----------------------------------------------------------------------*/
typedef struct mh_excp {
    int           field_id; /* Source field of heading                 */
    int           sf_id;    /* Source subfield, 'a' or 'x'             */
    MH_GRP        group_id; /* Exception group of which this is a part */
    unsigned char *datap;   /* Pointer to sf data in string buffer     */
    size_t        datalen;  /* Length of data.  No null terminator     */
} MH_EXCP;


/*----------------------------------------------------------------------\
| mh_sf                                                                 |
|   Subfield data for one subfield of a field.                          |
\----------------------------------------------------------------------*/
typedef struct mh_sf {
    MH_GRP        excp_grp;     /* Exception group for subfield, if any*/
    char          sf_code;      /* 0 = No subfield here                */
    unsigned char *datap;       /* Ptr to string in S_strbuf           */
    size_t        datalen;      /* Length.  Null term not present      */
} MH_SF;


/*----------------------------------------------------------------------\
| mh_fld                                                                |
|   Stores all information about a single heading.                      |
|   Actual strings are in the S_recstr buffer.                          |
\----------------------------------------------------------------------*/
typedef struct mh_fld {
    int   field_id;          /* Field id of this heading               */
    unsigned char  sf_count; /* Count of subfields in sf               */
    char  keep_indic;        /* True=Retain original indicators        */
    char  no_recombine;      /* 0=recombine ok, else sf or MH_NO_COMBOS*/
    unsigned char indic1;    /* Indicator 1                            */
    unsigned char indic2;    /* Indicator 2                            */
    MH_DISP disposition;     /* What do we do with this field          */
    MH_SF sf[MH_MAX_SFS];    /* Array of sfs, after last has sf_code=0 */
} MH_FLD;


/*----------------------------------------------------------------------\
| mh_lang                                                               |
|   One element of a language table                                     |
\----------------------------------------------------------------------*/
typedef struct mh_lang {
    char   abbrev[4];   /* 3 char abbreviation, null terminated        */
    char   *langp;      /* Ptr to full spelling of lang, in S_tblbuf   */
    size_t langlen;     /* Length of string, without null term         */
} MH_LANG;


/************************************************************************
*                                Statics                                *
************************************************************************/

static MH_EXCP S_excp[MH_MAX_EXCP];     /* All exception hdgs, sorted  */
static MH_STRBUF S_tblbuf;              /* Strings for exception hdgs  */
static MH_STRBUF S_tmpbuf;              /* Temp strings for 1 record   */
static MH_FLD S_flds[MH_MAX_HDGS];      /* Fields to process           */
static MH_LANG S_langs[MH_MAX_LANGS];   /* Language table              */
static CM_STAT S_return_code;           /* Retcode for entire process  */
static int S_excp_count;                /* Num entries in except table */
static int S_lang_count;                /* Num languages in S_langs    */
static int S_mhfld_count;               /* Num mh_fld structs used     */
static int S_dupmesh_warn;              /* To communicate with sort    */


/************************************************************************
*                   Prototypes for internal functions                   *
************************************************************************/

static char   *store_strbuf      (MH_STRBUF *, char *, size_t);
static void   clear_strbuf       (MH_STRBUF *);
static void   load_mesh_excp     (char *);
static void   load_lang_tbl      (char *);
static int    compare_lang       (const void *, const void *);

static int    mrule_ok_fields    (void);
static void   mrule_no_650s      (void);
static void   mrule_chk_age650   (void);
static void   mrule_case_report  (void);
static void   mrule_geographic   (void);
static void   mrule_no_9n_v      (void);
static void   mrule_forms        (MARCP);
static void   mrule_dict         (MARCP);
static void   mrule_non_dict     (void);
static void   mrule_indic2_2     (void);
static void   mrule_drop9        (void);
static void   output_nonmesh_655s(void);
static void   mrule_end_period   (void);
static int    is_mesh            (MH_FLD *);

static int    mesh_input_all     (MARCP);
static MH_FLD *mesh_extract      (MARCP);
static void   mesh_combine       (int, MH_GRP, unsigned char *, size_t,
                                  MH_CHK, MH_GRP, int, int);
static MH_FLD *mesh_duplicate    (MH_FLD *);
static MH_FLD *mesh_new_mhfld    (void);
static int    mesh_add_subfield  (MH_FLD *, int, MH_GRP, unsigned char *,
                                  size_t);
static MH_SF  *mesh_find_subfield(MH_FLD *, int, int *, char *);
static void   mesh_del_subfield  (MH_FLD *, int);
static int    mesh_find_excp     (MH_FLD *, MH_GRP);
static int    mesh_excp_lookup   (MH_FLD *);
static int    mesh_excp_compare  (const void *, const void *);
static int    mesh_sort_compare  (const void *, const void *);
static void   mesh_output_all    (MARCP);
static int    mesh_output        (MARCP, MH_FLD *);

static int    dumpmeshtbls       (void);


/*----------------------------------------------------------------------\
|                                                                       |
|                                                                       |
\----------------------------------------------------------------------*/


/************************************************************************
* cmp_Mesh ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Process all MeSH fields in an NLM internal MARC format          *
*       record to conform to requirements for sending the records       *
*       to the Library of Congress.                                     *
*                                                                       *
*       The principal effort consists in "recombining" headings         *
*       which are stored as separate fields in NLM internal marc        *
*       but which need to be combined into multiple subfields of        *
*       one field in standard marc, as specified in separate            *
*       requirements documentation.                                     *
*                                                                       *
*       The basic algorithm here is:                                    *
*                                                                       *
*           Walk through the input record, constructing an mh_fld       *
*           structure for each mesh field (650, 651, 655).              *
*                                                                       *
*           For each recombination or other processing rule:            *
*                                                                       *
*               Execute the routine which implements the rule.          *
*                                                                       *
*           Output all final mesh fields to the output marc record.     *
*                                                                       *
*       All of the physical data manipulation is done in a set          *
*       of internal mesh_ routines which process mh_fld structures.     *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       This routine should be called only once, as a record prep       *
*       or post process, or as a field prep on 650, 651, or 655.        *
*                                                                       *
*       Standard processing of 650, 651 and 655 must be blocked         *
*       via a "prep donefld" on each of those fields.                   *
*                                                                       *
*   ASSUMPTIONS                                                         *
*    1. A file of headings requiring exceptional processing must be     *
*       available in the current directory with the name                *
*       "meshexcp.tbl".  See load_mesh_excp() for format.               *
*                                                                       *
*    2. A file of language abbreviations->expansions must be available  *
*       in the current directory with the name "language.tbl".  See     *
*       load_lang_tbl() for format.                                     *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
************************************************************************/

CM_STAT cmp_Mesh (
    CM_PROC_PARMS *pp               /* Pointer to parameter struct  */
) {
    int stat;                       /* Return from lower level      */
    static int s_first_time = 1;    /* Force load of tbls on 1st rec*/

    /*=========================================================================*/
    /* For debugging purposes...                                               */
    /*=========================================================================*/
    unsigned char *uip;
    size_t         ui_len;
    /*=========================================================================*/
    /* For debugging purposes...                                               */
    /*=========================================================================*/


    /* First time through, load exception and language tables */
    if (s_first_time) {

        /* Functions will abort if error occurs */
        load_mesh_excp ("meshexcp.tbl");
        load_lang_tbl  ("language.tbl");

        if (getenv ("MESHTEST"))
            dumpmeshtbls ();

        s_first_time = 0;
    }


    /*=========================================================================*/
    /* For debugging purposes...                                               */
    /*=========================================================================*/
    cmp_buf_find(pp, "ui", &uip, &ui_len);

    if(strncmp((char *)uip,"9803041",7)==0)
      uip=uip;
    /*=========================================================================*/
    /* For debugging purposes...                                               */
    /*=========================================================================*/


    /* Reset strings temp buffer and field count from last record */
    clear_strbuf (&S_tmpbuf);
    S_mhfld_count = 0;

    /* Default return code is everything is okay
     * Lower level routine can kill record by changing this to
     *   CM_STAT_KILL_RECORD.
     */
    S_return_code = CM_STAT_OK;

    /* Find all mesh fields, filling in the S_flds array for them
     * Also looks up each subfield in the exception table
     */
    if ((stat = mesh_input_all (pp->inmp)) != 0)
        cm_error (CM_ERROR, "cmp_mesh: %d finding mesh fields, not all "
                            "fields processed.", stat);

    /* First rule is to find 650 fields which are ok to output
     * Most of the following rules only apply if there are some.
     */
    if (mrule_ok_fields ()) {

        /* Process 650's that have Age headings
         * Some will have status changed from output to something else
         *   and headings will be attached as subheadings to other
         *   fields.
         */
        mrule_chk_age650 ();

        /* If there is a case report heading, relabel the internal
         *   copy of the field so that it will be treated as if
         *   it were a 655 form heading.
         */
        mrule_case_report ();

        /* Process geographic headings, converting them to $z subheads
         *   except where exceptions have been noted in the table.
         */
        mrule_geographic ();

        /* If an output 650 field has $9='n', mark it to block recombination
         *   with $v form subheadings.
         */
        mrule_no_9n_v ();

        /* Process form headings, converting to $v where appropriate
         * This will handle exceptional cases and call mrule_dict()
         *   to handle "Dictionary", "Encyclopedias" and etc. headings.
         */
        mrule_forms (pp->inmp);

        /* Set indicator 2='2' in all cases except those for which
         *   "keep_indic" exceptions have been flagged.
         */
        mrule_indic2_2 ();
    }

    else {
        /* If no 650 exists, we specially prepare any 651's and/or
         *   655's for output.
         */
        mrule_no_650s ();
    }

    /* Delete all $9 subfields from the fields */
    mrule_drop9 ();

    /* Output non-mesh 655's */
    output_nonmesh_655s ();

    /* Ensure every field is terminated with a period */
    mrule_end_period ();

    /* Write everything to the output record */
    mesh_output_all (pp->outmp);

    return S_return_code;

} /* cmp_mesh */


/************************************************************************
*************************************************************************
**                                                                     **
**                        PROCESSING RULES                             **
**                                                                     **
** Each of these routines implements a rule, or part of a rule, from   **
** the catalog department's MeSH output processing specifications.     **
**                                                                     **
*************************************************************************
************************************************************************/


/************************************************************************
* mrule_ok_fields ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Make a pass through all fields, building a list of 650          *
*       headings which are our initial output fields.                   *
*                                                                       *
*       These are mesh 650 fields which have no exception conditions    *
*       which suppress output. though they may be affected later        *
*       by other fields.                                                *
*                                                                       *
*   PASS                                                                *
*       Void.  Uses file wide statics.                                  *
*                                                                       *
*   RETURN                                                              *
*       Number of simple 650 fields found                               *
************************************************************************/

static int mrule_ok_fields ()
{
    int    i,       /* Loop counter             */
           count;   /* Number found             */
    MH_FLD *meshp;  /* Ptr to current field     */


    count = 0;

    /* Examine each field */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {

        /* Field initiallly okay to output if it's a 650.
         * Exceptions to this rule get noted later, in
         *   subsequent passes through the data
         */
        if (meshp->field_id == 650) {
            meshp->disposition = MH_DSP_OUTPUT;
            ++count;
        }
        ++meshp;
    }

    return count;

} /* mrule_ok_fields */


/************************************************************************
* mrule_no_650s ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Called only when there are no 650 fields in a record.           *
*                                                                       *
*       If a record contains no 650 fields, then if there are any       *
*       651 or 655 fields, they will be output in some way directly     *
*       instead of being combined with 650 fields.                      *
*                                                                       *
*       If there are 651's, each one is output with normalized          *
*       indicators and final punctuation.                               *
*                                                                       *
*       If there are any 655's then:                                    *
*          If there are one or more 651's:                              *
*              Combine the 655's as subfield 'v' of the 651's.          *
*          Else                                                         *
*              Output the field as a 655, first checking that it        *
*              meets our requirements.                                  *
*                                                                       *
*       If mrule_no_650s() is invoked, most other processing is         *
*       skipped since most other rules have to do with processing       *
*       650s.                                                           *
*                                                                       *
*   PASS                                                                *
*       Void.  Uses file wide statics.                                  *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_no_650s ()
{
    int    i,           /* Loop counter             */
           sf_a_pos,    /* Position of subfield a   */
           sfpos,       /* Ordinal sf pos           */
           max_chg,     /* Max fields to change     */
           have_651,    /* True=At least 1 651 exist*/
           field_ok;    /* True=Passes validation   */
    MH_FLD *meshp;      /* Ptr to current field     */


    /* Look for 651 fields */
    have_651 = 0;
    meshp    = S_flds;
    max_chg  = S_mhfld_count;

    for (i=0; i<S_mhfld_count; i++) {
        if (meshp->field_id == 651) {

            /* Remember that we have them */
            have_651 = 1;

            /* Normalize indicators */
            meshp->indic1 = ' ';
            meshp->indic2 = '2';

            /* Force output of this field */
            meshp->disposition = MH_DSP_OUTPUT;
        }
        ++meshp;
    }

    /* Look for and process all 655 fields */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {
        if (meshp->field_id == 655) {

            /* Find $a.  Almost certainly the first subfield.
             * This subfield is required no matter what we do with
             *   the 655.
             */
            sf_a_pos = 0;
            if (!mesh_find_subfield (meshp, 'a', &sf_a_pos, NULL) ||
                    meshp->sf[sfpos].datalen == 0)
                cm_error (CM_ERROR, "cmp_mesh:mrule_no_650s: "
                          "Subfield 'a' missing or empty");

            /* If no 651s in the record */
            else if (!have_651) {

                /* Does this field meet our high standards? */
                field_ok = 1;

                /* 1st indicator blank */
                if (meshp->indic1 != ' ') {
                    cm_error (CM_ERROR, "cmp_mesh:mrule_no_650s: "
                              "Expected indic1 = blank not found");
                    field_ok = 0;
                }

                /* if 2nd indicator contains '7' */
                if (meshp->indic2 == '7') {

		  /* then $2 must exist and have data */
		  sfpos = 0;
		  if (!mesh_find_subfield (meshp, '2', &sfpos, NULL) ||
		      meshp->sf[sfpos].datalen == 0) {
                    cm_error (CM_ERROR, "cmp_mesh:mrule_no_650s: "
                              "Subfield '2' missing or empty with indic2 = '7'");
                    field_ok = 0;
		  }
                }
		/* otherwise 2nd indicator must contain '2' */
		else {
		  /* and $2 must NOT exist */
		  if (meshp->indic2 == '2') {
		    sfpos = 0;
		    if (mesh_find_subfield (meshp, '2', &sfpos, NULL) &&
			meshp->sf[sfpos].datalen != 0) {
		      cm_error (CM_ERROR, "cmp_mesh:mrule_no_650s: "
				"Subfield '2' present with indic2 = '2'");
		      field_ok = 0;
		    }
		  }
		  else {
                    cm_error (CM_ERROR, "cmp_mesh:mrule_no_650s: "
                              "Indic2 not '7' or '2'");
                    field_ok = 0;
		  }
		}


                /* Only if everything okay will we output this field */
                if (field_ok)
                    meshp->disposition = MH_DSP_OUTPUT;
            }

            else {
	      /* There is at least one 651 in the record.
	       * Make the 655 combine with each 651 as a $v subfield
	       * only if it is mesh.
	       */

	      if(is_mesh(meshp)) {
                /* First normalize indicators */
                meshp->indic1 = ' ';
                meshp->indic2 = '2';

                /* Combine $a with each 651 */
                mesh_combine ('v', MH_GRP_NONE, meshp->sf[sf_a_pos].datap,
                    meshp->sf[sf_a_pos].datalen, 0, MH_GRP_NONE, max_chg, 1);
	      }
            }
        }

        ++meshp;
    }
} /* mrule_no_650s */


/************************************************************************
* mrule_chk_age650 ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Analyze the 650 fields which contain Age650 headings.           *
*                                                                       *
*       650 fields with age headings are processed in one of three      *
*       ways depending upon the value of the indicators, and            *
*       of the values of $9 subfields of other fields.                  *
*                                                                       *
*       This is the most complex of the special purpose routines.       *
*       To make it work as simply as possible, using as much common     *
*       software from the other rules as possible, we employ some       *
*       ugly kludges.                                                   *
*                                                                       *
*   PASS                                                                *
*       Void.  Uses file wide statics.                                  *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_chk_age650 ()
{
    int    i, j,        /* Loop counters                */
           have_age,    /* True=field has age650 head   */
           have_nine_a, /* True=field has $9='a'        */
           have_combine,/* True=have age650, @1=2, $9=a */
           sfpos,       /* Ordinal subfield position    */
           max_chg;     /* Max fields to change         */
    MH_FLD *meshp;      /* Ptr to field we're checking  */
    MH_SF  *sfp;        /* Ptr to sf containing age head*/


    /* Nothing found to play with yet */
    have_age     = 0;
    have_nine_a  = 0;
    have_combine = 0;

    /* Make a pass through the record, checking for fields
     *   on the age650 list, and for fields with $9='a'.
     * Just trying to find if there are any.
     */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {
        if (meshp->field_id == 650 && meshp->sf[0].excp_grp == MH_GRP_AGE)
            have_age = 1;

        else {
            /* Else is there a $9=='a' in the field */
            sfpos = 1;
            if (mesh_find_subfield (meshp, '9', &sfpos, "a"))
                have_nine_a = 1;
        }
        ++meshp;
    }

    /* If any fields have age650 headings, analyze further */
    if (have_age) {

        /* Re-examine all age650's */
        meshp = S_flds;
        for (i=0; i<S_mhfld_count; i++) {

            if (meshp->field_id == 650 &&
                meshp->sf[0].excp_grp == MH_GRP_AGE) {

                /* Four cases in requirements document:
                 *   1: Indicator 1=='2' and there is a $x in field
                 *   2: Indicator 1=='2' and have_nine_a
                 *   3: Indicator 1=='2' and NOT have_nine_a
                 *   4: Indicator 1!='2'
                 */

                if (meshp->indic1 == '2') {

                    /* Case 1: if there's a $x in field then
                     *   Keep field as 650
                     *   Don't add any sfs to it
                     *   Retain original indicators
                     */
                    if (mesh_find_subfield (meshp, 'x', &sfpos, NULL)) {
                        meshp->disposition  = MH_DSP_OUTPUT;
                        meshp->keep_indic   = 1;
                        /* meshp->no_recombine = MH_NO_COMBOS; */
                    }

                    /* Case 2: suppress output, but use in recombinations */
                    else if (have_nine_a) {
                        meshp->disposition = MH_DSP_COMBINE;
                        meshp->sf[0].excp_grp = MH_GRP_AGE_SOURCE;
                        have_combine = 1;
                    }

                    /* Case 3: don't do anything with this field */
                    else
                        meshp->disposition = MH_DSP_COMPLETE;
                }

                else {
                    /* Case 4: Treat as for case 1 */
                    meshp->disposition  = MH_DSP_OUTPUT;
                    meshp->keep_indic   = 1;
                    /* meshp->no_recombine = MH_NO_COMBOS; */
                }
            }
            ++meshp;
        }
    }

    if (have_combine) {

        /* Re-examine all fields with $9='a'
         * If there were any fields with age650 & @1='2' and
         *   others with $9='a' then find all with $9='a' and
         *   mark them to retain their indicators and to receive
         *   recombination subfields.
         */
        meshp = S_flds;
        for (i=0; i<S_mhfld_count; i++) {
            if (meshp->field_id == 650) {
                sfpos = 1;
                if (mesh_find_subfield (meshp, '9', &sfpos, "a")) {
                    meshp->keep_indic = 1;
                    meshp->sf[0].excp_grp = MH_GRP_AGE_TARGET;
                }
            }
            ++meshp;
        }

        /* Perform the recombinations.
         * This combines all age source headings with all age targets.
         * We need one more kludge here:
         *    mesh_combine() can be told to duplicate headings if it
         *    finds the same subfield already in the field.
         *    But there may be other 'x' subfields in the field which
         *    aren't age headings.
         *    So, rather than kludge mesh_combine just for this, we
         *    kludge this by adding an illegal '|' subfield, and then
         *    converting them all back to 'x' at the end.
         */
        meshp   = S_flds;
        max_chg = S_mhfld_count;
        for (i=0; i<S_mhfld_count; i++) {
            if (meshp->sf[0].excp_grp == MH_GRP_AGE_SOURCE) {
                sfp = &meshp->sf[0];
                mesh_combine ('|', sfp->excp_grp, sfp->datap, sfp->datalen,
                              MH_CHK_PLUS, MH_GRP_AGE_TARGET, max_chg, 1);
            }
            ++meshp;
        }

        /* Final kludge completion is to convert the '|' sfs back to 'x' */
        meshp = S_flds;
        for (i=0; i<S_mhfld_count; i++) {
            for (j=1; j<meshp->sf_count; j++) {
                if (meshp->sf[j].sf_code == '|')
                    meshp->sf[j].sf_code = 'x';
            }
            ++meshp;
        }
    }
} /* mrule_chk_age650 */


/************************************************************************
* mrule_case_report ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Treat "Case Report" as if it were a 655 form heading.           *
*                                                                       *
*       We do that by changing its internal field id and disposition.   *
*       This does not affect our input record, just the internal        *
*       mh_fld structures used by cmp_mesh().                           *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_case_report ()
{
    int    i,       /* Loop counter         */
           sfpos;   /* Ordinal sf pos       */
    MH_FLD *meshp;  /* Ptr to current field */


    /* Examine each field */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {
        if (meshp->sf[0].excp_grp == MH_GRP_CASEREP) {

            /* Change internal field identity to be 655
             * This doesn't change the input record.
             */
            meshp->field_id    = 655;
            meshp->disposition = MH_DSP_COMBINE;

            /* Create or replace $2="mesh" to make 655
             *   recombination work.
             */
            sfpos = 1;
            if (mesh_find_subfield (meshp, '2', &sfpos, NULL))
                mesh_del_subfield (meshp, sfpos);
            mesh_add_subfield (meshp, '2', MH_GRP_NONE,
                               (unsigned char *) "mesh", 4);
        }
        ++meshp;
    }
} /* mrule_case_report */


/************************************************************************
* mrule_no_9n_v ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Mark all 650 fields which have $9='n' to prevent recombination  *
*       of those fields with 655's as $v.                               *
*                                                                       *
*   PASS                                                                *
*       Void.  Uses file wide statics.                                  *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_no_9n_v ()
{
    int    i,       /* Loop counter         */
           sfpos;   /* Ordinal sf pos       */
    MH_FLD *meshp;  /* Ptr to current field */


    /* Examine each field */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {
        if (meshp->field_id == 650) {
            sfpos = 1;
            if (mesh_find_subfield (meshp, '9', &sfpos, "n")) {
                /* If field not already blocked, block v's */
                if (meshp->no_recombine == 0)
                    meshp->no_recombine = 'v';
            }
        }
        ++meshp;
    }
} /* mrule_no_9_n */


/************************************************************************
* mrule_geographic ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Combine 651$a geographic headings with most 650's as new        *
*       $z subfield - except under certain circumstances as noted       *
*       in the specs.                                                   *
*                                                                       *
*       Specifically, we don't recombine "United States" with fields    *
*       in the USMED group (Medicare, Medicaid, etc.).                  *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Call after processing most $x and before all $v.  See specs.    *
*                                                                       *
*   PASS                                                                *
*       Void.  Examines all fields.                                     *
*                                                                       *
*   RETURN                                                              *
*       Void.  Errors should not happen and are therefore fatal.        *
************************************************************************/

static void mrule_geographic ()
{
    int   i,        /* Loop counter                     */
          max_chg;  /* Max fields to change             */
    MH_SF *sfp;     /* Ptr to $a in geographic heading  */


    /* See mesh_combine() for explanation of this */
    max_chg = S_mhfld_count;

    /* For each 651 geographic heading in the record */
    for (i=0; i<S_mhfld_count; i++) {
        if (S_flds[i].field_id == 651) {

            /* Already know that $a heading is first subfield
             * Checked it in mesh_extract and rejected field if not
             */
            sfp = &S_flds[i].sf[0];

            /* Add $a as $z on all output 650's which allow
             *   recombinations, and which don't have any
             *   existing $z.  Duplicate if they do.
             * For the special case of heading = "United States"
             *   (in MH_GRP_USMED1), don't add them to any
             *   fields in the Medicare group (USMED).
             */
            if (sfp->excp_grp == MH_GRP_USMED1)
                mesh_combine ('z', sfp->excp_grp, sfp->datap, sfp->datalen,
                              MH_CHK_MINUS, MH_GRP_USMED, max_chg, 1);
            else
                mesh_combine ('z', sfp->excp_grp, sfp->datap, sfp->datalen,
                              MH_CHK_NONE, MH_GRP_NONE, max_chg, 1);
        }
    }
} /* mrule_geographic */


/************************************************************************
* mrule_forms ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Combine 655$a form headings with most 650's as new              *
*       $v subfield - except under certain circumstances as noted       *
*       in the specs.                                                   *
*                                                                       *
*       Exceptions are for:                                             *
*           Statistics                                                  *
*           Legislation                                                 *
*           Publication type/language                                   *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Call after processing most $x and all $v.  See specs.           *
*                                                                       *
*   PASS                                                                *
*       Input marc control.  Passed to mrule_dict if need.              *
*                                                                       *
*   RETURN                                                              *
*       Void.  Errors should not happen and are therefore fatal.        *
************************************************************************/

static void mrule_forms (
    MARCP  inmp         /* Input marc record control */
) {
    int    i,           /* Loop counter              */
           sfpos,       /* Ordinal sf pos            */
           need_lang,   /* True=Add language as $x   */
           max_chg;     /* Max fields to change      */
    MH_FLD *meshp;      /* Ptr to 655 field          */
    MH_SF  *sfp;        /* Ptr to $a in form heading */
    MH_GRP egrp;        /* If we need to check excps */
    MH_CHK echk;        /*  "  "   "   "   "     "   */


    /* No 655's requiring language combinations yet encountered */
    need_lang = 0;

    /* See mesh_combine() for explanation of this */
    max_chg = S_mhfld_count;

    /* For each 655 geographic heading in the record */
    meshp = S_flds;
    for (i=0; i<S_mhfld_count; ++meshp, ++i) {

        if (meshp->field_id == 655) {

            /* No special exceptions found yet */
            egrp = MH_GRP_NONE;
            echk = MH_CHK_NONE;

            /* Only recombine if $2="mesh" OR if indicator2 = '2' */
            sfpos = 1;
            if ((!mesh_find_subfield (meshp, '2', &sfpos, "mesh")) &&
                (meshp->indic2 != '2'))
                continue;

            /* Point to $a subfield */
            sfp = &meshp->sf[0];

            /* Add $a as $v on all output 650's which allow
             *   recombinations, and which don't have any
             *   existing $v.  Duplicate if they do.
             * But first look for special cases.
             */

            /* Statistics exception group */
            if (sfp->excp_grp == MH_GRP_STAT5) {
                egrp = MH_GRP_STAT;
                echk = MH_CHK_MINUS;
            }

            /* Same for legislation group */
            else if (sfp->excp_grp == MH_GRP_LAW5) {
                egrp = MH_GRP_LAW;
                echk = MH_CHK_MINUS;
            }

            /* If the publication type was on our DICT list
             *   set a flag telling us to perform language
             *   combinations on those fields.
             */
            else if (sfp->excp_grp == MH_GRP_DICT)
                need_lang = 1;

            /* Make the combination */
            mesh_combine ('v', sfp->excp_grp, sfp->datap, sfp->datalen,
                              echk, egrp, max_chg, 1);
        }
    }

    /* If we got any members of the DICT group, add language combinations */
    if (need_lang)
        mrule_dict (inmp);

} /* mrule_forms */


/************************************************************************
* mrule_dict ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       If a record contains any of the dictionary group 655$a's        *
*       each of which has already been combined with 650's as $v        *
*       (in mrule_forms()), then combine language strings as            *
*       new $x subfields after the $v's.                                *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Call after processing most $v.                                  *
*                                                                       *
*   PASS                                                                *
*       Pointer to input marc control, providing access to language.    *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

#define MAX_LANGS_1REC 50   /* More langs is one rec is absurd */

static void mrule_dict (
    MARCP inmp              /* Input marc record        */
) {
    int   i,                /* Loop counter             */
          socc,             /* Subfield occurrence num  */
          max_chg,          /* Max fields to change     */
          no_specific_lang, /* True=Found 'mul' or 'und'*/
          found_lang_count, /* Num langs found          */
          stat;             /* Return from marc funcs   */
    MH_LANG lang,           /* For searching lang table */
          *foundp,          /* Ptr to found lang entry  */
          *found_langp[MAX_LANGS_1REC]; /* Ptrs to langs*/
    unsigned char *datap;   /* Ptr to data in field     */
    size_t        datalen;  /* Length at datap          */


    /* See mesh_combine() for explanation of this */
    max_chg = S_mhfld_count;

    /* Haven't found any non-specific languages ("und" or "mul") yet */
    no_specific_lang = 0;

    /* Haven't found any real languages yet */
    found_lang_count = 0;

    /* Languages in NLM marc are stored as repeating $a subfields
     *   of a single 041 field.
     * Get each one and convert it from a 3 char abbreviation to
     *   a spelled out language name using the table loaded from disk.
     */
    if ((stat = marc_get_field (inmp, 41, 0, &datap, &datalen)) == 0) {

        socc=0;
        while ((stat = marc_get_subfield (inmp, 'a', socc,
                       &datap, &datalen)) == 0) {

            ++socc;

            /* Validate */
            if (datalen != 3) {
                /* Kill it */
                cm_error (CM_ERROR, "041 language abbreviation %.*s not "
                          "expected 3 char length", datalen, datap);
                S_return_code = CM_STAT_KILL_RECORD;
                break;
            }

            /* Skip over "und" (Undetermined) and "mul" (Multiple).
             * It would be more efficient to take them out of the table
             *   but it's safer to do it here, since someone might
             *   update the table and put them back in.
             */
            if (  !strncmp ((char *) datap, "und", 3) ||
                  !strncmp ((char *) datap, "mul", 3)  ) {

                /* If there are some other languages, we'll have
                 *   to create a version of the heading without
                 *   a language recombination to stand for the und or mul.
                 */
                no_specific_lang = 1;

                continue;
            }

            /* Look it up in our language table */
            memcpy (&lang.abbrev, datap, 3);
            lang.abbrev[3] = '\0';
            if ((foundp = bsearch (&lang, S_langs, S_lang_count,
                          sizeof(MH_LANG), compare_lang)) == NULL) {
                /* Kill record */
                cm_error (CM_ERROR, "Language %.3s not found in table", datap);
                S_return_code = CM_STAT_KILL_RECORD;
                break;
            }

            else
                /* Save ptr to record with full name of the language */
                found_langp[found_lang_count++] = foundp;
        }
    }

    /* Check for marc structure errors on get field or subfield */
    if (stat < 0)
        cm_error (CM_FATAL, "mrule_dict: Error %d getting language");

    /* If we got any languages to recombine */
    if (found_lang_count > 0) {

        /* If there were any non specific languages, then we
         *   have to create a version of the heading without
         *   any language in it.
         * To do this, we call mrule_non_dict to duplicate
         *   the heading into a field which has no_recombine
         *   set to prevent a language $x from being appended.
         */
        if (no_specific_lang == 1)
            mrule_non_dict ();

        /* Add the expanded language strings as new subfield $x
         *   to all and only those fields with DICT exception.
         * Multiple languages force duplication of fields.
         * This is done once for each language found.
         */
        for (i=0; i<found_lang_count; i++) {
            foundp = found_langp[i];
            mesh_combine ('x', MH_GRP_NONE, (unsigned char *) foundp->langp,
                  foundp->langlen, MH_CHK_PLUS, MH_GRP_DICT, max_chg, 1);
        }
    }
} /* mrule_dict */


/************************************************************************
* mrule_non_dict ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       For records with dictionary group 655's, we must create         *
*       a version of each target field with a language $x after it.     *
*       That's done in mrule_dict().                                    *
*                                                                       *
*       In addition, if there is a non-specific language designator,    *
*       we also need a version of the target field without a            *
*       language $x in it.  For example, assume:                        *
*           041 $aeng $afre $aspa $und                                  *
*           650 $aChemistry                                             *
*           655 $aDictionary                                            *
*       Then we must produce:                                           *
*           650 $aChemistry $vDictionary                                *
*           650 $aChemistry $vDictionary $xEnglish                      *
*           650 $aChemistry $vDictionary $xFrench                       *
*           650 $aChemistry $vDictionary $xSpanish                      *
*                                                                       *
*       This routine creates the plain 650, the one that doesn't        *
*       re-combine with language.                                       *
*                                                                       *
*       It works by finding the headings which will re-combine          *
*       and duplicating them with a no_recombine flag to prevent        *
*       the duplicate from getting a language appendage.                *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Called from mrule_dict() after recognizing that language        *
*       re-combine(s) will take place, but before making any            *
*       re-combines.  Must only be called once per record.              *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_non_dict ()
{
    int    i,           /* Loop counter         */
           max_dup;     /* Max fields to examine*/
    MH_FLD *meshp,      /* Ptr to each field    */
           *newmeshp;   /* Ptr to duplicate fld */


    /* Loop through all fields, but not the duplicates we're adding */
    meshp   = S_flds;
    max_dup = S_mhfld_count;
    for (i=0; i<max_dup; i++) {

        /* Only for fields we'll output which allow recombinations */
        if ( meshp->disposition == MH_DSP_OUTPUT &&
             meshp->no_recombine != MH_NO_COMBOS ) {

            /* If the field has a subfield in the language recombine list */
            if (mesh_find_excp (meshp, MH_GRP_DICT)) {

                /* Duplicate the field */
                if ((newmeshp = mesh_duplicate (meshp)) != NULL)

                    /* Make this one non-recombinable with language */
                    newmeshp->no_recombine = MH_NO_COMBOS;
            }
        }

        ++meshp;
    }
} /* mrule_non_dict */


/************************************************************************
* mrule_indic2_2 ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Set indicator 2 to the value '2', in all fields except those    *
*       for for which we have specifically set keep_indic.              *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       This should only be called after any function which can         *
*       set the keep_indic flag.                                        *
*                                                                       *
*   PASS                                                                *
*       Void.  Examines all fields.                                     *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_indic2_2 ()
{
    MH_FLD *meshp;          /* Ptr to field     */
    int    i;               /* Loop counter     */


    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {

        /* Only do this for fields to be output with no keep_indic flag */
        if (meshp->disposition == MH_DSP_OUTPUT && !meshp->keep_indic)
            meshp->indic2 = '2';

        ++meshp;
    }
} /* mrule_indic2_2 */


/************************************************************************
* mrule_drop9 ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Delete subfield 9 from all fields to be output.                 *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Subfield 9 is already checked where needed.                     *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mrule_drop9 ()
{
    MH_FLD *meshp;          /* Ptr to field     */
    int    i,               /* Loop counter     */
           sfpos;           /* Ordinal sf pos   */


    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {

        /* Only do this for fields to be output which have $9 */
        if (meshp->disposition == MH_DSP_OUTPUT) {
            sfpos = 1;
            if (mesh_find_subfield (meshp, '9', &sfpos, NULL))
                mesh_del_subfield (meshp, sfpos);
        }

        ++meshp;
    }
} /* mrule_drop9 */


/************************************************************************
* output_nonmesh_655s ()                                                *
*                                                                       *
*   DEFINITION                                                          *
*       Output 655 that are not mesh                                    *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       655 have been processed as needed. This is just to make sure    *
*       those that have $2 != mesh are output as 655's                  *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void output_nonmesh_655s ()
{
    MH_FLD *meshp;          /* Ptr to field     */
    int    i;               /* Loop counter     */

    meshp = S_flds;

    for (i=0; i<S_mhfld_count; i++) {

      /* Only do this for 655 fields */
        if (meshp->field_id == 655) {

	  /* If it's not Mesh, flag it for output */
	  if(!is_mesh(meshp))
	    meshp->disposition = MH_DSP_OUTPUT;
	}

        ++meshp;
    }
} /* output_nonmesh_655s */


/************************************************************************
* is_mesh (MH_FLD* )                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       determine whether field is 'mesh' field                         *
*                                                                       *
*   ASSUMPTIONS                                                         *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static int is_mesh (
    MH_FLD *meshp
) {
    int sfpos;           /* Ordinal sf pos   */

    sfpos = 0;

    /* We are mesh if... */

    /* @2='7' & $2="mesh" */
    if ((meshp->indic2 == '7') &&
	(mesh_find_subfield(meshp, '2', &sfpos, "mesh")))
      return 1;

    else
      /* OR @2='2' and $2 does not exist */
      if ((meshp->indic2 == '2') &&
	  (!mesh_find_subfield(meshp, '2', &sfpos, NULL)))
	return 1;

      else
	/* OTHERWISE, we are not mesh... */
	return 0;

} /* is_mesh */


/************************************************************************
* mrule_end_period ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       If the last subfield of an output mesh field is not             *
*       terminated with a period, add a period.                         *
*                                                                       *
*       We are not allowed to modify the input marc record.  Since      *
*       the data pointers in an mh_fld struct generally point into      *
*       the input record, we copy the data out to a temporary           *
*       buffer before terminating it.                                   *
*                                                                       *
*       Loops through all output fields.                                *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       This should only be called after the field is complete          *
*       and ready to output.                                            *
*                                                                       *
*       Any modifications to the field after this point will            *
*       probably add or delete the last subfield, invalidating          *
*       what has been done here.                                        *
*                                                                       *
*   PASS                                                                *
*       Void.  Examines all fields.                                     *
*                                                                       *
*   RETURN                                                              *
*       Void.  Errors should not happen and are therefore fatal.        *
************************************************************************/

static void mrule_end_period ()
{
    MH_FLD *meshp;      /* Ptr to field     */
    MH_SF  *sfp;        /* Last sf in meshp */
    int    i;           /* Loop counter     */

    meshp = S_flds;
    for (i=0; i<S_mhfld_count; i++) {

        /* Only do this for fields to be output */
        if (meshp->disposition == MH_DSP_OUTPUT) {

            /* Find last subfield */
            sfp = &meshp->sf[meshp->sf_count-1];

            /* If it's a $2, it won't be output, so we
             *   want the period on the subfield before that.
             */
            if (sfp->sf_code == '2') {
                --sfp;
                if (meshp->sf_count == 1) {

                    /* Cancel output of this field and report error */
                    meshp->disposition = MH_DSP_NONE;
                    cm_error (CM_ERROR, "Field %d has $2, but no other "
                              "subfields");

                    /* Don't leave pointer dangling at invalid object */
                    ++sfp;
                }
            }

            /* Check (logical) last subfield for terminating period */
            if ((sfp->datap[sfp->datalen - 1] != ')') &&
		(sfp->datap[sfp->datalen - 1] != '.')) {

	      /* We want to trim spaces off the end... */
	      for(;sfp->datap[sfp->datalen-1] == ' ';sfp->datalen--);

	      /* Copy data out to temp buf, adding one byte to copy
	       * Added byte is whatever is there, it will be
	       *   overwritten by '.'
	       * Subfield will point to the copy from now on
	       */
	      sfp->datap = (unsigned char *) store_strbuf (&S_tmpbuf,
					   (char *) sfp->datap, ++sfp->datalen);

	      /* Add the terminating period */
	      *(sfp->datap + (sfp->datalen - 1)) = '.';
            }
        }

        ++meshp;
    }
} /* mrule_end_period */


/************************************************************************
*************************************************************************
**                                                                     **
**                      GENERIC MH_FLD FUNCTIONS                       **
**                                                                     **
** These functions manipulate a struct mh_fld object, loading,         **
** editing, comparing, etc.                                            **
**                                                                     **
*************************************************************************
************************************************************************/

/************************************************************************
* mesh_input_all ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Make a pass through all 650, 651 and 655 headings, building     *
*       our initial lists of fields to be processed.                    *
*                                                                       *
*       Information from every mesh field is extracted into an array    *
*       of mh_fld structures, from which it can more easily be          *
*       addressed and manipulated.                                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to marc control for the input marc record.              *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/

static int mesh_input_all (
    MARCP         inmp      /* input marc record*/
) {
    MH_FLD        *meshp;   /* Ptr to field ctl */
    int           fid,      /* Field id         */
                  fpos,     /* Ordinal position */
                  stat;     /* Return from func */
    unsigned char *datap;   /* Pointer to data  */
    size_t        datalen;  /* Field length     */


    /* Walk the record, finding all 650, 651 and 655 fields */
    fpos = 1;
    while ((stat = marc_pos_field (inmp, fpos, &fid, &datap, &datalen)) == 0) {
        if (fid == 650 || fid == 651 || fid == 655) {

            /* Create an mh_fld object for it */
            meshp = mesh_extract (inmp);

            /* Lookup all subfields in the exception table */
            if (meshp)
                mesh_excp_lookup (meshp);
        }
        ++fpos;
    }

    /* Should have ended with this */
    if (stat == MARC_RET_END_REC)
        stat = 0;
    else
        cm_error (CM_ERROR, "mesh_input_all: Error %d processing %dth field "
                  "in input record");

    return stat;

} /* mesh_input_all */


/************************************************************************
* mesh_extract ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Copy info about a mesh field from an input mesh field to        *
*       one of our internal mh_fld structures.                          *
*                                                                       *
*       Done for all 650, 651, and 655 fields.                          *
*                                                                       *
*   PASS                                                                *
*       Pointer for control for marc record from which to copy field.   *
*           Control should be already positioned to the field to        *
*           be copied.                                                  *
*                                                                       *
*   RETURN                                                              *
*       Pointer to filled out mesh struct.                              *
*       Null if failed.                                                 *
*                                                                       *
*       Errors are reported from here.                                  *
*       Marc structure errors are fatal.                                *
************************************************************************/

static MH_FLD *mesh_extract (
    MARCP  marcp            /* Input marc record control        */
) {
    MH_FLD *meshp;          /* Ptr to struct to fill out        */
    int    fid,             /* Field id                         */
           focc,            /* Field occurrence num within id   */
           fpos,            /* Ordinal fld pos in entire rec    */
           sid,             /* Subfield id                      */
           spos,            /* Ordinal subfield position        */
           stat;            /* Return code from called funcs    */
    unsigned char *datap;   /* Pointer to data                  */
    size_t datalen;         /* Field length                     */


    /* Get current field id, occ, pos, for error reporting if need */
    if ((stat = marc_cur_field (marcp, &fid, &focc, &fpos)) != 0)
        cm_error (CM_FATAL, "mesh_extract: Error %d on marc_cur_field "
                 " - can't happen", stat);

    /* Get the next available structure to fill out */
    if ((meshp = mesh_new_mhfld ()) == NULL) {
        cm_error (CM_ERROR, "mesh_extract: No room for another mesh field\n"
                  "field id=%d, occurrence=%d - shouldn't happen", fid, focc);
        return NULL;
    }

    /* Fill out info for entire field */
    meshp->field_id = fid;
    if ((stat = marc_get_indic (marcp, 1,
                (unsigned char *) &meshp->indic1)) == 0)
        stat = marc_get_indic (marcp, 2, (unsigned char *) &meshp->indic2);
    if (stat)
        cm_error (CM_FATAL, "mesh_extract: Error %d getting indics "
                  "- fieldid=%d, occurrence=%d", fid, focc);

    /* Add each subfield, starting with first varlen sf after indics */
    for (spos=2; ; spos++) {

        /* Get it from marc record */
        if ((stat = marc_pos_subfield (marcp, spos, &sid, &datap, &datalen))
                 < 0)
            cm_error (CM_FATAL, "mesh_extract: Error %d getting subfield "
                 "pos %d in field=%d, occurrence=%d", stat, spos, fid, focc);

        /* No more subfields? */
        if (stat != 0)
            break;

        /* Trim trailing blanks off all subfields */
	for(;(datalen>0)&&(datap[datalen-1]==' ');datalen--);

	/* Add it to our mh_fld structure */
	if(datalen)
	  if (mesh_add_subfield (meshp, sid, MH_GRP_NONE, datap, datalen) != 0) {
            cm_error (CM_ERROR, "mesh_extract: Exceeded max subfield count "
                      "with sf=%d, fid=%d, focc=%d - truncating field",
                      sid, fid, focc);
            break;
	  }
    }

    /* First subfield must be $a.  Check that */
    if (meshp->sf[0].sf_code != 'a') {
        cm_error (CM_ERROR, "mesh_extract: 1st sf in field %d has sfcode '%c'"
                  " - field skipped", meshp->field_id, meshp->sf[0].sf_code);

        /* Prevent further processing of this field */
        meshp->field_id    = -1;
        meshp->disposition = MH_DSP_ERROR;
    }

    return meshp;

} /* mesh_extract */


/************************************************************************
* mesh_combine ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Combine a subfield with all of the existing fields for which    *
*       it is legal to perform the combination.                         *
*                                                                       *
*       Called to add $x, $z, or $v.                                    *
*                                                                       *
*   PASS                                                                *
*       New subfield id.                                                *
*       Exception group for new subfield.                               *
*       Pointer to data to be added.                                    *
*       Length of data.                                                 *
*       Check for exception group:                                      *
*           MH_CHK_PLUS  = Field must be part of exception group.       *
*           MH_CHK_MINUS = Field must NOT be in exception group.        *
*           MH_CHK_NONE  = Exception group is irrelevant.               *
*       Exception group to check.                                       *
*           Ignored if MH_CHK_NONE.                                     *
*       Max number of fields to change.                                 *
*           The caller must control how many fields get changed.        *
*           If this isn't done right, a combinatorial explosion will    *
*           occur as follows:                                           *
*               Assume 6 650 subjects and 6 651 countries.              *
*               Country 1 added to 6 subjects.                          *
*               Country 2 added to 6 new subjects, making 12.           *
*               Country 3 added to 12 subjects, making 24.              *
*                  But it should only have been added to 6.             *
*               Country 4 added to 24 subjects, making 48.              *
*                  But it should only have been added to 6.             *
*               Country 5 added to 48 subjects, making 96.              *
*                  But it should only have been added to 6.             *
*               etc.                                                    *
*           Typically the caller passes the value of S_mhfld_count      *
*           as it was at the time he bagan processing.                  *
*       Duplicate field indicator, values are:                          *
*           True  = Duplicate field if new sf is already in the field,  *
*                   and it's the LAST subfield in the field.            *
*                   This keeps us from adding some type of component    *
*                   to a field twice, e.g., two geographic subdivisions,*
*                   two form subdivisions, or two language subdivisions.*
*           False = Don't duplicate field.                              *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mesh_combine (
    int           sfid,         /* Subfield code        */
    MH_GRP        new_group,    /* New exception group  */
    unsigned char *datap,       /* Subfield to add      */
    size_t        datalen,      /* Length of data       */
    MH_CHK        check_grp,    /* In, not, don't care  */
    MH_GRP        excp_grp,     /* Exception group      */
    int           max_chg,      /* Max fields to dup.   */
    int           duplicate     /* Yes or no            */
) {
    int           i;            /* Loop counter         */
    MH_FLD        *meshp,       /* Ptr to each field    */
                  *newmeshp;    /* Ptr to duplicate fld */


    /* Examine each field
     * Note that field count grows as we work
     */
    meshp = S_flds;
    for (i=0; i<max_chg; ++meshp, ++i) {

        /* Only for fields we'll output which allow recombinations */
        if ( meshp->disposition == MH_DSP_OUTPUT &&
                !(meshp->no_recombine == MH_NO_COMBOS ||
                  meshp->no_recombine == sfid) ) {

            /* Do we care about exception groups? */
            if (check_grp) {
                if (mesh_find_excp (meshp, excp_grp)) {
                    if (check_grp == MH_CHK_MINUS)
                        continue;
                }
                else {
                    if (check_grp == MH_CHK_PLUS)
                        continue;
                }
            }

            /* If multiple occurrences not allowed and we have
             *   an occurrence already, duplicate the field
             *   and delete the occurrence in the duplicate,
             *   but only if it's the last occurrence.
             * We don't actually add the new subfield here because
             *   we'll see the field again later in the loop and
             *   add it there.
             */
            if (duplicate) {

                /* Is the last subfield already of the type we want? */
                if (meshp->sf[meshp->sf_count - 1].sf_code == sfid) {

                    /* It may be that we already have exactly what we want.
                     * This rare case occurs, for example in the following
                     *   record:
                     *     650   $aFoobar
                     *     651   $aNigeria$xepidemiology
                     *     651   $aNigeria$xethnology
                     *
                     * This could produce the erroneous output:
                     *     650   $aFoobar$zNigeria
                     *     650   $aFoobar$zNigeria
                     *
                     * To prevent this I need to check to be sure that
                     *   the actual data of the last subfield is not
                     *   identical to the data I am currently combining
                     */
                    if (meshp->sf[meshp->sf_count - 1].datalen == datalen &&
                        memcmp (meshp->sf[meshp->sf_count - 1].datap, datap,
                                datalen) == 0)

                        /* Done with this field */
                        continue;

                    /* Duplicate and delete found subfield in duplicate */
                    if ((newmeshp = mesh_duplicate (meshp)) != NULL) {

                        /* Delete existing subfield and add new one in dup */
                        mesh_del_subfield (newmeshp, newmeshp->sf_count-1);
                        mesh_add_subfield (newmeshp, sfid, new_group,
                                           datap, datalen);
                    }

                    /* Done with this field */
                    continue;
                }
            }

            /* If here, there was no existing occurrence of sfid,
             *   or we don't care if there was
             * Add the subfield
             */
            mesh_add_subfield (meshp, sfid, new_group, datap, datalen);
        }
    }
} /* mesh_combine */


/************************************************************************
* mesh_duplicate ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Duplicate one of our mesh field structures.                     *
*                                                                       *
*       Called when we need to combine two incompatible headings        *
*       with another heading, e.g., two $z geographics with a           *
*       mesh $a, requiring us to make repeat the $a, once with          *
*       each $z.                                                        *
*                                                                       *
*   PASS                                                                *
*       Pointer to structure to duplicate.                              *
*                                                                       *
*   RETURN                                                              *
*       Pointer to filled out copy of structure.                        *
*       Null if failed.                                                 *
*                                                                       *
*       Errors are reported from here.                                  *
*       Marc structure errors are fatal.                                *
************************************************************************/

static MH_FLD *mesh_duplicate (
    MH_FLD *inmeshp         /* Ptr to input field object to copy    */
) {
    MH_FLD *outmeshp;       /* Return this pointer to new one       */


    /* Get a new one */
    if ((outmeshp = mesh_new_mhfld ()) == NULL) {
        cm_error (CM_ERROR, "mesh_duplicate:  No room for another mesh field"
                  " - shouldn't happen");
        return NULL;
    }

    /* Copy entire struct */
    memcpy (outmeshp, inmeshp, sizeof(MH_FLD));

    return outmeshp;

} /* mesh_duplicate */


/************************************************************************
* mesh_new_mhfld ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Get the next available mesh field structure from our pool       *
*       of them.                                                        *
*                                                                       *
*       Called when we recognize a new field in the record to           *
*       process, or when we have to split a copy a field for            *
*       complex recombinations.                                         *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Pointer to initialized mh_fld structure.                        *
*       Null if failed.                                                 *
************************************************************************/

static MH_FLD *mesh_new_mhfld ()
{
    MH_FLD *meshp;          /* Ptr to return to caller */


    /* Do we have room for another? */
    if (S_mhfld_count >= MH_MAX_HDGS) {
        cm_error (CM_ERROR, "mesh_new_mhfld: Attempting to create more "
                  "than %d mesh fields", MH_MAX_HDGS);
        return NULL;
    }

    /* Get next one */
    meshp = S_flds + S_mhfld_count++;

    /* Initialize as yet unused field struct */
    meshp->field_id     = -1;
    meshp->sf_count     = 0;
    meshp->keep_indic   = 0;
    meshp->no_recombine = 0;
    meshp->disposition  = MH_DSP_NONE;
    meshp->indic1       =
    meshp->indic2       = ' ';

    return meshp;

} /* mesh_new_mhfld */


/************************************************************************
* mesh_add_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Add a subfield to an mh_fld structure.                          *
*                                                                       *
*       The new subfield is always added at the end.                    *
*                                                                       *
*       Called either to create the original field structure using      *
*       subfields found in the input marc record, or as a result        *
*       of a recombination, taking a subfield from some other           *
*       651, 655, or 650 field and adding it to an output 650 field.    *
*                                                                       *
*   PASS                                                                *
*       Pointer to mesh object to receive the new subfield.             *
*       New subfield id (may be different from id at source).           *
*                                                                       *
*       Pointer to data.                                                *
*       Length of data.                                                 *
*                                                                       *
*   RETURN                                                              *
*       0 = Success, else too many subfields.                           *
************************************************************************/

static int mesh_add_subfield (
    MH_FLD        *meshp,   /* Ptr object to receive new subfield   */
    int           sid,      /* Subfield code for new sf             */
    MH_GRP        excp_grp, /* Exception group for new sf           */
    unsigned char *datap,   /* Ptr to data of subfield              */
    size_t        datalen   /* Length.  No null term expected       */
) {
    MH_SF         *sfp;     /* Ptr to sf control within field ctl   */


    /* Is there enough room? */
    if (meshp->sf_count >= MH_MAX_SFS) {
        cm_error (CM_ERROR, "mesh_add_subfield: Too many subfields in mesh");
        return -1;
    }

    /* Point to next available subfield control, incrementing count */
    sfp = &meshp->sf[meshp->sf_count++];

    /* Copy in all data */
    sfp->excp_grp = excp_grp;
    sfp->sf_code  = (char) sid;
    sfp->datap    = datap;
    sfp->datalen  = datalen;

    return 0;

} /* mesh_add_subfield */


/************************************************************************
* mesh_find_subfield ()                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Find a subfield with a particular subfield code and,            *
*       optionally, a particular value.                                 *
*                                                                       *
*   PASS                                                                *
*       Pointer to mesh object to be searched.                          *
*       Subfield code to search for.                                    *
*       Pointer to index of first subfield to look for.                 *
*           Index will be updated to subfield found, to facilitate      *
*           search for repeating subfields, if desired.                 *
*       Pointer to null terminated value string, or null.               *
*                                                                       *
*   RETURN                                                              *
*       Pointer to subfield control for found sf, or null.              *
*       Index is updated.                                               *
************************************************************************/

static MH_SF *mesh_find_subfield (
    MH_FLD *meshp,      /* Ptr to object from which to delete   */
    int    sfid,        /* Subfield code to find                */
    int    *sfpos,      /* Start looking here                   */
    char   *valp        /* Look for this, if non-null           */
) {
    MH_SF  *sfp;        /* Ptr to each subfield                 */
    int    i;           /* Loop counter                         */


    /* Are we already at end */
    if (*sfpos < 0 || *sfpos >= meshp->sf_count)
        return NULL;

    /* Search, starting at requested point */
    sfp = &meshp->sf[*sfpos];
    for (i=*sfpos; i<meshp->sf_count; i++) {

        /* Test subfield code */
        if (sfp->sf_code == sfid) {

            /* Found right sf, does it have value we want? */
            if (!valp) {
                *sfpos = i;
                return sfp;
            }
            if (!memcmp (valp, sfp->datap, sfp->datalen)) {
                if (strlen (valp) == sfp->datalen) {
                    *sfpos = i;
                    return sfp;
                }
            }
        }
        ++sfp;
    }

    /* If here, not found */
    return NULL;

} /* mesh_find_subfield */


/************************************************************************
* mesh_del_subfield ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       Delete a subfield from an mh_fld structure.                     *
*                                                                       *
*   PASS                                                                *
*       Pointer to mesh object from which to delete the subfield.       *
*       Index of subfield to delete, i.e., ordinal number within        *
*           the field's subfield array.                                 *
*                                                                       *
*   RETURN                                                              *
*       Void.  If there are any errors here, they're bugs.              *
************************************************************************/

static void mesh_del_subfield (
    MH_FLD *meshp,      /* Ptr to object from which to delete   */
    int    sfpos        /* Ordinal subfield number              */
) {
    int    i;           /* Loop counter                         */


    /* Sanity check */
    if (sfpos < 0 || sfpos > meshp->sf_count - 1)
        cm_error (CM_FATAL, "mesh_del_subfield: Invalid subfield pos %d - "
                            "can't happen", sfpos);

    /* Compress out subfield */
    --meshp->sf_count;
    for (i=sfpos; i<meshp->sf_count; i++) {
        meshp->sf[i].excp_grp = meshp->sf[i+1].excp_grp;
        meshp->sf[i].sf_code  = meshp->sf[i+1].sf_code;
        meshp->sf[i].datap    = meshp->sf[i+1].datap;
        meshp->sf[i].datalen  = meshp->sf[i+1].datalen;
    }
} /* mesh_del_subfield */


/************************************************************************
* mesh_find_excp ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Find out if a particular exception group pertains to any        *
*       subfield of a field.                                            *
*                                                                       *
*   PASS                                                                *
*       Pointer to mh_fld struct for field to be checked.               *
*       ID of exception group to look for.                              *
*                                                                       *
*   RETURN                                                              *
*       ID of first subfield containing that exception.                 *
*       0 = No subfields contain that exception.                        *
************************************************************************/

static int mesh_find_excp (
    MH_FLD *meshp,      /* Field to lookup  */
    MH_GRP excp         /* Look for this    */
) {
    int    i;           /* Loop counter     */


    /* Brute force search of all subfields */
    for (i=0; i<meshp->sf_count; i++) {
        if (meshp->sf[i].excp_grp == excp)
            return (meshp->sf[i].sf_code);
    }

    return 0;

} /* mesh_find_excp */


/************************************************************************
* mesh_excp_lookup ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Lookup each subfield in a field to find out if any of them      *
*       require exceptional processing.                                 *
*                                                                       *
*   PASS                                                                *
*       Pointer to mh_fld struct for field to lookup.                   *
*                                                                       *
*   RETURN                                                              *
*       Total count of exceptions found.                                *
************************************************************************/

static int mesh_excp_lookup (
    MH_FLD  *meshp          /* Field to lookup      */
) {
    int     i,              /* Loop counter         */
            count;          /* Number found         */
    MH_SF   *sfp;           /* Ptr to each subfield */
    MH_EXCP exc,            /* Need one for compare */
            *foundp;        /* Ptr to exc in table  */


    count = 0;

    /* For each subfield in the field */
    exc.field_id = meshp->field_id;
    for (i=0; i<meshp->sf_count; i++) {

        /* Create an exception record for comparison */
        sfp = &meshp->sf[i];
        exc.sf_id   = sfp->sf_code;
        exc.datap   = sfp->datap;
        exc.datalen = sfp->datalen;

        /* Search for it */
        if ((foundp = bsearch (&exc, S_excp, S_excp_count, sizeof(MH_EXCP),
                               mesh_excp_compare)) != NULL) {
            /* Save exception info */
            sfp->excp_grp = foundp->group_id;
            count        += 1;
        }
    }

    return (count);

} /* mesh_excp_lookup */


/************************************************************************
* mesh_excp_compare ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       qsort() / bsearch() compare routine for sorting or finding      *
*       exception records.                                              *
*                                                                       *
*   PASS                                                                *
*       Pointer to first mh_excp struct.                                *
*       Pointer to second.                                              *
*                                                                       *
*   RETURN                                                              *
*       Integer greater than, equal to, or less than 0 as compare       *
*       result.                                                         *
************************************************************************/

static int mesh_excp_compare (
    const void *e1pp,
    const void *e2pp
) {
    MH_EXCP *excp1p,    /* Ptr to first struct      */
            *excp2p;    /* Ptr to second            */


    /* De-reference and cast to proper type */
    excp1p = (MH_EXCP *) e1pp;
    excp2p = (MH_EXCP *) e2pp;

    /* Equality requires that field, subfield must be same */
    if (excp1p->field_id != excp2p->field_id)
        return (excp1p->field_id - excp2p->field_id);
    if (excp1p->sf_id != excp2p->sf_id)
        return (excp1p->sf_id - excp2p->sf_id);

    /* Use longest length, avoiding false match on leading substring */
    if (excp1p->datalen > excp2p->datalen)
        return (strncmp ((char *) excp1p->datap, (char *) excp2p->datap,
                excp1p->datalen));
    return (strncmp ((char *) excp1p->datap, (char *) excp2p->datap,
            excp2p->datalen));

} /* mesh_excp_compare */


/************************************************************************
* mesh_output_all ()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*       Output everything to the output record.                         *
*                                                                       *
*   PASS                                                                *
*       Pointer marc control structure for output record.               *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void mesh_output_all (
    MARCP  outmp                /* Ptr to output control    */
) {
    int    i;                   /* Loop counter             */
    MH_FLD *sortmp[MH_MAX_HDGS];/* Array of ptrs for sort   */

    /* Tell the sort routine that it hasn't put out any error
     *   messages yet for duplicate headings.
     */
    S_dupmesh_warn = 0;

    /* Get a sorted array of pointers to mesh fields */
    for (i=0; i<S_mhfld_count; i++)
        sortmp[i] = &S_flds[i];
    qsort (sortmp, S_mhfld_count, sizeof(MH_FLD *), mesh_sort_compare);

    /* Output fields in sorted order */
    for (i=0; i<S_mhfld_count; i++) {
        if (sortmp[i]->disposition == MH_DSP_OUTPUT)
            mesh_output (outmp, sortmp[i]);
    }
} /* mesh_output_all */


/************************************************************************
* mesh_output ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Write a mesh field to the output marc record.                   *
*                                                                       *
*       After writing the field, we change its disposition to           *
*       MH_DSP_COMPLETE.                                                *
*                                                                       *
*   PASS                                                                *
*       Pointer marc control structure for output record.               *
*       Pointer to internal mh_fld mesh field object for field to       *
*           output.                                                     *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Marc API errors should not happen and are fatal if they do.     *
************************************************************************/

static int mesh_output (
    MARCP  outmp,           /* Output marc control      */
    MH_FLD *meshp           /* Field to output          */
) {
    int    i,               /* Loop counter             */
           stat;            /* Return from marc funcs   */
    MH_SF  *sfp;            /* Ptr to each subfield     */


    /* Internal error check */
    if (meshp->disposition != MH_DSP_OUTPUT)
        cm_error (CM_FATAL, "mesh_output: Internal error, attempting to "
                  "output field with disposition = %d", meshp->disposition);

    /* Now output 651 and 655 if no 650 exists */
#ifdef IF_650_IS_ONLY_ALLOWED_OUTPUT
    if (meshp->field_id != 650)
        cm_error (CM_FATAL, "mesh_output: Internal error, attempting to "
                  "output field with field id = %d", meshp->field_id);
#endif

    /* Create output field */
    if ((stat = marc_add_field (outmp, meshp->field_id)) != 0)
        cm_error (CM_FATAL, "mesh_output: Error %d adding mesh field", stat);

    /* Indicators */
    if ((stat = marc_set_indic (outmp, 1, meshp->indic1)) == 0)
        stat = marc_set_indic (outmp, 2, meshp->indic2);
    if (stat)
        cm_error (CM_FATAL, "mesh_output: Error %d setting indicator",
                  stat);

    /* All subfields */
    for (i=0; i<meshp->sf_count; i++) {

        /* Add subfield */
        sfp = &meshp->sf[i];
        if ((stat = marc_add_subfield (outmp, sfp->sf_code, sfp->datap,
                                       sfp->datalen)) != 0) {

            /* There currently is bad data in the ILS database which can
             *   cause this to happen.
             * Log error and force kill of this record.
             */
            cm_error (CM_ERROR, "mesh_output: Error %d writing subfield '%c'"
                      " - killing record", stat, sfp->sf_code);
            S_return_code = CM_STAT_KILL_RECORD;
        }
    }

    /* Change disposition to prevent further output */
    meshp->disposition = MH_DSP_COMPLETE;

    return 0;

} /* mesh_output */


/************************************************************************
* mesh_sort_compare ()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       qsort() routine for sorting mesh headings by string values      *
*       prior to output.                                                *
*                                                                       *
*       Sorts by first indicator, then by each subfield string.         *
*                                                                       *
*       Sorts an array of pointers to mh_fld structures.                *
*                                                                       *
*   PASS                                                                *
*       Pointer to first pointer to mh_fld struct.                      *
*       Pointer to second.                                              *
*                                                                       *
*   RETURN                                                              *
*       Integer greater than, equal to, or less than 0 as compare       *
*       result.                                                         *
************************************************************************/

static int mesh_sort_compare (
    const void *m1pp,
    const void *m2pp
) {
    MH_FLD     *m1p,        /* Ptr to first                 */
               *m2p;        /* Second                       */
    int        i,           /* Loop counter                 */
               len,         /* Longest datalen for strncmp  */
               sf_count,    /* Shortest num sfs in 2 fields */
               cmp;         /* Result of compare            */


    /* De-reference and cast to proper type */
    m1p = (*((MH_FLD **) m1pp));
    m2p = (*((MH_FLD **) m2pp));

    /* Try sort on first indicator */
    if (m1p->indic1 != m2p->indic1)
        return (m1p->indic1 - m2p->indic1);

    /* Compare subfields in order */
    sf_count = (m1p->sf_count < m2p->sf_count) ? m1p->sf_count : m2p->sf_count;
    for (i=0; i<sf_count; i++) {

        /* Find length of compare */
        len = m1p->sf[i].datalen < m2p->sf[i].datalen ?
                m1p->sf[i].datalen : m2p->sf[i].datalen;

        /* Compare */
        if ((cmp = strncmp ((char *) m1p->sf[i].datap,
                            (char *) m2p->sf[i].datap, len)) != 0)
            return cmp;

        /* Compare may still be unequal if one was shorter than the other */
        if (m1p->sf[i].datalen != m2p->sf[i].datalen)
            return (m1p->sf[i].datalen - m2p->sf[i].datalen);
    }

    /* If we got here, two fields are identical up to the one with the
     *   lower number of subfields.
     * Return that lower numbered one.
     */
    if (m1p->sf_count > m2p->sf_count)
        return -1;
    else if (m1p->sf_count < m2p->sf_count)
        return 1;

    /* Else two output fields are identical.
     * Assuming no bug in the program, this is caused by having
     *   two identical MeSH fields in the record.
     * One record might generate a lot of these due to the
     *   way sorting is done, so we use a static set in the
     *   routine which invokes qsort to tell us whether an
     *   error has already been issued for this record.
     * Marti requested that we kill records with this condition.
     */
    if (!S_dupmesh_warn) {
        cm_error (CM_ERROR, "Identical mesh fields found in record");
        S_dupmesh_warn = 1;
        S_return_code  = CM_STAT_KILL_RECORD;
    }

    return 0;

} /* mesh_sort_compare */


/************************************************************************
*************************************************************************
**                                                                     **
**                        TABLE LOAD ROUTINES                          **
**                                                                     **
** These functions load the control table that helps us find records   **
** requiring exceptional handling, and the language table used for     **
** inserting language subfields into fields.                           **
**                                                                     **
*************************************************************************
************************************************************************/


/************************************************************************
* load_mesh_excp ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       Load a table of MeSH exceptions.  These are lists of            *
*       MeSH headings which require different handling than             *
*       all other headings.                                             *
*                                                                       *
*       Example input file line format is:                              *
*           650:a:Age650:Infant, Newborn                                *
*       Field^  ^    ^     ^                                            *
*         SF --/    /     /                                             *
*           Group -/     /                                              *
*             String to search for                                      *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

static void load_mesh_excp (
    char *fname             /* Name of mesh control table file      */
) {
    MH_EXCP *mfp;           /* Pointer into field control table     */
    FILE    *fp;            /* Input file                           */
    char    *textp,         /* Ptr to line in control table         */
            *fidp,          /* Pointer to field id in ctl table line*/
            *sidp,          /* Ptr to subfield id                   */
            *grpp,          /* Ptr to group id string               */
            *hdgp;          /* Ptr to heading string                */
    int     fid;            /* Field id, as number                  */

    /* Table for group name -> id lookup */
    static struct {
        MH_GRP grp_id;      /* Group id                             */
        char   *grp_name;   /* Text name of group                   */
    } *vgp, s_valid_grps[] = {
        { MH_GRP_AGE,     "Age650" },
        { MH_GRP_LAW,     "Law"    },
        { MH_GRP_LAW5,    "Law5"   },
        { MH_GRP_CASEREP, "CaseRep"},
        { MH_GRP_STAT,    "Stats"  },
        { MH_GRP_STAT5,   "Stats5" },
        { MH_GRP_DICT,    "Dict"   },
        { MH_GRP_USMED,   "USMed"  },
        { MH_GRP_USMED1,  "USMed1" },
        { MH_GRP_NONE,    NULL     }
     };


    /* Open text format control table file.  Exit on failure */
    open_ctl_file (fname, &fp);

    /* Point to file wide table of exception records */
    mfp = S_excp;

    /* For each line of the file */
    while (get_ctl_line (fp, &textp)) {

        /* Table full? */
        if (S_excp_count >= MH_MAX_EXCP)
            cm_error (CM_FATAL, "Exception table full.  Max=%d", MH_MAX_EXCP);

        /* Parse line into colon separated fields */
        fidp = strtok (textp, ":");
        sidp = strtok (NULL, ":");
        grpp = strtok (NULL, ":");
        hdgp = strtok (NULL, ":");

        /* Got everything? */
        if (!fidp || !sidp || !grpp || !hdgp) {
            cm_error (CM_ERROR,
                      "Missing data.  Expecting fid:sid:grp:Heading");
            continue;
        }

        /* Store field id in table */
        fid = atoi (fidp);
        if (fid != 650 && fid != 651 && fid != 655) {
            cm_error (CM_ERROR,
                      "Field %d invalid.  Expecting 650, 651, or 655", fid);
            continue;
        }
        mfp->field_id = fid;

        /* Subfield id */
        if (*sidp != 'a' && *sidp != 'x') {
            cm_error (CM_ERROR,
                      "Subfield %c invalid.  Expecting a or x", *sidp);
            continue;
        }
        mfp->sf_id = (int) *sidp;

        /* Decode group */
        vgp = s_valid_grps;
        while (vgp->grp_id != MH_GRP_NONE) {
            if (!strcmp (vgp->grp_name, grpp)) {
                mfp->group_id = vgp->grp_id;
                break;
            }
            ++vgp;
        }
        if (vgp->grp_id == MH_GRP_NONE) {
            cm_error (CM_ERROR, "Unknown group identifier \"%s\".  "
                      "See meshproc.c for valid groups", grpp);
            continue;
        }

        /* Store heading in table string buffer, pointer in table */
        mfp->datap   = (unsigned char *) store_strbuf (&S_tblbuf, hdgp, 0);
        mfp->datalen = strlen ((char *) mfp->datap);

        /* Ready for next */
        ++mfp;
        ++S_excp_count;
    }

    if (getenv ("MESHTEST"))
        dumpmeshtbls ();

    /* Sort the table by heading, ready for binary search */
    qsort (S_excp, S_excp_count, sizeof(MH_EXCP), mesh_excp_compare);

    close_ctl_file (&fp);

} /* load_mesh_excp */


/************************************************************************
* load_lang_tbl ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Load a table of language abbreviations and expansions.          *
*                                                                       *
*       Example input file line format is:                              *
*           pol:Polish                                                  *
*           scr:Serbo-Croation (Roman)                                  *
*                                                                       *
*   PASS                                                                *
*       Name of file to be loaded.                                      *
*                                                                       *
*   RETURN                                                              *
*       Void.  All errors are fatal.                                    *
************************************************************************/

static void load_lang_tbl (
    char    *fname          /* Name of text format language file    */
) {
    MH_LANG *mlp;           /* Ptr into language table              */
    FILE    *fp;            /* Input file                           */
    char    *abbrev,        /* Ptr to abbreviation                  */
            *expand;        /* Ptr to expansion                     */
    char    *textp;         /* Ptr to line in control table         */


    /* Open text format language file.  Exit on failure */
    open_ctl_file (fname, &fp);

    /* Point to file wide language table */
    mlp = S_langs;

    /* For each line of the file */
    while (get_ctl_line (fp, &textp)) {

        /* Table full? */
        if (S_lang_count >= MH_MAX_LANGS)
            cm_error (CM_FATAL, "Exception table full.  Max=%d", MH_MAX_EXCP);

        /* Parse line into colon separated fields */
        abbrev = strtok (textp, ":");
        expand = strtok (NULL, ":");

        /* Test */
        if (!abbrev || !expand) {
            cm_error (CM_ERROR,
                      "Missing data.  Expecting xxx:Language");
            continue;
        }
        if (strlen (abbrev) != 3) {
            cm_error (CM_ERROR, "Language abbreviation \"%s\" > 3 chars");
            continue;
        }

        /* Store */
        strcpy (mlp->abbrev, abbrev);
        mlp->langp   = store_strbuf (&S_tblbuf, expand, 0);
        mlp->langlen = strlen (mlp->langp);

        ++mlp;
        ++S_lang_count;
    }

    /* Sort the table by abbreviation, ready for binary search */
    qsort (S_langs, S_lang_count, sizeof(MH_LANG), compare_lang);

    close_ctl_file (&fp);

} /* load_lang_tbl */


/************************************************************************
* compare_lang ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Compare two language abbreviations for qsort() and bsearch().   *
*                                                                       *
*   PASS                                                                *
*       Ptr to ptr to record1.                                          *
*       Ptr to ptr to record2.                                          *
*                                                                       *
*   RETURN                                                              *
*       See qsort docs.                                                 *
************************************************************************/

static int compare_lang (
   const void *entry1,
   const void *entry2
) {
    /* Cast and compare using strcmp */
    return (strcmp (((MH_LANG *)entry1)->abbrev, ((MH_LANG *)entry2)->abbrev));
}


/************************************************************************
* store_strbuf ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Store a string in a string buffer.                              *
*                                                                       *
*   PASS                                                                *
*       Pointer to string buffer structure.                             *
*           If pointer to buffer within struct is null, allocate buf.   *
*       Pointer to string to store.                                     *
*       Optional data length.                                           *
*           If 0, assume null termination and use strlen().             *
*                                                                       *
*   RETURN                                                              *
*       Pointer to string within buffer.                                *
************************************************************************/

static char *store_strbuf (
    MH_STRBUF *sp,          /* Ptr to buffer structure  */
    char      *strp,        /* Store this string        */
    size_t    len           /* Len string, or 0         */
) {
    char      *retp;        /* Return this pointer      */


    /* If length not supplied, get length plus terminating null */
    if (!len)
        len = strlen (strp) + 1;

    /* Initial allocation */
    if (!sp->buflen) {

#ifdef DEBUG
        if ((sp->bufp = (char *) marc_alloc(MH_MAX_STRSIZE, 800)) == NULL) {  //TAG:800
            cm_error (CM_FATAL, "store_strbuf: Can't allocate space " "for string buffer");
        }
#else
        if ((sp->bufp = (char *) malloc (MH_MAX_STRSIZE)) == NULL) {
            cm_error (CM_FATAL, "store_strbuf: Can't allocate space " "for string buffer");
        }
#endif
        sp->buflen = MH_MAX_STRSIZE;
        sp->nextp  = sp->bufp;
    }

    /* Can't add more space since we've already set pointers
     * Shouldn't be a problem.
     * If it is, we'll have to go to a more sophisticated string
     *   buffer scheme.
     */
    if (len >= (sp->buflen - (sp->nextp - sp->bufp)))
        cm_error (CM_FATAL, "store_strbuf: Out of space in string buffer");

    /* New string goes at nextp */
    retp = sp->nextp;
    memcpy (retp, strp, len);
    sp->nextp += len;

    return (retp);

} /* store_strbuf */


/************************************************************************
* clear_strbuf ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Empty a string buffer.                                          *
*                                                                       *
*   PASS                                                                *
*       Pointer to string buffer structure.                             *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/


static void clear_strbuf (
    MH_STRBUF *sp           /* Ptr to buffer structure  */
) {
    sp->nextp = sp->bufp;
}


/************************************************************************
* dumpmeshtbls()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Testing only.                                                   *
************************************************************************/

static int dumpmeshtbls ()
{
    int i;
    MH_EXCP *mp;
    MH_LANG *lp;


    /* Dump mesh exception list */
    mp = S_excp;
    for (i=0; i<S_excp_count; i++) {
        printf ("%5d: %03d$%c:%02d:%3d:%s\n", i, mp->field_id, mp->sf_id,
                mp->group_id, mp->datalen, mp->datap);
        ++mp;
    }

    /* Same for language table */
    lp = S_langs;
    for (i=0; i<S_lang_count; i++) {
        printf ("%5d: %s:%s\n", i, lp->abbrev, lp->langp);
        ++lp;
    }

    return 0;
}

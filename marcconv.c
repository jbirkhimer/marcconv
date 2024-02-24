/************************************************************************
* marcconv.c                                                            *
*                                                                       *
*   DEFINITION                                                          *
*       Convert input records in MARC format to output records, still   *
*       in MARC format, but with different contents.                    *
*                                                                       *
*       NLM uses marc physical format in its integrated library         *
*       system, but the content of the fields and subfields does        *
*       not always conform to US MARC standards.                        *
*                                                                       *
*       This program uses control tables and various types of           *
*       processing routines to convert from internal to more            *
*       standard formats.                                               *
*                                                                       *
*       Derived from earlier "convmarc.c".                              *
*                                                                       *
*   COMMAND LINE ARGUMENTS                                              *
*       Name of "switch" file containing values of switches used        *
*           to control the process.                                     *
*       Name of conversion control file - specifying actual conversion  *
*           procedures.                                                 *
*       Name of input file in NLM MARC format.                          *
*       Name of output file.                                            *
*                                                                       *
*       See usage for optional arguments.                               *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       Else error.                                                     *
*                                                                       *
*                                               Author: Alan Meyer      *
************************************************************************/

/* $Id: marcconv.c,v 1.17 2001/07/23 20:26:15 meyer Exp $
 * $Log: marcconv.c,v $
 * Revision 1.17  2001/07/23 20:26:15  meyer
 * Modified call to exec_proc for field pre-processes.
 * Now passing &datalen to exec_proc to allow for changing the length
 * of a fixed field.  These aren't fixed length.  Procedures can modify
 * the length.  This change allows that.
 *
 * Revision 1.16  2000/04/18 01:29:17  meyer
 * Fixed Windows compiler warnings.
 * Opened files in binary mode to work in Windows as well as UNIX.
 * Added EOF check after failed marc_read_rec return.  Not assuming EOF.
 *
 * Revision 1.15  2000/03/06 16:16:07  meyer
 * Printing BibID or UI, in that order, in error logs.
 *
 * Revision 1.14  1999/11/24 00:23:38  meyer
 * Now allowing "field XXX" as well as "nXX" and "nnX".
 *
 * Revision 1.13  1999/07/22 03:17:38  meyer
 * Fixed compilation of "else" keyword.
 *
 * Revision 1.12  1999/03/29 20:33:26  meyer
 * Took the warning back out again.  Oh well...
 *
 * Revision 1.11  1999/03/29 18:03:17  meyer
 * Generating a warning to the logfile when a record is killed.
 *
 * Revision 1.10  1999/03/09 01:25:28  meyer
 * Improved error logging, added ui and formatting.
 *
 * Revision 1.9  1999/03/08 15:42:01  meyer
 * Added support for -p path to control tables
 *
 * Revision 1.8  1999/02/09 02:41:12  meyer
 * Copied input leader to output, overwriting default leader.
 *
 * Revision 1.7  1999/02/08 21:51:29  meyer
 * Changes for indicators, wider subfield range, and bug fixes.
 *
 * Revision 1.6  1999/01/05 02:26:00  meyer
 * Made some control table functionality global for use with mesh and lang.
 * Fixed uninitialized estat bug.
 * Made bad subfield code non-fatal error.
 *
 * Revision 1.5  1998/12/31 21:16:47  meyer
 * Made it possible to use any subfield code between '!' and '~', inclusive.
 *
 * Revision 1.4  1998/12/29 02:38:04  meyer
 * Made switches readable even if not defined.  They contain null string.
 *
 * Revision 1.3  1998/12/21 05:06:33  meyer
 * Added support for nnX and nXX field ids.
 *
 * Revision 1.2  1998/12/21 04:19:48  meyer
 * Added checks for invalid sf code.  Fixed bug.
 *
 * Revision 1.1  1998/12/14 19:45:48  meyer
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "marc.h"
#include "marcconv.h"
#include "amuopt.h"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


/*-------------------------------------------------------------------\
|   Statics                                                          |
\-------------------------------------------------------------------*/
static int      S_line_num;     /* Configuration file line number   */
static int      S_errs;         /* Count of errors                  */
static int      S_warns;        /* Count of warnings                */
static long     S_in_recs;      /* Number of recs read in           */
static long     S_out_recs;     /* Number of recs written out       */
static MARCP    S_inmp;         /* Input pseudo marc control struct */
static MARCP    S_outmp;        /* Output real marc struct          */
static CM_PROC  *S_sessprepp;   /* Head of session pre-process list */
static CM_PROC  *S_sesspostp;   /* Head of session post-process list*/
static CM_PROC  *S_recprepp;    /* Head of record pre-process list  */
static CM_PROC  *S_recpostp;    /* Head of record post-process list */
static CM_FIELD *S_fieldp[1000];/* One for each possible field      */
static CM_PARMS S_parms;        /* Command line parameters          */
static char     S_ctlfile[FILENAME_MAX]; /* Current open ctl file   */


/*-------------------------------------------------------------------\
|   Internal prototypes                                              |
\-------------------------------------------------------------------*/
static CM_STAT exec_proc    (CM_PROC *, unsigned char **, size_t, size_t *);
static CM_PROC *make_proc   (char **, CM_CND *, int, CM_LVL, int);
static int  load_switch_file(char *);
static int  load_ctl_file   (char *);
static int  get_key_line    (FILE *, char **, char **, int *);
static void parse_ctl       (char *, char **, int *);
static int  get_sf_ctl      (int);
static void get_parms       (CM_PARMS *, int, char **);
static void usage           (char *);
static int  tokenize        (char *, CM_TOK *, int *);
static void get_log_time    (char *);
static void report          (void);

#ifdef DEBUG
int g_recnum;
int g_reclen;
int check(int);
void *marc_alloc(int, int);
void *marc_calloc(int, int, int);
void marc_end(void);
#endif


int main (int argc, char *argv[])
{
    CM_FIELD *fieldp;       /* Current field control            */
    CM_SF    *sfp;          /* Current subfield control         */
    FILE     *infp,         /* Input sequential marc file       */
             *outfp;        /* Output marc file                 */
    unsigned char *datap,   /* Ptr to input/output data         */
             *data2p,       /* Second ptr for copies            */
             *inbuf;        /* Buffer for input records         */
    size_t   datalen,       /* Length of data                   */
             data2len,      /* Second length, for copies        */
             outlen;        /* Length of output field           */
    int      fpos,          /* Field position / loop counter    */
             spos,          /* Subfield position / loop counter */
             field_count,   /* Number of fields in current rec  */
             sf_count,      /* Num sfs in current field         */
             field_id,      /* Current field id/tag             */
             sf_id,         /* Current subfield id/code         */
             stat,          /* Return from lower level funcs    */
             estat;         /* Return from exec_proc()          */


    /* Load parameters from command line */
    get_parms (&S_parms, argc, argv);

    /* Load switches file */
    if (load_switch_file (S_parms.swfile) != 0)
        cm_error (CM_FATAL, "Aborting with %d switch file errors", S_errs);

    /* Load control table */
    if (load_ctl_file (S_parms.ctlfile) != 0)
        cm_error (CM_FATAL, "Aborting with %d control table errors", S_errs);

    /* Open input and output files */
    if ((infp = fopen (S_parms.infile, "rb")) == NULL) {
        perror (S_parms.infile);
        cm_error (CM_FATAL, "Unable to open input file \"%s\"",
                  S_parms.infile);
    }
    if ((outfp = fopen (S_parms.outfile, S_parms.omode)) == NULL) {
        perror (S_parms.outfile);
        cm_error (CM_FATAL, "Unable to open output file \"%s\"",
                  S_parms.outfile);
    }

    /* Create input record buffer */
#ifdef DEBUG
    if ((inbuf = (unsigned char *) marc_alloc(CM_MAX_MARC_SIZE, 200)) == NULL) {   //TAG:200
        cm_error (CM_FATAL, "Insufficient memory for input record buffer");
    }
#else
    if ((inbuf = malloc (CM_MAX_MARC_SIZE)) == NULL) {
        cm_error (CM_FATAL, "Insufficient memory for input record buffer");
    }
#endif

    /* Create record controls for input and output */
    if ((stat = marc_init (&S_inmp)) != 0)
        cm_error (CM_FATAL, "Error %d initializing input record control");
    if ((stat = marc_init (&S_outmp)) != 0)
        cm_error (CM_FATAL, "Error %d initializing output record control");

    /* Turn off subfield sorting on output */
    marc_subfield_sort (S_outmp, 0);

    /* Execute any session pre-processes */
    exec_proc (S_sessprepp, NULL, 0, NULL);

    setbuf(stdout, NULL);

    /* Read records until done */

#ifdef DEBUG
    while (!(stat = marc_read_rec(infp, inbuf, CM_MAX_MARC_SIZE, &g_reclen))) {
#else
    while ((stat = marc_read_rec(infp, inbuf, CM_MAX_MARC_SIZE)) == 0) {
#endif

        /*  g_recnum++ happens in function marc_read_rec */
#ifdef DEBUG
if (check(0) < 0) {
    printf("===> check failed for record %d\n", g_recnum);
}
#endif

        /* Are we skipping some? */
        if (++S_in_recs <= S_parms.skip_recs)
            continue;

        /* Parse the input record */
        if ((stat = marc_old (S_inmp, inbuf)) != 0) {
            cm_error (CM_FATAL, "marc_old error %d on input record", stat);
        }

        /* Start a new output record */
        if ((stat = marc_new (S_outmp)) != 0) {
            cm_error (CM_FATAL, "marc_new error %d on output record", stat);
        }

        /* Replace the default leader in the new output record
         *   with the leader in the original input record
         */
        if ((stat = marc_get_item (S_inmp, 0, 0, 0, 24, &datap, &datalen)) == 0) {
            if ((stat = marc_pos_field (S_outmp, 0, &field_id, &data2p, &data2len)) == 0)
                stat = marc_add_subfield (S_outmp, 0, datap, datalen);
        }

        if (stat != 0)
            cm_error (CM_FATAL, "Error %d copying leader");

        /* Execute any record level pre-processes */
        if ((estat = exec_proc (S_recprepp, NULL, 0, NULL)) >= CM_STAT_DONE_RECORD) {
            /* Don't do any more with this record.  Might kill it */
            goto done_rec;
        }

        /* Find count of input fields */
        if ((stat = marc_cur_field_count (S_inmp, &field_count)) != 0) {
            cm_error (CM_FATAL, "Error %d fetching field count", stat);
        }

        /* Loop through all fields after dummy leader */
        for (fpos=1; fpos<field_count; fpos++) {

            /* Position to field */
            if ((stat = marc_pos_field (S_inmp, fpos, &field_id, &datap, &datalen)) != 0) {
                cm_error (CM_FATAL, "Error %d positioning to field %d", stat, fpos);
            }

            /* Create the output field */
            if ((stat = marc_add_field (S_outmp, field_id)) != 0) {
                cm_error (CM_FATAL, "Error %d adding field %d", stat, field_id);
            }

            /* Do we have any procs for this field? */
            fieldp = S_fieldp[field_id];

            /* Execute all pre-processes */
            estat = CM_STAT_OK;
            if (fieldp) {
                if ((estat = exec_proc (fieldp->prepp, &datap, datalen, &datalen)) >= CM_STAT_DONE_FIELD) {
                    switch (estat) {
                        case CM_STAT_DONE_FIELD:
                        case CM_STAT_KILL_FIELD:
                            goto done_field;
                        case CM_STAT_DONE_RECORD:
                        case CM_STAT_KILL_RECORD:
                            goto done_rec;
                    }
                }
            }

            /* Process subfields in variable fields */
            if (field_id >= 10) {

                /* Get count of subfields */
                if ((stat = marc_cur_subfield_count (S_inmp, &sf_count)) != 0) {
                    cm_error (CM_FATAL, "Error %d fetching sf count", stat);
                }

                /* Process all subfields, starting with indicators */
                for (spos=0; spos<sf_count; spos++) {

                    /* Fetch next available subfield */
                    if ((stat = marc_pos_subfield (S_inmp, spos, &sf_id, &datap, &datalen)) != 0) {
                        cm_error (CM_FATAL, "Error %d positioning to field %d sf %d", stat, field_id, spos + 1);
                    }

                    /* Test for bad input subfield code
                     * spos 0 and 1 are indicators, so we only check
                     *   for subfields beyond that range.
                     */
                    if (spos > 1 && marc_ok_subfield (sf_id) != 0) {
                        cm_error (CM_ERROR, "Bad sf code (int) %d in field %d", sf_id, field_id);
                        estat = CM_STAT_KILL_RECORD;
                        goto done_rec;
                    }

                    /* Is there a matching subfield control? */
                    sfp = fieldp ? &fieldp->sf[get_sf_ctl (sf_id)] : NULL;

                    /* Pre-procs */
                    if (sfp) {
                        if ((estat = exec_proc (sfp->prepp, &datap,
                                               datalen, &datalen)) != 0) {
                            switch (estat) {
                                case CM_STAT_DONE_SF:
                                    continue;
                                case CM_STAT_DONE_FIELD:
                                case CM_STAT_KILL_FIELD:
                                    goto done_field;
                                case CM_STAT_DONE_RECORD:
                                case CM_STAT_KILL_RECORD:
                                    goto done_rec;
                            }
                        }
                    }

                    /* Insert subfield itself */
                    if ((stat = marc_add_subfield (S_outmp, sf_id, datap, datalen)) != 0) {
                        cm_error (CM_FATAL, "Error %d inserting sf %c in " "field %d", stat, sf_id, field_id);
                    }

                    /* Post-procs */
                    if (sfp) {
                        if ((estat = exec_proc (sfp->postp, &datap, datalen, &datalen)) > 0) {
                            switch (estat) {
                                case CM_STAT_DONE_FIELD:
                                case CM_STAT_KILL_FIELD:
                                    goto done_field;
                                case CM_STAT_DONE_RECORD:
                                case CM_STAT_KILL_RECORD:
                                    goto done_rec;
                            }
                        }
                    }

                }
            } else {

                /* Copy fixed field data */
                if ((stat = marc_add_subfield (S_outmp, 0, datap, datalen)) != 0) {
                    cm_error (CM_FATAL, "Error %d inserting fixed field %d", stat, field_id);
                }
            }

            /* Field post procs */
            estat = CM_STAT_OK;
            if (fieldp) {
                if ((estat = exec_proc (fieldp->postp, NULL, 0, NULL)) > 0)
                    if (estat >= CM_STAT_DONE_RECORD) goto done_rec;
            }

/* Go here if done all work on the field */
done_field:
            /* Is there data in the current field? */
            if ((stat = marc_cur_field_len (S_outmp, &outlen)) != 0) {
                cm_error (CM_FATAL, "Error %d fetching field length", stat);
            }

            /* If we quit before copying any data to field, kill it
             * Note that the input field id may no longer match the
             *   output field id if the program changed field ids.
             * However we don't care here since a fixed field could
             *   never be changed to a variable field, or vice versa.
             * So we'll use the info we already have.
             * Fixed fields with no data have len==1 for field term.
             * Var fields have len==3 for field term + 2 indicators.
             */
            if (estat == CM_STAT_KILL_FIELD || (field_id<10 && outlen==1) || (field_id>9 && outlen==3)) {
                if ((stat = marc_del_field (S_outmp)) != 0) {
                    cm_error (CM_FATAL, "Error %d deleting empty field, " "input id = %d", stat, field_id);
                }
            }
        }

        /* Record post procs */
        estat = exec_proc (S_recpostp, NULL, 0, NULL);


/* Go here to write the record and continue to the next */
done_rec:
        /* Write record if not killed and it contains data */
        if (estat != CM_STAT_KILL_RECORD) {

            /* Get field count */
            if ((stat = marc_cur_field_count (S_outmp, &field_count)) != 0) {
                cm_error (CM_FATAL, "Error %d fetching field count", stat);
            }

            /* If there is anything to write */
            if (field_count > 1) {

                /* Write to output */
                if ((stat = marc_write_rec (outfp, S_outmp)) != 0) {
                    cm_error (CM_FATAL, "Error %d writing record", stat);
                }

                /* Count and test after successful write */
                if (++S_out_recs >= S_parms.conv_recs) {
                    break;
                }
            }
        }
    }

    if (!feof (infp)) {
        cm_error (CM_ERROR, "Unexpected marc_read_rec error = %d", stat);
        perror ("Input file");
    }

    /* Execute any session post-processes */
    exec_proc (S_sesspostp, NULL, 0, NULL);

    /* Close output file */
    if (fclose (outfp) != 0)
        cm_error (CM_ERROR, "Failed to close output file, disk space full?");

    /* Report results */
    report ();

#ifdef DEBUG
    marc_end();
#endif
    return 0;

} /* main */


/************************************************************************
* exec_proc ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Execute a chain of procedures.                                  *
*                                                                       *
*   PASS                                                                *
*       Ptr to head of chain of procedures.                             *
*       Ptr to ptr to current data.                                     *
*           This points directly into the record.                       *
*           If we call any funcs, we'll copy the data out to another    *
*               buffer and point to this buffer instead.                *
*           NULL if no current data, i.e., we're at a level above       *
*               a level (field or subfield) which has actual data.      *
*       Length of current data.                                         *
*       Ptr to place to put new length                                  *
*           NULL if caller doesn't need new length.                     *
*                                                                       *
*       Rest of data comes from statics.                                *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/

/* Simple checksum of things that should never change */
#define CM_CHECKSUM \
    ( (long) s_pparms.outmp + (long) s_pparms.bufp + (long) s_pparms.buflen )

static CM_STAT exec_proc (
    CM_PROC       *linkp,           /* Ptr linked list of proc ctls */
    unsigned char **datapp,         /* Ptr to ptr to data           */
    size_t        datalen,          /* Length of data at *datpp     */
    size_t        *newlen           /* Put new length here          */
) {
    int           save_stat,        /* Return from marc_save_pos    */
                  retcode;          /* Return code                  */

    static int s_first_time = 1;    /* Initialization flag          */
    static CM_PROC_PARMS s_pparms;  /* Parm struct to pass arund    */
    static unsigned char *s_bufp;   /* Buffer for modifiable data   */
    static long s_checksum;         /* Checksum for static parms    */


    /* If no passed procedure list, nothing to do
     * This is actually the most common case since any field or
     *   subfield not requiring pre or post procs will have a
     *   null pointer to procs
     */
    if (!linkp)
        return CM_STAT_OK;

    /* If first time in function, initialize buffers and parms */
    if (s_first_time) {

        /* Get a buffer which procs can modify */
#ifdef DEBUG
        if ((s_bufp = (unsigned char *) marc_alloc(CM_PROC_BUF_SIZE, 201)) == NULL) { //TAG:201
            cm_error (CM_FATAL, "Proc buffer memory");
        }
#else
        if ((s_bufp = (unsigned char *) malloc (CM_PROC_BUF_SIZE)) == NULL) {
            cm_error (CM_FATAL, "Proc buffer memory");
        }
#endif

        /* Fill unchanging parts of the parameter structure */
        s_pparms.outmp  = S_outmp;
        s_pparms.bufp   = s_bufp;
        s_pparms.buflen = CM_PROC_BUF_SIZE;

        /* Compute checksum to find wayward procs */
        s_checksum = CM_CHECKSUM;

        s_first_time = 0;
    }

    /* Validate data length, longer is impossible in marc */
    if (datalen >= CM_PROC_BUF_SIZE)
        cm_error (CM_FATAL, "Data length exceeds max allowed (%d)",
                  CM_PROC_BUF_SIZE);

    /* May or may not be passed data */
    if (datalen) {

        /* Copy current data to modifiable buffer */
        memcpy (s_bufp, *datapp, datalen);
        s_bufp[datalen] = '\0';
    }
    else
        *s_bufp = '\0';

    /* Tell caller where to find (possibly) modified data, if he wants it */
    if (datapp)
        *datapp = s_bufp;

    do {
        /* Make arguments available to function */
        s_pparms.args      = linkp->args;
        s_pparms.arg_count = linkp->arg_count;

        /* Give proc an input rec to play with
         *   without disturbing the input position.
         */
        marc_dup (S_inmp, &s_pparms.inmp);

        /* Remember where we are in the output record so we
         *   can return here if the proc changes things
         */
        if ((save_stat = marc_save_pos (S_outmp)) < 0)
            cm_error (CM_FATAL, "Error %d from marc_save_pos", save_stat);

        /* Call next function */
        retcode = (linkp->procp) (&s_pparms);

        /* Ensure no obvious damage done */
        if (s_checksum != CM_CHECKSUM)
            cm_error (CM_FATAL, "Badly behaved function %s", linkp->func_name);

        /* Restore position if we saved one */
        if (!save_stat) {
            if ((save_stat = marc_restore_pos (S_outmp)) < 0)
                cm_error (CM_FATAL, "Error %d from marc_restore_pos",
                          save_stat);
        }

        /* Interpret return code */
        switch (retcode) {

            case CM_STAT_OK:
                /* Point to next proc on list */
                linkp = linkp->true_nextp;
                break;

            case CM_STAT_IF_FAILED:
                /* Evaluation of conditional requires jump to false branch */
                linkp   = linkp->false_nextp;
                retcode = 0;
                break;

            default:
                /* All other cases are handled by the caller
                 *   or ignored, as the caller chooses
                 */
                break;
        }
    } while (linkp && !retcode);

    /* If new length desired, return it */
    if (newlen)
        *newlen = strlen ((char *) s_bufp);

    return (retcode);

} /* exec_proc */


/************************************************************************
* report ()                                                             *
*                                                                       *
*   DEFINITION                                                          *
*       Report results of run.                                          *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void report ()
{
    char sepbuf[256];   /* Line buffer          */
    FILE *logfp;        /* Log file             */


    /* Open logfile */
    if ((logfp = fopen (S_parms.logfile, "a")) == NULL) {
        fprintf (stderr, "Error opening logfile \"%s\", using stderr\n",
                 S_parms.logfile);
        logfp = stderr;
    }

    /* Create date/time stampe */
    get_log_time (sepbuf);
    fprintf (logfp, "%s", sepbuf);

    /* Report */
    fprintf (logfp, "      Input records: %7ld\n", S_in_recs);
    fprintf (logfp, "     Output records: %7ld\n", S_out_recs);
    fprintf (logfp, "           Warnings: %7d\n", S_warns);
    fprintf (logfp, "             Errors: %7d\n", S_errs);

    if (logfp != stderr) {

        fclose (logfp);

        /* Alert user */
        fprintf (stderr, "See logfile \"%s\" for final report\n",
                 S_parms.logfile);
    }

} /* report */


/************************************************************************
* open_ctl_file ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Open a control file of any type, checking for errors and        *
*       re-setting the line number counter for error reporting.         *
*                                                                       *
*   PASS                                                                *
*       Name of file to load.                                           *
*       Pointer to place to put open file pointer.                      *
*                                                                       *
*   RETURN                                                              *
*       Void.  Abort here if errors.                                    *
************************************************************************/

void open_ctl_file (
    char *fname,                /* File name                */
    FILE **fpp                  /* Put file pointer here    */
) {
    char cpath[FILENAME_MAX];   /* Filename with path       */


    /* Test filename */
    if (!fname || !*fname)
        cm_error (CM_FATAL, "Null control file name");

    /* Save the filename for cm_error reporting */
    strcpy (S_ctlfile, fname);

    /* Open file */
    if ((*fpp = fopen (fname, "r")) == NULL) {

        /* File not found in current directory, try alternate */
        sprintf (cpath, "%s/%s", S_parms.ctlpath, fname);
        *fpp = fopen (cpath, "r");

        if (*fpp == NULL) {
            perror (fname);
            cm_error (CM_FATAL, "Unable to open control file \"%s\"", fname);
        }
    }

    /* Set line number counter for error reporting */
    S_line_num = 0;

} /* open_ctl_file */


/************************************************************************
* close_ctl_file ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*      Close control file, resetting filename and line counters used    *
*      in reporting errors in the file.                                 *
*                                                                       *
*   PASS                                                                *
*       Pointer to open file pointer.                                   *
*           We use a pointer to pointer so that we can null the         *
*           pointer as a safety feature.                                *
*                                                                       *
*   RETURN                                                              *
*       Void.  Abort here if errors.                                    *
************************************************************************/

void close_ctl_file (
    FILE **fpp              /* Pointer to file pointer  */
) {
    if (fclose (*fpp) != 0) {
        perror ("Closing file");
        cm_error (CM_FATAL, "Error closing %s", S_ctlfile);
    }

    /* In case caller tries to use it again */
    *fpp = NULL;

    /* Turn off filename and line number reporting in cm_error */
    *S_ctlfile = '\0';
    S_line_num = 0;

} /* close_ctl_file */


/************************************************************************
* load_switch_file ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       Load a switches file containing switch values that may be       *
*       tested during interpretation of the control table.              *
*                                                                       *
*   PASS                                                                *
*       Name of file to load.                                           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/

static int load_switch_file (
    char   *fname                   /* Control file name        */
) {
    FILE   *sfp;                    /* Ptr to switch file       */
    char   *keyp,                   /* Ptr to left side of line */
           *valpp[CM_CTL_MAX_VALS]; /* Array -> value parts     */
    unsigned char *bufp;            /* Dummy for get_named_buf()*/
    int    val_count;               /* Number of value parts    */
    size_t buflen;                  /* Dummy for get_named_buf()*/
    CM_PROC_PARMS pparm;            /* Dummy for buf_write()    */


    /* Null switch file is no error */
    if (!fname || !*fname)
        return 0;

    /* Open switch file */
    open_ctl_file (fname, &sfp);

    /* Read all lines from switch file
     * Switch lines have format:
     *   switchname = value
     * There can be only one value per switch, but we use the
     *   same line parser used for the control file which can
     *   support multiple, slash separated, values
     */
    while (get_key_line (sfp, &keyp, valpp, &val_count) == 0) {

        /* Validate */

        /* Leading '&' */
        if (*keyp != '&')
            cm_error (CM_ERROR, "switch \"%s\" missing leading '&'", keyp);
        /* Unique switch? */
        if (cmp_get_named_buf (keyp, &bufp, 0, 0, &buflen) == CM_STAT_OK)
            cm_error (CM_ERROR, "switch \"%s\" not unique", keyp);

        /* Too many values? */
        if (val_count > 1)
            cm_error (CM_ERROR,
                      "switch \"%s\" has multiple / separated values", keyp);

        /* Add switch and value to named buffer area */
        cmp_buf_write (&pparm, keyp, (unsigned char *) valpp[0],
                       strlen (valpp[0]), 0);
    }

    close_ctl_file (&sfp);

    /* Return count of errors */
    return (S_errs);

} /* load_switch_file */


/************************************************************************
* load_ctl_file ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       Load a control file containing conversion control information   *
*                                                                       *
*   PASS                                                                *
*       Name of file to load.                                           *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/

#define MAX_IFNEST  64      /* Depth of nesting allowed */

static int load_ctl_file (
    char *fname                     /* Control file name        */
) {
    FILE     *cfp;                  /* Ptr to control file      */
    CM_PROC  *newprocp,             /* Ptr to new proc block    */
             **procpp,              /* Ptr to place to put ptr  */
             *cond_stk[MAX_IFNEST]; /* Stack tracking open ifs  */
    CM_FIELD *curfdp;               /* Current field ctl block  */
    CM_SF    *cursfp;               /* Current sf ctl block     */
    CM_LVL   level;                 /* Record, field, or sf     */
    CM_TOK   token;                 /* Control file line type   */
    CM_CND   condition;             /* Controls if/else/endif   */
    int      i,                     /* Loop counter             */
             val_count,             /* Number of value parts    */
             tvalue,                /* Token value              */
             if_nest,               /* Index into cond_stack    */
             in_field_id,           /* Input field tag number   */
             range,                 /* For field ranges         */
             in_sf_id;              /* Elhill subelement pos    */
    char     *keyp,                 /* Ptr to left side of line */
             else_nest[MAX_IFNEST], /* Stop unmatched if/else...*/
            *valpp[CM_CTL_MAX_VALS];/* Array -> value parts     */

    /* Ptr to last proc created, before this one
     * Used when processing "else" and "endif" statements in
     *   order to find the statement before an else which should be
     *   linked to the statement at the endif.
     */
    static CM_PROC *s_lastprocp;


    /* Silence warnings */
    procpp = NULL;
    curfdp = NULL;
    cursfp = NULL;
    in_field_id = 0;

    /* Null control file is useless, but not an error
     * Without a control file, marcconv copies input to ouput, slowly.
     */
    if (!fname || !*fname)
        return 0;

    /* Open control file */
    open_ctl_file (fname, &cfp);

    /* Initialize stack index of open conditionals to none open
     * Will stack information each time an if or else is encountered:
     *   Ptr to cm_proc struct requiring backpatch.
     *   Number of else's encountered.  Only 0 or 1 allowed.
     * Endif pops stack.
     * If else_nest[...] > 1 then too many "else" after if
     * If stack underflow then too many endif's
     */
    if_nest = -1;

    /* Begin processing at the top, before establishing specific level */
    level = CM_LVL_START;

    /* Get a line from the control file */
    while (get_key_line (cfp, &keyp, valpp, &val_count) == 0) {

        /* Tokenize left side of line */
        if (tokenize (keyp, &token, &tvalue) != 0)
            continue;

        /* Process tokens */
        switch (token) {

            case CM_TK_PREP:
            case CM_TK_POST:
                /* Create a control block for the process */
                if ((newprocp = make_proc (valpp, &condition, val_count,
                                           level, token)) == NULL)
                    continue;

                /* Get pointer to place to receive proc block pointer */
                switch (level) {

                    case CM_LVL_START:
                        /* Error, but set procpp anyway so we can continue */
                        cm_error (CM_ERROR, "function named before session, "
                                  "record, field, or subfield scope set");
                        procpp = &S_sessprepp;
                        break;
                    case CM_LVL_SESSION:
                        procpp = (token == CM_TK_PREP) ?
                                    &S_sessprepp : &S_sesspostp;
                        break;
                    case CM_LVL_RECORD:
                        procpp = (token == CM_TK_PREP) ?
                                    &S_recprepp : &S_recpostp;
                        break;
                    case CM_LVL_FIELD:
                        procpp = (token == CM_TK_PREP) ?
                                    &curfdp->prepp : &curfdp->postp;
                        break;
                    case CM_LVL_SF:
                        procpp = (token == CM_TK_PREP) ?
                                    &cursfp->prepp : &cursfp->postp;
                        break;
                }

                /* Find last element of chain */
                while (*procpp)
                    procpp = &((*procpp)->true_nextp);

                /* Install pointer to new proc block */
                *procpp = newprocp;

                /* Maintain stack for testing syntax of conditionals */
                switch (condition) {

                    case CM_CND_NONE:
                        break;

                    case CM_CND_IF:
                        /* Increment nesting level */
                        ++if_nest;
                        if (if_nest >= MAX_IFNEST)
                            cm_error (CM_FATAL, "If nest overflow");

                        /* Save address of proc to backpatch false_nextp */
                        cond_stk[if_nest]  = newprocp;

                        /* Track count of else's encountered after if */
                        else_nest[if_nest] = 0;

                        break;

                    case CM_CND_ELSE:
                        /* Only one else allowed after if */
                        if (++else_nest[if_nest] > 1)
                            cm_error (CM_ERROR, "Else without if");

                        /* Backpatch false branch of previous if to here */
                        cond_stk[if_nest]->false_nextp = newprocp;

                        /* Setup last proc just before else for backpatch
                         *   to endif
                         */
                        cond_stk[if_nest] = s_lastprocp;

                        break;

                    case CM_CND_ENDIF:
                        /* Must be preceding if
                         * Otherwise report error but fix situation
                         *   so compile can continue
                         */
                        if (if_nest < 0) {
                            cm_error (CM_ERROR, "Endif without if");
                            if_nest = 0;
                        }

                        /* Backpatch if or else, whichever is on stack
                         * "if" fail condition points to here.
                         * "else" true condition points to here
                         *   (skipping code only executing if previous
                         *   "if" failed)
                         */
                        if (else_nest[if_nest])
                            cond_stk[if_nest]->true_nextp = newprocp;
                        else
                            cond_stk[if_nest]->false_nextp = newprocp;

                        --if_nest;
                }

                /* Remember newly created proc in case it's before an else */
                s_lastprocp = newprocp;

                break;

            case CM_TK_SESSION:
                /* Set current level for procs */
                level = CM_LVL_SESSION;
                break;

            case CM_TK_RECORD:
                /* Set current level for procs */
                level = CM_LVL_RECORD;
                break;

            case CM_TK_FIELD:
                /* Should have received one value with that */
                if (val_count != 1) {
                    cm_error (CM_ERROR, "field statement requires field "
                              "id/tag");
                    valpp[0] = "000";
                    val_count = 1;
                }

                /* Must have digits or X's */
                for (i=0; i<3; i++) {
                    if (!isdigit(valpp[0][i]) &&
                           valpp[0][i] != 'X' &&
                           valpp[0][i] != 'x')
                        cm_error (CM_ERROR, "Non-digit, non-X in field id %s",
                                  valpp[0]);
                }

                /* Create a field control block */
#ifdef DEBUG
                if ((curfdp = (CM_FIELD *) marc_calloc (sizeof(CM_FIELD), 1, 1001)) == NULL) {  //TAG:1001
                    cm_error (CM_FATAL, "Field control memory");
                }
#else
                if ((curfdp = (CM_FIELD *) calloc (sizeof(CM_FIELD), 1)) == NULL) {
                    cm_error (CM_FATAL, "Field control memory");
                }
#endif

                /* Get source and target field ids */
                in_field_id = atoi (valpp[0]);

                /* Deal with special forms nnX and nXX
                 * These are ways to specify a range of input fields
                 * Examples:
                 *    98X = 980..989
                 *    9XX = 900..999
                 * We handle them by setting multiple field pointers to
                 *   the current field proc control.
                 */
                range = 1;
                if (valpp[0][2] == 'X' || valpp[0][2] == 'x') {

                    /* Handle nnX in field spec */
                    curfdp->x_count = 1;
                    range        = 10;
                    in_field_id *= 10;

                    /* Handle nXX in field spec */
                    if (valpp[0][1] == 'X' || valpp[0][1] == 'x') {
                        curfdp->x_count = 2;
                        range        = 100;
                        in_field_id *= 10;    /* id now * 100 */
                    }

                    /* XXX in field spec - apply to all varlen fields */
                    if (valpp[0][0] == 'X' || valpp[0][0] == 'x') {
                        curfdp->x_count = 3;
                        in_field_id = 10;
                        range = 990;
                    }
                }

                /* Validate */
                if (in_field_id < 0 || in_field_id > 999)
                    cm_error (CM_ERROR, "Invalid input field %d",
                              in_field_id);

                /* Install pointer in array
                 * If nnX, nXX, or XXX then install 10, 100, 990
                 *    pointers to one proc structure
                 * But preserve priority of specific numbers over general:
                 *   If 9XX
                 *     97X overrides 970..979
                 *   975 overrides 975 in 97X or 9XX
                 * Never allow vice versa to occur
                 * Note: most common case is simple range==1
                 */
                for (i=in_field_id; i<in_field_id+range; i++) {
                    if (!S_fieldp[i])
                        S_fieldp[i] = curfdp;
                    else {
                        if (S_fieldp[i]->x_count > curfdp->x_count)
                            S_fieldp[i] = curfdp;
                        else if (S_fieldp[i]->x_count == curfdp->x_count)
                            cm_error (CM_ERROR,
                                      "Duplicate input field id = %d", i);
                    }
                }

                /* Set context for subsequent statements */
                level = CM_LVL_FIELD;

                break;

            case CM_TK_INDIC:
            case CM_TK_SF:
                /* Validate input subfield id.
                 * We'll leave output unvalidated since we are
                 *   treating positions in unsubfielded fields as
                 *   if they were subfield numbers.
                 * Rules for validation are very complex.
                 */
                if (val_count != 1) {
                    cm_error (CM_ERROR, "subfield statement requires "
                              "subfield id/code");
                    valpp[0] = "1";
                    val_count = 1;
                }
                in_sf_id = *valpp[0];

                /* Indicator "subfield" codes are not printable chars */
                if (token == CM_TK_INDIC) {
                    if (in_field_id < 10)
                        cm_error (CM_ERROR, "Indicators only on variable "
                                            "fields");
                    if (in_sf_id == '1')
                        in_sf_id = MARC_INDIC1;
                    else if (in_sf_id == '2')
                        in_sf_id = MARC_INDIC2;
                    else {
                        cm_error (CM_ERROR, "Only valid indicator numbers "
                                            "are 1 and 2");
                        /* Force successful compile - see comment below */
                        in_sf_id = MARC_INDIC1;
                    }
                }

                /* Check for valid subfield code */
                else if (in_field_id >= 10) {
                    if (marc_ok_subfield (in_sf_id) != 0) {
                        cm_error (CM_ERROR, "Invalid sf code = %c", in_sf_id);

                        /* In order to complete compile, we'll just
                         *   overwrite an existing one.  Doesn't
                         *   matter since we won't execute anyway.
                         */
                        in_sf_id = 'a';
                    }
                }

                /* Point to subfield control block */
                cursfp = &curfdp->sf[get_sf_ctl (in_sf_id)];

                /* Set context for subsequent statements */
                level = CM_LVL_SF;
        }
    }

    /* Done with control file */
    close_ctl_file (&cfp);

    /* Return count of errors */
    return (S_errs);

} /* load_ctl_file */


/************************************************************************
*   make_proc ()                                                        *
*                                                                       *
*   DEFINITION                                                          *
*       Create and initialize a procedure control block.                *
*                                                                       *
*   PASS                                                                *
*       Pointer to array of pointers to components.                     *
*           First component (part[0]) is procedure name.                *
*           Additional components are optional arguments.               *
*       Pointer to place to put pointer to condition info.              *
*           Caller needs this to keep track of if/else/endif            *
*           groupings of procedure calls.                               *
*       Count of total passed components.                               *
*       Current processing level: record, field or subfield.            *
*       Current procedure position: pre or post-process.                *
*                                                                       *
*                                                                       *
*   RETURN                                                              *
*       Pointer to initialized block.                                   *
************************************************************************/

static CM_PROC *make_proc (
    char    **part,         /* Ptr to array of part ptrs*/
    CM_CND  *condition,     /* Controls if/else/endif   */
    int     count,          /* Number of parts          */
    CM_LVL  level,          /* Record, field or subfield*/
    int     ppos            /* Procedure position       */
) {
    int     i,              /* Loop counter             */
            arg_min,        /* Min arg count for proc   */
            arg_max,        /* Max arg count            */
            flags,          /* Bit flags from func table*/
            valid_pos;      /* Valid position bit flag  */
    CM_PROC *pp;            /* Ptr to new proc block    */


    /* Allocate block */
#ifdef DEBUG
    if ((pp = (CM_PROC *) marc_calloc (sizeof(CM_PROC), 1, 1002)) == NULL) {  // TAG:1002
        cm_error (CM_FATAL, "Procedure block memory");
    }
#else
    if ((pp = (CM_PROC *) calloc (sizeof(CM_PROC), 1)) == NULL) {
        cm_error (CM_FATAL, "Procedure block memory");
    }
#endif

    /* Lookup procedure name and convert to token */
    if ((pp->procp = cmp_lookup_proc (part[0], condition, &arg_min,
                                      &arg_max, &flags)) == NULL)
        cm_error (CM_ERROR, "Unknown procedure %s", part[0]);

    /* Validate argument count, args are in part[1], part[2] ... */
    if (arg_min > count - 1)
        cm_error (CM_ERROR, "Insufficient arguments for function %s",
                  part[0]);
    if (arg_max < count - 1)
        cm_error (CM_ERROR, "Too many arguments for function %s",
                  part[0]);

    /* Create bit flag to validate positioning */
    valid_pos = 0;
    switch (level) {
        case CM_LVL_RECORD: valid_pos = CMP_RE; break;
        case CM_LVL_FIELD:  valid_pos = CMP_FE; break;
        case CM_LVL_SF:     valid_pos = CMP_SE; break;
        default: break;
    }
    if (ppos == CM_TK_POST)
        valid_pos <<= 1;

    /* Perform position validation */
    if (!(flags | valid_pos))
        cm_error (CM_ERROR, "Can't use this procedure in this position");

    /* Deep copy of function name */
    if ((pp->func_name = strdup (part[0])) == NULL)
        cm_error (CM_FATAL, "Procedure name memory");

    /* Deep copy of args */
    for (i=0; i<count-1; i++) {
        if (i >= CM_MAX_ARGS) {
            cm_error (CM_ERROR, "Exceeded max argument count of %d",
                      CM_MAX_ARGS);
            break;
        }
        if ((pp->args[i] = strdup (part[i+1])) == NULL)
            cm_error (CM_FATAL, "Procedure argument memory");
    }
    pp->arg_count = count - 1;

    /* Initialize next pointers
     * Not necessary, but aids debugging
     */
    pp->true_nextp = pp->false_nextp = NULL;

    return (pp);

} /* make_proc */


/************************************************************************
* tokenize ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Convert the left side of a configuration line to a token        *
*       and value.                                                      *
*                                                                       *
*   PASS                                                                *
*       Pointer to data to tokenize.                                    *
*       Pointer to place to put token.                                  *
*       Pointer to place to put value.                                  *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
************************************************************************/
static int tokenize (
    char   *keyp,           /* Ptr to key to analyze    */
    CM_TOK *tokenp,         /* Put token here           */
    int    *valp            /* Put value here           */
) {
    char   *p;              /* For reading keyword      */
    int    retcode;         /* Return code              */


    /* Okay to start */
    retcode = 0;

    /* Default value */
    *valp = 0;

    /* Search for known tokens
     * This could have been done via a table search, but it
     *   started out with fewer tokens and inline code seemed
     *   more attractive at the time
     */
    if (!strncmp (keyp, "prep", 4)) {

        /* Set token and clear from further search */
        *tokenp = CM_TK_PREP;
        keyp    = NULL;
    }
    else if (!strncmp (keyp, "post", 4)) {
        *tokenp = CM_TK_POST;
        keyp    = NULL;
    }
    else if (!strncmp (keyp, "session", 7)) {
        *tokenp = CM_TK_SESSION;
        keyp    = NULL;
    }
    else if (!strncmp (keyp, "record", 6)) {
        *tokenp = CM_TK_RECORD;
        keyp    = NULL;
    }
    else if (!strncmp (keyp, "field", 5))
        *tokenp = CM_TK_FIELD;
    else if (!strncmp (keyp, "subfield", 8))
        *tokenp = CM_TK_SF;
    else if (!strncmp (keyp, "indicator", 9))
        *tokenp = CM_TK_INDIC;

    else {
        p = keyp;
        while (isalnum(*p))
            ++p;
        *p = '\0';
        cm_error (CM_ERROR, "Unknown keyword \"%s\"", keyp);
        retcode = -1;
    }

    /* If we need a value, get anything after keyword + whitespace
     *   plus equal sign
     */
    if (keyp) {
        p = keyp;
        while (isalnum(*p))
            ++p;
        while (isspace(*p) || *p == '=')
            ++p;
        *valp = atoi (keyp);
    }

    return (retcode);

} /* tokenize */


/************************************************************************
* get_ctl_line ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Read one line of a control file, eliminating comments, blank    *
*       lines, and leading and trailing whitespace.                     *
*                                                                       *
*       Used here and elsewhere to read control tables.                 *
*                                                                       *
*   ASSUMPTIONS                                                         *
*       Caller must use the data before calling again.  Only one        *
*       buffer is used for input lines.                                 *
*                                                                       *
*   PASS                                                                *
*       Pointer to open input control file.                             *
*       Pointer to place to put pointer to line.                        *
*                                                                       *
*   RETURN                                                              *
*       Non-zero = success.                                             *
*       0        = End of file.                                         *
************************************************************************/

int get_ctl_line (
    FILE *fp,           /* Read from this open file                 */
    char **linep        /* Put pointer to start of read data here   */
) {
    char *startp,       /* Ptr to start of real data on line        */
         *endp;         /* Ptr to end of real data                  */

    /* Buffer for one line.  Need static because we return ptrs into it */
    static char s_linebuf[CM_CTL_MAX_LINE];


    /* Nothing found yet */
    *linep = NULL;

    /* Read lines until got one we want or eof */
    while (fgets (s_linebuf, CM_CTL_MAX_LINE, fp)) {

        ++S_line_num;

        /* Eliminate trailing comments from line */
        if ((endp = strchr (s_linebuf, CM_CTL_COMMENT)) != NULL)
            *endp = '\0';

        /* Trim whitespace from front */
        startp = s_linebuf;
        while (*startp && (*startp == ' ' || *startp == '\t'))
            ++startp;

        /* If nothing but whitespace, fetch another line */
        if (!isprint (*startp))
            continue;

        /* Trim whitespace and newline from end */
        /* Already know there's something there */
        endp = startp + strlen (startp) - 1;
        while (*endp == '\n' || *endp == ' ' || *endp == '\t')
            --endp;
        *(endp + 1) = '\0';

        /* We've got it */
        *linep = startp;
        break;
    }

    /* If nothing found, check for error or EOF */
    if (!*linep) {
        if (!feof (fp))
            cm_error (CM_FATAL, "Error reading control file");
        return 0;
    }

    return 1;

} /* get_ctl_line */


/************************************************************************
* get_key_line ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Read one line of a control file, parsing all of the elements    *
*       on it.                                                          *
*                                                                       *
*       Elements of control file contain lines of key=value where       *
*       key and value are strings with embedded '/' separating the      *
*       key and value components.                                       *
*                                                                       *
*       Ignore blank or comment lines.                                  *
*                                                                       *
*       This would be easier with lex and yacc, but it's easy enough    *
*       without them and we'll avoid the extra configuration            *
*       issues of using them.                                           *
*                                                                       *
*   PASS                                                                *
*       Pointer to open input control file.                             *
*       Pointer to place to put pointer to left, "key" side of line.    *
*       Pointer to array of pointers to right "value" side of line.     *
*           Array must be at least CM_CTL_MAX_OBJS long.                *
*       Pointer to place to put count of object components,             *
*                                                                       *
*   RETURN                                                              *
*       0 = Success.                                                    *
*       EOF = End of file.                                              *
************************************************************************/

static int get_key_line (
    FILE *fp,               /* Input control file                       */
    char **keypp,           /* Put ptr to parsed key component here     */
    char **valpp,           /* Put ptrs to parsed value components here */
    int  *val_countp        /* Put count here                           */
) {
    char *startkp,          /* Start of key data on line                */
         *startvp,          /* Start of value after key                 */
         *endp;             /* End of data on line                      */


    /* Get a line of control info, w/o comments or whitespace */
    if (!get_ctl_line (fp, &startkp))
        return EOF;

    /* Find end of key
     * Switch definitions have a leading '&' followed by alnums
     * All the rest are pure alphanumeric
     */
    endp = startkp;
    if (*endp == '&')
        ++endp;
    while (isalnum(*endp))
        ++endp;

    /* Find start of value */
    startvp = endp;
    while (isspace(*startvp) || *startvp == '=')
        ++startvp;

    /* Terminate key portion */
    *endp = '\0';

    /* Return pointer to key portion */
    *keypp = startkp;

    /* Parse the value, looking for separators */
    parse_ctl (startvp, valpp, val_countp);

    return 0;

} /* get_key_line */

/************************************************************************
* parse_ctl ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Parse one side of a line of a control file, dividing it up      *
*       into separate components based on a separator.                  *
*                                                                       *
*       Subroutine of get_key_line().                                   *
*                                                                       *
*   PASS                                                                *
*       Pointer to string to parse.                                     *
*       Pointer to array of pointers to components.                     *
*           Fill in the array.                                          *
*       Pointer to place to put count of components,                    *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void parse_ctl (
    char *string,           /* String to parse      */
    char **arrayp,          /* Fill in ptrs here    */
    int  *countp            /* Put count here       */
) {
    char *p;                /* Pointer into string  */
    int  count,             /* Component count      */
         inquotes;          /* True=in a string     */


    /* Start at beginning */
    p        = string;
    count    = 0;
    inquotes = 0;

    /* Loop until end */
    for (;;) {

        /* Point to start of one component */
        arrayp[count] = p;
		++count;

        /* Skip to next separator or end of input string
         * Slash is separator, unless it's inside a quoted string
         * Might add a convention for escaping quote marks XXXX
         */
        while (*p && (*p != CM_CTL_SEPARATOR || inquotes)) {

            /* If quote mark, toggle in a string or not */
            if (*p == '"')
                inquotes = 1 - inquotes;
            ++p;
        }

        /* If at end of string, we're done */
        if (!*p)
            break;

        /* Else terminate previous component and advance to next */
        *p++ = '\0';

        /* Final slash terminator might have been used to allow
         *   spaces at the end of the last parm.
         * If so, it's also an end of string.
         */
        if (!*p)
            break;
    }

    /* Tell caller how many we found */
    *countp = count;

} /* parse_ctl */


/************************************************************************
* get_sf_ctl ()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       Return an index of the subfield control corresponding to        *
*       a particular subfield within a CM_FIELD structure.              *
*                                                                       *
*       Changes things to make '!' (first char after space) the         *
*       0th index into the array of up to 94 printable ASCII's.         *
*                                                                       *
*       Indicator 1 = 0                                                 *
*       Indicator 2 = 1                                                 *
*       '!' = 2.                                                        *
*       '"' = 3.                                                        *
*       '#' = 4.                                                        *
*       etc.                                                            *
*                                                                       *
*   PASS                                                                *
*       Subfield code.                                                  *
*                                                                       *
*   RETURN                                                              *
*       Position of subfield in 0..94.                                  *
************************************************************************/

static int get_sf_ctl (
    int sf_id               /* Input subfield code      */
) {
    /* Indicator 1 or 2 is first */
    if (sf_id == MARC_INDIC1)
        return 0;
    if (sf_id == MARC_INDIC2)
        return 1;

    /* 94 printables follow.
     * Currently, we are accepting every printable ASCII character
     *   from 33 ('!') to 126 ('~').
     */
    if (marc_ok_subfield (sf_id) == 0)
        return (sf_id - 34);

    cm_error (CM_ERROR, "get_sf_ctl: Invalid subfield code (int) %d", sf_id);
    return 'X';

} /* get_sf_ctl */


/************************************************************************
* cm_error ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Display and log messages.                                       *
*       Exit program if fatal error.                                    *
*                                                                       *
*       This non-static routine may be called by the custom procedures  *
*       as well as by routines in this file.                            *
*                                                                       *
*   PASS                                                                *
*       Variable arguments in vsprintf format.                          *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

void cm_error (
    CM_SEVERITY severity,   /* Fatal or warning             */
    char        *fmt,       /* Printf format for message    */
    ...                     /* Additional vsprintf args     */
) {
    va_list args;           /* Ptr to first variable arg.   */
    char    hdrbuf[256],    /* Message header               */
            msgbuf[CM_MAX_LOG_MSG], /* Message buffer       */
            *notep,         /* Note to add into msgbuf      */
            *uip,           /* Ptr to record UI             */
            *fmtp,          /* Output print format          */
            sepbuf[256];    /* For separator and time string*/
    size_t  buflen;         /* Length of buffer at uip      */
    FILE    *logfp;         /* Log file                     */

    static int s_first_time = 1;


    /* Create a message string */
    va_start (args, fmt);
    vsprintf (msgbuf, fmt, args);
    va_end (args);

    /* Overflow?  Might have already crashed.  If not, say what happened */
    if (strlen (msgbuf) >= CM_MAX_LOG_MSG)
        cm_error (CM_FATAL, "Log message too big, message buffer overflow");

    /* Create message file line or record number prefix */
    *hdrbuf = '\0';
    if (S_line_num)
        sprintf (hdrbuf, "%s(%d) : ", S_ctlfile, S_line_num);

    else if (S_in_recs) {

        /* Start with input record number */
        sprintf (hdrbuf, "Input rec# %ld : ", S_in_recs);

        /* If there's a UI, add it to the header */
        uip = NULL;

        /* Look for Bib ID first */
        if (cmp_get_named_buf ("bibid", (unsigned char **) &uip, 0, 0, &buflen)
                              == CM_STAT_OK) {
            if (*uip)
                strcat (hdrbuf, "BibID=");
            else
                uip = NULL;
        }

        /* If no Bib ID, try for UI */
        if (!uip) {
            if (cmp_get_named_buf ("ui", (unsigned char **) &uip,
                     0, 0, &buflen) == CM_STAT_OK) {
                if (*uip)
                    strcat (hdrbuf, "UI=");
                else
                    uip = NULL;
            }
        }

        /* If we have something, print it */
        if (uip) {
            strcat (hdrbuf, uip);
            strcat (hdrbuf, " : ");
        }
    }

    /* Count and check occurrences */
    if (severity == CM_WARNING)
        ++S_warns;
    else if (severity == CM_ERROR) {
        if (++S_errs > S_parms.max_errs)
            severity = CM_FATAL;
    }

    /* Set ouput print format for console and file */
    fmtp = "-----\n%s\n  %s\n";

    /* Add severity indicator */
    notep = NULL;
    switch (severity) {
        case CM_CONTINUE: notep = ""; *hdrbuf = '\0';
                          fmtp = "%s  %s\n";   break;
        case CM_NO_ERROR: notep = "";          break;
        case CM_WARNING:  notep = "Warning: "; break;
        case CM_ERROR:    notep = "Error: ";   break;
        case CM_FATAL:    notep = "Fatal error: ";
    }
    strcat (hdrbuf, notep);

    /* Output to console */
    fprintf (stderr, fmtp, hdrbuf, msgbuf);

    /* Open in current directory */
    if ((logfp = fopen (S_parms.logfile, "a")) != NULL) {

        /* If first time in run, print separator, date and time */
        if (s_first_time) {
            get_log_time (sepbuf);
            fprintf (logfp, "%s", sepbuf);
            s_first_time = 0;
        }

        /* Print header, and message */
        fprintf (logfp, fmtp, hdrbuf, msgbuf);

        /* Abort message */
        if (severity == CM_FATAL)
            fprintf (logfp, "Aborting after %d errors\n", S_errs);

        /* Close/flush */
        fclose (logfp);
    }

    /* Fatal error? */
    if (severity == CM_FATAL) {

        /* Perform any cleanup we can and exit */
        report ();
        exit (1);
    }
} /* cm_error */


/************************************************************************
* get_errs ()                                                           *
*                                                                       *
*   DEFINITION                                                          *
*       Retrieve the current error count.                               *
*                                                                       *
*   PASS                                                                *
*       Void.                                                           *
*                                                                       *
*   RETURN                                                              *
*       Count of errors.                                                *
************************************************************************/

int get_errs ()
{
    return S_errs;
}


/************************************************************************
* get_log_time ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Generate a string containing a separator and time stamp         *
*       for logging purposes.                                           *
*                                                                       *
*   PASS                                                                *
*       Pointer to buffer to receive string - must be long enough.      *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void get_log_time (
    char *bufp      /* Put separator + time string here */
) {
    time_t ltime;   /* Time in seconds                  */

    /* Print separator followed by time string */
    sprintf (bufp, "=======================\n");
    bufp += strlen (bufp);
    time (&ltime);
    sprintf (bufp, "%s\n", ctime (&ltime));

} /* get_log_time */


/************************************************************************
* get_parms ()                                                          *
*                                                                       *
*   DEFINITION                                                          *
*       Get command line parameters.                                    *
*                                                                       *
*   PASS                                                                *
*       Pointer to parameter structure to fill in.                      *
*       Number of args.                                                 *
*       Ptr to array of args.                                           *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
*       Abort if error.                                                 *
************************************************************************/

static void get_parms (
    CM_PARMS *parmp,        /* Ptr to parm struct                   */
    int      argc,          /* From main                            */
    char     **argv         /* From main                            */
) {
    int      opt;           /* Individual option letter or position */
    char     *argptr;       /* Ptr to info associated with option   */


    /* Default parameters */
    parmp->omode     = "wb";
    parmp->skip_recs = 0L;
    parmp->conv_recs = 999999999L;
    parmp->max_errs  = CM_DFT_MAX_ERRORS;
    parmp->logfile   = CM_DFT_LOGFILE;
    parmp->outfile   = NULL;
    parmp->ctlpath   = ".";

    /* Process each command line arg */
    while ((opt = amuopt (argc, argv, "ae:l:n:p:s:?h", &argptr))
               != AMU_OPT_DONE) {

        switch (opt) {
            case 'a':
                /* Append to parmameters */
                parmp->omode = "ab";
                break;

            case 'e':
                /* New max errors */
                parmp->max_errs = atoi (argptr);
                break;

            case 'l':
                /* Change logfile name */
                parmp->logfile = strdup (argptr);
                break;

            case 'n':
                /* Convert this many records */
                parmp->conv_recs = atol (argptr);
                break;

            case 'p':
                /* Alternate path for all control tables */
                parmp->ctlpath = strdup (argptr);
                break;

            case 's':
                /* Skip this many before conversion */
                parmp->skip_recs = atol (argptr);
                break;

            case 1:
                /* Input marc record file */
                parmp->infile = strdup (argptr);
                break;

            case 2:
                /* Output marc record file */
                parmp->outfile = strdup (argptr);
                break;

            case 3:
                /* Control file name */
                parmp->ctlfile = strdup (argptr);
                break;

            case 4:
                /* Name of switches file */
                parmp->swfile = strdup (argptr);
                break;

            case '?':
            case 'h':
                usage (NULL);

            default:
                usage ("Unrecognized argument");
        }
    }

    /* Did we get all required args */
    if (!parmp->infile || !parmp->outfile)
        usage ("Insufficient arguments");

} /* get_parms */


/************************************************************************
* usage ()                                                              *
*                                                                       *
*   DEFINITION                                                          *
*       Display message with usage.                                     *
*                                                                       *
*   PASS                                                                *
*       Message, or NULL if none.                                       *
*                                                                       *
*   RETURN                                                              *
*       Void.                                                           *
************************************************************************/

static void usage (
    char *msg           /* Display this */
) {
  if (msg)
      fprintf (stderr, "%s\n\n", msg);

  fprintf (stderr, "MaRC->MaRC conversion\n");
  fprintf (stderr, "usage: marcconv {options} infile outfile "
                   "{ctlfile} {swfile}\n");
  fprintf (stderr, "    infile  = Name input sequential MaRC file\n");
  fprintf (stderr, "    outfile = Name output sequential MaRC file\n");
  fprintf (stderr, "    ctlfile = Optional name of conversion control file\n");
  fprintf (stderr, "    swfile  = Optional name of file of \"switches\"\n");
  fprintf (stderr, "  options:\n");
  fprintf (stderr, "    -a      = Append to output file, else overwrite\n");
  fprintf (stderr, "    -e<num> = Max allowed errs, default=%d\n",
                                      CM_DFT_MAX_ERRORS);
  fprintf (stderr, "    -l<str> = Error log file, default=%s\n",
                                      CM_DFT_LOGFILE);
  fprintf (stderr, "    -n<num> = Num records to convert, default=all\n");
  fprintf (stderr, "    -p<str> = Alternate path to ctl files if not "
                                  "in current directory\n");
  fprintf (stderr, "    -s<num> = Starting record number, default=0\n");

  exit (1);

} /* usage */

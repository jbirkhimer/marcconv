/*********************************************************************
* marcconv.h                                                         *
*                                                                    *
*   PURPOSE                                                          *
*       Header file for National Library of Medicine MaRC->MaRC      *
*       conversion program.                                          *
*                                                                    *
*                                               Author: Alan Meyer   *
*********************************************************************/

/* $Id: marcconv.h,v 1.10 1999/12/14 14:48:35 vereb Exp vereb $
 * $Log: marcconv.h,v $
 * Revision 1.10  1999/12/14 14:48:35  vereb
 * doubled size of CM_MAX_LOG_MSG to 1024
 *
 * Revision 1.9  1999/10/21 19:25:51  vereb
 * Added CM_CONTINUE for call to cm_error
 *
 * Revision 1.9  1999/10/21 15:18  vereb
 * Added CM_CONTINUE for call to cm_error
 *
 * Revision 1.8  1999/09/21 18:01:24  vereb
 * Increased CM_CTL_MAX_LINE from 256 to 512
 *
 * Revision 1.7  1999/04/05 19:22:25  meyer
 * Added prototype for strip_quotes(), added parm struct element ctlpath
 *
 * Revision 1.6  1999/02/20 22:46:59  meyer
 * Additional prototype - get_fixed_num().
 *
 * Revision 1.5  1999/01/05 02:27:18  meyer
 * Added prototypes for global control table functions.
 *
 * Revision 1.4  1998/12/31 21:13:23  meyer
 * Added prototype for cmp_call().
 *
 * Revision 1.3  1998/12/29 02:35:23  meyer
 * Removed prototypes for specialized procs (now in marcproclist.h)
 * Added definition of struct cmp_table so multiple proc files can exist.
 *
 * Revision 1.2  1998/12/21 05:07:00  meyer
 * Removed unneeded "field_id" from CM_PROC structure.
 * Added new x_count to support nnX and nXX field ids.
 *
 * Revision 1.1  1998/12/17 16:51:44  meyer
 * Initial revision
 *
 */

#ifndef MARCCONV_H
#define MARCCONV_H

#include "marc.h"       /* Public definitions   */

/*********************************************************************
*   Constants                                                        *
*********************************************************************/

/*-------------------------------------------------------------------\
|   Configuration info                                               |
\-------------------------------------------------------------------*/
#define CM_CTL_MAX_ERRS         10  /* Max ctl file errs till abort */
#define CM_CTL_MAX_KEYS         10  /* Max ctl line key parts       */
#define CM_CTL_MAX_VALS         20  /* Max ctl line value parts     */
#define CM_CTL_MAX_LINE        512  /* Max line len in ctl file     */
#define CM_CTL_SEPARATOR       '/'  /* Between parts in proc/arg lst*/
#define CM_CTL_COMMENT         '#'  /* Comment from her to eol      */

/*-------------------------------------------------------------------\
|   Defaults                                                         |
\-------------------------------------------------------------------*/
#define CM_DFT_LOGFILE "marcconv.log" /* Default log file name      */
#define CM_DFT_MAX_ERRORS         50  /* Default max errors allowed */

/*-------------------------------------------------------------------\
| Procedure placement bit flags                                      |
|                                                                    |
|   These are or'd together to indicate where a procedure may        |
|   be legally applied.                                              |
\-------------------------------------------------------------------*/
#define CMP_EE                   1  /* Session pre-process okay     */
#define CMP_EO                   2  /*         post-process         */
#define CMP_RE                   1  /* Record pre-process okay      */
#define CMP_RO                   2  /*        post-process          */
#define CMP_FE                   4  /* Field pre-porcess            */
#define CMP_FO                   8  /*       post-process           */
#define CMP_SE                  16  /* Subfield pre-process         */
#define CMP_SO                  32  /*          post-process        */
#define CMP_ANY                 63  /* Any position okay            */


/*-------------------------------------------------------------------\
| Condition operators                                                |
|                                                                    |
|   Operators used in conditional comparisons are bound to           |
|   specific characters here.                                        |
\-------------------------------------------------------------------*/
#define CM_OP_EXISTS    '*'     /* Object exists and had data in it */
#define CM_OP_EQ        '='     /* Equality                         */
#define CM_OP_LT        '<'     /* Less than                        */
#define CM_OP_GT        '>'     /* Greater than                     */
#define CM_OP_CONTAINS  '?'     /* String is within string          */
#define CM_OP_BEGINS    '^'     /* Leading substring of string      */
#define CM_OP_NOT       '!'     /* Negation                         */
#define CM_OP_NOCASE    '~'     /* Case insensitive compare         */
#define CM_OP_NUMERIC   '9'     /* Is it a number, unary operator   */
#define CM_OP_GTE       257     /* Greater or equal, composite op>= */
#define CM_OP_LTE       258     /* Less or equal, composite op <=   */


/*-------------------------------------------------------------------\
|   Other constants                                                  |
\-------------------------------------------------------------------*/
#define CM_PROC_BUF_SIZE     16384  /* Size buffer for one field    */
#define CM_MAX_ARGS              8  /* Max Elhill positional sfs    */
#define CM_MAX_MARC_SFS         96  /* Max marc sfs, !..~ +2 indics */
#define CM_MAX_MARC_SIZE    100000  /* Biggest record we support    */
#define CM_MIN_INPUT_FIELDS      3  /* Reject record if fewer fields*/
#define CM_MAX_LOG_MSG        1024  /* Max loggable msg             */
#define CM_FID_UNUSED         (-1)  /* No field assigned to CM_FIELD*/

#define CMP_PROC_ERROR        (-1)  /* Error from custom proc       */


/********************************************************************
*   Types                                                            *
*********************************************************************/

/*-------------------------------------------------------------------\
| Token types                                                        |
|                                                                    |
|   Types of configuration control lines.                            |
\-------------------------------------------------------------------*/
typedef enum cm_tok {
    CM_TK_SESSION,              /* Start of session preps/posts     */
    CM_TK_RECORD,               /* Start of record preps/posts      */
    CM_TK_FIELD,                /* Field declaration                */
    CM_TK_SF,                   /* Subfield declaration             */
    CM_TK_INDIC,                /* Indicator declaration            */
    CM_TK_PREP,                 /* Pre-process                      */
    CM_TK_POST                  /* Post-process                     */
} CM_TOK;


/*-------------------------------------------------------------------\
| Processing levels                                                  |
|                                                                    |
|   At any point in reading a configuration file we have a context,  |
|   either session, record, field, or subfield level.                |
\-------------------------------------------------------------------*/
typedef enum cm_lvl {
    CM_LVL_START   = 0,         /* Start or end of entire session   */
    CM_LVL_SESSION = 1,         /* Start or end of entire session   */
    CM_LVL_RECORD  = 2,         /* Full record                      */
    CM_LVL_FIELD   = 4,         /* Field                            */
    CM_LVL_SF      = 8          /* Subfield                         */
} CM_LVL;


/*-------------------------------------------------------------------\
| Data source types                                                  |
|                                                                    |
|   Tell what kind of object data is coming from or going to.        |
\-------------------------------------------------------------------*/
typedef enum cm_id {
    CM_ID_UNKNOWN = 0,          /* Error, type not recognized       */
    CM_ID_MARC,                 /* To or from a MARC record         */
    CM_ID_BUF,                  /* Internal conversion buffer       */
    CM_ID_CURRENT,              /* Current data, internal accum.    */
    CM_ID_BUILTIN,              /* Descriptor of current object     */
    CM_ID_LITERAL,              /* Literal data from ctl table      */
    CM_ID_SUBSTR                /* Substring of another id/buf      */
} CM_ID;


/*-------------------------------------------------------------------\
| Condition types                                                    |
|                                                                    |
|   Used by make_proc to tell when conditionals are being            |
|   processed, i.e., if/else/endif procedure tokens.                 |
|                                                                    |
|   These are needed at compile time to backpatch the                |
|   address of the next procedure into the previous ones when        |
|   jumps occur.                                                     |
\-------------------------------------------------------------------*/
typedef enum cm_cnd {
    CM_CND_NONE = 0,            /* Ordinary proc, no if/else/endif  */
    CM_CND_IF,                  /* Conditional                      */
    CM_CND_ELSE,                /* After if                         */
    CM_CND_ENDIF                /* Ends conditional                 */
} CM_CND;


/*-------------------------------------------------------------------\
| Procedure return status codes                                      |
|                                                                    |
|   Procedures return one of these to indicate that everything       |
|   is okay, or that we have to stop or delete something.            |
\-------------------------------------------------------------------*/
typedef enum cm_stat {
    CM_STAT_OK = 0,             /* Proc executed, proceed normally  */
    CM_STAT_IF_FAILED,          /* exec_proc internal control code  */
    CM_STAT_ERROR,              /* Log error but continue           */
    CM_STAT_DONE_SF,            /* Done procs in sf chain           */
    CM_STAT_DONE_FIELD,         /* No more sfs or procs in chain    */
    CM_STAT_KILL_FIELD,         /* Delete field and end chain       */
    CM_STAT_DONE_RECORD,        /* Finished with this record        */
    CM_STAT_KILL_RECORD         /* Delete (don't output) this record*/
} CM_STAT;


/*-------------------------------------------------------------------\
| Log message severity codes                                         |
|                                                                    |
|   Tells cm_error() how to process an error message.                |
\-------------------------------------------------------------------*/
typedef enum {
    CM_FATAL,                   /* Display message, log it, and exit*/
    CM_ERROR,                   /* Display and log, but do not exit */
    CM_WARNING,                 /* Display only, no log             */
    CM_NO_ERROR,                /* No error occurred, do nothing    */
    CM_CONTINUE                 /* Display continuation line        */
} CM_SEVERITY;


/*********************************************************************
*   Structures                                                       *
*********************************************************************/

/*-------------------------------------------------------------------\
| Argument passed to a custom procedure.                             |
|                                                                    |
|   One of these structures contains all arguments accessible        |
|   to the custom proc.                                              |
\-------------------------------------------------------------------*/
typedef struct cm_proc_parms {
    MARCP  inmp;                /* Input marc record                */
    MARCP  outmp;               /* Output marc record               */
    unsigned char *bufp;        /* Ptr to current data              */
    size_t buflen;              /* Length of datap buffer           */
    char   **args;              /* Ptr to array of arg pointers     */
    int    arg_count;           /* Number of argument pointers      */
} CM_PROC_PARMS;

typedef CM_STAT (*CM_FUNCP)(struct cm_proc_parms *);

/*-------------------------------------------------------------------\
| Custom procedure control                                           |
|                                                                    |
|   Each reference to a procedure invocation in the control table    |
|   generates one of these.                                          |
|                                                                    |
|   They are chained together when multiple procedures must          |
|   be invoked in sequence.                                          |
\-------------------------------------------------------------------*/
typedef struct cm_proc {
    CM_FUNCP procp;             /* Pointer to procedure             */
    char     *func_name;        /* For error reporting              */
    char     *args[CM_MAX_ARGS+1];/* Array of pointers to arguments */
    int      arg_count;         /* Number of arguments              */
    struct cm_proc *true_nextp; /* Next procedure in chain, or null */
    struct cm_proc *false_nextp;/* Next proc if condition fails     */
} CM_PROC;


/*-------------------------------------------------------------------\
| Subfield control                                                   |
|                                                                    |
|   One for each input subfield which causes any processing to occur.|
\-------------------------------------------------------------------*/
typedef struct cm_sf {
    int     marc_sf;            /* Output marc subfield id, or -1   */
    CM_PROC *prepp;             /* First proc in chain, or null     */
    CM_PROC *postp;             /* First proc in chain, or null     */
} CM_SF;


/*-------------------------------------------------------------------\
| Field control                                                      |
|                                                                    |
|   One of these for each input field which causes any processing    |
|   to occur.                                                        |
\-------------------------------------------------------------------*/
typedef struct cm_field {
    int     x_count;            /* Count of X's in field id, e.g.99X*/
    int     subfield_count;     /* Number of subfields in use       */
    CM_SF   sf[CM_MAX_MARC_SFS];/* Ctls for possible sf positions   */
    CM_PROC *prepp;             /* First proc in chain, or null     */
    CM_PROC *postp;             /* First proc in chain, or null     */
} CM_FIELD;


/*-------------------------------------------------------------------\
| Tokenization control                                               |
|                                                                    |
|   Used while compiling a control table to recognize and            |
|   validate keywords in the input text.                             |
\-------------------------------------------------------------------*/
typedef struct cm_token {
    char   *tokenstr;           /* String form of token in table    */
    CM_TOK token;               /* Return this to compiler          */
    int    context;             /* Legal contexts for token         */
    CM_LVL new_context;         /* Switch to this after getting tok */
    int    args;                /* Need an rvalue after token       */
} CM_TOKEN;


/*-------------------------------------------------------------------\
| Lookup table for custom procedures                                 |
|                                                                    |
|   This table provides the conversion program with access to        |
|   function pointers for custom conversion procedures.              |
\-------------------------------------------------------------------*/
typedef struct cmp_table {
    char     *pname;        /* External procedure name      */
    CM_FUNCP proc;          /* Pointer to procedure         */
    CM_CND   condition;     /* Indicates if/else/endif/none */
    int      min_args;      /* Min required arguments       */
    int      max_args;      /* Max required args            */
    int      position;      /* Record/fld/sf/pre/post info  */
} CMP_TABLE;


/*-------------------------------------------------------------------\
| Command line parameters                                            |
|                                                                    |
|   All stored in this structure.                                    |
\-------------------------------------------------------------------*/
typedef struct cm_parms {
    char *infile;       /* Input MaRC filename                      */
    char *outfile;      /* Output MaRC file name                    */
    char *ctlfile;      /* Control table file                       */
    char *swfile;       /* Switch file for modifying control data   */
    char *logfile;      /* Error/warning log file name              */
    char *ctlpath;      /* Look here for tables not in current dir. */
    char *omode;        /* Output file mode, "a" or "w"             */
    long skip_recs;     /* Skip this many before starting           */
    long conv_recs;     /* Convert this many, or to end of file     */
    int  max_errs;      /* Stop after this many                     */
} CM_PARMS;


/*********************************************************************
*   Externally callable functions                                    *
*********************************************************************/

void     cm_error        (CM_SEVERITY, char *, ...);
CM_FUNCP cmp_lookup_proc (char *, CM_CND *, int *, int *, int *);

/* Call this to call one of the others from outside with modified args */
CM_STAT cmp_call    (CM_PROC_PARMS *, CM_FUNCP, int, ... );

/* Procedures */
CM_STAT cmp_if        (CM_PROC_PARMS *);
CM_STAT cmp_nop       (CM_PROC_PARMS *);
CM_STAT cmp_indic     (CM_PROC_PARMS *);
CM_STAT cmp_clear     (CM_PROC_PARMS *);
CM_STAT cmp_append    (CM_PROC_PARMS *);
CM_STAT cmp_copy      (CM_PROC_PARMS *);
CM_STAT cmp_substr    (CM_PROC_PARMS *);
CM_STAT cmp_normalize (CM_PROC_PARMS *);
CM_STAT cmp_makefld   (CM_PROC_PARMS *);
CM_STAT cmp_makesf    (CM_PROC_PARMS *);
CM_STAT cmp_y2toy4    (CM_PROC_PARMS *);
CM_STAT cmp_killrec   (CM_PROC_PARMS *);
CM_STAT cmp_killfld   (CM_PROC_PARMS *);
CM_STAT cmp_donerec   (CM_PROC_PARMS *);
CM_STAT cmp_donefld   (CM_PROC_PARMS *);
CM_STAT cmp_donesf    (CM_PROC_PARMS *);
CM_STAT cmp_renfld    (CM_PROC_PARMS *);
CM_STAT cmp_rensf     (CM_PROC_PARMS *);
CM_STAT cmp_today     (CM_PROC_PARMS *);
CM_STAT cmp_log       (CM_PROC_PARMS *);

/* Deeper routines, but can be called by user written C procedures */
void    cmp_buf_find     (CM_PROC_PARMS *, char *, unsigned char **, size_t *);
CM_STAT cmp_buf_write    (CM_PROC_PARMS *, char *, unsigned char *,size_t,int);
CM_STAT cmp_buf_copy     (CM_PROC_PARMS *, char *, char *, int);
CM_STAT cmp_get_named_buf(char *, unsigned char **, int, size_t, size_t *);
int     cmp_get_builtin  (CM_PROC_PARMS *, char *);

/* Some control table routines used for different tables */
void    open_ctl_file    (char *, FILE **);
int     get_ctl_line     (FILE *, char **);
void    close_ctl_file   (FILE **);

/* Utilities */
int     get_errs         (void);
int     get_fixed_num    (char *, size_t, int *);
int     strip_quotes     (char *, char *, size_t);


#endif /* MARCCONV_H */

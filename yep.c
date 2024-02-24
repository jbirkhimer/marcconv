/************************************************************************
* yep.c                                                                 *
*                                                                       *
*   DEFINITION                                                          *
*       Custom procedures used for Year-End-Processing (YEP)            *
*                                                                       *
*                                               Author: Mark Vereb      *
************************************************************************/

#include <string.h>
#include <ctype.h>
#include <time.h>
#include "mrv_util.h"


#ifdef MARC16
#define IBSIZE  0x2000        /* Input buffer size     */
#else
#define IBSIZE  100000
#endif

CM_STAT cmp_generic_yep035_processing (CM_PROC_PARMS *);
CM_STAT add_field_and_indics (CM_PROC_PARMS *pp, int field_id);
CM_STAT cmp_Link_ISSN                 (CM_PROC_PARMS *);
CM_STAT cmp_lang_041                  (CM_PROC_PARMS *);
CM_STAT cmp_proc_210                  (CM_PROC_PARMS *);
CM_STAT cmp_proc_650                  (CM_PROC_PARMS *);
CM_STAT cmp_proc_651                  (CM_PROC_PARMS *);
CM_STAT cmp_proc_655                  (CM_PROC_PARMS *);
CM_STAT cmp_dedupe_650_1              (CM_PROC_PARMS *);
CM_STAT cmp_dedupe_655_9              (CM_PROC_PARMS *);
CM_STAT cmp_reorder_650_1             (CM_PROC_PARMS *);
CM_STAT cmp_reorder_655_9             (CM_PROC_PARMS *);

CMP_TABLE G_proc_list[] = {
    /* Generic functions */
    {"if",      cmp_if,      CM_CND_IF,    2, 3, CMP_ANY},
    {"else",    cmp_nop,     CM_CND_ELSE,  0, 0, CMP_ANY},
    {"endif",   cmp_nop,     CM_CND_ENDIF, 0, 0, CMP_ANY},
    {"indic",   cmp_indic,   CM_CND_NONE,  2, 2, CMP_FO | CMP_SO},
    {"clear",   cmp_clear,   CM_CND_NONE,  1, 1, CMP_ANY},
    {"append",  cmp_append,  CM_CND_NONE,  2, 6, CMP_ANY},
    {"copy",    cmp_copy,    CM_CND_NONE,  2, 6, CMP_ANY},
    {"makefld", cmp_makefld, CM_CND_NONE,  1, 1, CMP_RE|CMP_RO|CMP_FO|CMP_SO},
    {"makesf",  cmp_makesf,  CM_CND_NONE,  1, 1, CMP_RE|CMP_RO|CMP_FO|CMP_SO},
    {"y2toy4",  cmp_y2toy4,  CM_CND_NONE,  2, 2, CMP_ANY},
    {"killrec", cmp_killrec, CM_CND_NONE,  0, 0, CMP_ANY},
    {"killfld", cmp_killfld, CM_CND_NONE,  0, 0, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"donerec", cmp_donerec, CM_CND_NONE,  0, 0, CMP_ANY},
    {"donefld", cmp_donefld, CM_CND_NONE,  0, 0, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"donesf",  cmp_donesf,  CM_CND_NONE,  0, 0, CMP_SE|CMP_SO},
    {"renfld",  cmp_renfld,  CM_CND_NONE,  1, 1, CMP_FE|CMP_FO|CMP_SE|CMP_SO},
    {"rensf",   cmp_rensf,   CM_CND_NONE,  1, 1, CMP_SO},
    {"today",   cmp_today,   CM_CND_NONE,  2, 2, CMP_ANY},
    {"substr",  cmp_substr,  CM_CND_NONE,  3, 4, CMP_ANY},
    {"log",     cmp_log,     CM_CND_NONE,  2,99, CMP_ANY},

    /* Specialized procedures.  Add more here           */
    /* 000 processing must be forced...                 */
    /* 'generic' field processing begins with field 001 */

    {"generic_yep035_processing",cmp_generic_yep035_processing,
                                                   CM_CND_NONE,  0, 1, CMP_RE},
    {"Link_ISSN",          cmp_Link_ISSN,          CM_CND_NONE,  0, 0, CMP_RE},
    {"lang_041",           cmp_lang_041,           CM_CND_NONE,  0, 0, CMP_FE},
    {"proc_210",           cmp_proc_210,           CM_CND_NONE,  0, 0, CMP_RE},
    {"proc_650",           cmp_proc_650,           CM_CND_NONE,  1, 1, CMP_RE},
    {"proc_651",           cmp_proc_651,           CM_CND_NONE,  0, 0, CMP_RE},
    {"proc_655",           cmp_proc_655,           CM_CND_NONE,  0, 0, CMP_RE},
    {"dedupe_650_1",       cmp_dedupe_650_1,       CM_CND_NONE,  1, 1, CMP_RE},
    {"dedupe_655_9",       cmp_dedupe_655_9,       CM_CND_NONE,  1, 1, CMP_RE},
    {"reorder_650_1",      cmp_reorder_650_1,      CM_CND_NONE,  1, 1, CMP_RE},
    {"reorder_655_9",      cmp_reorder_655_9,      CM_CND_NONE,  1, 1, CMP_RE},

    {0}
};

#define ACTION_OUTPUT 1       /* for dedupe-65x        */
#define ACTION_NEUTRAL 0      /* for dedupe-65x        */
#define ACTION_SKIP -1        /* for dedupe-65x        */

#define INSERT_HERE 0         /* for add-to-sort_array */
#define NEXT_LEVEL 1          /* for add-to-sort_array */
#define NEXT_ENTRY 2          /* for add-to-sort_array */

typedef struct sort_6xx_array {
  int     occ;      /* occurrence number */
  char    *srtind1;  /* sort1             */
  char    *srtind2;  /* sort2             */
  char    *srtsf8;   /* sort3             */
  char    *srtsf2e;  /* sort4             */
  char    *srtsfa;   /* sort5             */
} SORT_6XX_ARRAY;

#define MAX_6XX_FIELDS   600  /* Max RUBRICS          */

static SORT_6XX_ARRAY sort_650_array[MAX_6XX_FIELDS];
static SORT_6XX_ARRAY sort_655_array[MAX_6XX_FIELDS];

#define MAX_ISSN_CODES 60000
#define MAX_YEP_CODES  12000 
#define MAX_YEPSpecial_CODES 100

static DECODE_TBL ISSNtable[MAX_ISSN_CODES];    
static DECODE_TBL YEPtable[MAX_YEP_CODES];    
static DECODE_TBL YEP651table[MAX_YEP_CODES];    
static DECODE_TBL YEP655table[MAX_YEP_CODES];    
static DECODE_TBL YEP650_655table[MAX_YEP_CODES];    

CM_STAT marc_put_field (CM_PROC_PARMS *pp, int tag, int fldcnt);
CM_STAT marc_replace_subfields (CM_PROC_PARMS *pp,
				int     tag,  
				char *infld,
				char *outfld);

char *concat_sfs(CM_PROC_PARMS *pp, int tag, int occ, char *sfs);
void set_modified(CM_PROC_PARMS *pp,
		  char *modified);

void add_to_650_array(CM_PROC_PARMS *pp,
		       int   tag, int socc,
		       char *sf1, size_t len1, 
		       char *sf2, size_t len2,
		       char *sf3, size_t len3,
		       char *sf4, size_t len4,
		       char *sf5, size_t len5);

void add_to_655_array(CM_PROC_PARMS *pp,
		       int   tag, int socc,
		       char *sf1, size_t len1, 
		       char *sf2, size_t len2,
		       char *sf3, size_t len3,
		       char *sf4, size_t len4,
		       char *sf5, size_t len5);

int sort_cnt;
unsigned char *uip;
size_t         ui_len;

char *luip;
size_t lui_len;

CM_SEVERITY    msgtp;

  static FILE   *fpLinking;            /* Input file                           */
  static int inbibid=0;
  static char *linking=NULL;

/************************************************************************
* cmp_generic_yep035_processing ()                                      *
*                                                                       *
*   DEFINITION                                                          *
*       remarc_035 is custom procedure designed to:                     *
*       move $9 to 001 (and verify there is only one $9)                *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_generic_yep035_processing (
  CM_PROC_PARMS *pp           /* Pointer to parameter structure      */
) {
  unsigned char *srcp,        /* Ptr to data                         */
                *srctmp,      /* Ptr to move through data            */
                *srctmp2;      /* Ptr to move through data            */
  size_t        src_len,      /* Length of source field              */
                srctlen,      /* Length of temp source field         */
                srctlen2;      /* Length of temp source field         */
  char          *tmpp;
  int           occ,          /* Field occurrence counter            */
                socc,rc,         /* Subfield occurrence counter         */
                messages,     /* Subfield occurrence counter         */
                sf_code,      /* Subfield code                       */
                cnt9,         /* Count of $9's                       */
                cnta;         /* Count of $a's                       */
  int           is_monograph;    /*                                     */

  cnta = cnt9 = messages = 0;

  /*=========================================================================*/
  /* Determine whether we're supposed to send out messages...                */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &srcp, &src_len);
  messages = (*srcp == '1');


  /*=========================================================================*/
  /* First, we loop through the 035's looking for $9's...                    */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,35,occ,&srcp,&src_len)==0;occ++) {

    /*=======================================================================*/
    /* For each $9...                                                        */
    /*=======================================================================*/
    for (socc=0;marc_get_item(pp->inmp,35,occ,'9',socc,&srcp,&src_len)==0;
	 socc++) {

      /*=====================================================================*/
      /* Increment the counter (but we shouldn't have more than one...       */
      /*=====================================================================*/
      cnt9++;

      if(cnt9>1) {
	cm_error(CM_ERROR, "There are %d 035$9's for this record. "
		 "Record not processed.", cnt9);
	return CM_STAT_KILL_RECORD;
      }

      /*=====================================================================*/
      /* Copy the value to the UI buffer for future reference...             */
      /*=====================================================================*/
      cmp_buf_write(pp, "ui", srcp, src_len, 0);
      srctmp=srcp;
      srctlen=src_len;

    } /* end of $9 loop */
	
  } /* end of 035 loop */


  /*=========================================================================*/
  /* If we don't have exactly one $9, exit with error message...             */
  /*=========================================================================*/
  if(cnt9!=1) {
    cm_error(CM_ERROR, "There are %d 035$9's for this record. "
	     "Record not processed.", cnt9);
    return CM_STAT_KILL_RECORD;
  }

  /* Extract Leader 7... */
  cmp_buf_find(pp, "000:0$7:1", &srcp, &src_len);
  if((*srcp!='s') && (*srcp!='b'))
    is_monograph=1;
  else
    is_monograph=0;

  /*=========================================================================*/
  /* Then, loop through them again for output...                             */
  /*=========================================================================*/
  for(occ=0;(rc=marc_get_field(pp->inmp,35,occ,&srcp,&src_len))==0;occ++) {

    // REMOVE FROM PROCESSING!!!
    //    /*=======================================================================*/
    //    /* If it's a monograph and starts with "(OCoLC)", skip it...             */
    //    /*=======================================================================*/
    //    if(marc_get_item(pp->inmp,35,occ,'a',0,&srctmp2,&srctlen2)==0) 
    //      if((is_monograph) && (strncmp((char *)srctmp2,"(OCoLC)",7)==0)) {
    //    	continue;
    //      }

    /*=======================================================================*/
    /* For each one, create a new output occurrence...                       */
    /*=======================================================================*/
    if (add_field_and_indics(pp, 35) != 0 )
	return CM_STAT_KILL_RECORD;
	
    /*=======================================================================*/
    /* and begin copying over contents of subfields (starting at 2)          */
    /*=======================================================================*/
    for(socc=2;marc_pos_subfield(pp->inmp,socc,&sf_code,&srcp,&src_len)==0;
	socc++) {
	
      tmpp=(char *)srcp;
      /*=====================================================================*/
      /* We need to check the $a's starting with (DNLM)...                   */
      /*=====================================================================*/
      if((sf_code=='a')&&(strncmp(tmpp,"(DNLM)",6)==0)) {

	tmpp+=6;
	/*===================================================================*/
	/* If $a = $9, drop the $a...                                        */
	/*===================================================================*/
	if((strncmp(tmpp,(char *)srctmp,srctlen)==0)&&(srctlen==src_len-6)) {
	  cnta++;
	  continue;
	}

	/*===================================================================*/
	/* Ignore it if $a is from Special Producers                         */
	/*===================================================================*/
	if(
	   (strncmp(tmpp,"BKSHLF:",7)==0)||
	   (strncmp(tmpp,"CIT:",4)==0)||
	   (strncmp(tmpp,"HISTLINE",8)==0)||
	   (strncmp(tmpp,"HSTAR",5)==0)||
	   (strncmp(tmpp,"IDM",3)==0)||
	   (strncmp(tmpp,"IDX",3)==0)||
	   (strncmp(tmpp,"IHM",3)==0)||
	   (strncmp(tmpp,"KIE",3)==0)||
	   (strncmp(tmpp,"NASA",4)==0)||
	   (strncmp(tmpp,"NYA",3)==0)||
	   (strncmp(tmpp,"PIP",3)==0)
	   ) {
	  /* nop */
	}
	else
	/*===================================================================*/
	/* Otherwise, send message (if appropriate) if it doesn't end in 's' */
	/*===================================================================*/
	if(messages) {
	  tmpp=(char *)srcp+src_len-3;
	  if(strncmp(tmpp,"(s)",3)!=0) {
	    cm_error(CM_WARNING,"035 $a begins with (DNLM), doesn't contain a partner value, "
		     "doesn't end in '(s)' and does NOT match 035 $9");
	    cm_error(CM_CONTINUE,
		     "035 $a=%*.*s",src_len,src_len,(char *)srcp);
	    cm_error(CM_CONTINUE,
		     "035 $9=%*.*s",srctlen,srctlen,srctmp);
	  }
	}
      }

      /*=====================================================================*/
      /* Send everything that gets to here over 'as-is'...                   */
      /*=====================================================================*/
      marc_add_subfield(pp->outmp, sf_code, srcp, src_len);


    } /* end of subfield loop */
	
  } /* end of 035 loop */

  if(cnta>1)
    cm_error(CM_WARNING,"There were %d 035 $a's matching the 035$9. "
	     "All have been removed.",cnta);

  return CM_STAT_OK;
  
} /* cmp_generic_yep035_processing */


/************************************************************************
* cmp_Link_ISSN  ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_Link_ISSN (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  static int s_first_time = 1;/* Force load of tbls on 1st rec       */
  static int readflag=1;

  static int reccnt = 0;
  static int prnmsg = 1;
  static int matching = 1;

  unsigned char *srcp;       /* Ptr to data w/o spaces collapsed    */
  char          *inbibp,         /* Pointer to field id in ctl table line*/
                *intextp;
  size_t        src_len;     /* Length of source (collapsed)        */
  int           occ,         /* Field occurrence counter            */
                occ2,          /* Field occurrence counter            */
                num9s,          /* Field occurrence counter            */
                numtodos,          /* Field occurrence counter            */
                bibid,
                oldinbibid,
                entry_cnt,      /* Count of entries in file             */
                sf_code;      /* Subfield code                       */

  /*=========================================================================*/
  /* First time through, load the YEP table                                  */
  /*=========================================================================*/
  if (s_first_time) {
    
    inbibp = NULL;
    
    cmp_buf_find(pp, "SUPPRESS_LINKING_MSG", &srcp, &src_len);
    prnmsg=(strncmp((char *)srcp, "1",1) != 0);
    
    /*=======================================================================*/
    /* Open decode table file...                                             */
    /*=======================================================================*/
    char cpath[FILENAME_MAX];   /* Filename with path       */
    
    /* Open file */
    if ((fpLinking = fopen ("LinkingISSN.tbl", "r")) == NULL) {
      
      if ((fpLinking = fopen ("/apps/medmarc/ctl/LinkingISSN.tbl", "r")) == NULL) {
	
	if ((fpLinking = fopen ("/m2/medmarc/ctl/LinkingISSN.tbl", "r")) == NULL) {
	  
	  perror ("LinkingISSN.tbl");
	  cm_error (CM_FATAL, "Unable to open control file LinkingISSN.tbl");
        }
      }
    }
    
    s_first_time=entry_cnt=0;
  }
  
  //  reccnt++;
  //  if(reccnt%1000==0) {
  //    cm_error(CM_NO_ERROR,"");
  //  }
  
  /*=========================================================================*/
  /* Start off by getting the BibID...                                       */
  /*=========================================================================*/
  if(marc_get_field(pp->inmp,1,0,&srcp,&src_len)==0) {
    if (!get_fixed_num ((char *) srcp, src_len, &bibid)) {
      cm_error(CM_FATAL,"Unable to determine numeric from %*.*s",
	       src_len,src_len,(char *)srcp);
    }
  }
    
  /*=========================================================================*/
  /* ... and seeing if it's in the table...                                  */
  /*=========================================================================*/

  /*=========================================================================*/
  /* Read each line of the file...                                           */
  /*=========================================================================*/
  while ((readflag)&&(bibid>inbibid)) {
    oldinbibid=inbibid;

    char *startp,       /* Ptr to start of real data on line        */
      *endp;         /* Ptr to end of real data                  */
    
    /* Buffer for one line.  Need static because we return ptrs into it */
    static char s_linebuf[CM_CTL_MAX_LINE];
    
    
    /* Nothing found yet */
    intextp = NULL;
    
    /* Read lines until got one we want or eof */
    while (fgets (s_linebuf, CM_CTL_MAX_LINE, fpLinking)) {
      
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
      intextp = startp;
      break;
    }
    
    /* If nothing found, check for error or EOF */
    if (!intextp) {
      if (!feof (fpLinking))
	cm_error (CM_FATAL, "Error reading control file");

      inbibid=-1;
      linking=NULL;
      readflag=0;

      if (fclose (fpLinking) != 0) {
        perror ("Closing file");
        cm_error (CM_FATAL, "Error closing LinkingISSN.tbl");
      }

      break;
    }
    
    
    entry_cnt++;
    /*=====================================================================*/
    /* Then break it apart at the first record separator ("@") sign        */
    /*=====================================================================*/
    inbibp = strtok(intextp, DLM_REC_SEPARATOR);
    if (!inbibp) {
      cm_error (CM_WARNING,
		"Missing delimiter or data from entry number %d. "
		"Expecting bibid@linking_issn", entry_cnt);
      cm_error(CM_CONTINUE,"Entry='%s'", intextp);
      continue;
    }
    
    linking = inbibp + strlen(inbibp) + 1;

    if (!get_fixed_num(inbibp, strlen(inbibp), &inbibid)) {
      cm_error(CM_FATAL,"Unable to determine numeric from %s",inbibp);
    }

    if(inbibid>=bibid) {
      if(!matching) {
	if(prnmsg) {
	  cm_error(CM_CONTINUE," ");
	  cm_error(CM_CONTINUE,"==================================================================================");
	  cm_error(CM_CONTINUE,"LINKING ISSN TABLE: BibID %d not found in database - "
		   "Should have Linking ISSN of %s", oldinbibid, linking);
	  cm_error(CM_CONTINUE,"==================================================================================");
	  cm_error(CM_CONTINUE," ");
	}
      }
      
      if (!*linking) {
	cm_error (CM_WARNING,
		  "Missing marc data from entry number %d.  "
		  "Expecting decode_value@marc_data", entry_cnt);
	cm_error(CM_CONTINUE,"Entry='%s'", intextp);
	continue;
      }
    }
    /***********************************/
    else {
      if(prnmsg) {
	cm_error(CM_CONTINUE," ");
	cm_error(CM_CONTINUE,"==================================================================================");
	cm_error(CM_CONTINUE,"LINKING ISSN TABLE: BibID %d not found in database. "
		 "Should have Linking ISSN of %s", inbibid, linking);
	cm_error(CM_CONTINUE,"==================================================================================");
	cm_error(CM_CONTINUE," ");
      }
    }
  }

  matching= (inbibid==bibid);

  /*=========================================================================*/
  /* Now start copying over the 022's...                                     */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,22,occ,&srcp,&src_len)==0;occ++) {
    
    /*=======================================================================*/
    /* If we have a Linking ISSN, we need to place it in the first 022...    */
    /*=======================================================================*/
    if((occ==0)&&(bibid==inbibid)) {
      
      /*=====================================================================*/
      /* Create new instance of tag output field...                          */
      /*=====================================================================*/
      if (marc_add_field(pp->outmp, 22)!=0) {
	cm_error(CM_FATAL, "Error creating field 022. Record not processed.");
	return CM_STAT_KILL_RECORD;
      }
      
      /*=====================================================================*/
      /* If no $a, send message out...                                       */
      /*=====================================================================*/
      if(marc_get_subfield(pp->inmp,'a',0,&srcp,&src_len)!=0) {
	cm_error(CM_WARNING,"First 022 does not contain $a "
		 "[BibID=%d; Linking ISSN=%s]", bibid, linking);
	marc_add_subfield(pp->outmp, 'l', (unsigned char *)linking,
			  strlen(linking));
	set_modified(pp, "1");
      }	
      
      /*=====================================================================*/
      /* Now loop through each subfield and write it out...                  */
      /*=====================================================================*/
      for(occ2=0;marc_pos_subfield(pp->inmp,occ2,&sf_code,&srcp,&src_len)==0;
	  occ2++){
	
	/*===================================================================*/
	/* If occ<2, we're looking at indicators...                          */
	/*===================================================================*/
	if(occ2<2) {
	  if(marc_set_indic(pp->outmp, sf_code, *srcp)!=0) { 
	    cm_error(CM_FATAL, "Error setting indicator %d for 022. "
		     "Record not processed.", occ2);
	    return CM_STAT_KILL_RECORD;
	  }
	}
	/*===================================================================*/
	/* Otherwise, send out as-is...                                      */
	/*===================================================================*/
	else {
	  marc_add_subfield(pp->outmp, sf_code, srcp, src_len);

	  /*=================================================================*/
	  /* The Linking ISSN goes out as $l following the $a...             */
	  /*=================================================================*/
	  if(sf_code=='a') {
	    marc_add_subfield(pp->outmp, 'l', (unsigned char *)linking,
			      strlen(linking));
	    set_modified(pp, "1");
	  }
	}
      }
    }
    /*=======================================================================*/
    /* ... otherwise, just put the field out as-is...                        */
    /*=======================================================================*/
    else 
      marc_put_field(pp,22,-1);
  }

  if((occ==0)&&(bibid==inbibid)) {
    cm_error(CM_WARNING,"BibID %d has no 022. Should have Linking ISSN of %s",
	     bibid, linking);
  }


  return CM_STAT_OK;
  
} /* cmp_Link_ISSN */



/************************************************************************
* cmp_lang_041  ()                                                      *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_lang_041 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  unsigned char *srcp;       /* Ptr to data w/o spaces collapsed    */
  char          *inbibp,         /* Pointer to field id in ctl table line*/
                *intextp;
  size_t        src_len;     /* Length of source (collapsed)        */
  int           occ,         /* Field occurrence counter            */
                occ2,          /* Field occurrence counter            */
                num9s,          /* Field occurrence counter            */
                numtodos,          /* Field occurrence counter            */
                bibid,
                oldinbibid,
                entry_cnt,      /* Count of entries in file             */
                sf_code;      /* Subfield code                       */
  
  /*=====================================================================*/
  /* Create new instance of tag output field...                          */
  /*=====================================================================*/
  if (marc_add_field(pp->outmp, 41)!=0) {
    cm_error(CM_FATAL, "Error creating field 041. Record not processed.");
    return CM_STAT_KILL_RECORD;
  }
  
  /*=====================================================================*/
  /* Now loop through each subfield and write it out...                  */
  /*=====================================================================*/
  for(occ2=0;marc_pos_subfield(pp->inmp,occ2,&sf_code,&srcp,&src_len)==0;
      occ2++){
    
    /*===================================================================*/
    /* If occ<2, we're looking at indicators...                          */
    /*===================================================================*/
    if(occ2<2) {
      if(marc_set_indic(pp->outmp, sf_code, *srcp)!=0) { 
	cm_error(CM_FATAL, "Error setting indicator %d for 041. "
		 "Record not processed.", occ2);
	return CM_STAT_KILL_RECORD;
      }
    }
    /*===================================================================*/
    /* Otherwise, convert appropriate language codes...                  */
    /*===================================================================*/
    else 
      if((strncmp((char *)srcp,"scr",src_len)==0)&&(src_len==3)) {
	marc_add_subfield(pp->outmp, sf_code, (unsigned char *)"hrv", 3);
	set_modified(pp, "1");
      }
      else
	if((strncmp((char *)srcp,"mol",src_len)==0)&&(src_len==3)) {
	  marc_add_subfield(pp->outmp, sf_code, (unsigned char *)"rum", 3);
	  set_modified(pp, "1");
	}
	else
	  if((strncmp((char *)srcp,"scc",src_len)==0)&&(src_len==3)) {
	    marc_add_subfield(pp->outmp, sf_code, (unsigned char *)"srp", 3);
	    set_modified(pp, "1");
	  }
	  else
	    marc_add_subfield(pp->outmp, sf_code, srcp, src_len);
  }
  
  return CM_STAT_OK;
  
} /* cmp_lang_041 */



/************************************************************************
* cmp_proc_210   ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_210 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  unsigned char *srcp;       /* Ptr to data w/o spaces collapsed    */
  size_t        src_len;     /* Length of source (collapsed)        */
  int           occ,         /* Field occurrence counter            */
                occ2,          /* Field occurrence counter            */
                add8,          /* Field occurrence counter            */
                num8s,          /* Field occurrence counter            */
                num9s;          /* Field occurrence counter            */

  num9s=0;    
  /*=========================================================================*/
  /* First, loop through 210's to see if we have any work to do...           */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,210,occ,&srcp,&src_len)==0;occ++) {
    
    /*=======================================================================*/
    /* We can skip any that have a $9...                                     */
    /*=======================================================================*/
    if(marc_get_subfield(pp->inmp,'9',0,&srcp,&src_len)==0) {
      num9s++;
      break;
    }
  }
  
  /*=========================================================================*/
  /* Loop back through 210's to write them out...                            */ 
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,210,occ,&srcp,&src_len)==0;occ++) {
    
    /*=======================================================================*/
    /* If we have any $9's, we don't have to do anything else...             */
    /*=======================================================================*/
    if(num9s)
      marc_put_field(pp,210,-1);
    else {
      
      num8s=0;
      
      /*=====================================================================*/
      /* Let's see if any $8's exist that are not 'g'...                     */
      /*=====================================================================*/
      for(occ2=0;marc_get_subfield(pp->inmp,'8',occ2,&srcp,&src_len)==0;occ2++){
	if((strncmp((char *)srcp,"g",src_len)!=0)||(src_len>1)) {
	  num8s++;
	  cm_error(CM_WARNING,"210 has $8 other than 'g' ($8='%*.*s')",
		   src_len,src_len,(char*)srcp);
	}	  
      }
      
      /*=====================================================================*/
      /* If so, we still don't have anything to do... output it and move on  */
      /*=====================================================================*/
      if(num8s) {
	marc_put_field(pp,210,-1);
      }
      else {
		
	/*===================================================================*/
	/* Otherwise, we need to search for $2=DNLM w/o $8='g'...            */     
	/*===================================================================*/
	if(marc_get_subfield(pp->inmp,'2',0,&srcp,&src_len)==0) {
	  
	  add8=0;
	  /*=================================================================*/
	  /* If we have $2=DNLM...                                           */
	  /*=================================================================*/
	  if((strncmp((char *)srcp,"DNLM",src_len)==0)&&
	     (src_len==4)) {
	    
	    add8++;
	    /*===============================================================*/
	    /* Check for $8...                                               */
	    /*===============================================================*/
	    if(marc_get_subfield(pp->inmp,'8',0,&srcp,&src_len)==0) {
	      
	      /*=============================================================*/
	      /* Nothing to do if it is 'g'...                               */
	      /*=============================================================*/
	      if((strncmp((char *)srcp,"g",src_len)==0) && (src_len==1))
		add8=0;
	      /*=============================================================*/
	      /* but give a message if it's not...                           */
	      /*=============================================================*/
	      else
		cm_error(CM_WARNING,"210 $2=DNLM has $8 that is not 'g'.");
	    }
	  }
	  /*=================================================================*/
	  /* If $2 is NOT DNLM, give a message...                            */
	  /*=================================================================*/
	  else
	    cm_error(CM_WARNING,"210 has $2 other than 'DNLM' ($2='%*.*s')",
		     src_len,src_len,(char*)srcp);

	  
	  /*=================================================================*/
	  /* Now output the field                                            */
	  /*=================================================================*/
	  marc_put_field(pp,210,-1);
	  
	  /*=================================================================*/
	  /* and $8='g' if necessary                                         */
	  /*=================================================================*/
	  if(add8) {
	    cmp_buf_write(pp, "210:-1$8:+", (unsigned char *)"g", 1, 0);	    
	    set_modified(pp, "1");
	  }
	}
	else
	  marc_put_field(pp,210,-1);
      }
    }
  }

  return CM_STAT_OK;
  
} /* cmp_proc_210 */



/************************************************************************
* cmp_proc_440 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Recognized only in RECORD PREP processing                       *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_440 (
    CM_PROC_PARMS *pp               /* Pointer to parameter struct  */
) {
  static int s_first_time = 1;/* Force load of tbls on 1st rec       */
  unsigned char *tsrcp,       /* Ptr to data w/o spaces collapsed    */
                *tmp1,
                *tmp2,
                *tmp3,
                *tagp,
                 indic;        /* Ptr to input data                   */
  char          *srca,        /* Ptr to data with spaces collapsed   */
                *srcn, 
                *srcp, 
                *srcv, 
                *srcx, 
                *last490a, 
                *outbeg, 
                *outend,
                *tmp1a,
                *tmp2a,
                 indchar,
                *tmpp;        /* Ptr to data with spaces collapsed   */
  size_t        tsrc_len,     /* Length of source (collapsed)        */
                tmp1len,
                tmp2len,
                tmp3len,
                tagplen,
                src_len;      /* Length of source (collapsed)        */
  int           focc,         /* Field occurrence counter            */
                ind2,         /* Field occurrence counter            */
                occ,          /* Field occurrence counter            */
                history,      /* Field occurrence counter            */
                missing8,     /* Field occurrence counter            */
                occ2,         /* Field occurrence counter            */
                sf_code;      /* Subfield code                       */
  DECODE_TBL    *mshp;
  char          sf_order[9];

  /*=========================================================================*/
  /* For debugging purposes...                                               */
  /*=========================================================================*/
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"101269813",9)==0)
    uip=uip;

  /*=========================================================================*/
  /* Loop through all the 440's...                                           */
  /*=========================================================================*/
  for(focc=0;(marc_get_field(pp->inmp,440,focc,&tmp1,&tmp1len)==0);
      focc++){

    srca=srcn=srcp=srcv=srcx=NULL;
    memset(sf_order,'\0',9);
    /*=======================================================================*/
    /* Now loop through each subfield...                                     */
    /*=======================================================================*/
    for(occ=2;marc_pos_subfield(pp->inmp,occ,&sf_code,&tsrcp,&src_len)==0;
	occ++){

      if(strncmp((char *)uip,"0424471",7)==0)
	uip=uip;

        switch (sf_code) {
	  /*=================================================================*/
	  /* $a may need to drop leading article                             */
	  /*=================================================================*/
	case 'a':
	  strcat(sf_order,"a");
	  tmpp=(char *)tsrcp;

	  marc_get_indic(pp->inmp,2,&indic);
	  if(indic!=' ') {
	    indchar=indic;
	    ind2=atoi(&indchar);
	    src_len=src_len-ind2;

	    for(;ind2>0;ind2--)
	      tmpp++;
	  
	    *tmpp=toupper(*tmpp);
	  }

	  if(allocate_n_ncopy(&srca,src_len,1,(char *)tmpp,
			      NO_COLLAPSE_SPACES,"440$a")!=0)
	    return CM_STAT_KILL_RECORD;
	  break;
	  
	  /*=================================================================*/
	  /* $n, $p, $v & $x also need to go over...                         */
	  /*=================================================================*/
	case 'n':
	  strcat(sf_order,"n");
	  if(allocate_n_ncopy(&srcn,src_len,1,(char *)tsrcp,
			      NO_COLLAPSE_SPACES,"440$n")!=0)
	    return CM_STAT_KILL_RECORD;
	  break;
	  
	case 'p':
	  strcat(sf_order,"p");
	  if(allocate_n_ncopy(&srcp,src_len,1,(char *)tsrcp,
			      NO_COLLAPSE_SPACES,"440$p")!=0)
	    return CM_STAT_KILL_RECORD;
	  break;
	  
	case 'v':
	  strcat(sf_order,"v");
	  if(allocate_n_ncopy(&srcv,src_len,1,(char *)tsrcp,
			      NO_COLLAPSE_SPACES,"440$v")!=0)
	    return CM_STAT_KILL_RECORD;
	  break;
	  
	case 'x':
	  strcat(sf_order,"x");
	  if(allocate_n_ncopy(&srcx,src_len,1,(char *)tsrcp,
			      NO_COLLAPSE_SPACES,"440$x")!=0)
	    return CM_STAT_KILL_RECORD;
	  break;
	  
	  /*=================================================================*/
	  /* everything else => error message                                */
	  /*=================================================================*/
	default:
	  marc_put_field(pp,440,-1);
	  return CM_STAT_OK;
	  break;
	  
        }
    }
	
    /*=====================================================================*/
    /* Start outputting to new fields...                                   */
    /*=====================================================================*/
    last490a=NULL;

    /*=====================================================================*/
    /* Create new 490                                                      */
    /*=====================================================================*/
    if (marc_add_field(pp->outmp, 490)!=0) {
      cm_error(CM_FATAL, "Error creating field 490. "
	       "Record not processed.");
      return CM_STAT_KILL_RECORD;
    }
    marc_set_indic(pp->outmp,1,'1');
    
    if(strncmp((char *)uip,"9412033",7)==0)
      uip=uip;


    for(occ=0;occ<strlen(sf_order);occ++) {

      switch (sf_order[occ]) {
	
	/*=================================================================*/
	/* $a goes to both fields, capture last character...               */
	/*=================================================================*/
      case 'a':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srca+strlen(srca)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	}
	cmp_buf_write(pp,"490:-1$a:+",(unsigned char *)srca, strlen(srca),0);

	last490a=srca+strlen(srca)-1;
	break;

	/*=================================================================*/
	/* $n is concatenated to 490$a, goes to 830$n...                   */
	/*=================================================================*/
      case 'n':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcn+strlen(srcn)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	}
	
	if(last490a) {
	  if((*last490a!=' ')&&(srcn[0]!=' '))
	    cmp_buf_write(pp,"490:-1$a",(unsigned char *)" ",1,1);
	  
	  cmp_buf_write(pp,"490:-1$a",(unsigned char *)srcn,strlen(srcn),1);
	}
	else
	  cmp_buf_write(pp,"490:-1$a:+",(unsigned char *)srcn,strlen(srcn),0);
	
	last490a=srcn+strlen(srcn)-1;
	break;

	/*=================================================================*/
	/* $p is concatenated to 490$a, goes to 830$p...                   */
	/*=================================================================*/
      case 'p':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcp+strlen(srcp)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	}
	
	if(last490a) {
	  if((*last490a!=' ')&&(srcp[0]!=' '))
	    cmp_buf_write(pp,"490:-1$a",(unsigned char *)" ",1,1);
	  
	  cmp_buf_write(pp,"490:-1$a",(unsigned char *)srcp,strlen(srcp),1);
	}
	else
	  cmp_buf_write(pp,"490:-1$a:+",(unsigned char *)srcp,strlen(srcp),0);
	
	last490a=srcp+strlen(srcp)-1;
	break;
	  
	/*=================================================================*/
	/* $v goes to 490$v, goes to 830$v...                              */
	/*=================================================================*/
      case 'v':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcv+strlen(srcv)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	}
	
	cmp_buf_write(pp,"490:-1$v:+",(unsigned char *)srcv,strlen(srcv),0);

	break;

	/*=================================================================*/
	/* $x goes to 490$x only...                                        */
	/*=================================================================*/
      case 'x':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcx+strlen(srcx)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	}

	cmp_buf_write(pp,"490:-1$x:+",(unsigned char *)srcx,strlen(srcx),0);

	break;

      }
	}
    /*=====================================================================*/
    /* ...and new 830                                                      */
    /*=====================================================================*/
    if (marc_add_field(pp->outmp, 830)!=0) {
      cm_error(CM_FATAL, "Error creating field 830. "
	       "Record not processed.");
      return CM_STAT_KILL_RECORD;
    }
    marc_set_indic(pp->outmp,2,'0');
    
    for(occ=0;occ<strlen(sf_order);occ++) {
      if(sf_order[occ]=='x') {
	for(occ2=occ+1;occ2<strlen(sf_order);occ2++) {
	  sf_order[occ]=sf_order[occ2];
	  occ++;
	}
	sf_order[occ]='\0';
      }
    }

    for(occ=0;occ<strlen(sf_order);occ++) {
      
      switch (sf_order[occ]) {
	
	/*=================================================================*/
	/* $a goes to both fields, capture last character...               */
	/*=================================================================*/
      case 'a':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srca+strlen(srca)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	  strcat(srca,".");
	}

	cmp_buf_write(pp,"830:-1$a:+",(unsigned char *)srca, strlen(srca),0);
	break;

	/*=================================================================*/
	/* $n is concatenated to 490$a, goes to 830$n...                   */
	/*=================================================================*/
      case 'n':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcn+strlen(srcn)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	  strcat(srcn,".");
	}

	cmp_buf_write(pp,"830:-1$n:+",(unsigned char *)srcn,strlen(srcn),0);
	break;

	/*=================================================================*/
	/* $p is concatenated to 490$a, goes to 830$p...                   */
	/*=================================================================*/
      case 'p':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcp+strlen(srcp)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	  strcat(srcp,".");
	}

	cmp_buf_write(pp,"830:-1$p:+",(unsigned char *)srcp,strlen(srcp),0);
	break;
	  
	/*=================================================================*/
	/* $v goes to 490$v, goes to 830$v...                              */
	/*=================================================================*/
      case 'v':
	if(occ==strlen(sf_order)-1) {
	  for(tmpp=srcv+strlen(srcv)-1;
	      (*tmpp!=')')&&(*tmpp!=']')&&((ispunct(*tmpp)||*tmpp==' '));
	      tmpp--) 
	    *tmpp='\0';
	  strcat(srcv,".");
	}

	cmp_buf_write(pp,"830:-1$v:+",(unsigned char *)srcv,strlen(srcv),0);
	break;
      }
    }
    
    set_modified(pp,"1");
  }
    
  return CM_STAT_OK;
  
} /* cmp_proc_440 */


/************************************************************************
* cmp_proc_650 ()                                                       *
*                                                                       *
*   DEFINITION                                                          *
*       Recognized only in FIELD PREP processing                        *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_650 (
    CM_PROC_PARMS *pp               /* Pointer to parameter struct  */
) {
  static int s_first_time = 1;/* Force load of tbls on 1st rec       */
  unsigned char *tsrcp,       /* Ptr to data w/o spaces collapsed    */
                *tmp1,
                *tmp2,
                *tmp3,
                *tagp,
                 indic;        /* Ptr to input data                   */
  char          *srcp,        /* Ptr to temp data                    */
                *src2,        /* Ptr to data with spaces collapsed   */
                *srca,
                *srcx,
                *outbeg, 
                *outend,
                *tmp1a,
                *tmp2a,
                *tmpp;        /* Ptr to data with spaces collapsed   */
  size_t        tsrc_len,     /* Length of source (collapsed)        */
                tmp1len,
                tmp2len,
                tmp3len,
                sfalen,
                sfxlen,
                src_len;      /* Length of source (collapsed)        */
  int           focc,         /* Field occurrence counter            */
                chktagprob,   /* Field occurrence counter            */
                occ,          /* Field occurrence counter            */
                rc,           /* Field occurrence counter            */
                tagno,        /* Field occurrence counter            */
                history,      /* Field occurrence counter            */
                missing8,     /* Field occurrence counter            */
                sf_code;      /* Subfield code                       */
  DECODE_TBL    *mshp;


  /*=========================================================================*/
  /* For debugging purposes...                                               */
  /*=========================================================================*/
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"101311145",9)==0)
    uip=uip;

  /*=========================================================================*/
  /* First time through, load the YEP table                                  */
  /*=========================================================================*/
  if (s_first_time) {
    
    /* Functions will abort if error occurs */
    load_validation_tbl("yep.tbl",         YEPtable,        "@", 
			MAX_YEP_CODES, NO_NORMALIZE_DATA);
    
    load_validation_tbl("yep651.tbl",      YEP651table,     "@", 
			MAX_YEP_CODES, NO_NORMALIZE_DATA);
    
    load_validation_tbl("yep655.tbl",      YEP655table,     "@", 
			MAX_YEP_CODES, NO_NORMALIZE_DATA);
    
    load_validation_tbl("yep650_655.tbl", YEP650_655table, "@",
			MAX_YEP_CODES, NO_NORMALIZE_DATA);
  
    s_first_time=0;
  }
  
  history = chktagprob = 0;
  
  /*=========================================================================*/
  tagno = atoi (pp->args[0]);
  
  /* Validate */
  if (tagno < 1 || tagno > 999)
    cm_error (CM_FATAL, "cmp_rename_field: Invalid field id %d", tagno);

  
  /*=========================================================================*/
  /* Loop through all the fld's...                                           */
  /*=========================================================================*/
  for(focc=0;marc_get_item(pp->inmp,tagno,focc,'a',0,&tsrcp,&tsrc_len)==0;
      focc++){
    
    /*=======================================================================*/
    /* We want to look at the $a data...                                     */
    /*=======================================================================*/
    if(allocate_n_ncopy(&srca,tsrc_len,1,(char *)tsrcp,
			NO_COLLAPSE_SPACES,pp->args[0])!=0)
      return CM_STAT_KILL_RECORD;

    srcp = srca+tsrc_len-1;
    sfalen = tsrc_len;

    if(*srcp=='.') {
      *srcp='\0';
      sfalen=strlen(srca);
    }

    srcp=src2=srca;    
    src_len=sfalen;

    /*=======================================================================*/
    /* and the $x data...                                                    */
    /*=======================================================================*/
    marc_get_item(pp->inmp,tagno,focc,'x',0,&tmp1,&tmp1len);
    
    /*=======================================================================*/
    /* concatenate $a & $x together with '!' separator                       */
    /*=======================================================================*/
    if(tmp1len>0) {
      
      if(allocate_n_ncopy(&srcx,tmp1len,1,(char *)tmp1,
			  NO_COLLAPSE_SPACES,pp->args[0])!=0)
	return CM_STAT_KILL_RECORD;
      
      srcp = srcx+tmp1len-1;
      sfxlen=tmp1len;

      if(*srcp=='.') {
	*srcp='\0';
	sfxlen=strlen(srcx);
      }
      
     if(allocate_n_ncopy(&src2,sfalen,sfxlen+1,(char *)srca,
			  NO_COLLAPSE_SPACES,pp->args[0])!=0)
	return CM_STAT_KILL_RECORD;
      
      strcat(src2,"!");
      strncat(src2,srcx,sfxlen);
      free(srcx);

      srcp=src2;
      src_len=sfalen+sfxlen+1;
    }
    

    /*=======================================================================*/
    /* Part 1 of specs... Match against YEP table and replace flds           */
    /*=======================================================================*/
    
    /*=======================================================================*/
    /* See if this YEP is in the table...                                    */
    /*=======================================================================*/ 
                   rc=decode_value(YEPtable,   srcp,src_len,&mshp,EXACT_MATCH,NO_NORMALIZE_DATA);
    if(tagno==651) rc=decode_value(YEP651table,srcp,src_len,&mshp,EXACT_MATCH,NO_NORMALIZE_DATA);
    if(tagno==655) rc=decode_value(YEP655table,srcp,src_len,&mshp,EXACT_MATCH,NO_NORMALIZE_DATA);

    if(!rc) {

      /*=====================================================================*/
      /* If not, check to see if $a is in 650-655 table...                   */
      /*=====================================================================*/
      if(tagno==650) {
	rc=decode_value(YEP650_655table,tsrcp,tsrc_len,&mshp,EXACT_MATCH,NO_NORMALIZE_DATA);
      }
      
      if(!rc) {
	/*=====================================================================*/
	/* If not, copy field over as-is...                                    */  
	/*=====================================================================*/
	marc_put_field(pp,tagno,-1);
	free(src2);
	continue;
      }
      else {
	/*=====================================================================*/
	/* If it is, check to make sure @2='2' but NOT for 698s...             */
	/*=====================================================================*/
	marc_get_indic(pp->inmp,2,&indic);
	/*=====================================================================*/
	/* If not, send out error message...                                   */
	/*=====================================================================*/
	if(indic!='2') {
	  cm_error(CM_WARNING,"%d indicator 2 not '2'...", tagno);
	  if(marc_get_field(pp->inmp,tagno,focc,&tmp1,&tmp1len)==0)
	    cm_error(CM_CONTINUE,"%d='%*.*s'",tagno,
		     tmp1len,tmp1len,(char *)tmp1);
	  
	  if(marc_get_item(pp->inmp,995,0,'a',0,&tmp2,&tmp2len)==0)
	    cm_error(CM_CONTINUE,"995$a='%*.*s'",
		     tmp2len,tmp2len,(char *)tmp2);
	  
	  if(marc_get_item(pp->inmp,999,0,'a',0,&tmp3,&tmp3len)==0)
	    cm_error(CM_CONTINUE,"999$a='%*.*s'",
		     tmp3len,tmp3len,(char *)tmp3);
	  if(marc_get_field(pp->inmp,tagno,focc,&tmp1,&tmp1len)==0)
	    marc_put_field(pp,tagno,-1);
	}
	/*=====================================================================*/
	/* Otherwise, replace/add as necessary...                              */
	/*=====================================================================*/
	else {
	  /*===================================================================*/
	  /* Create instance of 655 with new data...                           */
	  /*===================================================================*/
	  if (marc_add_field(pp->outmp, 655)!=0) {
	    cm_error(CM_FATAL, "Error creating field 655. Record not processed.");
	    return CM_STAT_KILL_RECORD;
	  }
	  marc_set_indic(pp->outmp,2,'2');

	  marc_add_subfield(pp->outmp,'a',(unsigned char *)mshp->marc_data, 
			    strlen(mshp->marc_data));

	  set_modified(pp,"1");
	}
      }
    }
    else {
      /*=====================================================================*/
      /* If it is, check to make sure @2='2' but NOT for 698s...             */
      /*=====================================================================*/
      marc_get_indic(pp->inmp,2,&indic);
      /*=====================================================================*/
      /* If not, send out error message...                                   */
      /*=====================================================================*/
      if((tagno!=698)&&(indic!='2')) {
	cm_error(CM_WARNING,"%d indicator 2 not '2'...", tagno);
	if(marc_get_field(pp->inmp,tagno,focc,&tmp1,&tmp1len)==0)
	  cm_error(CM_CONTINUE,"%d='%*.*s'",tagno,
		   tmp1len,tmp1len,(char *)tmp1);
	
	if(marc_get_item(pp->inmp,995,0,'a',0,&tmp2,&tmp2len)==0)
	  cm_error(CM_CONTINUE,"995$a='%*.*s'",
		   tmp2len,tmp2len,(char *)tmp2);
	
	if(marc_get_item(pp->inmp,999,0,'a',0,&tmp3,&tmp3len)==0)
	  cm_error(CM_CONTINUE,"999$a='%*.*s'",
		   tmp3len,tmp3len,(char *)tmp3);
	if(marc_get_field(pp->inmp,tagno,focc,&tmp1,&tmp1len)==0)
	  marc_put_field(pp,tagno,-1);
      }
      /*=====================================================================*/
      /* Otherwise, replace/add as necessary...                              */
      /*=====================================================================*/
      else {
	marc_replace_subfields(pp,tagno,mshp->hstar_data,mshp->marc_data);
	set_modified(pp,"1");
      }
    }
  }    

  return CM_STAT_OK;
  
} /* cmp_proc_650 */



/************************************************************************
* cmp_proc_651   ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_651 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  static int s_first_time = 1;/* Force load of tbls on 1st rec       */
  unsigned char *tsrcp,       /* Ptr to data w/o spaces collapsed    */
                *tmp1,
                *tmp2,
                *tmp3,
                *tagp,
                 indic;        /* Ptr to input data                   */
  char          *srcp,        /* Ptr to temp data                    */
                *src2,        /* Ptr to data with spaces collapsed   */
                *src3, 
                *outbeg, 
                *outend,
                *tmp1a,
                *tmp2a,
                *tmpp;        /* Ptr to data with spaces collapsed   */
  size_t        tsrc_len,     /* Length of source (collapsed)        */
                tmp1len,
                tmp2len,
                tmp3len,
                tagplen,
                src_len;      /* Length of source (collapsed)        */
  int           focc,         /* Field occurrence counter            */
                chktagprob,   /* Field occurrence counter            */
                occ,          /* Field occurrence counter            */
                delAnt,        /* Field occurrence counter            */
                Antigua,      /* Field occurrence counter            */
                delMac,        /* Field occurrence counter            */
                Macau,      /* Field occurrence counter            */
                history,      /* Field occurrence counter            */
                missing8,     /* Field occurrence counter            */
                socc,         /* Field occurrence counter            */
                sf_code;      /* Subfield code                       */


  /*=========================================================================*/
  /* For debugging purposes...                                               */
  /*=========================================================================*/
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  
  for(focc=0;marc_get_item(pp->inmp,651,focc,'a',0,&tsrcp,&src_len)==0;focc++){
    srcp=(char *)tsrcp;

    if((strncmp(srcp,"Barbuda and Antigua",19)==0)&&(src_len==19))   {
      marc_replace_subfields(pp,651,"Barbuda and Antigua","Antigua and Barbuda");
      set_modified(pp,"1");
    } else marc_put_field(pp,651,-1);

  }   /*  for  */

  return CM_STAT_OK;
    
}   /* cmp_proc_651  */


/************************************************************************
* cmp_proc_655   ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_proc_655 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  unsigned char *srcp,       /* Ptr to data w/o spaces collapsed    */
                 ind1,        /* Ptr to input data                   */
                 ind2;        /* Ptr to input data                   */
  size_t        src_len;      /* Length of source (collapsed)        */
  int           occ,         /* Field occurrence counter            */
                occ2,          /* Field occurrence counter            */
                webcasts,          /* Field occurrence counter            */
                citrel;          /* Field occurrence counter            */

  citrel = webcasts = 0;
  /*=========================================================================*/
  /* First, loop through 998's to see if any have $a=CITREL...               */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,998,occ,&srcp,&src_len)==0;occ++) {
    
    /*=======================================================================*/
    /* If $a = CITREL...                                                     */
    /*=======================================================================*/
    if(marc_get_subfield(pp->inmp,'a',0,&srcp,&src_len)==0) {
      if((strncmp((char *)srcp,"CITREL",src_len)==0)&&
	 (src_len==6)) {
	citrel=1;
	break;
      }
    }
  }

  /*=========================================================================*/
  /* Loop through 655's...                                                   */
  /*=========================================================================*/
  for(occ2=0;marc_get_field(pp->inmp,655,occ2,&srcp,&src_len)==0;
      occ2++) {
    
    /*=======================================================================*/
    /* Check for existing $a=Webcasts...                                     */
    /*=======================================================================*/
    if(marc_get_subfield(pp->inmp,'a',0,&srcp,&src_len)==0) {
      if((strncmp((char *)srcp,"Webcasts",src_len)==0)&&
	 (src_len==8)) {
	
	webcasts++;
	/*===================================================================*/
	/* If it's already there, make sure indicators are ' 2'...           */
	/*===================================================================*/
	marc_get_indic(pp->inmp,1,&ind1);
	marc_get_indic(pp->inmp,2,&ind2);

	/*===================================================================*/
	/* If they aren't, send out warning message if we have 998$a=CITREL  */
	/*===================================================================*/
	if((citrel) &&
	   ((ind1!=' ')||(ind2!='2'))) {
	  cm_error(CM_WARNING,"655 $a=Webcasts already exists but "
		   "indicators are not ' 2'.");
	}
      }
    }
    
    /*=======================================================================*/
    /* Regardless, output the field...                                       */
    /*=======================================================================*/
    marc_put_field(pp,655,-1);
  }
  
  /*=========================================================================*/
  /* If we have 998 $a=CITREL and no 655's have $a=Webcasts, add one...      */
  /*=========================================================================*/
  if((citrel) && (!webcasts)) {
    cmp_buf_write(pp, "655:+$a:0", (unsigned char *)"Webcasts", 8, 0);
    
    /*=========================================================================*/
    /* @1 = ' ' and @2 = '2'                                                   */
    /*=========================================================================*/
    cmp_buf_write(pp, "655:-1@1",  (unsigned char *) " ", 1, 0);
    cmp_buf_write(pp, "655:-1@2",  (unsigned char *) "2", 1, 0);
    
    set_modified(pp, "1");
  }

  return CM_STAT_OK;
  
} /* cmp_proc_655 */



/************************************************************************
* marc_put_field ()                                                     *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT marc_put_field (
    CM_PROC_PARMS *pp,              /* Pointer to parameter struct     */
    int     tag,                    /* Field tag/identifier            */
    int     fldcnt                  /* # of flds to copy or -1 for all */
) {
  unsigned char *tsrcp;        /* Ptr to input data                   */
  size_t         src_len;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                 sf_code;      /* Subfield code                       */

  /*=========================================================================*/
  /* Create new instance of tag output field...                              */
  /*=========================================================================*/
  if (marc_add_field(pp->outmp, tag)!=0) {
    cm_error(CM_FATAL, "Error creating field %d. Record not processed.",tag);
    return CM_STAT_KILL_RECORD;
  }

  /*=========================================================================*/
  /* Now loop through each subfield and write it out...                      */
  /*=========================================================================*/
  for(occ=0;marc_pos_subfield(pp->inmp,occ,&sf_code,&tsrcp,&src_len)==0;occ++){

    if((fldcnt<0)||(fldcnt>occ)) {
      /*=====================================================================*/
      /* If occ<2, we're looking at indicators...                            */
      /*=====================================================================*/
      if(occ<2) {
	  if(marc_set_indic(pp->outmp, sf_code, *tsrcp)!=0) { 
	    cm_error(CM_FATAL, "Error setting indicator %d for %d. "
		     "Record not processed.", occ, tag);
	    return CM_STAT_KILL_RECORD;
	  }
      }
      /*=====================================================================*/
      /* Otherwise, send out as-is...                                        */
      /*=====================================================================*/
      else {
	marc_add_subfield(pp->outmp, sf_code, tsrcp, src_len);
      }
    }
  }
  
  return CM_STAT_OK;
} /* marc_put_field */



/************************************************************************
* marc_replace_subfields ()                                             *
*                                                                       *
*   DEFINITION                                                          *
*       Creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT marc_replace_subfields (
    CM_PROC_PARMS *pp,              /* Pointer to parameter struct     */
    int     tag,                     /* Field tag/identifier            */
    char *infld,
    char *outfld
) {
  unsigned char *tsrcp,
                *tmpp,        /* Ptr to input data                   */
                 ind;         /* Ptr to input data                   */
  char          *out1,
                *out2,
                *inhash,
                *outhash,
                *outend;
  size_t         src_len,
                 tmp_len;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                 sf_code,      /* Subfield code                       */
                 inx,
                 anothertodo,
                 outx;
  int            have9n;

  /*=========================================================================*/
  /* For debugging purposes...                                               */
  /*=========================================================================*/
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"1040371",8)==0)
    uip=uip;

  /*=========================================================================*/
  /* First, let's find out if we have $a ! $x or just $a...                  */
  /*=========================================================================*/
  inhash=strchr(infld,'!');
  if(inhash==NULL)
    inx=0;
  else
    inx=1;

  out1=out2=outhash=outend=outfld;
  anothertodo=1;
  outend++;

  /*=========================================================================*/
  /* Now, we output this field as many times as necessary with replacements  */
  /*=========================================================================*/
  while(anothertodo==1) {

    /*=======================================================================*/
    /* Create new instance of tag output field...                            */
    /*=======================================================================*/
    if (marc_add_field(pp->outmp, tag)!=0) {
      cm_error(CM_FATAL, "Error creating field %d. Record not processed.",
	       tag);
      return CM_STAT_KILL_RECORD;
    }

    outend=strchr(out1,'@');
    if(outend!=NULL)
      anothertodo=1;
    else {
      anothertodo=0;
      outend=out1+strlen(out1);
    }

    outhash=strchr(out1,'!');
    if(outhash!=NULL)
      outx=1;
    else {
      outhash=outend;
      outx=0;
    }

    if((anothertodo==1)&&(outx==1))
      if(outhash>outend) {
	outhash=outend;
	outx=0;
      }
    if(outx==1)
      out2=outhash+1;

    have9n=0;
    marc_get_indic(pp->inmp,1,&ind);

    /*=========================================================================*/
    /* Now loop through each subfield and write it out...                      */
    /*=========================================================================*/
    for(occ=0;marc_pos_subfield(pp->inmp,occ,&sf_code,&tsrcp,&src_len)==0;occ++){
      
      /*=====================================================================*/
      /* If occ<2, we're looking at indicators...                            */
      /*=====================================================================*/
      if(occ<2) {
	// !!! 2016 change: make second, NEW, field have indicators of 22
	// new logic goes from here...
	if((outend==out1+strlen(out1)) &&
	   (out1!=outfld)) {
	  if(marc_set_indic(pp->outmp, sf_code, '2')!=0) { 
	    cm_error(CM_FATAL, "Error setting indicator %d for %d. "
		     "Record not processed.", occ, tag);
	    return CM_STAT_KILL_RECORD;
	  }
	}
	else
	  // !!! ...to here. The following is original and just copies 
	  // the indicators out as-is
	  if(marc_set_indic(pp->outmp, sf_code, *tsrcp)!=0) { 
	    cm_error(CM_FATAL, "Error setting indicator %d for %d. "
		     "Record not processed.", occ, tag);
	    return CM_STAT_KILL_RECORD;
	  }
	  

      }
      /*=======================================================================*/
      /* Otherwise, check to see if it needs replaced...                       */
      /*=======================================================================*/
      else {
	if(sf_code=='a') {
	  marc_add_subfield(pp->outmp, sf_code, (unsigned char *)out1,
			    outhash-out1);
	}
	else {
	  if((sf_code=='x')&&(inx==1)) {
	    if(outx==1)
	      marc_add_subfield(pp->outmp, sf_code, (unsigned char *)out2,
				outend-out2);
	  }
	  /*===================================================================*/
	  /* If not, send it out as-is...                                      */
	  /*===================================================================*/
	  else {
	    have9n=((sf_code=='9') && (*tsrcp=='n')) || have9n;
	    marc_add_subfield(pp->outmp, sf_code, tsrcp, src_len);
	  }
	}
      }
    }
    
    if(anothertodo==1)
      out1=outend+1;
  }

  return CM_STAT_OK;
} /* marc_replace_subfields */


/************************************************************************
*  concat_sfs()                                                         *
*                                                                       *
*   DEFINITION                                                          *
*       returns concatenation of specific subfields                     *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
char *concat_sfs (
    CM_PROC_PARMS *pp,             /* Pointer to parameter struct     */
    int tag,
    int occ,
    char *sfs
) {
  unsigned char *tsrc1,       /* Ptr to data w/o spaces collapsed    */
                *tsrc2,       /* Ptr to input data                   */
                *tsrc3,       /* Ptr to input data                   */
                 ind;         /* Ptr to input data                   */
  char          *indc,
                *outp,
                *tmp2,        /* Ptr to temp data                    */
                *tmpp;        /* Ptr to temp data                    */
  char          delim[3];
  size_t        src1len,      /* Length of source (collapsed)        */
                src2len,      /* Length of source (collapsed)        */
                src3len,
                outlen;      /* Length of source (collapsed)        */
  
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"100942269",9)==0)
    uip=uip;

  tsrc1=tsrc2=tsrc3=NULL;
  src1len=src2len=src3len=outlen=0;
  outp=NULL;

  memset(delim, '\0', 3);

  for(tmpp=sfs;*tmpp;tmpp++) {
    if(*tmpp=='@') {
      tmpp++;
      if(marc_get_indic(pp->inmp,*tmpp,&ind)==0) {
	if(allocate_n_ncopy(&tmp2, outlen, 5, outp,
			    NO_COLLAPSE_SPACES,"concat_sfs")==0) {
	  free(outp);
	  outp=tmp2;
	  printf(delim,"@%c",31+tmpp-sfs);
	  strcat(outp,delim);

	  //	  	  indc=(char *)ind;
		  indc=(char *)&ind;
	  	  strncat(outp,(char *)indc,1);
	  //	  strncat(outp,(char *)ind,1);
	  outlen=strlen(outp);
	}
      }
    }
    else {
      if(marc_get_item(pp->inmp,tag,occ,*tmpp,0,&tsrc1,&src1len)==0) {
	
	if(allocate_n_ncopy(&tmp2,outlen,src1len+4,outp,
			    NO_COLLAPSE_SPACES,"concat_sfs")==0) {
	  
	  free(outp);
	  outp=tmp2;
	  printf(delim,"\\%c",31+tmpp-sfs);
	  strcat(outp,delim);
	  if(src1len>0)
	    strncat(outp,(char *)tsrc1,src1len);
	  outlen=strlen(outp);
	}
      }
    }
  }
  
  return outp;
} /* concat_sfs */


/************************************************************************
* cmp_dedupe_650_1 ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_dedupe_650_1 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  unsigned char *tsrc1a,       /* Ptr to data w/o spaces collapsed    */
                *tsrc2a,       /* Ptr to input data                   */
                *tsrc18,      /* Ptr to input data                   */
                *tsrc28,      /* Ptr to input data                   */
                *tsrc1x,      /* Ptr to input data                   */
                *tsrc2x,      /* Ptr to input data                   */
                 ind1,        /* Ptr to input data                   */
                 ind2,        /* Ptr to input data                   */
                *ttmp2,       /* Ptr to input data                   */
                *ttmp3,       /* Ptr to input data                   */
                *tmpp;        /* Ptr to input data                   */
  char          *src1a,       /* Ptr to temp data                    */
                *src2a,       /* Ptr to data with spaces collapsed   */
                *src18,       /* Ptr to data with spaces collapsed   */
                *src28,       /* Ptr to data with spaces collapsed   */
                *src1x,       /* Ptr to data with spaces collapsed   */
                *src2x;       /* Ptr to data with spaces collapsed   */
  size_t        ttmplen,      /* Length of source (collapsed)        */
                ttmp2len,     /* Length of source (collapsed)        */
                ttmp3len,     /* Length of source (collapsed)        */
                src1alen,     /* Length of source (collapsed)        */
                src2alen,     /* Length of source (collapsed)        */
                src18len,     /* Length of source (collapsed)        */
                src28len,     /* Length of source (collapsed)        */
                src1xlen,     /* Length of source (collapsed)        */
                src2xlen;     /* Length of source (collapsed)        */
  int           l1,           /* Field occurrence counter            */
                l2,           /* Field occurrence counter            */
                current_action,/* Field occurrence counter           */
                tag;      /* Subfield code                       */
  
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  if(strncmp((char *)uip,"1200713",9)==0)
    uip=uip;

  /*=========================================================================*/
  /* Determine which field we're to process...                               */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &tsrc1a, &ttmplen);
  if (!get_fixed_num ((char *) tsrc1a, ttmplen, &tag)) {
    cm_error(CM_FATAL,"Unable to determine numeric from %*.*s",
	     ttmplen,ttmplen,(char *)tsrc1a);
  }

  /*=========================================================================*/
  /* Loop through all the fields...                                          */
  /*=========================================================================*/
  for(l1=0;marc_get_item(pp->inmp,tag,l1,'a',0,&tsrc1a,&src1alen)==0;l1++){
    src1a=(char *)tsrc1a;

    /*=======================================================================*/
    /* If we have $x, we can skip this one since it won't be a dupe...       */
    /*=======================================================================*/
    if(marc_get_item(pp->inmp,tag,l1,'x',0,&tsrc1x,&src1xlen)==0) 
      src1x=(char *)tsrc1x;

    /*=======================================================================*/
    /* We'll need to check first indicator later...                          */
    /*=======================================================================*/
    marc_get_indic(pp->inmp,1,&ind1);

    current_action=ACTION_NEUTRAL;
    /*=======================================================================*/
    /* Otherwise, we need to see if this one is duplicated elsewhere...      */
    /*=======================================================================*/
    for(l2=0;
	((marc_get_item(pp->inmp,tag,l2,'a',0,&tsrc2a,&src2alen)==0) &&
	 (current_action!=ACTION_SKIP)&&
	 (current_action!=ACTION_OUTPUT));
	l2++){
      src2a=(char *)tsrc2a;

      /*=====================================================================*/
      /* Make sure we're not looking at ourselves...                         */
      /*=====================================================================*/
      if(l1==l2) {
	continue;
      }
    
      /*=====================================================================*/
      /* or any other $x's...                                                */
      /*=====================================================================*/      
      if(marc_get_item(pp->inmp,tag,l2,'x',0,&tsrc2x,&src2xlen)==0)
	src2x=(char *)tsrc2x;

      /*=====================================================================*/
      /* If $a's don't match, there's no duplicate...                        */
      /*=====================================================================*/      
      if((strncmp(src1a,src2a,src1alen)!=0)||(src1alen!=src2alen)) {
	continue;
      }

      /*=====================================================================*/
      /* If $x's don't match, there's no duplicate...                        */
      /*=====================================================================*/
      if(src1xlen!=src2xlen) {
	continue;
      }
      else
	if(src1xlen>0)
	  if(strncmp(src1x,src2x,src1xlen)!=0) {
	    continue;
	  }

      /*=====================================================================*/
      /* Let's get first indicator for later...                              */
      /*=====================================================================*/      
      marc_get_indic(pp->inmp,1,&ind2);

      /*=====================================================================*/
      /* If current one has NO $8...                                         */
      /*=====================================================================*/
      if(marc_get_item(pp->inmp,tag,l1,'8',0,&tsrc18,&src18len)!=0) {

	/*===================================================================*/
	/* If second one has $8, take current one...                         */
	/*===================================================================*/
	if(marc_get_item(pp->inmp,tag,l2,'8',0,&tsrc28,&src28len)==0) 
	  current_action=ACTION_OUTPUT;

	/*===================================================================*/
	/* If second one has NO $8, take current one but only if second has  */
	/* not already been sent out...                                      */
	/*===================================================================*/
	else {
	  /*=================================================================*/
	  /* If one has $e and other doesn't, send both out with warning     */
	  /*=================================================================*/
	  if(((marc_get_item(pp->inmp,tag,l1,'e',0,&ttmp2,&ttmp2len)==0)&&
	      (marc_get_item(pp->inmp,tag,l2,'e',0,&ttmp2,&ttmp2len)!=0))
	     ||
	     ((marc_get_item(pp->inmp,tag,l1,'e',0,&ttmp2,&ttmp2len)!=0)&&
	      (marc_get_item(pp->inmp,tag,l2,'e',0,&ttmp2,&ttmp2len)==0))) {
	    cm_error(CM_WARNING,"Check %d: Found matching occurrences with $e",
		     tag);
	    current_action=ACTION_OUTPUT;
	  }
	  else {
	    if(l1<l2) {
	      current_action=ACTION_OUTPUT;
	    }
	    else {
	      current_action=ACTION_SKIP;
	      continue;
	    }
	  }
	}
      }
      /*=====================================================================*/
      /* If current one has $8...                                            */
      /*=====================================================================*/      
      else {
	src18=(char *)tsrc18;
	/*===================================================================*/
	/* If second one has NO $8, we want second one...                    */
	/*===================================================================*/
	if(marc_get_item(pp->inmp,tag,l2,'8',0,&tsrc28,&src28len)!=0) {
	  current_action=ACTION_SKIP;
	  continue;
	}
        /*===================================================================*/
        /* If second one has $8, that means they both have one...            */
        /*===================================================================*/	
	else {
	  src28=(char *)tsrc28;

	  /*=================================================================*/
	  /* If first letters of $8's are different, send current one out... */
	  /*=================================================================*/	
	  if(*src18!=*src28) {
	    current_action=ACTION_OUTPUT;

	    /*===============================================================*/
	    /* Also send warning message if we haven't already done so...    */
	    /*===============================================================*/
	    /* NO MESSAGE ON DUPLICATE? 
	    if(l1<l2) {
	      cm_error(CM_WARNING,"Duplicate %d's with different $8's",tag);
	      
	      if(marc_get_field(pp->inmp,tag,l1,&tmpp,&ttmplen)==0) 
		cm_error(CM_CONTINUE,"%d(%02d): %*.*s",tag,
			 l1+1,ttmplen,ttmplen,(char *)tmpp);
	      else
		cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l1+1);
	      
	      if(marc_get_field(pp->inmp,tag,l2,&tmpp,&ttmplen)==0) 
		cm_error(CM_CONTINUE,"tag(%02d): %*.*s",tag,
			 l2+1,ttmplen,ttmplen,(char *)tmpp);
	      else
		cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l2+1);
	    }
	    */
	    
	    continue;
	  }
	  /*=================================================================*/
	  /* If first letters of $8's are the same, keep the 'prime' one...  */
	  /*=================================================================*/	
	  else {
	    src18++;
	    if((src18len==src28len+5)&&(strncmp(src18,"prime",5)==0)) {
	      current_action=ACTION_OUTPUT;
	    }
	    else {
	      src18--;
	      src28++;
	      if((src28len==src18len+5)&&(strncmp(src28,"prime",5)==0)) {
		current_action=ACTION_SKIP;
	      }
	      /*=================================================================*/
	      /* At this point, first indicators dictate which one to take...*/
	      /*=================================================================*/	
	      else {
		/*===============================================================*/
		/* Order is 1, 2, ' '... so let's check for a 1 first...         */
		/*===============================================================*/	
		if(ind1=='1') {
		  /*=============================================================*/
		  /* If we have one, we need to make sure there aren't any       */
		  /* duplicates of these so we need to walk through the rest of  */
		  /* them until we catch up to ourselves. If we encounter a 1    */
		  /* along the way we can ignore the current one but we can't    */
		  /* take the current one until we know for sure it is the first */
		  /* of the 1's...                                               */
		  /*=============================================================*/	
		  if(l2<l1) {
		    if(ind2=='1')
		      current_action=ACTION_SKIP;
		  }
		  else
		    current_action=ACTION_OUTPUT;
		}
		/*===============================================================*/
		/* If the second has a 1, we know we can skip the current one... */
		/*===============================================================*/	
		else {
		  if(ind2=='1')
		    current_action=ACTION_SKIP;
		/*===============================================================*/
		/* If the second is 2 and we're ' ', we'll eventually be taking  */
		/* the second one, so skip this one now...                       */
		/*===============================================================*/	
		  else {
		    if((ind2=='2')&&(ind1!='2'))
		      current_action=ACTION_SKIP;
		/*===============================================================*/
		/* We get here if we can't conclusively rule the current one out */
		/*===============================================================*/	
		    else {
		      /*		      if(current_action==ACTION_NEUTRAL)
			current_action=l2+101;
		      */
		      /*=========================================================*/
		      /* If one has $e and other doesn't, output both w/warning  */
		      /*=========================================================*/
		      if(((marc_get_item(pp->inmp,tag,l1,'e',0,&ttmp2,&ttmp2len)==0)&&
			  (marc_get_item(pp->inmp,tag,l2,'e',0,&ttmp2,&ttmp2len)!=0))
			 ||
			 ((marc_get_item(pp->inmp,tag,l1,'e',0,&ttmp2,&ttmp2len)!=0)&&
			  (marc_get_item(pp->inmp,tag,l2,'e',0,&ttmp2,&ttmp2len)==0))) {
			cm_error(CM_WARNING,"Check %d: Found matching occurrences "
				 "with $e", tag);
			current_action=ACTION_OUTPUT;
		      }
		      else {
			if(l1<l2) {
			  current_action=ACTION_OUTPUT;
			}
			else {
			  current_action=ACTION_SKIP;
			  continue;
			}
		      }
		    }
		  }
		}
	      }
	    }
	  }
	}
      }
    }
    

    /*=======================================================================*/
    /* Output current the field if we're not to skip it...                   */
    /*=======================================================================*/
    if(marc_get_field(pp->inmp,tag,l1,&tmpp,&ttmplen)!=0) 
      cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l1+1);

    if(current_action!=ACTION_SKIP) {
      marc_put_field(pp,tag,-1);
    }

    if(current_action>100) {
      l2=current_action-101;
      
      if(l1<l2) {
	cm_error(CM_WARNING,"Check collaborative partner %d's", tag);
	
	if(marc_get_item(pp->inmp,995,0,'a',0,&ttmp2,&ttmp2len)!=0)
	  cm_error(CM_CONTINUE,"Unable to retrieve 995$a");
	else 
	  cm_error(CM_CONTINUE,"995$a='%*.*s'",
		   ttmp2len,ttmp2len,(char *)ttmp2);
	
	if(marc_get_item(pp->inmp,999,0,'a',0,&ttmp3,&ttmp3len)!=0)
	  cm_error(CM_CONTINUE,"Unable to retrieve 999$a");
	else
	  cm_error(CM_CONTINUE,"999$a='%*.*s'",
		   ttmp3len,ttmp3len,(char *)ttmp3);
	
      }
      
      cm_error(CM_CONTINUE,"Possible duplicate %d #%02d: %*.*s",tag,
	       l1+1,ttmplen,ttmplen,(char *)tmpp);
      /*---	if(marc_get_field(pp->inmp,tag,l2,&ttmp2,&ttmp2len)!=0) 
	cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l2+1);
	cm_error(CM_CONTINUE,"because of  %d #%02d: %*.*s",tag,
	l2+1,ttmplen,ttmplen,(char *)tmpp);
	---*/
      
    }
  }
  
  return CM_STAT_OK;
  
} /* cmp_dedupe_650_1 */



/************************************************************************
*  cmp_reorder_650_1()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_reorder_650_1 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  
  unsigned char *tsrcp,        /* Ptr to input data                   */
                *tsrc2,        /* Ptr to input data                   */
                 indic1,        /* Ptr to input data                   */
                 indic2;        /* Ptr to input data                   */
  char          *srtind1,        /* Ptr to output data                  */
                *srtind2,        /* Ptr to output data                  */
                *srtsf8,        /* Ptr to output data                  */
                *src2,        /* Ptr to output data                  */
                *srtsf2e,        /* Ptr to output data                  */
                *srtsfa;        /* Ptr to output data                  */
  size_t         tsrclen,      /* Length of source field              */
                 src1len,      /* Length of source field              */
                 src2len,      /* Length of source field              */
                 srt8len,      /* Length of source field              */
                 srt2len,      /* Length of source field              */
                 srtalen;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                tag;      /* Subfield code                       */

  cmp_buf_find(pp, "bibid", &uip, &ui_len);

  /*=========================================================================*/
  /* Determine which field we're to process...                               */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &tsrcp, &src1len);
  if (!get_fixed_num ((char *) tsrcp, src1len, &tag)) {
    cm_error(CM_FATAL,"Unable to determine numeric from %*.*s",
	     src1len,src1len,(char *)tsrcp);
  }

  sort_cnt =  0;


  /*=========================================================================*/
  /* For each occurrence of specified tag...                                 */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,tag,occ,&tsrcp,&src1len)==0;occ++) {
    
    srtind1=srtind2=srtsf8=srtsf2e=srtsfa=NULL;
    
    /*=======================================================================*/
    /* tags sort on @1, $e, $8, $a...                                        */
    /*=======================================================================*/
    if(marc_get_indic(pp->inmp,1,&indic1)==0)
      srtind1 = (char *) &indic1;
    
    if(marc_get_item(pp->inmp,tag,occ,'8',0,&tsrcp,&srt8len)==0)
      srtsf8 = (char *) tsrcp;
    
    if(marc_get_item(pp->inmp,tag,occ,'e',0,&tsrcp,&srt2len)==0)
      srtsf2e = (char *) tsrcp;
    
    if(marc_get_item(pp->inmp,tag,occ,'a',0,&tsrcp,&srtalen)==0)
      srtsfa = (char *) tsrcp;
    
    if(marc_get_item(pp->inmp,tag,occ,'x',0,&tsrc2,&tsrclen)==0) {
      if(allocate_n_ncopy(&src2,srtalen,tsrclen+1,(char *)tsrcp,
			  NO_COLLAPSE_SPACES,"650")!=0)
	return CM_STAT_KILL_RECORD;
      
      strcat(src2,"!");
      strncat(src2,(char *)tsrc2,tsrclen);
      
      srtsfa = src2;
      srtalen=strlen(src2);
    }
    
    /*=======================================================================*/
    /* Place it into the array in sorted order...                            */
    /*=======================================================================*/
    add_to_650_array(pp, tag, occ,
		      srtind1, 1,
		      srtind2, 1,
		      srtsf2e, srt2len,
		      srtsf8, srt8len,
		      srtsfa, srtalen);
  }
  
  /*=========================================================================*/
  /* Now go back through and write them in sorted order...                   */
  /*=========================================================================*/
  for(occ=0;occ<sort_cnt;occ++) {

    /*=======================================================================*/
    /* Retrieve specified occurrence of input field...                       */
    /*=======================================================================*/
    if(marc_get_field(pp->inmp,tag,sort_650_array[occ].occ,&tsrcp,&src1len)!=0) {
      cm_error(CM_FATAL,"Error retrieving %d #%d. Record not processed.",
	       tag,sort_650_array[occ].occ+1);
      return CM_STAT_KILL_RECORD;
    }

    /*=======================================================================*/
    /* Create new occurrence of output field...                              */
    /*=======================================================================*/
    marc_put_field(pp,tag,-1);

    /*=======================================================================*/
    /* Release memory for sort fields...                                     */
    /*=======================================================================*/
    if(sort_650_array[occ].srtind1)
      free(sort_650_array[occ].srtind1);

    if(sort_650_array[occ].srtind2)
      free(sort_650_array[occ].srtind2);

    if(sort_650_array[occ].srtsf8)
      free(sort_650_array[occ].srtsf8);

    if(sort_650_array[occ].srtsf2e)
      free(sort_650_array[occ].srtsf2e);

    if(sort_650_array[occ].srtsfa)
      free(sort_650_array[occ].srtsfa);

  }

  return CM_STAT_OK;
  
} /* cmp_reorder_650_1 */


/************************************************************************
* add_to_650_array()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
void add_to_650_array(CM_PROC_PARMS *pp,
		       int   tag, int socc,
		       char *ind1, size_t len1, 
		       char *ind2, size_t len2,
		       char *sfe, size_t lensfe,
		       char *sf8, size_t lensf8,
		       char *sfa, size_t lensfa)
{
  unsigned char *tsrcp;
  char *tmp1,
       *tmp2,
       *tmp3,
       *tmp4,
       *tmp5,
       *tmpp;
  size_t tsrclen;
  int   action,
        occ,
        occ2;

  if(strncmp((char *)uip,"9111489",7)==0)
    uip=uip;

  tmp1=tmp2=tmp3=tmp4=tmp5=NULL;

  /*=========================================================================*/
  /* Allocate and copy each sort field over to a separate area of memory...  */
  /*=========================================================================*/
  if(ind1) {
    if(allocate_n_ncopy(&tmp1,len1,1,ind1,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp1;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }
  
  if(ind2) {
    if(allocate_n_ncopy(&tmp2,len2,1,ind2,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp2;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);  
  }

  if(sf8) {
    if(allocate_n_ncopy(&tmp3,lensf8,1,sf8,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp3;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  if(sfe) {
    if(allocate_n_ncopy(&tmp4,lensfe,1,sfe,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp4;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  if(sfa) {
    if(allocate_n_ncopy(&tmp5,lensfa,1,sfa,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp5;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  /*=========================================================================*/
  /* Then figure out where it belongs in the sort array...                   */
  /*=========================================================================*/
  for(occ=0;(occ<sort_cnt)&&(occ<MAX_6XX_FIELDS);occ++) {

    /*=======================================================================*/
    /* @1's are to be ordered as follows:                                    */
    /*   1) '1'                                                              */
    /*   2) '2'                                                              */
    /*   3) '0'                                                              */
    /*   4) ' '                                                              */
    /*=======================================================================*/


    /*=======================================================================*/
    /* MISSING first field...                                                */
    /*=======================================================================*/
    if(!tmp1) {
      /*=====================================================================*/
      /* Array starts with MISSING first field...                            */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtind1) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with first field present...                            */
      /*=====================================================================*/
      else {
	action=NEXT_ENTRY;
      }
    }
    /*=======================================================================*/
    /* First field present...                                                */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING first field...                            */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtind1) {
	action=INSERT_HERE;
      }
      /*=====================================================================*/
      /* Array starts with first field present...                            */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* '1' comes first...                                                */
	/*===================================================================*/
	if(strcmp(tmp1,"1")==0) {
	  /*=================================================================*/
	  /* If array starts with '1', check next level...                   */
	  /*=================================================================*/
	  if(strcmp(sort_650_array[occ].srtind1,"1")==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* If array starts with something else, insert here...             */
	  /*=================================================================*/
	  else {
	    action=INSERT_HERE;
	  }
	}
	else {
	  /*=================================================================*/
	  /* '2' comes next...                                               */
	  /*=================================================================*/
	  if(strcmp(tmp1,"2")==0) {
	    /*===============================================================*/
	    /* If array starts with '1', go to next entry...                 */
	    /*===============================================================*/
	    if(strcmp(sort_650_array[occ].srtind1,"1")==0) {
	      action=NEXT_ENTRY;
	    }
	    else {
	      /*=============================================================*/
	      /* If array starts with '2', check next level...               */
	      /*=============================================================*/
	      if(strcmp(sort_650_array[occ].srtind1,"2")==0) {
		action=NEXT_LEVEL;
	      }
	      /*=============================================================*/
	      /* If array starts with something else, insert here...         */
	      /*=============================================================*/
	      else {
		action=INSERT_HERE;
	      }
	    }
	  }
	  else {
	    /*===============================================================*/
	    /* '0' comes next...                                             */
	    /*===============================================================*/
	    if(strcmp(tmp1,"0")==0) {
	      /*=============================================================*/
	      /* If array starts with '1', go to next entry...               */
	      /*=============================================================*/
	      if(strcmp(sort_650_array[occ].srtind1,"1")==0) {
		action=NEXT_ENTRY;
	      }
	      else {
		/*===========================================================*/
		/* If array starts with '2', go to next entry...             */
		/*===========================================================*/
		if(strcmp(sort_650_array[occ].srtind1,"2")==0) {
		  action=NEXT_ENTRY;
		}
		else {
		  /*=========================================================*/
		  /* If array starts with '0', check next level...           */
		  /*=========================================================*/
		  if(strcmp(sort_650_array[occ].srtind1,"0")==0) {
		    action=NEXT_LEVEL;
		  }
		  /*=========================================================*/
		  /* If array starts with something else, insert here...     */
		  /*=========================================================*/
		  else {
		    action=INSERT_HERE;
		  }
		}
	      }
	    }
	    else {
	      /*=============================================================*/
	      /* blank comes next...                                         */
	      /*=============================================================*/
	      if(strcmp(tmp1," ")==0) {
		/*===========================================================*/
		/* If array starts with '1', go to next entry...             */
		/*===========================================================*/
		if(strcmp(sort_650_array[occ].srtind1,"1")==0) {
		  action=NEXT_ENTRY;
		}
		else {
		  /*=========================================================*/
		  /* If array starts with '2', go to next entry...           */
		  /*=========================================================*/
		  if(strcmp(sort_650_array[occ].srtind1,"2")==0) {
		    action=NEXT_ENTRY;
		  }
		  else {
		    /*=======================================================*/
		    /* If array starts with '0', go to next entry...         */
		    /*=======================================================*/
		    if(strcmp(sort_650_array[occ].srtind1,"0")==0) {
		      action=NEXT_ENTRY;
		    }
		    else {
		      /*=====================================================*/
		      /* If array starts with blank, check next level...     */
		      /*=====================================================*/
		      if(strcmp(sort_650_array[occ].srtind1," ")==0) {
			action=NEXT_LEVEL;
		      }
		      /*=====================================================*/
		      /* If array starts with something else, insert here... */
		      /*=====================================================*/
		      else {
			action=INSERT_HERE;
		      }
		    }
		  }
		}
	      }
	      /*=============================================================*/
	      /* After blank, do normal sort routine...                      */
	      /*=============================================================*/
	      else {
		cm_error(CM_WARNING,"%d has first indicator of %s",tag, tmp1);
		/*===========================================================*/
		/* First field PRECEDES array entry...                       */
		/*===========================================================*/
		if(strcmp(tmp1,sort_650_array[occ].srtind1)<0) {
		  action=INSERT_HERE;
		}
		else {
		  /*=========================================================*/
		  /* First field EQUALS array entry...                       */
		  /*=========================================================*/
		  if(strcmp(tmp1,sort_650_array[occ].srtind1)==0) {
		    action=NEXT_LEVEL;
		  }
		  /*=========================================================*/
		  /* First field FOLLOWS array entry...                      */
		  /*=========================================================*/
		  else {
		    action=NEXT_ENTRY;
		  }
		}
	      }
	    }
	  }
	}
      }
    }
    
    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    
    /*=======================================================================*/
    /* $e's are to be ordered alphabetically...                              */
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp4) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtsf2e) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtsf2e) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present, do normal sort routine...          */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* Field PRECEDES array entry...                                     */
	/*===================================================================*/
	if(strcmp(tmp4,sort_650_array[occ].srtsf2e)<0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
	  /* Field EQUALS array entry...                                     */
	  /*=================================================================*/
	  if(strcmp(tmp4,sort_650_array[occ].srtsf2e)==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* Field FOLLOWS array entry...                                    */
	  /*=================================================================*/
	  else {
	    action=NEXT_ENTRY;
	  }
	}
      }
    }
    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    

    /*=======================================================================*/
    /* $8 goes in alphabetical order with missing values first...            */
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp3) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtsf8) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with first field present...                            */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtsf8) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present, do normal sort routine...          */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* First field PRECEDES array entry...                               */
	/*===================================================================*/
	if(strcmp(tmp3,sort_650_array[occ].srtsf8)<0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
	  /* First field EQUALS array entry...                               */
	  /*=================================================================*/
	  if(strcmp(tmp3,sort_650_array[occ].srtsf8)==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* First field FOLLOWS array entry...                              */
	  /*=================================================================*/
	  else {
	    action=NEXT_ENTRY;
	  }
	}
      }
    }

    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    
    /*=======================================================================*/
    /* Moving on the last level, $a's are to be ordered alphabetically       */
    /*=======================================================================*/

    /*=======================================================================*/
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp5) {
      action=INSERT_HERE;
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_650_array[occ].srtsfa) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* Incoming field PRECEDES or EQUALS array entry...                  */
	/*===================================================================*/
	if(strcmp(tmp5,sort_650_array[occ].srtsfa)<=0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
          /* Incoming field FOLLOWS array entry...                           */
          /*=================================================================*/
	  action=NEXT_ENTRY;
	}
      }
    }

    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
  }

    
  /*=========================================================================*/
  /* Make sure we don't have too many entries...                             */
  /*=========================================================================*/
  if(occ==MAX_6XX_FIELDS)
    cm_error(CM_FATAL,"EXCEEDED MAXIMUM NUMBER OF 6XX FIELDS FOR SORT.");
  else {
    /*=======================================================================*/
    /* Make room in the array for the new entry...                           */
    /*=======================================================================*/
    for(occ2=sort_cnt;occ2>occ;occ2--) {
      sort_650_array[occ2].occ=sort_650_array[occ2-1].occ;
      sort_650_array[occ2].srtind1=sort_650_array[occ2-1].srtind1;
      sort_650_array[occ2].srtind2=sort_650_array[occ2-1].srtind2;
      sort_650_array[occ2].srtsf8=sort_650_array[occ2-1].srtsf8;
      sort_650_array[occ2].srtsf2e=sort_650_array[occ2-1].srtsf2e;
      sort_650_array[occ2].srtsfa=sort_650_array[occ2-1].srtsfa;
    }

    /*=======================================================================*/
    /* Insert the new entry in the appropriate slot...                       */
    /*=======================================================================*/
    sort_650_array[occ].occ  =socc;
    sort_650_array[occ].srtind1=tmp1;
    sort_650_array[occ].srtind2=tmp2;
    sort_650_array[occ].srtsf8=tmp3;
    sort_650_array[occ].srtsf2e=tmp4;
    sort_650_array[occ].srtsfa=tmp5;
    sort_cnt++;
  }  
  
} /* add_to_650_array */


/************************************************************************
* cmp_dedupe_655_9 ()                                                   *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_dedupe_655_9 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  unsigned char *tsrc1,       /* Ptr to data w/o spaces collapsed    */
                *tsrc2,       /* Ptr to input data                   */
                *ttmp,        /* Ptr to input data                   */
                 ind1,        /* Ptr to input data                   */
                 ind2,        /* Ptr to input data                   */
                 outind;        /* Ptr to input data                   */
  char          *src1,        /* Ptr to temp data                    */
                *src2,        /* Ptr to data with spaces collapsed   */
                *a2_1,        /* Ptr to data with spaces collapsed   */
                *a2_2,        /* Ptr to data with spaces collapsed   */
                *a22_1,       /* Ptr to data with spaces collapsed   */
                *a22_2,        /* Ptr to data with spaces collapsed   */
                *tmpp;        /* Ptr to data with spaces collapsed   */
  size_t        ttmplen,      /* Length of source (collapsed)        */
                src1len,      /* Length of source (collapsed)        */
                src2len,      /* Length of source (collapsed)        */
                src3len;      /* Length of source (collapsed)        */
  int           l1,           /* Field occurrence counter            */
                l2,           /* Field occurrence counter            */
                tag,          /* Field occurrence counter            */
                fldsentout;       /* Field occurrence counter            */
  
  cmp_buf_find(pp, "ui", &uip, &ui_len);

  /*=========================================================================*/
  /* Determine which field we're to process...                               */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &tsrc1, &ttmplen);
  if (!get_fixed_num ((char *) tsrc1, ttmplen, &tag)) {
    cm_error(CM_FATAL,"Unable to determine numeric from %*.*s",
	     ttmplen,ttmplen,(char *)tsrc1);
  }

  if(strncmp((char *)uip,"7607468",7)==0)
    uip=uip;

  src1=src2=NULL;

  /*=========================================================================*/
  /* Loop through all the fields...                                          */
  /*=========================================================================*/
  for(l1=0;marc_get_field(pp->inmp,tag,l1,&tsrc1,&src1len)==0;l1++) {

    /*=======================================================================*/
    /* If missing $a, send message and copy over as-is...                    */
    /*=======================================================================*/
    if(marc_get_item(pp->inmp,tag,l1,'a',0,&tsrc1,&src1len)!=0) {
      cm_error(CM_WARNING,"Check %d - $a is missing.",tag);
      marc_put_field(pp,tag,-1);
      continue;
    }

    fldsentout=0;
    /*=======================================================================*/
    /* Concatenate the $a, $2 and $8 for comparisons...                      */
    /*=======================================================================*/
    src1=concat_sfs(pp,tag,l1,"a@228"); 
    a2_1=concat_sfs(pp,tag,l1,"a@2"); 
    a22_1=concat_sfs(pp,tag,l1,"a@22"); 

    /*=======================================================================*/
    /* Now start looking to see if the next ones are equal to this one...    */
    /*=======================================================================*/
    for(l2=0;marc_get_item(pp->inmp,tag,l2,'a',0,&tsrc2,&src2len)==0;l2++){

      /*=====================================================================*/
      /* We'll need $a, $2, and $8's for these ones, too...                  */
      /*=====================================================================*/
      src2=concat_sfs(pp,tag,l2,"a@228");
      a2_2=concat_sfs(pp,tag,l2,"a@2");
      a22_2=concat_sfs(pp,tag,l2,"a@22");
      
      /*=====================================================================*/
      /* If they're equal, we're going to drop the second one...             */
      /*=====================================================================*/
      if(strcmp(src1,src2)==0) {

	if(l1<=l2) {
	  if(marc_get_field(pp->inmp,tag,l1,&tsrc1,&src1len)!=0) 
	    cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l1);
	  
	  if(!fldsentout) {
	    marc_put_field(pp,tag,-1);
	    fldsentout++;
	  }
	}
	else if (l1>l2) {
	  cm_error(CM_NO_ERROR,"Eliminating %d #%02d: %*.*s",
		   tag,l1+1,src1len,src1len,(char *)tsrc1);
	  cm_error(CM_CONTINUE,"because of  %d #%02d: %*.*s",
		   tag,l2+1,src2len,src2len,(char *)tsrc2);
	  break;
	}
      }

      /*=====================================================================*/
      /* If $a & @2 match, but $2 does not (where at least one has a $2)     */
      /* Take all $a's but log message
      /*=====================================================================*/
      else {
	if((strcmp(a2_1,a2_2)==0) &&
	   (strcmp(a22_1,a22_2)!=0) &&
	   ((marc_get_item(pp->inmp,tag,l1,'2',0,&tsrc1,&src1len)==0) ||
	    (marc_get_item(pp->inmp,tag,l2,'2',0,&tsrc2,&src2len)==0))) {
	  
	  cm_error(CM_WARNING,"Check 655's");
	  
	  if(marc_get_field(pp->inmp,tag,l1,&tsrc1,&src1len)!=0)
	    cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l1);
	  else
	    cm_error(CM_CONTINUE,"%d='%*.*s'",tag,src1len,src1len,(char *)tsrc1);

	  if(marc_get_item(pp->inmp,995,0,'a',0,&tsrc1,&src1len)!=0)
	    cm_error(CM_CONTINUE,"Unable to retrieve 995$a");
	  else 
	    cm_error(CM_CONTINUE,"995$a='%*.*s'",
		     src1len,src1len,(char *)tsrc1);
	  
	  if(marc_get_item(pp->inmp,999,0,'a',0,&tsrc1,&src1len)!=0)
	    cm_error(CM_CONTINUE,"Unable to retrieve 999$a");
	  else
	    cm_error(CM_CONTINUE,"999$a='%*.*s'",
		     src1len,src1len,(char *)tsrc1);
	  
	  if(!fldsentout) {
	    marc_put_field(pp,tag,-1);
	    fldsentout++;
	  }
	}

	/*===================================================================*/
	/* If they're different, let's see if the difference is only in $8   */
	/* where one of the $8's ends in prime...                            */
	/*===================================================================*/
	else {
	  tmpp=src1+strlen(src1)-5;
	  /*=================================================================*/
	  /* If first one ends in 'prime' keep it...                         */
	  /*=================================================================*/
	  if((strlen(src1)>5)&&
	     (strlen(src1)==strlen(src2)+5)&&
	     (strcmp(tmpp,"prime")==0)&&
	     (strncmp(src1,src2,strlen(src2))==0)) {
	    
	    if(marc_get_field(pp->inmp,tag,l1,&tsrc1,&src1len)!=0) 
	      cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,l1);
	    
	    if(!fldsentout) {
	      marc_put_field(pp,tag,-1);
	      fldsentout++;
	    }
	    
	  }
	  else {
	    tmpp=src2+strlen(src2)-5;
	    /*===============================================================*/
	    /* If second one ends in 'prime' discard the current one...      */
	    /*===============================================================*/
	    if((strlen(src2)>5)&&
	       (strlen(src2)==strlen(src1)+5)&&
	       (strcmp(tmpp,"prime")==0)&&
	       (strncmp(src2,src1,strlen(src1))==0)) {
	      
	      cm_error(CM_NO_ERROR,"Eliminating %d #%02d: %*.*s",
		       tag,l1+1,src1len,src1len,(char *)tsrc1);
	      cm_error(CM_CONTINUE,"because of  %d #%02d: %*.*s",
		       tag,l2+1,src2len,src2len,(char *)tsrc2);
	    }
	  }
	}
      }
    }
    
    if(src1)
      free(src1);
    
    if(src2)
      free(src2);
  }
  
  return CM_STAT_OK;
  
} /* cmp_dedupe_655_9 */



/************************************************************************
*  cmp_reorder_655_9()                                                  *
*                                                                       *
*   DEFINITION                                                          *
*       creates new occurrence of tag with contents of srcp...          *
*                                                                       *
*   PARAMETERS                                                          *
*       CM_PROC_PARMS parameter structure                               *
*       tag           tag id                                            *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
CM_STAT cmp_reorder_655_9 (
    CM_PROC_PARMS *pp              /* Pointer to parameter struct     */
) {
  
  unsigned char *tsrcp,        /* Ptr to input data                   */
                 indic1,        /* Ptr to input data                   */
                 indic2;        /* Ptr to input data                   */
  char          *srtind1,        /* Ptr to output data                  */
                *srtind2,        /* Ptr to output data                  */
                *srtsf8,        /* Ptr to output data                  */
                *srtsf2e,        /* Ptr to output data                  */
                *srtsfa;        /* Ptr to output data                  */
  size_t         src1len,      /* Length of source field              */
                 src2len,      /* Length of source field              */
                 srt8len,      /* Length of source field              */
                 srt2len,      /* Length of source field              */
                 srtalen;      /* Length of source field              */
  int            occ,          /* Field occurrence counter            */
                tag;      /* Subfield code                       */

  cmp_buf_find(pp, "bibid", &uip, &ui_len);

  /*=========================================================================*/
  /* Determine which field we're to process...                               */
  /*=========================================================================*/
  cmp_buf_find (pp, pp->args[0], &tsrcp, &src1len);
  if (!get_fixed_num ((char *) tsrcp, src1len, &tag)) {
    cm_error(CM_FATAL,"Unable to determine numeric from %*.*s",
	     src1len,src1len,(char *)tsrcp);
  }

  sort_cnt =  0;

  /*=========================================================================*/
  /* For each occurrence of specified tag...                                 */
  /*=========================================================================*/
  for(occ=0;marc_get_field(pp->inmp,tag,occ,&tsrcp,&src1len)==0;occ++) {
    
    srtind1=srtind2=srtsf8=srtsf2e=srtsfa=NULL;
    
    /*=======================================================================*/
    /* tags sort on @1, @2, $8, $2, $a (not necessarily in that order)...    */
    /*=======================================================================*/
    if(marc_get_indic(pp->inmp,1,&indic1)==0)
      srtind1 = (char *) &indic1;
    
    if(marc_get_indic(pp->inmp,2,&indic2)==0)
      srtind2 = (char *) &indic2;
    
    if(marc_get_item(pp->inmp,tag,occ,'8',0,&tsrcp,&srt8len)==0)
      srtsf8 = (char *) tsrcp;
    
    if(marc_get_item(pp->inmp,tag,occ,'2',0,&tsrcp,&srt2len)==0)
      srtsf2e = (char *) tsrcp;
    
    if(marc_get_item(pp->inmp,tag,occ,'a',0,&tsrcp,&srtalen)==0)
      srtsfa = (char *) tsrcp;
    
    if((*srtind1!=' ') ||
       ((*srtind2=='2') && (srt2len>0)) ||
       (srtsf2e && ((*srtind2=='7') && (strncmp(srtsf2e,"mesh",4)==0) && (srt2len==4)))) {
      cm_error(CM_WARNING,"Check %d indicators", tag);

      if(marc_get_field(pp->inmp,tag,occ,&tsrcp,&src1len)!=0)
	cm_error(CM_FATAL,"Unable to retrieve %d number %d",tag,occ);
      else
	cm_error(CM_CONTINUE,"%d(%d)='%*.*s'",tag,occ+1,src1len,src1len,(char *)tsrcp);

      if(marc_get_item(pp->inmp,995,0,'a',0,&tsrcp,&src1len)!=0)
	cm_error(CM_CONTINUE,"Unable to retrieve 995$a");
      else 
	cm_error(CM_CONTINUE,"995$a='%*.*s'",
		 src1len,src1len,(char *)tsrcp);
      
      if(marc_get_item(pp->inmp,999,0,'a',0,&tsrcp,&src1len)!=0)
	cm_error(CM_CONTINUE,"Unable to retrieve 999$a");
      else
	cm_error(CM_CONTINUE,"999$a='%*.*s'",
		 src1len,src1len,(char *)tsrcp);
    }

    /*=======================================================================*/
    /* Place it into the array in sorted order...                            */
    /*=======================================================================*/
    add_to_655_array(pp, tag, occ,
		      srtind1, 1,
		      srtind2, 1,
		      srtsf8, srt8len,
		      srtsf2e, srt2len,
		      srtsfa, srtalen);
  }
  
  /*=========================================================================*/
  /* Now go back through and write them in sorted order...                   */
  /*=========================================================================*/
  for(occ=0;occ<sort_cnt;occ++) {

    /*=======================================================================*/
    /* Retrieve specified occurrence of input field...                       */
    /*=======================================================================*/
    if(marc_get_field(pp->inmp,tag,sort_655_array[occ].occ,&tsrcp,&src1len)!=0) {
      cm_error(CM_FATAL,"Error retrieving %d #%d. Record not processed.",
	       tag,sort_655_array[occ].occ+1);
      return CM_STAT_KILL_RECORD;
    }

    /*=======================================================================*/
    /* Create new occurrence of output field...                              */
    /*=======================================================================*/
    marc_put_field(pp,tag,-1);

    /*=======================================================================*/
    /* Release memory for sort fields...                                     */
    /*=======================================================================*/
    if(sort_655_array[occ].srtind1)
      free(sort_655_array[occ].srtind1);

    if(sort_655_array[occ].srtind2)
      free(sort_655_array[occ].srtind2);

    if(sort_655_array[occ].srtsf8)
      free(sort_655_array[occ].srtsf8);

    if(sort_655_array[occ].srtsf2e)
      free(sort_655_array[occ].srtsf2e);

    if(sort_655_array[occ].srtsfa)
      free(sort_655_array[occ].srtsfa);

  }

  return CM_STAT_OK;
  
} /* cmp_reorder_655_9 */


/************************************************************************
* add_to_655_array()                                                    *
*                                                                       *
*   DEFINITION                                                          *
*                                                                       *
*   CONFIGURATION FILE PARAMETERS                                       *
*       None.                                                           *
*                                                                       *
*   RETURN                                                              *
*       0 if successful                                                 *
*   non-0 if problem encountered                                        *
************************************************************************/
void add_to_655_array(CM_PROC_PARMS *pp,
		       int   tag, int socc,
		       char *ind1, size_t len1, 
		       char *ind2, size_t len2,
		       char *sf8, size_t lensf8,
		       char *sf2, size_t lensf2,
		       char *sfa, size_t lensfa)
{
  unsigned char *tsrcp;
  char *tmp1,
       *tmp2,
       *tmp3,
       *tmp4,
       *tmp5,
       *tmpp;
  size_t tsrclen;
  int   action,
        occ,
        occ2;

  if(strncmp((char *)uip,"9111489",7)==0)
    uip=uip;

  tmp1=tmp2=tmp3=tmp4=tmp5=NULL;

  /*=========================================================================*/
  /* Allocate and copy each sort field over to a separate area of memory...  */
  /*=========================================================================*/
  if(ind1) {
    if(allocate_n_ncopy(&tmp1,len1,1,ind1,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp1;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }
  
  if(ind2) {
    if(allocate_n_ncopy(&tmp2,len2,1,ind2,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp2;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);  
  }

  if(sf8) {
    if(allocate_n_ncopy(&tmp3,lensf8,1,sf8,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp3;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  if(sf2) {
    if(allocate_n_ncopy(&tmp4,lensf2,1,sf2,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp4;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  if(sfa) {
    if(allocate_n_ncopy(&tmp5,lensfa,1,sfa,NO_COLLAPSE_SPACES,"6XX")!=0)
      cm_error(CM_FATAL,"Error allocating memory in sort 6xx");

    for(tmpp=tmp5;*tmpp;tmpp++)
      *tmpp=tolower(*tmpp);
  }

  /*=========================================================================*/
  /* Then figure out where it belongs in the sort array...                   */
  /*=========================================================================*/
  for(occ=0;(occ<sort_cnt)&&(occ<MAX_6XX_FIELDS);occ++) {

    /*=======================================================================*/
    /* @1's are to be ordered as follows:                                    */
    /*   1) ' ' first                                                        */
    /*   2) any others at end                                                */
    /*=======================================================================*/
    /*=======================================================================*/
    /* incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp1) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtind1) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtind1) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* ' ' comes first...                                                */
	/*===================================================================*/
	if(strcmp(tmp1," ")==0) {
	  /*=================================================================*/
	  /* If array starts with ' ', check next level...                   */
	  /*=================================================================*/
	  if(strcmp(sort_655_array[occ].srtind1," ")==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* If array starts with something else, insert here...             */
	  /*=================================================================*/
	  else {
	    action=INSERT_HERE;
	  }
	}
	/*===================================================================*/
	/* After blank, do normal sort routine...                            */
	/*===================================================================*/
	else {
	  /*=================================================================*/
	  /* Field PRECEDES array entry...                                   */
	  /*=================================================================*/
	  if(strcmp(tmp1,sort_655_array[occ].srtind1)<0) {
	    action=INSERT_HERE;
	  }
	  else {
	    /*===============================================================*/
	    /* Field EQUALS array entry...                                   */
	    /*===============================================================*/
	    if(strcmp(tmp1,sort_655_array[occ].srtind1)==0) {
	      action=NEXT_LEVEL;
	    }
	    /*===============================================================*/
	    /* Field FOLLOWS array entry...                                  */
	    /*===============================================================*/
	    else {
	      action=NEXT_ENTRY;
	    }
	  }
	}
      }
    }
    
    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    

    /*=======================================================================*/
    /* $8 goes in alphabetical order with missing values first...            */
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp3) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtsf8) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with first field present...                            */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtsf8) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present, do normal sort routine...          */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* First field PRECEDES array entry...                               */
	/*===================================================================*/
	if(strcmp(tmp3,sort_655_array[occ].srtsf8)<0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
	  /* First field EQUALS array entry...                               */
	  /*=================================================================*/
	  if(strcmp(tmp3,sort_655_array[occ].srtsf8)==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* First field FOLLOWS array entry...                              */
	  /*=================================================================*/
	  else {
	    action=NEXT_ENTRY;
	  }
	}
      }
    }

    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    
    /*=======================================================================*/
    /* @2's are in alphabetical order                                        */
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp2) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtind2) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtind2) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present, do normal sort routine...          */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* Field PRECEDES array entry...                                     */
	/*===================================================================*/
	if(strcmp(tmp2,sort_655_array[occ].srtind2)<0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
	  /* Field EQUALS array entry...                                     */
	  /*=================================================================*/
	  if(strcmp(tmp2,sort_655_array[occ].srtind2)==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* Field FOLLOWS array entry...                                    */
	  /*=================================================================*/
	  else {
	    action=NEXT_ENTRY;
	  }
	}
      }
    }
    
    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    

    /*=======================================================================*/
    /* $2's are to be ordered alphabetically...                              */
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp4) {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtsf2e) {
	action=NEXT_LEVEL;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	action=INSERT_HERE;
      }
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtsf2e) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present, do normal sort routine...          */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* Field PRECEDES array entry...                                     */
	/*===================================================================*/
	if(strcmp(tmp4,sort_655_array[occ].srtsf2e)<0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
	  /* Field EQUALS array entry...                                     */
	  /*=================================================================*/
	  if(strcmp(tmp4,sort_655_array[occ].srtsf2e)==0) {
	    action=NEXT_LEVEL;
	  }
	  /*=================================================================*/
	  /* Field FOLLOWS array entry...                                    */
	  /*=================================================================*/
	  else {
	    action=NEXT_ENTRY;
	  }
	}
      }
    }
    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
    

    /*=======================================================================*/
    /* Moving on the last level, $a's are to be ordered alphabetically       */
    /*=======================================================================*/

    /*=======================================================================*/
    /* Incoming field MISSING...                                             */
    /*=======================================================================*/
    if(!tmp5) {
      action=INSERT_HERE;
    }
    /*=======================================================================*/
    /* Incoming field present...                                             */
    /*=======================================================================*/
    else {
      /*=====================================================================*/
      /* Array starts with MISSING field...                                  */
      /*=====================================================================*/
      if(!sort_655_array[occ].srtsfa) {
	action=NEXT_ENTRY;
      }
      /*=====================================================================*/
      /* Array starts with field present...                                  */
      /*=====================================================================*/
      else {
	/*===================================================================*/
	/* Incoming field PRECEDES or EQUALS array entry...                  */
	/*===================================================================*/
	if(strcmp(tmp5,sort_655_array[occ].srtsfa)<=0) {
	  action=INSERT_HERE;
	}
	else {
	  /*=================================================================*/
          /* Incoming field FOLLOWS array entry...                           */
          /*=================================================================*/
	  action=NEXT_ENTRY;
	}
      }
    }

    /*=======================================================================*/
    /* Keep going?                                                           */
    /*=======================================================================*/
    if(action==INSERT_HERE)
      break;
    else
      if(action==NEXT_ENTRY)
	continue;
  }

    
  /*=========================================================================*/
  /* Make sure we don't have too many entries...                             */
  /*=========================================================================*/
  if(occ==MAX_6XX_FIELDS)
    cm_error(CM_FATAL,"EXCEEDED MAXIMUM NUMBER OF 6XX FIELDS FOR SORT.");
  else {
    /*=======================================================================*/
    /* Make room in the array for the new entry...                           */
    /*=======================================================================*/
    for(occ2=sort_cnt;occ2>occ;occ2--) {
      sort_655_array[occ2].occ=sort_655_array[occ2-1].occ;
      sort_655_array[occ2].srtind1=sort_655_array[occ2-1].srtind1;
      sort_655_array[occ2].srtind2=sort_655_array[occ2-1].srtind2;
      sort_655_array[occ2].srtsf8=sort_655_array[occ2-1].srtsf8;
      sort_655_array[occ2].srtsf2e=sort_655_array[occ2-1].srtsf2e;
      sort_655_array[occ2].srtsfa=sort_655_array[occ2-1].srtsfa;
    }

    /*=======================================================================*/
    /* Insert the new entry in the appropriate slot...                       */
    /*=======================================================================*/
    sort_655_array[occ].occ  =socc;
    sort_655_array[occ].srtind1=tmp1;
    sort_655_array[occ].srtind2=tmp2;
    sort_655_array[occ].srtsf8=tmp3;
    sort_655_array[occ].srtsf2e=tmp4;
    sort_655_array[occ].srtsfa=tmp5;
    sort_cnt++;
  }  
  
} /* add_to_655_array */


void set_modified(CM_PROC_PARMS *pp, char *modified) {
  unsigned char *tsrcp;
  size_t src_len;

  cmp_buf_find(pp, "modified", &tsrcp, &src_len);
  if(*tsrcp=='0')
    cmp_buf_write(pp,"modified",(unsigned char *)modified,1,0);
}

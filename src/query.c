/*****************************************************************************
 * AreaFix library for Husky (FTN Software Package) supplemental file
 *****************************************************************************
 * Copyright (C) 1998-2003
 *
 * val khokhlov (Fido: 2:550/180), Kiev, Ukraine
 *
 * Based on original HPT code by Max Chernogor 2:464/108
 *
 * This file is part of libareafix.
 *
 * libareafix is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libareafix; see the file COPYING.  If not, write to the Free
 * Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/
/* $Id$ */

/* libc */
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* compiler.h */
#include <huskylib/compiler.h>
#include <huskylib/huskylib.h>

#ifdef HAS_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAS_DOS_H
#include <dos.h>
#endif

/* smapi */

/* fidoconf */
#include <fidoconf/fidoconf.h>
#include <fidoconf/common.h>
#include <huskylib/xstr.h>
#include <fidoconf/areatree.h>
#include <fidoconf/afixcmd.h>
#include <fidoconf/arealist.h>

/* hpt */
/*
#include <global.h>
#include <toss.h>
*/

#define DLLEXPORT
#include <huskylib/huskyext.h>
#include <areafix.h>
#include <query.h>
#include <afglobal.h>
#include <callback.h>

static  time_t  tnow;
const   long    secInDay = 3600*24;
const char czFreqArea[] = "freq";
const char czIdleArea[] = "idle";
const char czKillArea[] = "kill";
const char czChangFlg_hpt[]   = "changed.qfl";
const char czChangFlg_htick[] = "filefix.qfl";
s_query_areas *queryAreasHead = NULL;

extern char       *af_versionStr;


int isValidConference(const char *s) {
    /*  according to FSC-0074 with lowercase symbols */
    /*  lowercase symbols only for internal use */
    while (*s) {
        if ( !(*s >= 33 && *s <= 126) ) return 0;
        s++;
    }
    return 1;
}

int checkRefuse(char *areaName)
{
    FILE *fp;
    char *line;

    if (af_robot->newAreaRefuseFile == NULL)
        return 0;

    fp = fopen(af_robot->newAreaRefuseFile, "r+b");
    if (fp == NULL) {
        w_log(LL_ERR, "Can't open newAreaRefuseFile \"%s\" : %d\n",
              af_robot->newAreaRefuseFile, strerror(errno));
        return 0;
    }
    while((line = readLine(fp)) != NULL)
    {
        line = trimLine(line);
        if (patimat(areaName, line)) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

void del_tok(char **ac, char *tok) {
    char *p, *q;

    q = fc_stristr(*ac,tok);
    if (q) {
	p = q+strlen(tok);
	while (*p && !isspace(*p)) p++;
	if (*p) memmove(q, p+1, strlen(p+1)+1); /*  begin or middle */
	else {
	    if (q > *ac) *(q-1)='\0'; /*  end */
	    else *q='\0'; /*  "-token" defaults */
	}
    }
}

char* makeAreaParam(s_link *creatingLink, char* c_area, char* msgbDir)
{
    char *msgbFileName=NULL, *acDef;
    char *msgbtype, *newAC=NULL, *desc, *quote_areaname;
    char *cp, *buff=NULL;                    /* temp. usage */

    msgbFileName = makeMsgbFileName(af_config, c_area);

    /*  translating name of the area to lowercase/uppercase */
    if (af_config->createAreasCase == eUpper) strUpper(c_area);
    else strLower(c_area);

    /*  translating filename of the area to lowercase/uppercase */
    if (af_config->areasFileNameCase == eUpper) strUpper(msgbFileName);
    else strLower(msgbFileName);

    acDef = creatingLink->autoAreaCreateDefaults;

    xscatprintf(&newAC, "%s%s", (acDef) ? " " : "", (acDef) ? acDef : "");

    msgbtype = fc_stristr(newAC, "-b ");

    if(!msgbDir)
        msgbDir=(creatingLink->msgBaseDir) ?
        creatingLink->msgBaseDir : af_config->msgBaseDir;

    quote_areaname = strchr(TRUE_COMMENT "\"", *c_area) ? "\"" : "";

    if (stricmp(msgbDir, "passthrough")!=0 && NULL==fc_stristr(newAC,"passthrough"))
    {
        /*  we have to find a file name */
        int need_dos_file;

#ifndef MSDOS
        need_dos_file = fc_stristr(newAC, "-dosfile")!=NULL;
#else
        need_dos_file = 1;
#endif
        if (creatingLink->autoAreaCreateSubdirs && !need_dos_file)
        {
             /* "subdirify" the message base path if the */
             /* user wants this. this currently does not */
             /* work with the -dosfile option */
            for (cp = msgbFileName; *cp; cp++)
            {
                if (*cp == '.')
                {
                    *cp = PATH_DELIM;
                }
            }
        }
        if (!need_dos_file)
            xscatprintf(&buff, "EchoArea %s%s%s %s%s",
            quote_areaname, c_area, quote_areaname,
            msgbDir, msgbFileName);
        else {
            sleep(1); /*  to prevent time from creating equal numbers */
            xscatprintf(&buff,"EchoArea %s%s%s %s%8lx",
                quote_areaname, c_area, quote_areaname,
                msgbDir, (long)time(NULL));
        }

    } else {
        /*  passthrough */
        xscatprintf(&buff, "EchoArea %s%s%s passthrough",
	    quote_areaname, c_area, quote_areaname);

        del_tok(&newAC, "passthrough");
        del_tok(&newAC, "-b ");  /*  del "-b msgbtype" from autocreate defaults */
        del_tok(&newAC, "-$m "); /*  del "-$m xxx" from autocreate defaults */
        del_tok(&newAC, "-p ");  /*  del "-p xxx" from autocreate defaults */

        del_tok(&newAC, "-killsb");
        del_tok(&newAC, "-nokillsb");
        del_tok(&newAC, "-tinysb");
        del_tok(&newAC, "-notinysb");
        del_tok(&newAC, "-pack");
        del_tok(&newAC, "-nopack");
        del_tok(&newAC, "-link");
        del_tok(&newAC, "-nolink");
        del_tok(&newAC, "-killread");
        del_tok(&newAC, "-nokillread");
        del_tok(&newAC, "-keepunread");
        del_tok(&newAC, "-nokeepunread");
    }

    nfree(msgbFileName);
    if (creatingLink->LinkGrp) {
        if (fc_stristr(newAC, " -g ")==NULL)
            xscatprintf(&buff, " -g %s", creatingLink->LinkGrp);
    }

    if (IsAreaAvailable(c_area,creatingLink->forwardRequestFile,&desc,1)==1) {
        if (desc) {
            if (fc_stristr(newAC, " -d ")==NULL)
                xscatprintf(&buff, " -d \"%s\"", desc);
            nfree(desc);
        }
    }
    if (*newAC) xstrcat(&buff, newAC);
    nfree(newAC);
    return buff;
}

e_BadmailReasons autoCreate(char *c_area, char *descr, hs_addr pktOrigAddr, ps_addr forwardAddr)
{
    FILE *f;
    char *fileName, *fileechoFileName;
    char *buff=NULL, *hisaddr=NULL;
    char *msgbDir=NULL, *bDir=NULL;
    s_link *creatingLink;
    s_area *area;
    s_query_areas* areaNode=NULL;
    size_t i=0;
    unsigned int j;
    char pass[] = "passthrough";
    char CR;

    w_log( LL_FUNC, "%s::autoCreate() begin", __FILE__ );

    if (af_app->module == M_HPT) {
      if (strlen(c_area)>60){
         w_log( LL_FUNC, "%s::autoCreate() rc=11", __FILE__ );
         return BM_AREATAG_TOO_LONG;
      }
      if (!isValidConference(c_area) || isPatternLine(c_area)){
         w_log( LL_FUNC, "%s::autoCreate() rc=7", __FILE__ );
         return BM_ILLEGAL_CHARS;
      }
    }

    if (checkRefuse(c_area))
    {
        w_log(LL_WARN, "Can't create %sarea %s : refused by New%sAreaRefuseFile\n", af_app->module == M_HTICK ? "file" : "", c_area, af_app->module == M_HTICK ? "File" : "");
        return BM_DENY_NEWAREAREFUSEFILE;
    }

    creatingLink = getLinkFromAddr(af_config, pktOrigAddr);

    if (creatingLink == NULL) {
	w_log(LL_ERR, "creatingLink == NULL !!!");
        w_log( LL_FUNC, "%s::autoCreate() rc=8", __FILE__ );
	return BM_SENDER_NOT_FOUND;
    }

    if (af_app->module == M_HTICK) {
      fileName = creatingLink->autoFileCreateFile ? creatingLink->autoFileCreateFile : (af_cfgFile ? af_cfgFile : getConfigFileName());
    } else {
      fileName = creatingLink->autoAreaCreateFile;
      if (fileName == NULL) fileName = af_cfgFile ? af_cfgFile : getConfigFileName();
    }

    f = fopen(fileName, "a+b");
    if (f == NULL) {
	fprintf(stderr,"autocreate: cannot open af_config file\n");
        w_log( LL_FUNC, "%s::autoCreate() rc=9", __FILE__ );
	return BM_CANT_OPEN_CONFIG;
    }
    /*  setting up msgbase dir */
    if (af_config->createFwdNonPass == 0 && forwardAddr)
        msgbDir = pass;
    else
        msgbDir = creatingLink->msgBaseDir;

    if (af_robot->queueFile)
    {
        areaNode = af_CheckAreaInQuery(c_area, &pktOrigAddr, NULL, FIND);
        if( areaNode ) /*  if area in query */
        {
            if( stricmp(areaNode->type,czKillArea) == 0 ){
                w_log( LL_FUNC, "%s::autoCreate() rc=4", __FILE__ );
                return BM_AREA_KILLED;  /*  area already unsubscribed */
            }
            if( stricmp(areaNode->type,czFreqArea) == 0 &&
                addrComp(pktOrigAddr, areaNode->downlinks[0])!=0)
            {
                w_log( LL_FUNC, "%s::autoCreate() rc=4", __FILE__ );
                return BM_WRONG_LINK_TO_AUTOCREATE;  /*  wrong link to autocreate from */
            }
            if( stricmp(areaNode->type,czFreqArea) == 0 )
            {
                /*  removinq area from query. it is autocreated now */
                queryAreasHead->nFlag = 1; /*  query was changed */
                areaNode->type[0] = '\0';  /*  mark as deleted */
            }
            if (af_config->createFwdNonPass == 0)
               msgbDir = pass;
            /*  try to find our aka in links of queried area */
            /*  if not foun area will be passthrough */
            for (i = 1; i < areaNode->linksCount; i++)
                for(j = 0; j < af_config->addrCount; j++)
                    if (addrComp(areaNode->downlinks[i],af_config->addr[j])==0)
                    {
                        bDir = (creatingLink->fileBaseDir) ? creatingLink->fileBaseDir : af_config->fileAreaBaseDir;
                        msgbDir = creatingLink->msgBaseDir; 
                        break;
                    }
        }
    }

    /*  making address of uplink */
    xstrcat(&hisaddr, 
            aka2str(af_app->module == M_HTICK ? creatingLink->hisAka : pktOrigAddr)
           );

    /* HPT stuff */
    if (af_app->module == M_HPT) {
      buff = makeAreaParam(creatingLink, c_area, msgbDir);
    }
    /* HTICK stuff */
    else if (af_app->module == M_HTICK) {
      char *NewAutoCreate = NULL;

      fileechoFileName = makeMsgbFileName(af_config, c_area);
      /*  translating name of the area to lowercase/uppercase */
      if (af_config->createAreasCase == eUpper) strUpper(c_area);
      else strLower(c_area);
      /*  translating filename of the area to lowercase/uppercase */
      if (af_config->areasFileNameCase == eUpper) strUpper(fileechoFileName);
      else strLower(fileechoFileName);

      if (bDir==NULL)
          bDir = (creatingLink->fileBaseDir) ? creatingLink->fileBaseDir : af_config->fileAreaBaseDir;

      if( strcasecmp(bDir,"passthrough") )
      {
          if (creatingLink->autoFileCreateSubdirs)
          {
              char *cp;
              for (cp = fileechoFileName; *cp; cp++)
              {
                  if (*cp == '.')
                  {
                      *cp = PATH_DELIM;
                  }
              }
          }
          xscatprintf(&buff,"%s%s",bDir,fileechoFileName);
          if (_createDirectoryTree(buff))
          {
              w_log(LL_ERROR, "cannot make all subdirectories for %s\n",
                  fileechoFileName);
              nfree(buff);
              w_log( LL_FUNC, "%s::autoCreate() rc=1", __FILE__ );
              return 1;
          }
#if defined (__UNIX__)
          if(af_config->fileAreaCreatePerms && chmod(buff, af_config->fileAreaCreatePerms))
              w_log(LL_ERR, "Cannot chmod() for newly created filearea directory '%s': %s",
              sstr(buff), strerror(errno));
#endif
          nfree(buff);
      }


      fileName = creatingLink->autoFileCreateFile ? creatingLink->autoFileCreateFile : (af_cfgFile ? af_cfgFile : getConfigFileName());

      /* write new line in config file */

      xscatprintf(&buff, "FileArea %s %s%s -a %s ",
          c_area, bDir,
          (strcasecmp(bDir,"passthrough") == 0) ? "" : fileechoFileName,
          aka2str(*(creatingLink->ourAka))
          );

      if ( creatingLink->LinkGrp &&
          !( creatingLink->autoFileCreateDefaults && fc_stristr(creatingLink->autoFileCreateDefaults, "-g ") )
          )
      {
          xscatprintf(&buff,"-g %s ",creatingLink->LinkGrp);
      }

      if (creatingLink->forwardFileRequestFile!=NULL && descr==NULL)
          /* try to find description in forwardFileRequestFile */
          IsAreaAvailable(c_area,creatingLink->forwardFileRequestFile,&descr,1);

      if (creatingLink->autoFileCreateDefaults) {
          NewAutoCreate = sstrdup(creatingLink->autoFileCreateDefaults);
          if ((fileName=strstr(NewAutoCreate,"-d ")) !=NULL ) {
              if (descr) {
                  *fileName = '\0';
                  xscatprintf(&buff,"%s -d \"%s\"",NewAutoCreate,descr);
              } else {
                  xstrcat(&buff, NewAutoCreate);
              }
          } else {
              if (descr)
                  xscatprintf(&buff,"%s -d \"%s\"",NewAutoCreate,descr);
              else
                  xstrcat(&buff, NewAutoCreate);
          }
          nfree(NewAutoCreate);
      }
      else if (descr)
      {
          xscatprintf(&buff,"-d \"%s\"",descr);
      }
    } /* end of HTICK stuff */

    /* add newly created echo to af_config in memory */
    parseLine(buff, af_config);
    if (af_app->module == M_HTICK) RebuildFileAreaTree(af_config);
    else RebuildEchoAreaTree(af_config);

    /*  subscribe uplink if he is not subscribed */
    area = (af_app->module == M_HTICK) 
           ? &(af_config->fileAreas[af_config->fileAreaCount-1])
           : &(af_config->echoAreas[af_config->echoAreaCount-1]);
    if ( !isLinkOfArea(creatingLink,area) ) {
	xscatprintf(&buff, " %s", hisaddr);
	Addlink(af_config, creatingLink, area);
        if (af_config->createAddUplink) {
          xstrcat(&buff, " -def");
          if (area) area->downlinks[area->downlinkCount-1]->defLink = 1;
        }
    }

    /*  subscribe downlinks if present */
    if(areaNode) { /*  areaNode == NULL if areafixQueueFile isn't used */
        /*  prevent subscribing of defuault links */
        /*  or not existing links */
        for(i = 1; i < areaNode->linksCount; i++) {
            if( ( isAreaLink( areaNode->downlinks[i],area ) == -1 ) &&
                ( getLinkFromAddr(af_config,areaNode->downlinks[i])) &&
                ( !isOurAka(af_config,areaNode->downlinks[i]) )
            ) {
              xstrcat( &buff, " " );
              xstrcat( &buff, aka2str(areaNode->downlinks[i]) );
              Addlink(af_config, getLinkFromAddr(af_config,areaNode->downlinks[i]), area);
            }
        }
    }

    /*  fix if dummys del \n from the end of file */
    if( fseek (f, -2L, SEEK_END) == 0)
    {
        CR = getc (f); /*   may be it is CR aka '\r'  */
        if (getc(f) != '\n') {
            fseek (f, 0L, SEEK_END);  /*  not neccesary, but looks better ;) */
            fputs (cfgEol(), f);
        } else {
            fseek (f, 0L, SEEK_END);
        }
        i = ftell(f); /* af_config length */
        /* correct EOL in memory */
        if(CR == '\r')
            xstrcat(&buff,"\r\n"); /* DOS EOL */
        else
            xstrcat(&buff,"\n");   /* UNIX EOL */
    }
    else
    {
        xstrcat(&buff,cfgEol());   /* af_config depended EOL */
    }

    /*  add line to af_config */
    if ( fprintf(f, "%s", buff) != (int)(strlen(buff)) || fflush(f) != 0)
    {
        w_log(LL_ERR, "Error creating area %s, config write failed: %s!",
            c_area, strerror(errno));
        fseek(f, i, SEEK_SET);
        setfsize(fileno(f), i);
    }
    fclose(f);
    nfree(buff);

    /*  echoarea addresses changed by safe_reallocating of af_config->echoAreas[] */
    if (af_app->module == M_HPT) carbonNames2Addr(af_config);

    w_log(LL_AUTOCREATE, "%sArea %s autocreated by %s", (af_app->module == M_HTICK ? "File" : ""), c_area, hisaddr);

    if (hook_onAutoCreate) (*hook_onAutoCreate)(c_area, descr, pktOrigAddr, forwardAddr);

    if (af_app->module == M_HPT) {
      /* check if downlinks are already paused, pause area if it is so */
      /* skip if forwardAddr is NULL: will be checked in subscribe() */
      if (af_config->autoAreaPause && area->msgbType == MSGTYPE_PASSTHROUGH && forwardAddr == NULL)
        if (pauseArea(ACT_PAUSE, NULL, area)) sendAreafixMessages();
    }

    nfree(hisaddr);

    /* val: update perl structures */
    if (hook_onConfigChange) (*hook_onConfigChange)(PERL_CONF_AREAS);

    /*  create flag */
    if (af_robot->autoCreateFlag) {
	if (NULL == (f = fopen(af_robot->autoCreateFlag, "a")))
	    w_log(LL_ERR, "Could not open autoAreaCreate flag: %s", af_robot->autoCreateFlag);
	else {
	    w_log(LL_FLAG, "Created autoAreaCreate flag: %s", af_robot->autoCreateFlag);
	    fclose(f);
	}
    }

    w_log( LL_FUNC, "%s::autoCreate() end", __FILE__ );
    return 0;
}


s_query_areas*  af_AddAreaListNode(char *areatag, const char *type);
void            af_DelAreaListNode(s_query_areas* node);
void            af_AddLink(s_query_areas* node, ps_addr link);

s_query_areas* af_CheckAreaInQuery(char *areatag, ps_addr uplink, ps_addr dwlink, e_query_action act)
{
    size_t i = 0;
    int bFind = 0;
    s_query_areas *areaNode = NULL;
    s_query_areas *tmpNode  = NULL;

    if( !queryAreasHead ) af_OpenQuery();
    tmpNode = queryAreasHead;
    while(tmpNode->next && !bFind)
    {
        if( tmpNode->next->name && !stricmp(areatag, tmpNode->next->name) )
            bFind = 1;
        tmpNode = tmpNode->next;
    }

    switch( act )
    {
    case FIND:
        if( !bFind || tmpNode == queryAreasHead )
            tmpNode = NULL;
        break;
    case FINDFREQ:
        if( !bFind || tmpNode == queryAreasHead || stricmp(tmpNode->type,czKillArea) == 0 )
            tmpNode = NULL;
        break;
    case ADDFREQ:
        if( bFind ) {
            if( stricmp(tmpNode->type,czKillArea) == 0 && 
                uplink && addrComp(tmpNode->downlinks[0],*uplink) != 0 )
            {
                memcpy( &(tmpNode->downlinks[0]), uplink, sizeof(hs_addr) );
            }
            if( stricmp(tmpNode->type,czFreqArea) == 0 )
            {
                i = 1;
                while( i < tmpNode->linksCount && addrComp(*dwlink, tmpNode->downlinks[i])!=0)
                    i++;
                if(i == tmpNode->linksCount) {
                    af_AddLink( tmpNode, dwlink ); /*  add link to queried area */
                    tmpNode->eTime = tnow + af_config->forwardRequestTimeout*secInDay;
                } else {
                    tmpNode = NULL;  /*  link already in query */
                }
            } else {
                strcpy(tmpNode->type,czFreqArea); /*  change state to @freq" */
                af_AddLink( tmpNode, dwlink );
                tmpNode->eTime = tnow + af_config->forwardRequestTimeout*secInDay;
            }
        } else { /*  area not found, so add it */
            areaNode = af_AddAreaListNode( areatag, czFreqArea );
            if(strlen( areatag ) > queryAreasHead->linksCount) /* max areanane lenght */
                queryAreasHead->linksCount = strlen( areatag );
            af_AddLink( areaNode, uplink );
            af_AddLink( areaNode, dwlink );
            areaNode->eTime = tnow + af_config->forwardRequestTimeout*secInDay;
            tmpNode =areaNode;
        }
        break;
    case ADDIDLE:
        if( bFind ) {
        } else {
            areaNode = af_AddAreaListNode( areatag, czIdleArea );
            if(strlen( areatag ) > queryAreasHead->linksCount)
                queryAreasHead->linksCount = strlen( areatag );
            af_AddLink( areaNode, uplink );
            areaNode->eTime = tnow + af_config->idlePassthruTimeout*secInDay;
            w_log(LL_AREAFIX, "%s: make request idle for area: %s", af_robot->name, areaNode->name);
            tmpNode =areaNode;
        }
        break;
    case DELIDLE:
        if( bFind && stricmp(tmpNode->type,czIdleArea) == 0 )
        {
            queryAreasHead->nFlag = 1;
            tmpNode->type[0] = '\0';
            w_log( LL_AREAFIX, "%s: idle request for %s removed from queue file", af_robot->name, tmpNode->name);
        }
        break;

    }
    return tmpNode;
}

char* af_Req2Idle(char *areatag, char* report, hs_addr linkAddr)
{
    size_t i;
    s_query_areas *tmpNode  = NULL;
    s_query_areas *areaNode  = NULL;
    if( !queryAreasHead ) af_OpenQuery();
    tmpNode = queryAreasHead;
    while(tmpNode->next)
    {
        areaNode = tmpNode->next;
        if( ( areaNode->name ) &&
            ( stricmp(areaNode->type,czFreqArea) == 0 ) &&
            ( patimat(areaNode->name,areatag)==1) )
        {
            i = 1;
            while( i < areaNode->linksCount)
            {
                  if( addrComp(areaNode->downlinks[i],linkAddr) == 0)
                      break;
                  i++;
            }
            if( i < areaNode->linksCount )
            {
                if( i != areaNode->linksCount-1 )
                    memmove(&(areaNode->downlinks[i]),&(areaNode->downlinks[i+1]),
                    sizeof(hs_addr)*(areaNode->linksCount-i));
                areaNode->linksCount--;
                queryAreasHead->nFlag = 1; /*  query was changed */
                if(areaNode->linksCount == 1)
                {
                    strcpy(areaNode->type,czIdleArea);
                    areaNode->bTime = tnow;
                    areaNode->eTime = tnow + af_config->idlePassthruTimeout*secInDay;
                    w_log(LL_AREAFIX, "%s: make request idle for area: %s", af_robot->name, areaNode->name);
                }
                xscatprintf(&report, " %s %s  request canceled\r",
                    areaNode->name,
                    print_ch(49-strlen(areaNode->name), '.'));
                w_log(LL_AREAFIX, "%s: request canceled for [%s] area: %s", af_robot->name, aka2str(linkAddr),
                    areaNode->name);
            }
        }
        tmpNode = tmpNode->next;
    }
    return report;
}

char* af_GetQFlagName()
{
    char *chanagedflag = NULL;
    char *logdir       = NULL;
    const char *czChangFlg   = af_app->module == M_HTICK ? czChangFlg_htick : czChangFlg_hpt;

#ifdef DEBUG_HPT
w_log(LL_FUNC, "af_GetQFlagName(): begin");
#endif
    if (af_config->lockfile)
    {
       logdir = dirname(af_config->lockfile);  /* slash-trailed */
       xstrscat(&chanagedflag,logdir,(char*)czChangFlg,NULL);
       nfree(logdir);
    }
    else if (af_config->echotosslog)
    {
       logdir = dirname(af_config->echotosslog);  /* slash-trailed */
       xstrscat(&chanagedflag,logdir,(char*)czChangFlg,NULL);
       nfree(logdir);
    }
    else if (af_config->semaDir)
    {
       logdir = dirname(af_config->echotosslog);  /* slash-trailed */
       xstrscat(&chanagedflag,logdir,(char*)czChangFlg,NULL);
       nfree(logdir);
    }
    else
    {
        chanagedflag = (*call_sstrdup)(czChangFlg);
    }

    w_log(LL_FUNC, "af_GetQFlagName(): end");

    return chanagedflag;
}

void af_QueueReport()
{
    s_query_areas *tmpNode  = NULL;
    const char rmask[]="%-37.37s %-4.4s %11.11s %-16.16s %-7.7s\r";
    char type[5]="\0\0\0\0";
    char state[8]= "\0\0\0\0\0\0\0";
    char link1[17]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char link2[17]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char* report = NULL;
    char* header = NULL;
    int netmail=0;
    char *reportFlg = NULL;
    s_message *msg = NULL;

    w_log(LL_FUNC, "af_QueueReport(): begin");

    if( !af_robot->queueFile ){
      w_log(LL_WARN, "queueFile for %s not defined in af_config", af_robot->name);
      w_log(LL_FUNC, "af_QueueReport(): end");
      return;
    }

    w_log(LL_DEBUGU, __FILE__ ":%u:af_QueueReport()", __LINE__);

    reportFlg = af_GetQFlagName();

    w_log(LL_DEBUGU, __FILE__ ":%u:af_QueueReport()", __LINE__);

    if(!fexist(reportFlg))
    {
        w_log(LL_STOP, "Queue file hasn't been changed. Exiting...");
        nfree(reportFlg);
        return;
    }

    w_log(LL_DEBUGU, __FILE__":%u:af_QueueReport()", __LINE__);

    if( !queryAreasHead ) af_OpenQuery();

    tmpNode = queryAreasHead;

    w_log(LL_DEBUGU, __FILE__":%u:af_QueueReport() tmpNode=%X", __LINE__, tmpNode);

    while(tmpNode->next)
    {

    w_log(LL_DEBUGU, __FILE__":%u:af_QueueReport() tmpNode=%X", __LINE__, tmpNode);

        tmpNode = tmpNode->next;
        strcpy(link1,aka2str(tmpNode->downlinks[0]));
        strcpy(type,tmpNode->type);
        if( stricmp(tmpNode->type,czFreqArea) == 0 )
        {
            strcpy(link2,aka2str(tmpNode->downlinks[1]));
            if( strcmp(tmpNode->type,czFreqArea) == 0 )
            {
                queryAreasHead->nFlag = 1;
                strUpper(tmpNode->type);
                xscatprintf(&report,rmask, tmpNode->name, tmpNode->type,
                    link1,link2,
                    "request");
                continue;
            }
            if (af_report_changes) continue; /* report changes in queue file only */
            if(tmpNode->eTime < tnow )
            {
                strcpy(state,"rr_or_d");
            }
            else
            {
                int days = (tnow - tmpNode->bTime)/secInDay;
                sprintf(state,"%2d days",days);
            }
            xscatprintf(&report,rmask, tmpNode->name, type,
                link1,link2,
                state);
        }
        if( stricmp(tmpNode->type,czKillArea) == 0 )
        {
            if( strcmp(tmpNode->type,czKillArea) == 0 )
            {
                queryAreasHead->nFlag = 1;
                strUpper(tmpNode->type);
                xscatprintf(&report,rmask, tmpNode->name, tmpNode->type,
                    link1,"",
                    "timeout");
                continue;
            }
            if (af_report_changes) continue;
            if(tmpNode->eTime < tnow )
            {
                strcpy(state,"to_kill");
            }
            else
            {
                int days = (tnow - tmpNode->bTime)/secInDay;
                sprintf(state,"%2d days",days);
            }
            xscatprintf(&report,rmask, tmpNode->name, type,
                link1,"",
                state);

        }
        if( stricmp(tmpNode->type,czIdleArea) == 0 )
        {
            if( strcmp(tmpNode->type,czIdleArea) == 0 )
            {
                queryAreasHead->nFlag = 1;
                strUpper(tmpNode->type);
                xscatprintf(&report,rmask, tmpNode->name, tmpNode->type,
                    link1,"",
                    "timeout");
                continue;
            }
            if (af_report_changes) continue;
            if(tmpNode->eTime < tnow )
            {
                strcpy(state,"to_kill");
            }
            else
            {
                int days = (tnow - tmpNode->bTime)/secInDay;
                sprintf(state,"%2d days",days);
            }
            xscatprintf(&report,rmask, tmpNode->name, type,
                link1,"",
                state);
        }
    }
    if(!report) {
        remove(reportFlg);
        nfree(reportFlg);
        return;
    }

    w_log(LL_START, "Start generating queue report");
    xscatprintf(&header, rmask, 
                af_app->module == M_HTICK ? "FileArea" : "Area",
                "Act","From","By","Details");
    xscatprintf(&header, "%s\r", print_ch(79,'-'));
    xstrcat(&header, report);
    report = header;
    if (af_config->ReportTo) {
        if (stricmp(af_config->ReportTo,"netmail")==0)                netmail=1;
        else if (getNetMailArea(af_config, af_config->ReportTo) != NULL) netmail=1;
    } else netmail=1;

    msg = makeMessage(&(af_config->addr[0]),&(af_config->addr[0]),
                                af_robot->fromName ? af_robot->fromName : af_versionStr,
                                netmail ? (af_config->sysop ? af_config->sysop : "Sysop") : "All", "Requests report",
                                netmail,
                                af_robot->reportsAttr);
    msg->text = createKludges(af_config,
                                netmail ? NULL : af_config->ReportTo,
                                &(af_config->addr[0]), &(af_config->addr[0]),
                                af_versionStr);

    msg->recode |= (REC_HDR|REC_TXT);

    if (af_robot->reportsFlags)
	xstrscat( &(msg->text), "\001FLAGS ", af_robot->reportsFlags, "\r", NULL);
    xstrcat( &(msg->text), report );

    w_log(LL_STOP, "End generating queue report");

    (*call_writeMsgToSysop)(msg);
    nfree(msg);
    remove(reportFlg);
    nfree(reportFlg);
    w_log(LL_FUNC, "af_QueueReport(): end");
}

void af_QueueUpdate()
{
    s_query_areas *tmpNode  = NULL;
    s_link *lastRlink;
    s_link *dwlink;
    s_message **tmpmsg = NULL;
    size_t i = 0;
    int j = 0;

    tmpmsg = (s_message**) (*call_smalloc)( af_config->linkCount * sizeof(s_message*));
    for (i = 0; i < af_config->linkCount; i++)
    {
        tmpmsg[i] = NULL;
    }

    w_log(LL_START, "Start updating queue file");
    if( !queryAreasHead ) af_OpenQuery();

    tmpNode = queryAreasHead;
    while(tmpNode->next)
    {
        tmpNode = tmpNode->next;
        if( tmpNode->eTime > tnow )
            continue;
        if( stricmp(tmpNode->type,czFreqArea) == 0 )
        {
            lastRlink = getLinkFromAddr(af_config,tmpNode->downlinks[0]);
            dwlink    = getLinkFromAddr(af_config,tmpNode->downlinks[1]);
            forwardRequestToLink(tmpNode->name, lastRlink, NULL, 2);
            w_log( LL_AREAFIX, "%s: request for %s is canceled for node %s",
                   af_robot->name, tmpNode->name, aka2str(lastRlink->hisAka));
            if(dwlink && !forwardRequest(tmpNode->name, dwlink, &lastRlink))
            {
                tmpNode->downlinks[0] = lastRlink->hisAka;
                tmpNode->bTime = tnow;
                tmpNode->eTime = tnow + af_config->forwardRequestTimeout*secInDay;
                w_log( LL_AREAFIX, "%s: request for %s is going to node %s",
                       af_robot->name, tmpNode->name, aka2str(lastRlink->hisAka));
            }
            else
            {
                strcpy(tmpNode->type, czKillArea);
                tmpNode->bTime = tnow;
                tmpNode->eTime = tnow + af_config->killedRequestTimeout*secInDay;
                w_log( LL_AREAFIX, "%s: request for %s is going to be killed", af_robot->name, tmpNode->name);

                /* send notification messages */
                for (i = 1; i < tmpNode->linksCount; i++)
                {
                    dwlink = getLinkFromAddr(af_config,tmpNode->downlinks[i]);
                    for (j = 0; j < af_config->linkCount; j++)
                    {
                        if ( addrComp(dwlink->hisAka,af_config->links[j]->hisAka)==0 && dwlink->sendNotifyMessages)
                        {
                   	    if (tmpmsg[j] == NULL)
                   	    {
                                int ra = 0;
                                char *rf = NULL;
                                if (af_app->module == M_HTICK) {
                                  ra = dwlink->filefixReportsAttr ? dwlink->filefixReportsAttr : af_robot->reportsAttr;
                                  rf = dwlink->filefixReportsFlags ? dwlink->filefixReportsFlags : af_robot->reportsFlags;
                                } else {
                                  ra = dwlink->areafixReportsAttr ? dwlink->areafixReportsAttr : af_robot->reportsAttr;
                                  rf = dwlink->areafixReportsFlags ? dwlink->areafixReportsFlags : af_robot->reportsFlags;
                                }
                                tmpmsg[j] = makeMessage(dwlink->ourAka,
                                    &(dwlink->hisAka),
                                    af_robot->fromName ? af_robot->fromName : af_versionStr,
                                    dwlink->name,
                                    "Notification message", 1,
                                    ra);
                                tmpmsg[j]->text = createKludges(af_config, NULL,
                                    dwlink->ourAka,
                                    &(dwlink->hisAka),
                                    af_versionStr);
                                    if (rf)
                                        xstrscat(&(tmpmsg[j]->text), "\001FLAGS ", rf, "\r",NULL);

                                  xstrcat(&tmpmsg[j]->text, "\r Your requests for the following areas were forwarded to uplinks,\r");
                                xscatprintf(&tmpmsg[j]->text, " but no messages were received at least in %u days. Your requests\r",af_config->forwardRequestTimeout);
                                    xstrcat(&tmpmsg[j]->text, " are killed by timeout.\r\r");
                            }
                            xscatprintf(&tmpmsg[j]->text, " %s\r",tmpNode->name);
                        }
                    }
                }
                tmpNode->linksCount = 1;
            }
            queryAreasHead->nFlag = 1; /*  query was changed */
            continue;
        }
        if( stricmp(tmpNode->type,czKillArea) == 0 )
        {
            queryAreasHead->nFlag = 1;
            tmpNode->type[0] = '\0';
            w_log( LL_AREAFIX, "%s: request for %s removed from queue file", af_robot->name, tmpNode->name);
            continue;
        }
        if( stricmp(tmpNode->type,czIdleArea) == 0 )
        {
            ps_area delarea;
            queryAreasHead->nFlag = 1; /*  query was changed */
            strcpy(tmpNode->type, czKillArea);
            tmpNode->bTime = tnow;
            tmpNode->eTime = tnow + af_config->killedRequestTimeout*secInDay;
            tmpNode->linksCount = 1;
            w_log( LL_AREAFIX, "%s: request for %s is going to be killed", af_robot->name, tmpNode->name);
            dwlink = getLinkFromAddr(af_config, tmpNode->downlinks[0]);
            /* delete area from config, unsubscribe at downlinks */
            if (af_app->module == M_HTICK) {
              delarea = getFileArea(tmpNode->name);
              if (delarea != NULL) do_delete(dwlink, delarea);
            } else {
              delarea = getArea(af_config, tmpNode->name);
              if (delarea != &(af_config->badArea)) do_delete(dwlink, delarea);
            }
            /* unsubscribe at uplink */
            if (dwlink) forwardRequestToLink(tmpNode->name, dwlink, NULL, 2);
        }
    }
    /* send notification messages */
    for (i = 0; i < af_config->linkCount; i++)
    {
        if (tmpmsg[i])
        {
            xscatprintf(&tmpmsg[i]->text, "\r\r--- %s %s\r", af_versionStr, af_robot->name);
            tmpmsg[i]->textLength = strlen(tmpmsg[i]->text);
/*
            processNMMsg(tmpmsg[i], NULL,
                getNetMailArea(af_config,af_config->robotsArea),
                0, MSGLOCAL);
            closeOpenedPkt();
            freeMsgBuffers(tmpmsg[i]);
*/
            (*call_sendMsg)(tmpmsg[i]);
            w_log( LL_AREAFIX, "%s: write notification msg for %s", af_robot->name, aka2str(af_config->links[i]->hisAka));
        }
        nfree(tmpmsg[i]);
    }
    /*  send msg to the links (forward requests to areafix) */
    sendAreafixMessages();
    w_log(LL_STOP, "End updating queue file");
}

int af_OpenQuery()
{
    FILE *queryFile;
    char *line = NULL;
    char *token = NULL;
    struct  tm tr;
    char seps[]   = " \t\n";

    if( queryAreasHead )  /*  list already exists */
        return 0;

    time( &tnow );

    queryAreasHead = af_AddAreaListNode("\0","\0");

    if( !af_robot->queueFile ) /* Queue File not defined in af_config */
    {
        w_log(LL_ERR, "queueFile for %s not defined in af_config", af_robot->name);
        return 0;
    }
    if ( !(queryFile = fopen(af_robot->queueFile,"r")) ) /* can't open query file */
    {
       w_log(LL_ERR, "Can't open queueFile %s: %s", af_robot->queueFile, strerror(errno) );
       return 0;
    }

    while ((line = readLine(queryFile)) != NULL)
    {
        s_query_areas *areaNode = NULL;
        token = strtok( line, seps );
        if( token != NULL )
        {
            areaNode = af_AddAreaListNode(token, "");
            if(strlen( areaNode->name ) > queryAreasHead->linksCount)
                queryAreasHead->linksCount = strlen( areaNode->name );
            token = strtok( NULL, seps );
            strncpy( areaNode->type ,token, 4);
            token = strtok( NULL, seps );
            memset(&tr, '\0', sizeof(tr));
            if(sscanf(token, "%d-%d-%d@%d:%d",
                          &tr.tm_year,
                          &tr.tm_mon,
                          &tr.tm_mday,
                          &tr.tm_hour,
                          &tr.tm_min
                  ) != 5)
            {
                af_DelAreaListNode(areaNode);
                continue;
            } else {
                tr.tm_year -= 1900;
                tr.tm_mon--;
                tr.tm_isdst =- 1;
                areaNode->bTime = mktime(&tr);
            }
            token = strtok( NULL, seps );
            memset(&tr, '\0', sizeof(tr));
            if(sscanf(token, "%d-%d-%d@%d:%d",
                          &tr.tm_year,
                          &tr.tm_mon,
                          &tr.tm_mday,
                          &tr.tm_hour,
                          &tr.tm_min
                  ) != 5)
            {
                af_DelAreaListNode(areaNode);
                continue;
            } else {
                tr.tm_year -= 1900;
                tr.tm_mon--;
                tr.tm_isdst =- 1;
                areaNode->eTime = mktime(&tr);
            }

            token = strtok( NULL, seps );
            while( token != NULL )
            {

                areaNode->linksCount++;
                areaNode->downlinks = (*call_srealloc)( areaNode->downlinks, sizeof(hs_addr)*areaNode->linksCount );
                string2addr(token ,
                            &(areaNode->downlinks[areaNode->linksCount-1]));
                token = strtok( NULL, seps );
            }
        }
        nfree(line);
    }
    fclose(queryFile);
    return 0;
}

int af_CloseQuery()
{
    char buf[2*1024] = "";
    char *p;
    int nSpace = 0;
    size_t i = 0;
    struct  tm t1,t2;
    int writeChanges = 0;
    FILE *queryFile=NULL;
    s_query_areas *delNode = NULL;
    s_query_areas *tmpNode  = NULL;
    char *chanagedflag = NULL;
    FILE *QFlag        = NULL;

    w_log(LL_FUNC, __FILE__ ":%u:af_CloseQuery() begin", __LINE__);

    if( !queryAreasHead ) {  /*  list does not exist */
        w_log(LL_FUNC, __FILE__ ":%u:af_CloseQuery() end", __LINE__);
        return 0;
    }

    if(queryAreasHead->nFlag == 1) {
        writeChanges = 1;
    }
    if (writeChanges)
    {
        if ((queryFile = fopen(af_robot->queueFile, "w")) == NULL)
        {
            w_log(LL_ERR,"%s: queueFile not saved", af_robot->name);
            writeChanges = 0;
        }
        else
        {
            if( (chanagedflag = af_GetQFlagName()) )
            {
              if( (QFlag = fopen(chanagedflag,"w")) )
                fclose(QFlag);
              nfree(chanagedflag);
            }
        }
    }

    tmpNode = queryAreasHead->next;
    nSpace = queryAreasHead->linksCount+1;
    p = buf+nSpace;
    while(tmpNode) {
        if(writeChanges && tmpNode->type[0] != '\0')    {
            memset(buf, ' ' ,nSpace);
            memcpy(buf, tmpNode->name, strlen(tmpNode->name));
            t1 = *localtime( &tmpNode->bTime );
            t2 = *localtime( &tmpNode->eTime );
            sprintf( p , "%s %d-%02d-%02d@%02d:%02d\t%d-%02d-%02d@%02d:%02d" ,
                tmpNode->type,
                t1.tm_year + 1900,
                t1.tm_mon  + 1,
                t1.tm_mday,
                t1.tm_hour,
                t1.tm_min,
                t2.tm_year + 1900,
                t2.tm_mon  + 1,
                t2.tm_mday,
                t2.tm_hour,
                t2.tm_min   );
            p = p + strlen(p);
            for(i = 0; i < tmpNode->linksCount; i++) {
                strcat(p," ");
                strcat(p,aka2str(tmpNode->downlinks[i]));
            }
            strcat(buf, "\n");
            fputs( buf , queryFile );
            p = buf+nSpace;
        }
        delNode = tmpNode;
        tmpNode = tmpNode->next;
        af_DelAreaListNode(delNode);
    }

    nfree(queryAreasHead->name);
    nfree(queryAreasHead->downlinks);
    nfree(queryAreasHead->report);
    nfree(queryAreasHead);

    if(queryFile) fclose(queryFile);

    w_log(LL_FUNC, __FILE__ ":%u:af_CloseQuery() end", __LINE__);
    return 0;
}

s_query_areas* af_MakeAreaListNode()
{
    s_query_areas *areaNode =NULL;
    areaNode = (s_query_areas*)(*call_smalloc)( sizeof(s_query_areas) );
    memset( areaNode ,'\0', sizeof(s_query_areas) );
    return areaNode;
}

s_query_areas* af_AddAreaListNode(char *areatag, const char *type)
{
    s_query_areas *tmpNode      = NULL;
    s_query_areas *tmpPrevNode  = NULL;
    s_query_areas *newNode  = af_MakeAreaListNode();

    newNode->name = sstrlen(areatag) > 0 ? (*call_sstrdup)(areatag) : NULL;
    strcpy( newNode->type ,type);

    tmpPrevNode = tmpNode = queryAreasHead;

    while(tmpNode)
    {
        if( tmpNode->name && strlen(tmpNode->name) > 0 )
            if( stricmp(areatag,tmpNode->name) < 0 )
                break;
        tmpPrevNode = tmpNode;
        tmpNode = tmpNode->next;
    }
    if(tmpPrevNode)
    {
        tmpPrevNode->next = newNode;
        newNode->next     = tmpNode;
    }
    return newNode;
}

void af_DelAreaListNode(s_query_areas* node)
{
    s_query_areas* tmpNode = queryAreasHead;

    while(tmpNode->next && tmpNode->next != node)
    {
        tmpNode = tmpNode->next;
    }
    if(tmpNode->next)
    {
        tmpNode->next = node->next;
        nfree(node->name);
        nfree(node->downlinks);
        nfree(node->report);
        nfree(node);
    }
}

void af_AddLink(s_query_areas* node, ps_addr link)
{
    node->linksCount++;
    node->downlinks = (*call_srealloc)( node->downlinks, sizeof(hs_addr)*node->linksCount );
    memcpy( &(node->downlinks[node->linksCount-1]) ,link, sizeof(hs_addr) );
    node->bTime = tnow;
    queryAreasHead->nFlag = 1; /*  query was changed */
}

/* originally from hpt/src/toss.c */
s_arealink *getAreaLink(s_area *area, hs_addr aka)
{
    UINT i;

    for (i = 0; i <area->downlinkCount; i++) {
        if (addrComp(aka, area->downlinks[i]->link->hisAka)==0) return area->downlinks[i];
    }

    return NULL;
}

/* originally from hpt/src/toss.c */
/*  import: type == 0, export: type != 0 */
/*  return value: 0 if access ok, 3 if import/export off, 4 if not linked, */
/*  15 if area is paused */
int checkAreaLink(s_area *area, hs_addr aka, int type)
{
    s_arealink *arealink = NULL;
    int writeAccess = 0;

    arealink = getAreaLink(area, aka);
    if (arealink) {
	if (type==0) {
	    if (!arealink->import) writeAccess = BM_DENY_IMPORT;
	} else {
	    if (!arealink->export) writeAccess = BM_DENY_IMPORT;
	}
    } else {
	if (addrComp(aka, *area->useAka)!=0) writeAccess = BM_NOT_LINKED;
    }

    if (writeAccess==0 && area->paused) writeAccess = BM_AREA_IS_PAUSED;
	
    return writeAccess;
}

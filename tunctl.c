/* 
 * Copyright 2002 Jeff Dike
 * Licensed under the GPL
 */

/*
 *  This code is originally written by Jeff Dike
 * 
 *  Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

/*
 *  Rewrote original tunctl command to make it work for
 *  tun/tap driver of Solaris. 
 *  
 *  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/varargs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sockio.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include "if_tun.h"

#ifndef TUNMAXPPA
#define TUNMAXPPA	20
#endif
#define MUXFILENAME     "/tmp/tunctl.mid" /* store mux_id */

static int   get_ppa (char *);
static char *get_devname (char *);
static void  Usage(char *);

static void
Usage(char *name)
{
  fprintf(stderr, "Create: %s -t device-name [-f tun-clone-device] [-b]\n", name);
  fprintf(stderr, "Delete: %s -d device-name [-f tun-clone-device]\n\n", name);
  fprintf(stderr, "-b will result in brief output (just the device name)\n");
  exit(1);
}

int
main(int argc, char **argv)
{
    int   tap_fd, opt, delete = 0, brief = 0, ip_fd, muxid = 0;
    char *tun = NULL, *file = NULL, *name = argv[0];
    char *ip_node = "/dev/ip", entry[30], backup[1024];
    FILE *muxfp;
    int   ppa = 0, newppa = 0;
    struct strioctl  strioc;    

    while((opt = getopt(argc, argv, "bd:f:t:u:")) > 0){
        switch(opt) {
            case 'b':
                brief = 1;
                break;
            case 'd':
                delete = 1;
                tun = optarg;
                break;
            case 'f':
                file = optarg;
                break;
            case 't':
                tun = optarg;
                break;
            case 'h':
            default:
                Usage(name);
        }
    }

    argv += optind;
    argc -= optind;

    if(argc > 0)
        Usage(name);

    if(tun == NULL)
        Usage(name);

    if ((ppa = get_ppa(tun)) < 0){
        fprintf(stderr, "Can't get instance number\n");
        Usage(name);
    }

    if(file == NULL){
        file = malloc(30);
        bzero(file, 30);      
        sprintf(file, "/dev/%s", get_devname(tun));
    }
  
    if((tap_fd = open(file, O_RDWR)) < 0){
        perror("open");
        fprintf(stderr, "Can't open '%s'\n", file);
        exit(1);
    }
  
    if(delete){
        char    *p;
        
        bzero(entry, 30);
        bzero(backup, 1024);
    
        if ((muxfp = fopen(MUXFILENAME, "r")) == NULL) {
            perror("fopen");
            fprintf(stderr,"Can't open %s\n", MUXFILENAME);
            exit(0);
        }
      
        /*
         * Search entry of the interface for /tmp/tunclt.mid file.
         */
        while (fgets(entry, sizeof(entry), muxfp) != NULL){
            if (strncmp(entry, tun, strlen(tun)) == 0){
                p = strpbrk(entry, ":");                
                muxid = atoi(p+1);
            } else {
                strcat(backup, entry);
            }
        }        
        if (muxid == 0){
            fprintf(stderr,"%s is not created.\n", tun );
            fclose(muxfp);
            exit(0);
        }
        fclose(muxfp);

        if ((muxfp = fopen(MUXFILENAME, "w")) == NULL) {
            perror("fopen");
            fprintf(stderr,"Can't open %s\n", MUXFILENAME);
            exit(1);
        }
        fputs(backup, muxfp);
        fclose(muxfp);
    
        if ((ip_fd = open(ip_node, O_RDWR)) < 0){
            perror("open");
            fprintf(stderr,"Can't open %s\n", ip_node);          
            exit(1);
        }    

        if (ioctl (ip_fd, I_PUNLINK, muxid) < 0){
            perror("ioctl");
            fprintf(stderr, "Can't unlink interface\n");
            exit(1);
        }
        close (ip_fd);
      
    } else{
        /*
         * Check /tmp/tunctl.mid to see if interface is already created.
         */
        if ((muxfp = fopen(MUXFILENAME, "r")) != NULL) {
            while (fgets(entry, sizeof(entry), muxfp) != NULL){
                if (strncmp(entry, tun, strlen(tun)) == 0){
                    fprintf(stderr, "%s is already created.\n", tun);
                    exit(0);
                }
            }
            fclose(muxfp);
        }

        /* Open ip device for persist link */
        if ((ip_fd = open (ip_node, O_RDWR, 0)) < 0){
            perror("open");                  
            fprintf(stderr, "Can't open '%s'\n", ip_node);
            exit(1);
        }
      
        /* Assign a new PPA and get its unit number. */
        strioc.ic_cmd = TUNNEWPPA;
        strioc.ic_timout = 0;
        strioc.ic_len = sizeof(ppa);
        strioc.ic_dp = (char *)&ppa;
        if ((newppa = ioctl (tap_fd, I_STR, &strioc)) < 0){
            perror("ioctl");
            fprintf(stderr, "Can't create %s\n", tun); 
            exit(1);
        }

        if(newppa != ppa){
            fprintf(stderr, "Can't create %s\n", tun);             
            exit(1);
        }

        if ((muxid = ioctl (ip_fd, I_PLINK, tap_fd)) < 0){
            perror("ioctl");
            fprintf (stderr, "Can't link %s device to IP\n", tun);
            exit(1);
        }

        if ((muxfp = fopen(MUXFILENAME, "a")) == NULL) {
            perror("fopen");
            fprintf(stderr,"Can't open %s\n", MUXFILENAME);
            exit(0);	
        }
        sprintf(entry, "%s:%d\n", tun, muxid);
        fputs(entry, muxfp);
        fclose(muxfp);

        if(brief)
            printf("%s\n", tun);
        else
            printf("Set '%s' persistent\n", tun);      
    }
    return(0);
}

int
get_ppa (char *interface)
{
    int    i;
    int    ppa = 0;      /* PPA(Physical Point of Attachment). */
    int    ifnamelen;    /* Length of interface name */
    char  *tempchar;
    
    ifnamelen = strlen(interface);
    tempchar = interface;
    
    if ( isdigit ((int) tempchar[ifnamelen - 1]) == 0 ){
        /* interface doesn't have instance number */
        fprintf(stderr, "Please specify instance number (e.g. tun0, tap1)\n");
        return(-1);
    }
    
    for ( i = ifnamelen - 2 ; i >= 0 ; i--){
        if ( isdigit ((int) tempchar[i]) == 0 ){
            ppa = atoi(&(tempchar[i + 1]));
            break;
        }
        if (i == 0) {
            /* looks all char are digit.. can't handle */
            fprintf(stderr, "Invalid interface name\n");
            return(-1);
        }
        continue;
    }
    return(ppa);
}

char *
get_devname (char *interface)
{
      char *instance;
      char *devname = NULL;
      int   i;
      
      for ( i = 0 ; i < TUNMAXPPA ; i++){
          if ((instance = strpbrk(interface, "0123456789")) == NULL)
              continue;
          devname = malloc(30);
          bzero(devname, 30);
          strncpy(devname, interface, instance - interface);
      }
      return(devname);
}

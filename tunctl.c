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
#define IP_NODE "/dev/udp"
#define NODE_LEN 30

typedef enum {
    NOP = 0,
    DELETE,
    ADD
} operation_t ;

typedef enum {
    TUN = 0,
    TAP,
    UNKNOWN
} dev_type_t;

static int  get_ppa (char *);
static char* get_dev_node (char *);
static void Usage(char *);
static void add_if(int, int, char *, int, char *);
static void delete_if(int, int, char *);
static dev_type_t get_dev_type(char *); 

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
    char *tun = NULL;
    char *dev_node = NULL;
    char *name = argv[0];
    int opt, brief = 0, if_fd, ip_fd;
    operation_t op = NOP;

    while((opt = getopt(argc, argv, "bd:f:t:u:")) > 0){
        switch(opt) {
            case 'b':
                brief = 1;
                break;
            case 'd':
                op = DELETE;
                tun = optarg;
                break;
            case 'f':
                dev_node = optarg;
                break;
            case 't':
                op = ADD;
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

    if(dev_node == NULL)
        dev_node = get_dev_node(tun);

    if ((ip_fd = open(IP_NODE, O_RDWR)) < 0){
        perror("open");
        fprintf(stderr,"Can't open %s\n", IP_NODE);          
        exit(1);
    }    

    if((if_fd = open(dev_node, O_RDWR)) < 0){
        perror("open");
        fprintf(stderr, "Can't open '%s'\n", dev_node);
        exit(1);
    }

    switch(op){
        case(DELETE):
            delete_if(ip_fd, if_fd, tun);
            break;
        case(ADD):
            add_if(ip_fd, if_fd, tun, brief, dev_node);
            break;
        default:
            break;
    }
    close(ip_fd);
    close(if_fd);
    
    exit(0);
}

void
add_if(int ip_fd, int if_fd, char *tun, int brief, char *dev_node){
    int arp_fd;
    int ip_muxid = 0;
    int arp_muxid = 0;
    int newppa = 0;
    int ppa = 0;
    struct lifreq ifr;
    struct strioctl  strioc, strioc_if;
    dev_type_t type;
    
    memset(&ifr, 0, sizeof(struct lifreq));

    if((type = get_dev_type(tun)) == UNKNOWN){
        fprintf(stderr, "Unknown device name: %s\n" , tun);
        exit(1);
    }

    if ((ppa = get_ppa(tun)) < 0){
        fprintf(stderr, "Can't get instance number\n");
        exit(1);
    }
    
    /* Assign a new PPA and get its unit number. */
    strioc.ic_cmd = TUNNEWPPA;
    strioc.ic_timout = 0;
    strioc.ic_len = sizeof(ppa);
    strioc.ic_dp = (char *)&ppa;
    if ((newppa = ioctl (if_fd, I_STR, &strioc)) < 0){
        perror("ioctl");
        fprintf(stderr, "Can't create %s\n", tun); 
        exit(1);
    }

    if(newppa != ppa){
        fprintf(stderr, "Can't create %s\n", tun);             
        exit(1);
    }

    /* push ip module to the stream */
    if (ioctl(if_fd, I_PUSH, "ip") < 0) {
        perror("ioctl");
        fprintf (stderr, "Can't push ip module to %s device\n", tun);
        exit(1);
    }

    if(type == TUN)
    {
        /* set unit number to the device */
        if (ioctl (if_fd, IF_UNITSEL, (char *) &ppa) < 0){
            perror("ioctl");
            fprintf (stderr, "Can't set unit number to %s device\n", tun);
            exit(1);
        }
    }

    if(type == TAP)
    {
        if (ioctl(if_fd, SIOCGLIFFLAGS, &ifr) < 0){
            fprintf (stderr, "Can't get flags\n");
            exit(1);
        }
        strncpy (ifr.lifr_name, tun, sizeof (ifr.lifr_name));
        ifr.lifr_ppa = ppa;
        /* assign ppa according to the unit number returned by tun device */
        if (ioctl (if_fd, SIOCSLIFNAME, &ifr) < 0){
            fprintf (stderr, "Can't set PPA %d\n", ppa);
            exit(1);
        }
        if (ioctl(if_fd, SIOCGLIFFLAGS, &ifr) <0) {
            fprintf (stderr, "Can't get flags\n");
            exit(1);
        }

        /* push arp module to the device stream */
        if (ioctl(if_fd, I_PUSH, "arp") < 0) {
            perror("ioctl");
            fprintf (stderr, "Can't push ARP module to device stream\n");
            exit(1);
        }

        /* push arp module to the ip stream */
        if (ioctl(ip_fd, I_PUSH, "arp") < 0) {
            perror("ioctl");
            fprintf (stderr, "Can't push ARP module to ip stream\n");	
            exit(1);
        }

        /* open arp fd */
        if((arp_fd = open(dev_node, O_RDWR)) < 0){
            perror("open");
            fprintf(stderr, "Can't open '%s'\n", dev_node);
            exit(1);
        }
        /* push arp module to the stream */
        if (ioctl(arp_fd, I_PUSH, "arp") < 0) {
            perror("ioctl");
            fprintf (stderr, "Can't push ARP module to arp stream\n");
            exit(1);
        }

        /* set ifname to arp */
        strioc_if.ic_cmd = SIOCSLIFNAME;
        strioc_if.ic_timout = 0;
        strioc_if.ic_len = sizeof(ifr);
        strioc_if.ic_dp = (char *)&ifr;
	/* set interface name to arp stream */
        if (ioctl(arp_fd, I_STR, &strioc_if) < 0){
            fprintf (stderr, "Can't set ifname to arp stream\n");
            exit(1);
        }
    }

    /* link interface stream to ip stream */
    if ((ip_muxid = ioctl (ip_fd, I_PLINK, if_fd)) < 0){
        perror("ioctl");
        fprintf (stderr, "Can't link %s device to ip\n", tun);
        exit(1);
    }

    if(type == TAP){
        if ((arp_muxid = ioctl (ip_fd, I_PLINK, arp_fd)) < 0){
            perror("ioctl");
            fprintf (stderr, "Can't link %s device to ARP\n", tun);
            exit(1);
        }
    }

    memset((void *)&ifr, 0x0, sizeof(struct lifreq));        
    strncpy (ifr.lifr_name, tun, sizeof (ifr.lifr_name));
    ifr.lifr_ip_muxid  = ip_muxid;
    if(type == TAP){
        ifr.lifr_arp_muxid  = arp_muxid;
    }
    if (ioctl (ip_fd, SIOCSLIFMUXID, &ifr) < 0){
        perror("ioctl");
        fprintf (stderr, "Can't set muxid of  %s device\n", tun);
        exit(1);
    }

    if(brief)
        printf("%s\n", tun);
    else
        printf("Set '%s' persistent\n", tun);      
}
    
void
delete_if(int ip_fd, int if_fd, char *tun)
{
    int arp_muxid = 0, ip_muxid = 0;
    struct lifreq ifr;
    dev_type_t type;

    if((type = get_dev_type(tun)) == UNKNOWN){
        fprintf(stderr, "Unknown device name: %s\n" , tun);
        exit(1);
    }

    memset((void *)&ifr, 0x0, sizeof(struct lifreq));
    strncpy (ifr.lifr_name, tun, sizeof (ifr.lifr_name));

    if (ioctl (ip_fd, SIOCGLIFFLAGS, &ifr) < 0){
        perror("ioctl");
        fprintf(stderr, "Can't get flag\n");
        exit(1);
    }

    if (ioctl (ip_fd, SIOCGLIFMUXID, &ifr) < 0){
        perror("ioctl");
        fprintf(stderr, "Can't get muxid\n");
        exit(1);
    }

    if(type == TAP){
        /* just in case, unlink arp's stream */
        arp_muxid = ifr.lifr_arp_muxid;
        if (ioctl (ip_fd, I_PUNLINK, arp_muxid) < 0){
            /* ignore err */
        }
    }
               
    ip_muxid = ifr.lifr_ip_muxid;
    if (ioctl (ip_fd, I_PUNLINK, ip_muxid) < 0){
        perror("ioctl");
        fprintf(stderr, "Can't unlink interface\n");
        exit(1);
    }
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
get_dev_node (char *interface)
{
    char *instance;
    char devname[NODE_LEN]; 
    int   i;
    char *dev_node;
    bzero(devname, NODE_LEN);
      
    for ( i = 0 ; i < TUNMAXPPA ; i++){
      if ((instance = strpbrk(interface, "0123456789")) == NULL)
	continue;
      strncpy(devname, interface, instance - interface);
    }
    dev_node = malloc(NODE_LEN);
    bzero(dev_node, NODE_LEN);      
    snprintf(dev_node, NODE_LEN, "/dev/%s", devname);
    return dev_node;
}

dev_type_t 
get_dev_type (char *interface)
{
      dev_type_t type;

      if(strncmp("tun", interface, 3) == 0){
          type = TUN;
      } else if (strncmp("tap", interface, 3) == 0){
          type = TAP;
      } else {
          type = UNKNOWN;
      }
      return type;
}

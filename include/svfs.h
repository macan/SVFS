/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-10 11:08:13 macan>
 *
 * klagent supply the interface between BLCR and LAGENT(user space)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __SVFS_H__
#define __SVFS_H__

/* Define the versions of different modules */
#define SVFS_CLIENT_VERSION "0.0.1"

/* headers file needed by SVFS */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/backing-dev.h>
#include <linux/statfs.h>
#include <linux/mount.h>

/* svfs inode structures */
#include "svfs_i.h"

/* svfs super magic */
#define SVFS_SUPER_MAGIC 0x51455145
#define SVFS_NAME_LEN 255

/* svfs tracing */
#define SVFS_INFO    0x80000000
#define SVFS_WARNING 0x40000000
#define SVFS_PRECISE 0x00000002
#define SVFS_DEBUG   0x00000001

#define svfs_tracing(mask, flag, lvl, f, a...) do {     \
        if (mask & flag) {                              \
            if (mask & SVFS_PRECISE) {                  \
                printk(lvl "SVFS (%s, %d): %s:",        \
                       __FILE__, __LINE__, __func__);   \
            }                                           \
            printk(lvl f, ## a);                        \
        }                                               \
    } while (0)

#define svfs_info(module, f, a...)                              \
    svfs_tracing(SVFS_INFO, svfs_##module##_tracing_flags,      \
                 KERN_INFO, f, ## a)

#define svfs_debug(module, f, a...)             \
    svfs_tracing((SVFS_DEBUG | SVFS_PRECISE),   \
                 svfs_##module##_tracing_flags, \
                 KERN_DEBUG, f, ## a)

#define svfs_warning(module, f, a...)           \
    svfs_tracing((SVFS_WARNING | SVFS_PRECISE), \
                 svfs_##module##_tracing_flags, \
                 KERN_WARNING, f, ##a)

/* APIs for super.c */
extern struct kmem_cache *svfs_inode_cachep;
extern int svfs_get_sb(struct file_system_type *, int, const char *,
                       void *, struct vfsmount *);
extern void svfs_kill_super(struct super_block *);
/* APIs for inode.c */
extern int svfs_write_inode(struct inode *, int);
extern void svfs_dirty_inode(struct inode*);
extern void svfs_delete_inode(struct inode*);


/* Include all the tracing flags */
#include "svfs_tracing.h"

#endif

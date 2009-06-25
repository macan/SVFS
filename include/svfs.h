/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-24 16:42:51 macan>
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
#define SVFS_CLIENT_VERSION "0.0.2"

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
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/file.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/namei.h>
#include <linux/fsnotify.h>
#include <linux/writeback.h>

/* svfs inode structures */
#include "svfs_i.h"

/* svfs super magic */
#define SVFS_SUPER_MAGIC 0x51455145
#define SVFS_NAME_LEN 255

/* svfs tracing */
#define SVFS_INFO    0x80000000
#define SVFS_WARNING 0x40000000
#define SVFS_ERR     0x20000000
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

#define svfs_err(module, f, a...)               \
    svfs_tracing((SVFS_ERR | SVFS_PRECISE),     \
                 svfs_##module##_tracing_flags, \
                 KERN_ERR, f, ##a)

/* APIs for super.c */
extern struct kmem_cache *svfs_inode_cachep;
extern int svfs_get_sb(struct file_system_type *, int, const char *,
                       void *, struct vfsmount *);
extern void svfs_kill_super(struct super_block *);
/* APIs for inode.c */
extern int svfs_write_inode(struct inode *, int);
extern void svfs_dirty_inode(struct inode*);
extern void svfs_delete_inode(struct inode*);
extern void svfs_set_inode_flags(struct inode *);
extern void svfs_get_inode_flags(struct svfs_inode *);
extern int svfs_mark_inode_dirty(struct inode *);
extern void svfs_set_aops(struct inode *);
extern struct inode *svfs_iget(struct super_block *, unsigned long);
extern void svfs_truncate(struct inode *);
/* APIs for namei.c */
extern const struct inode_operations svfs_dir_inode_operations;
/* APIs for sync.c */
extern int svfs_sync_file(struct file *, struct dentry *, int);
/* APIs for dir.c */
extern const struct file_operations svfs_dir_operations;
/* APIs for ialloc.c */
extern struct inode *svfs_new_inode(struct inode *, int);
extern void svfs_free_inode(struct inode *);
/* APIs for buffer.c */
extern void svfs_sync_page(struct page *page);
/* APIs for file.c */
extern const struct file_operations svfs_file_operations;
extern const struct inode_operations svfs_file_inode_operations;
extern int llfs_lookup(struct inode *);
/* APIs for datastore.c */
extern void svfs_datastore_init(void);
extern struct svfs_datastore *svfs_datastore_add_new(int, char *);
extern void svfs_datastore_free(struct svfs_datastore *);
extern void svfs_datastore_exit(void);
extern struct svfs_datastore *svfs_datastore_get(int type);

/* Include all the tracing flags */
#include "svfs_tracing.h"

#ifdef SVFS_LOCAL_TEST
extern char *svfs_backing_store;
extern char *svfs_targeting_store;
#define SVFS_BACKING_STORE_SIZE (128 * 1024)
extern ssize_t svfs_backing_store_write(struct svfs_super_block *);
extern ssize_t svfs_backing_store_read(struct svfs_super_block *);
extern void svfs_backing_store_commit_bse(struct inode *);
extern int svfs_backing_store_update_bse(struct svfs_super_block *,
                                         struct dentry *, struct inode *);
extern unsigned long svfs_backing_store_find_mark_ino(
    struct svfs_super_block *);
extern unsigned long svfs_backing_store_lookup(struct svfs_super_block *,
                                               unsigned long, 
                                               const char *);
extern void svfs_backing_store_set_root(struct svfs_super_block *);
extern int svfs_backing_store_get_path(struct svfs_super_block *,
                                       struct backing_store_entry *,
                                       char *, size_t);
extern void svfs_backing_store_write_dirty(struct svfs_super_block *);
extern unsigned long svfs_backing_store_find_child(
    struct svfs_super_block *,
    unsigned long, unsigned long);
extern int svfs_backing_store_delete(struct svfs_super_block *,
                                     unsigned long, unsigned long,
                                     const char *);
#endif

/* relay operations */
#define svfs_relay(name, type, inode) ({                \
            void *retval;                               \
            switch (type) {                             \
            case LLFS_TYPE_EXT4:                        \
                retval = svfs_relay_ext4_##name(inode); \
                break;                                  \
            default:                                    \
                retval = ERR_PTR(-EINVAL);              \
                ;                                       \
            }                                           \
            retval;                                     \
        })
#define svfs_relay_define(name, type, ret, args ...)    \
    ret svfs_relay_##type##_##name(args)

svfs_relay_define(lookup, ext4, struct dentry *, struct inode *);

#endif

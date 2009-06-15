/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-15 19:36:20 macan>
 *
 * klagent supply the interface between BLCR and LAGENT(user space)
 *
 * Armed by EMACS.
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

#include "svfs_dep.h"
#include "svfs.h"

static unsigned int svfs_client_tracing_flags = 0x80000000;

module_param(svfs_client_tracing_flags, uint, S_IRUGO);
MODULE_PARM_DESC(svfs_client_tracing_flags,
                 "SVFS Client Tracing Flags: 0xffffffff for all events");

#ifdef MDC_TRACING_EXTERNAL
module_param(svfs_mdc_tracing_flags, uint, S_IRUGO);
MODULE_PARM_DESC(svfs_mdc_tracing_flags,
                 "SVFS MDC Tracing Flags: 0xffffffff for all events");
#endif

#ifdef SVFS_LOCAL_TEST
char *svfs_backing_store = "/tmp/svfs_backing_store";
char *svfs_targeting_store = "/mnt/tmp";
module_param(svfs_backing_store, charp, S_IRUGO);
MODULE_PARM_DESC(svfs_backing_store,
                 "SVFS Backing Store: pathname");
module_param(svfs_targeting_store, charp, S_IRUGO);
MODULE_PARM_DESC(svfs_targeting_store,
                 "SVFS Targeting Store: pathname");
#endif

MODULE_AUTHOR("Ma Can <macan@ncic.ac.cn>");
MODULE_DESCRIPTION("SVFS Client");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(SVFS_CLIENT_VERSION);

static char svfs_client_name[] = "SVFS Client";
static char svfs_client_string[] = 
    "Copyright (c) 2009 Ma Can <macan@ncic.ac.cn>";

struct kmem_cache *svfs_inode_cachep;

static void init_once(void *foo)
{
    struct svfs_inode *si = (struct svfs_inode *)foo;

    inode_init_once(&si->vfs_inode);
}

static int init_inodecache(void)
{
    svfs_inode_cachep = kmem_cache_create("svfs_inode_cache",
                                          sizeof(struct svfs_inode),
                                          0, (SLAB_RECLAIM_ACCOUNT |
                                              SLAB_MEM_SPREAD),
                                          init_once);
    if (svfs_inode_cachep == NULL)
        return -ENOMEM;
    return 0;
}

static void destroy_inodecache(void)
{
    kmem_cache_destroy(svfs_inode_cachep);
}

static struct file_system_type svfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "svfs",
    .get_sb = svfs_get_sb,
    .kill_sb = svfs_kill_super,
    .fs_flags = FS_BINARY_MOUNTDATA | FS_RENAME_DOES_D_MOVE,
};

static int __init init_svfs(void)
{
    int err;

    printk(KERN_INFO "%s: %s\n"
              "Compiled at %s @ %s\n", 
              svfs_client_name, svfs_client_string,
              CDATE, CHOST);

    printk(KERN_INFO "svfs_client_tracing_flags 0x%x\n", 
           svfs_client_tracing_flags);
    printk(KERN_INFO "svfs_mdc_tracing_flags 0x%x\n", 
           svfs_mdc_tracing_flags);
#ifdef SVFS_LOCAL_TEST
    printk(KERN_INFO "svfs_backing_store %s\n", 
           svfs_backing_store);
#endif

    err = init_inodecache();
    if (err)
        goto out;

    err = register_filesystem(&svfs_fs_type);
    if (err)
        goto out1;

    return 0;
out1:
    destroy_inodecache();
out:
    return err;
}

static void __exit exit_svfs(void)
{
    unregister_filesystem(&svfs_fs_type);
    destroy_inodecache();
}

module_init(init_svfs);
module_exit(exit_svfs);

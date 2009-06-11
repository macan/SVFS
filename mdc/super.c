/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-11 21:33:32 macan>
 *
 * Supporting SVFS superblock operations.
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

#include "svfs.h"

static int svfs_compare_super(struct super_block *sb, void *data)
{
    struct svfs_sb_mountdata *sb_mntdata = data;
    struct svfs_super_block *new = sb_mntdata->ssb, *old = SVFS_SB(sb);
    int mntflags = sb_mntdata->mntflags;

    if (memcmp(&old->fsid, &new->fsid, sizeof(old->fsid)) != 0)
        return 0;
    /* TODO: need to handle mount options */
    mntflags = 0;
    return 1;
}

static struct svfs_super_block *svfs_alloc_sb(void)
{
    struct svfs_super_block *ssb;

    ssb = kzalloc(sizeof(struct svfs_super_block), GFP_KERNEL);
    if (!ssb)
        return ERR_PTR(-ENOMEM);
    /* TODO: init svfs_super_block here */
    svfs_debug(mdc, "kzalloc ssb %p size %ld\n", ssb,
               sizeof(struct svfs_super_block));
    return ssb;
}

/**
 * TODO: Setting up the server names and path.
 */
static int svfs_init_sb(struct svfs_super_block *ssb, int flags, 
                        const char *dev_name, void *raw_data)
{
    /* FIXME: */
    ssb->flags = SVFS_FREE;
    ssb->fsid = 0;
    return 0;
}

static void svfs_free_sb(struct svfs_super_block *ssb)
{
    if (!ssb)
        return;
    /* TODO: finalize other fields first */
    svfs_debug(mdc, "kfree ssb %p\n", ssb);
    kfree(ssb);
}

static int svfs_set_super(struct super_block *s, void *data)
{
    struct svfs_sb_mountdata *sb_mntdata = data;
    struct svfs_super_block *ssb = sb_mntdata->ssb;
    int ret;

    s->s_flags = sb_mntdata->mntflags;
    s->s_fs_info = ssb;
    ret = set_anon_super(s, ssb);
    if (ret == 0)
        ssb->s_dev = s->s_dev;
    return ret;
}

static struct inode *svfs_alloc_inode(struct super_block *sb)
{
    struct svfs_inode *si;

    si = kmem_cache_alloc(svfs_inode_cachep, GFP_NOFS);
    if (!si)
        return NULL;
    /* TODO: init the svfs inode here */
    /* TODO: should journal the new inode? */

    svfs_debug(mdc, "alloc new svfs_inode: %p\n", si);
    return &si->vfs_inode;
}

static void svfs_destroy_inode(struct inode *inode)
{
    svfs_debug(mdc, "destroy svfs_inode: %p\n", SVFS_I(inode));
    /* TODO: free the info in svfs_inode? */
    kmem_cache_free(svfs_inode_cachep, SVFS_I(inode));
}

static void svfs_put_super(struct super_block *sb)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);

    /* TODO: release the super block @ MDS & OSD? */
    svfs_debug(mdc, "release the svfs_super_blcok %p\n", ssb);
}

static int svfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    struct super_block *sb = dentry->d_sb;
    struct svfs_super_block *ssb = SVFS_SB(sb);

    /* TODO: query the MDS to get the fs states */
    /* TODO: fill the buf by the returned states */
    buf->f_type = SVFS_SUPER_MAGIC;
    buf->f_bsize = sb->s_blocksize;
    buf->f_blocks = 0x1000;     /* FIXME */
    buf->f_bfree = 0x500;
    buf->f_bavail = 0x400;
    buf->f_files = 0;
    buf->f_ffree = 0x1000;
    buf->f_namelen = SVFS_NAME_LEN;
    buf->f_fsid.val[0] = ssb->fsid & 0xFFFFFFFFUL;
    buf->f_fsid.val[1] = (ssb->fsid >> 32) & 0xFFFFFFFFUL;

    svfs_debug(mdc, "svfs statfs: type 0x%lx\n", buf->f_type);
    return 0;
}

static int svfs_sync_fs(struct super_block *sb, int wait)
{
    sb->s_dirt = 0;
    /* TODO: commit the dirty contents? */
    svfs_debug(mdc, "svfs sync fs now, wait %d\n", wait);
    return 0;
}

static int svfs_remount_fs(struct super_block *sb, int *flags, char *data)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);
    unsigned long old_sb_flags;
    int err = 0;

    old_sb_flags = sb->s_flags;
    ssb->flags |= SVFS_MOUNTED;
    /* TODO: get the old options, compare with the new options */
    svfs_debug(mdc, "svfs remount fs, ssb %p\n", ssb);

    return err;
}

/* 
 * Invoked when a disk inode is being destroyed to perform 
 * filesystem-specific operations.
 */
static void svfs_clear_inode(struct inode *inode)
{
    /* TODO: release the inode @ MDS */
    svfs_debug(mdc, "svfs clear inode\n");
    return;
}

static const struct super_operations svfs_sops = {
    .alloc_inode = svfs_alloc_inode,
    .destroy_inode = svfs_destroy_inode,
    .write_inode = svfs_write_inode,
    .dirty_inode = svfs_dirty_inode,
    .delete_inode = svfs_delete_inode,
    .put_super = svfs_put_super,
    .statfs = svfs_statfs,
    .sync_fs = svfs_sync_fs,
    .remount_fs = svfs_remount_fs,
    .clear_inode = svfs_clear_inode,
};

static int svfs_fill_super(struct super_block *sb, struct vfsmount *vfsmnt)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);
    struct inode *inode;
    int err;

    /* TODO: should statfs to get the superblock from the stable storage? */

    sb->s_magic = SVFS_SUPER_MAGIC;   /* 5145 <-> SVFS */
    sb->s_blocksize_bits = 12;  /* 4K? */
    sb->s_blocksize = (1ULL << 12);
    sb->s_maxbytes = ~0ULL;
    sb->s_op = &svfs_sops;

    if (sb->s_flags & MS_RDONLY)
        ssb->flags |= SVFS_RDONLY;
    ssb->sb = sb;
    ssb->mtime = CURRENT_TIME;

    /* TODO: call iget to get the root inode? */
    err = -ENOMEM;
    inode = iget_locked(sb, SVFS_ROOT_INODE);
    if (inode == NULL) {
        goto out;
    }

    if (inode->i_state & I_NEW) {
        struct svfs_inode *si = SVFS_I(inode);

        si->version = 0;
        
        inode->i_ino = SVFS_ROOT_INODE;
        inode->i_flags = S_NOATIME | S_NOCMTIME;
        inode->i_mode = S_IFDIR;
        inode->i_op = &svfs_dir_inode_operations;
        inode->i_fop = &svfs_dir_operations;

        /* FIXME: */
        inode->i_atime = inode->i_mtime = 
            inode->i_ctime = CURRENT_TIME;
        inode->i_size = (1ULL << 12);    /* get from the statfs */
        inode->i_nlink = 2;
        inode->i_uid = 0;
        inode->i_gid = 0;
        inode->i_blocks = 8;

        unlock_new_inode(inode);
        svfs_debug(mdc, "root inode state I_NEW, ct=%d\n", 
                   atomic_read(&inode->i_count));
    }

    err = -ENOMEM;
    sb->s_root = d_alloc_root(inode);
    if (sb->s_root == NULL) {
        iput(inode);
        goto out_iput;
    }
    atomic_inc(&inode->i_count);
    spin_lock(&dcache_lock);
    list_del_init(&sb->s_root->d_alias);
    spin_unlock(&dcache_lock);

    svfs_debug(mdc, "root inode ct=%d\n",
               atomic_read(&inode->i_count));
    svfs_info(mdc, "DUMP root dentry:\n"
              ".d_count %d, " ".d_flags 0x%x, .d_inode %p, \n"
              ".d_parent %p, .d_hash %p, .d_lru %d, .d_sb %p, \n"
              ".d_subdirs %d, .d_u %d, .d_op %p,\n"
              ".d_alias %d, .d_mounted %d\n"
              , atomic_read(&sb->s_root->d_count),
              sb->s_root->d_flags, sb->s_root->d_inode,
              sb->s_root->d_parent, sb->s_root->d_hash.pprev,
              list_empty(&sb->s_root->d_lru), sb->s_root->d_sb,
              list_empty(&sb->s_root->d_subdirs), 
              list_empty(&sb->s_root->d_u.d_child),
              sb->s_root->d_op, list_empty(&sb->s_root->d_alias),
              sb->s_root->d_mounted);
    err = 0;
out:
    svfs_debug(mdc, "err %d\n", err);
    return err;
out_iput:
    iput(inode);
    goto out;
}

int svfs_get_sb(struct file_system_type *fs_type,
                int flags, const char *dev_name, void *raw_data,
                struct vfsmount *mnt)
{
    int err = 0;
    struct super_block *s;
    struct svfs_sb_mountdata sb_mntdata = {
        .mntflags = flags,
    };
    struct svfs_super_block *ssb;

    ssb = svfs_alloc_sb();
    if (IS_ERR(ssb)) {
        err = PTR_ERR(ssb);
        goto out_alloc_sb;
    }
    sb_mntdata.ssb = ssb;

    /* TODO: to compare the fsid, you should probe the fsid first */
    err = svfs_init_sb(ssb, flags, dev_name, raw_data);
    if (err)
        goto out_alloc_sb;

    svfs_debug(mdc, "pre sget()\n");
    s = sget(fs_type, svfs_compare_super, svfs_set_super, &sb_mntdata);
    if (IS_ERR(s)) {
        err = PTR_ERR(s);
        goto out_err_nosb;
    }

    if (s->s_fs_info != ssb) {
        svfs_free_sb(ssb);
        ssb = NULL;
    } else {
        /* It is a new ssb */
        err = bdi_register_dev(&ssb->backing_dev_info, ssb->s_dev);
        if (err)
            goto out_splat_super;
        ssb->flags |= SVFS_MOUNTED;
    }

    if (!s->s_root) {
        /* initial superblock/root creation */
        svfs_debug(mdc, "pre svfs_fill_super()\n");
        err = svfs_fill_super(s, mnt);
        if (err)
            goto out_splat_root;
        svfs_debug(mdc, "after svfs_fill_super(), err %d\n", err);
    }

    s->s_flags |= MS_ACTIVE;
    simple_set_mnt(mnt, s);
    
    err = 0;
out:
    return err;
out_err_nosb:
    svfs_free_sb(ssb);
out_alloc_sb:
    goto out;

out_splat_root:
out_splat_super:
    up_write(&s->s_umount);
    deactivate_super(s);
    /* do not release ssb? */
    goto out_err_nosb;
}

/* 
 * This function is called with the reference count equal 1,
 * which means the last ref.
 */
void svfs_kill_super(struct super_block *s)
{
    struct svfs_super_block *ssb = SVFS_SB(s);

    svfs_debug(mdc, ".s_count %d, .s_active %d, "
               ".d_count %d, .i_count %d\n",
               s->s_count,
               atomic_read(&s->s_active),
               atomic_read(&s->s_root->d_count),
               atomic_read(&s->s_root->d_inode->i_count));

    /* NOTE: why should we do atomic_dec? */
    atomic_dec(&s->s_root->d_inode->i_count);
    bdi_unregister(&ssb->backing_dev_info);
    kill_anon_super(s);
    svfs_free_sb(ssb);
}

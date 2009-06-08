/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-08 22:12:54 macan>
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
    return 1;
}

static struct svfs_super_block *svfs_alloc_sb(void)
{
    struct svfs_super_block *ssb;

    ssb = kzalloc(sizeof(struct svfs_super_block), GFP_KERNEL);
    if (!ssb)
        return ERR_PTR(-ENOMEM);
    /* TODO: init svfs_super_block here */
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

static int svfs_fill_super(struct super_block *sb, int flags)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);
    struct inode *inode;
    int err;

    /* TODO: should statfs to get the superblock from the stable storage? */

    sb->s_magic = 0x51455145;   /* 5145 <-> SVFS */
    sb->s_blocksize_bits = 12;  /* 4K? */
    sb->s_blocksize = (1ULL << 12);
    sb->maxbytes = ~0ULL;
    sb->s_op = &svfs_super_operations;

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

        inode->i_ino = SVFS_ROOT_INODE;
        inode->i_flags = S_NOATIME | S_NOCMTIME;
        inode->i_mode = S_IFDIR;
        inode->i_op = &svfs_dir_inode_operations;
        inode->i_fop = &svfs_dir_operations;

        /* FIXME: */
        inode->i_atime = inode->mtime = 
            inode->ctime = CURRENT_TIME;
        inode->i_size = (1ULL << 12);    /* get from the statfs */
        inode->i_nlink = 2;
        inode->i_uid = 0;
        inode->i_gid = 0;
        inode->i_blocks = 8;

        unlock_new_inode(inode);
        svfs_debug(client, "root inode state I_NEW, ct=%d\n", 
                   atomic_read(&inode->i_count));
    }

    sb->s_root = d_alloc_root(inode);
    if (sb->s_root == NULL) {
        iput(inode);
        goto out_iput;
    }
    atomic_inc(&inode->i_count);
    spin_lock(&dcache_lock);
    list_del_init(&sb->s_root->d_alias);
    spin_unlock(&dcache_lock);

    mnt->mnt_sb = s;
    mnt->mnt_root = sb->s_root;
    
    svfs_debug(client, "root inode ct=%d\n",
               atomic_read(&inode->i_count));
out:
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

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (data == NULL)
        goto out_free;

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

    s = sget(fs_type, svfs_compare_super, svfs_set_super, &sb_mntdata);
    if (IS_ERR(s)) {
        err = PTR_ERR(s);
        goto out_err_nosb;
    }

    if (s->s_fs_info != sb_mntdata.ssb) {
        svfs_free_sb();
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
        svfs_fill_super(s, mnt);
    }

    s->s_flags |= MS_ACTIVE;
    
    err = 0;
out:
out_free:
    kfree(data);
    return err;
out_err_nosb:
    svfs_free_sb();
out_alloc_sb:
    goto out;

out_splat_root:
out_splat_super:
    up_write(&s->s_umount);
    deactivate_super(s);
    /* do not release ssb? */
    goto out;
}

void svfs_kill_super(struct super_block *s)
{
    struct svfs_super_block *ssb = SVFS_SB(s);

    bdi_unregister(ssb->backing_dev_info);
    kill_anon_super(s);
    svfs_free_sb(ssb);
}

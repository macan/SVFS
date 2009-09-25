/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-07-22 17:08:39 macan>
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
    ssb->flags = SVFS_SB_FREE;
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
    si->state = 0;
    /* TODO: should journal the new inode? */

    svfs_debug(mdc, "alloc new svfs_inode: %p\n", si);
    return &si->vfs_inode;
}

static void svfs_destroy_inode(struct inode *inode)
{
    svfs_entry(mdc, "destroy svfs_inode: %p CONN 0x%x\n", 
               SVFS_I(inode),
               (SVFS_I(inode)->state & SVFS_STATE_CONN));
    /* TODO: free the info in svfs_inode? */
    if (SVFS_I(inode)->state & SVFS_STATE_CONN) {
        fput(SVFS_I(inode)->llfs_md.llfs_filp);
    }
    kmem_cache_free(svfs_inode_cachep, SVFS_I(inode));
}

static void svfs_put_super(struct super_block *sb)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);

    /* TODO: release the super block @ MDS & OSD? */
    svfs_debug(mdc, "release the svfs_super_blcok %p\n", ssb);
}

static struct inode *svfs_nfs_get_inode(struct super_block *sb,
                                        u64 ino, u32 generation)
{
    struct inode *inode;

    if (ino > SVFS_SB(sb)->bs_size)
        return ERR_PTR(-ESTALE);

    /* iget isn't really right if the inode is currently unallocated!!
     *
     * ext4_read_inode will return a bad_inode if the inode had been
     * deleted, so we should be safe.
     *
     * Currently we don't know the generation for parent directory, so
     * a generation of 0 means "accept any"
     */
    inode = svfs_iget(sb, ino);
    if (IS_ERR(inode))
        return ERR_CAST(inode);
    if (generation && inode->i_generation != generation) {
        iput(inode);
        return ERR_PTR(-ESTALE);
    }

    return inode;
}

static struct dentry *svfs_fh_to_dentry(struct super_block *sb, struct fid *fid,
                                        int fh_len, int fh_type)
{
    return generic_fh_to_dentry(sb, fid, fh_len, fh_type,
                                svfs_nfs_get_inode);
}

static struct dentry *svfs_fh_to_parent(struct super_block *sb, struct fid *fid,
                                        int fh_len, int fh_type)
{
    return generic_fh_to_parent(sb, fid, fh_len, fh_type,
                                svfs_nfs_get_inode);
}

static const struct export_operations svfs_export_ops = {
    .fh_to_dentry = svfs_fh_to_dentry,
    .fh_to_parent = svfs_fh_to_parent,
    .get_parent = svfs_get_parent,
};

static int svfs_statfs(struct dentry *dentry, struct kstatfs *buf)
{
    struct super_block *sb = dentry->d_sb;
    struct svfs_super_block *ssb = SVFS_SB(sb);

    /* TODO: query the MDS to get the fs states */
    /* TODO: fill the buf by the returned states */
    buf->f_type = SVFS_SUPER_MAGIC;
    buf->f_bsize = sb->s_blocksize;
    /* Geting f_blocks,f_bfree,f_bavail from LLFSs */
    svfs_datastore_statfs(buf);
    buf->f_files = ssb->bs_size;
    buf->f_ffree = ssb->bs_size - (int)atomic_read(&ssb->bs_inuse);
    buf->f_namelen = SVFS_NAME_LEN;
    buf->f_fsid.val[0] = ssb->fsid & 0xFFFFFFFFUL;
    buf->f_fsid.val[1] = (ssb->fsid >> 32) & 0xFFFFFFFFUL;

    svfs_debug(mdc, "svfs statfs: type 0x%lx, files %d, ffree %d\n", 
               buf->f_type, (int)buf->f_files, (int)buf->f_ffree);
    return 0;
}

static int svfs_sync_fs(struct super_block *sb, int wait)
{
    sb->s_dirt = 0;
    /* TODO: commit the dirty contents? */
    svfs_debug(mdc, "svfs sync fs now, wait %d, s_io %d, s_dirty %d\n", 
               wait, list_empty(&sb->s_io), list_empty(&sb->s_dirty));
    if (IS_SVFS_VERBOSE(mdc))
        dump_stack();
    return 0;
}

static int svfs_remount_fs(struct super_block *sb, int *flags, char *data)
{
    struct svfs_super_block *ssb = SVFS_SB(sb);
    unsigned long old_sb_flags;
    int err = 0;

    old_sb_flags = sb->s_flags;
    ssb->flags |= SVFS_SB_MOUNTED;
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
    svfs_entry(mdc, "svfs clear inode %ld\n", inode->i_ino);

    svfs_write_inode(inode, 1);
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

#ifdef SVFS_LOCAL_TEST
    /* FIXME: opening the backing store here? */
    static int backing_store_counter = 0;
    ssize_t br;
    err = -ENOMEM;
    ssb->backing_store = __getname();
    if (!ssb->backing_store)
        goto out;
    sprintf(ssb->backing_store, "%s_%d", svfs_backing_store, 
            backing_store_counter++);
    ssb->bs_filp = filp_open(ssb->backing_store, O_RDWR | O_CREAT, 
                             S_IRWXU);
    if (!ssb->bs_filp)
        goto out1;
    ssb->bse = vmalloc(SVFS_BACKING_STORE_SIZE);
    if (!ssb->bse)
        goto out2;
    memset(ssb->bse, 0, SVFS_BACKING_STORE_SIZE);
    br = svfs_backing_store_read(ssb);
    if (br < 0)
        goto out3;
    /* setting the SVFS_SB_LOCAL_TEST flags */
    ssb->flags |= SVFS_SB_LOCAL_TEST;
    ssb->bs_size = SVFS_BACKING_STORE_SIZE / 
        sizeof(struct backing_store_entry);
    svfs_debug(mdc, "Reading %d bytes %d entries from backing_store %s\n",
               (int)br, ssb->bs_size, ssb->backing_store);
#endif
    /* TODO: should statfs to get the superblock from the stable storage? */

    sb->s_magic = SVFS_SUPER_MAGIC;   /* 5145 <-> SVFS */
    sb->s_blocksize_bits = 12;  /* 4K? */
    sb->s_blocksize = (1ULL << 12);
    sb->s_maxbytes = ~0ULL;
    sb->s_op = &svfs_sops;
    sb->s_export_op = &svfs_export_ops;

    if (sb->s_flags & MS_RDONLY)
        ssb->flags |= SVFS_SB_RDONLY;
    ssb->sb = sb;
    ssb->mtime = CURRENT_TIME;
    get_random_bytes(&ssb->next_generation, sizeof(u32));
    spin_lock_init(&ssb->next_gen_lock);

    /* TODO: call iget to get the root inode? */
    err = -ENOMEM;
    inode = iget_locked(sb, SVFS_ROOT_INODE);
    if (inode == NULL) {
        goto out3;
    }

    if (inode->i_state & I_NEW) {
        struct svfs_inode *si = SVFS_I(inode);

        si->version = 0;
        si->state = SVFS_STATE_DISC;
        
        inode->i_ino = SVFS_ROOT_INODE;
        inode->i_flags = S_NOATIME | S_NOCMTIME;
        inode->i_mode = S_IFDIR | S_IRUGO | S_IXUGO | S_IWUSR;
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
        svfs_get_inode_flags(SVFS_I(inode));
        /* FIXME: setting DA here? */
        si->flags |= SVFS_IF_DA;

        unlock_new_inode(inode);
#ifdef SVFS_LOCAL_TEST
        svfs_backing_store_set_root(ssb);
        atomic_set(&ssb->bs_inuse, svfs_backing_store_scan(ssb));
#endif        
        svfs_debug(mdc, "root inode state I_NEW, ct=%d, i_flags 0x%x\n", 
                   atomic_read(&inode->i_count), inode->i_flags);
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

    svfs_debug(mdc, "root inode ct=%d, i_flags 0x%x\n",
               atomic_read(&inode->i_count), inode->i_flags);
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
out3:
#ifdef SVFS_LOCAL_TEST
    vfree(ssb->bse);
#endif
out2:__attribute__((unused))
#ifdef SVFS_LOCAL_TEST
    fput(ssb->bs_filp);
#endif
out1:
#ifdef SVFS_LOCAL_TEST
    __putname(ssb->backing_store);
#endif
    goto out;
out_iput:
    iput(inode);
    goto out1;
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
        ssb->flags |= SVFS_SB_MOUNTED;
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
#ifdef SVFS_LOCAL_TEST
    {
        ssize_t bw;
        svfs_backing_store_write_dirty(ssb);
        bw = svfs_backing_store_write(ssb);
        if (bw >= 0)
            svfs_debug(mdc, "Write %d bytes to backing_store %s\n",
                       (int)bw, ssb->backing_store);
        else
            svfs_err(mdc, "Write to backing store failed, err %d\n",
                      (int)bw);
    }
    __putname(ssb->backing_store);
    fput(ssb->bs_filp);
    vfree(ssb->bse);
#endif
    svfs_free_sb(ssb);
}

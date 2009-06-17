/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-17 10:37:07 macan>
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

/* Adding one dentry in the dir, filesystem special opeartions */
static int svfs_add_dentry(struct dentry *dentry, struct inode *inode)
{
    struct inode *dir = dentry->d_parent->d_inode;
    struct super_block *sb = dir->i_sb;
    int retval;
    
#ifdef SVFS_LOCAL_TEST
    /* setting up the relationship */
    retval = svfs_backing_store_update_bse(SVFS_SB(sb), dentry, inode);
#endif
    return retval;
}

static int svfs_add_nondir(struct dentry *dentry, struct inode *inode)
{
    int err = svfs_add_dentry(dentry, inode);
    if (!err) {
        svfs_mark_inode_dirty(inode);
        d_instantiate(dentry, inode);
        return 0;
    }
    drop_nlink(inode);
    iput(inode);
    return err;
    
}

static int svfs_create(struct inode *dir, struct dentry *dentry, int mode,
                       struct nameidata *nd)
{
    struct inode *inode;
    int err;

    /* TODO: redirect the request to ext4 file system */
    inode = svfs_new_inode(dir, mode);
    err = PTR_ERR(inode);
    if (!IS_ERR(inode)) {
        inode->i_op = &svfs_file_inode_operations;
        inode->i_fop = &svfs_file_operations;
        svfs_set_aops(inode);
        err = svfs_add_nondir(dentry, inode);
    }

    svfs_debug(mdc, "create a new entry in %ld with %s(%ld)\n",
               dir->i_ino, dentry->d_name.name, inode->i_ino);
    return err;
}

unsigned long svfs_find_entry(struct dentry *dentry)
{
    struct super_block *sb;
    struct inode *dir = dentry->d_parent->d_inode;
    unsigned long ino = -1UL;

    sb = dir->i_sb;
#ifdef SVFS_LOCAL_TEST
    ino = svfs_backing_store_lookup(SVFS_SB(sb), dir->i_ino, 
                                    dentry->d_name.name);
    if (ino == -1UL) {
        svfs_warning(mdc, "svfs_find_dentry can not find the entry %s\n",
            dentry->d_name.name);
        goto out;
    } else {
        svfs_info(mdc, "svfs_find_dentry find the entry %s @ %ld\n",
                  dentry->d_name.name, ino);
    }
#endif
out:
    return ino;
}

static struct dentry *svfs_lookup(struct inode *dir, struct dentry *dentry,
                                  struct nameidata *nd)
{
    struct inode *inode;
    struct dentry *retval, *llfs_dentry;
    unsigned long ino;

    if (dentry->d_name.len > SVFS_NAME_LEN)
        return ERR_PTR(-ENAMETOOLONG);
    
    /* Step 1: find the svfs inode */
    ino = svfs_find_entry(dentry);
    if (ino == -1UL)
        return NULL;
    /* Step 2: get the svfs inode */
    inode = NULL;
    inode = svfs_iget(dir->i_sb, ino);
    if (IS_ERR(inode))
        return ERR_CAST(inode);
    /* Step 3: d_add(inode, dentry) */
    retval = d_splice_alias(inode, dentry);

    /* Step 4: lookup the llfs inode */
    if (S_ISDIR(inode->i_mode))
        goto out;
    llfs_dentry = svfs_relay(lookup, inode);
    if (IS_ERR(llfs_dentry)) {
        SVFS_I(inode)->state |= SVFS_STATE_DISC;
        svfs_err(mdc, "svfs_relay 'lookup' failed %ld\n", 
                 PTR_ERR(llfs_dentry));
        goto out;
    }
    SVFS_I(inode)->state |= SVFS_STATE_CONN;
out:
    return retval;
}

static int svfs_link(struct dentry *old_dentry, struct inode *dir,
                     struct dentry *dentry)
{
    return -ENOSYS;
}

static int svfs_unlink(struct inode *dir, struct dentry *dentry)
{
    return -ENOSYS;
}

static int svfs_symlink(struct inode *dir, struct dentry *dentry, 
                        const char *symname)
{
    return -ENOSYS;
}

static int svfs_mkdir(struct inode *dir, struct dentry *dentry,
                      int mode)
{
    return -ENOSYS;
}

static int svfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    return -ENOSYS;
}

static int svfs_mknod(struct inode *dir, struct dentry *dentry,
                      int mode, dev_t rdev)
{
    return -ENOSYS;
}

static int svfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                       struct inode *new_dir, struct dentry *new_dentry)
{
    return -ENOSYS;
}

static int svfs_setattr(struct dentry *dentry, struct iattr *attr)
{
    return -ENOSYS;
}

const struct inode_operations svfs_dir_inode_operations = {
    .create = svfs_create,
    .lookup = svfs_lookup,
    .link = svfs_link,
    .unlink = svfs_unlink,
    .symlink = svfs_symlink,
    .mkdir = svfs_mkdir,
    .rmdir = svfs_rmdir,
    .mknod = svfs_mknod,
    .rename = svfs_rename,
    .setattr = svfs_setattr,
};


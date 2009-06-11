/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-10 14:56:42 macan>
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

static int svfs_create(struct inode *dir, struct dentry *dentry, int mode,
                       struct nameidata *nd)
{
    /* TODO: redirect the request to ext4 file system */
    return -ENOTSUPP;
}

static struct dentry *svfs_lookup(struct inode *dir, struct dentry *dentry,
                                  struct nameidata *nd)
{
    return ERR_PTR(-ENOTSUPP);
}

static int svfs_link(struct dentry *old_dentry, struct inode *dir,
                     struct dentry *dentry)
{
    return -ENOTSUPP;
}

static int svfs_unlink(struct inode *dir, struct dentry *dentry)
{
    return -ENOTSUPP;
}

static int svfs_symlink(struct inode *dir, struct dentry *dentry, 
                        const char *symname)
{
    return -ENOTSUPP;
}

static int svfs_mkdir(struct inode *dir, struct dentry *dentry,
                      int mode)
{
    return -ENOTSUPP;
}

static int svfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    return -ENOTSUPP;
}

static int svfs_mknod(struct inode *dir, struct dentry *dentry,
                      int mode, dev_t rdev)
{
    return -ENOTSUPP;
}

static int svfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                       struct inode *new_dir, struct dentry *new_dentry)
{
    return -ENOTSUPP;
}

static int svfs_setattr(struct dentry *dentry, struct iattr *attr)
{
    return -ENOTSUPP;
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


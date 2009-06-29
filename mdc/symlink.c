/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-29 16:14:37 macan>
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

static void *svfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
#ifdef SVFS_LOCAL_TEST
    struct super_block *sb = dentry->d_inode->i_sb;
    struct backing_store_entry *bse = SVFS_SB(sb)->bse + 
        dentry->d_inode->i_ino;
    nd_set_link(nd, (char *)bse->ref_path);
#endif
    return NULL;
}

const struct inode_operations svfs_fast_symlink_inode_operations = {
    .readlink = generic_readlink,
    .follow_link = svfs_follow_link,
};

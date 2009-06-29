/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-27 13:54:52 macan>
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

static int svfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
    struct inode *inode = filp->f_path.dentry->d_inode;
    struct super_block *sb;
    struct backing_store_entry *bse;
    unsigned long offset;
    int ret = 0, stored = 0;
    unsigned char dtype;

    sb = inode->i_sb;
    offset = filp->f_pos;

    svfs_entry(mdc, "find from offset %ld\n", offset);

    while (offset < SVFS_SB(sb)->bs_size) {
#ifdef SVFS_LOCAL_TEST
        offset = svfs_backing_store_find_child(SVFS_SB(sb), 
                                               inode->i_ino, offset);
        if (offset == -1UL) {
            goto out;
        }
        bse = SVFS_SB(sb)->bse + offset;
        ASSERT(bse->parent_offset == inode->i_ino);
        svfs_debug(mdc, "get dentry %ld: %s 0x%x\n", 
                   offset, bse->relative_path, bse->state);
        if (S_ISDIR(bse->mode))
            dtype = DT_DIR;
        else if (S_ISREG(bse->mode))
            dtype = DT_REG;
        else if (S_ISLNK(bse->mode))
            dtype = DT_LNK;
        else
            dtype = DT_UNKNOWN;
        ret = filldir(dirent, bse->relative_path, 
                      strlen(bse->relative_path),
                      filp->f_pos, offset, dtype);
        if (ret)
            break;
        stored++;
        filp->f_pos = offset;
#endif
    }

out:
    ret = stored;
    return ret;
}

static int svfs_release_dir(struct inode *inode, struct file *filp)
{
    /* TODO: should we notify MDS? */
    return 0;
}

const struct file_operations svfs_dir_operations = {
    .llseek = generic_file_llseek,
    .read = generic_read_dir,
    .readdir = svfs_readdir,
    .fsync = svfs_sync_file,
    .release = svfs_release_dir,
};


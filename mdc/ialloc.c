/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-30 14:06:51 macan>
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

void svfs_free_inode(struct inode *inode)
{
    struct super_block *sb = inode->i_sb;

    /* TODO: should we do sth here? */
    if (atomic_read(&inode->i_count) > 1) {
        svfs_err(mdc, "inode has count=%d\n",
                 atomic_read(&inode->i_count));
        return;
    }
    if (inode->i_nlink) {
        svfs_err(mdc, "inode has nlink=%d\n",
                 inode->i_nlink);
        return;
    }
    if (!sb) {
        svfs_err(mdc, "inode on nonexistent device\n");
        return;
    }
    svfs_debug(mdc, "freeing inode %lu\n", inode->i_ino);
    
    clear_inode(inode);
    return;
}

struct inode *svfs_new_inode(struct inode *dir, int mode)
{
    struct super_block* sb;
    struct svfs_super_block *ssb;
    struct svfs_inode *si;
    struct inode *inode;
    unsigned long ino = 0;
    int err;

    if (!dir || !dir->i_nlink)
        return ERR_PTR(-EPERM);

    sb = dir->i_sb;
    inode = new_inode(sb);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    si = SVFS_I(inode);
    ssb = SVFS_SB(sb);

    /* TODO: get the new inode from the MDS? */
#ifdef SVFS_LOCAL_TEST
    {
        ino = svfs_backing_store_find_mark_ino(ssb);
        if (ino >= SVFS_BACKING_STORE_SIZE / 
            sizeof(struct backing_store_entry)) {
            /* get an invalid ino */
            svfs_warning(mdc, "Invalid ino %ld\n", ino);
            err = -ENOSPC;
            goto fail_drop;
        }
    }
#endif

    sb->s_dirt = 1;
    inode->i_uid = current_fsuid();
    /* TODO: please see ext4 */
    if (dir->i_mode & S_ISGID) {
        inode->i_gid = dir->i_gid;
        if (S_ISDIR(mode))
            mode |= S_ISGID;
    } else
        inode->i_gid = current_fsgid();
    inode->i_mode = mode;

    inode->i_ino = ino;
    inode->i_blocks = 0;
    inode->i_mtime = inode->i_atime = inode->i_ctime = si->crtime =
        CURRENT_TIME;

    si->disksize = 0;
    si->flags = SVFS_I(dir)->flags; /* inherit from the dir */
    si->dtime = 0;

    svfs_set_inode_flags(inode);
    insert_inode_hash(inode);
    spin_lock(&ssb->next_gen_lock);
    inode->i_generation = ssb->next_generation++;
    spin_unlock(&ssb->next_gen_lock);

    si->state = SVFS_STATE_NEW;
    
    /* TODO: need to do what? */
    err = svfs_mark_inode_dirty(inode);
    if (err)
        goto fail_drop_put;

    svfs_debug(mdc, "allocating inode %lu, i_flags 0x%x\n", 
               inode->i_ino, inode->i_flags);

    return inode;
fail_drop_put:
#ifdef SVFS_LOCAL_TEST
    /* FIXME: put/return the inode in backing store! */
    svfs_backing_store_delete(ssb, 0, ino, NULL);
#endif
fail_drop:
    inode->i_nlink = 0;
    iput(inode);
    return ERR_PTR(err);
}


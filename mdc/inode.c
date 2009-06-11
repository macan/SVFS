/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-11 17:08:02 macan>
 *
 * inode.c for SVFS
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

int svfs_force_commit(struct super_block *sb)
{
    /* TODO: force commit the superblock and the related dirty inodes */
    return 0;
}

int svfs_write_inode(struct inode *inode, int wait)
{
    if (current->flags & PF_MEMALLOC)
        return 0;

    if (!wait)
        return 0;

    svfs_debug(mdc, "svfs write inode now, wait %d\n", wait);

    return svfs_force_commit(inode->i_sb);
}

int svfs_mark_inode_dirty(struct inode *inode)
{
    /* TODO: dirty the disk structure to write */
    return 0;
}

void svfs_dirty_inode(struct inode *inode)
{
    svfs_mark_inode_dirty(inode);
}

void svfs_truncate(struct inode *inode)
{
    /* 
     * TODO: how to handle truncate?
     *
     * Step 1: checking
     * Step 2: truncate the i_disksize, caculate new OSD stripes
     * Step 3: do it on OSD and MDS
     */
    return;
}

void svfs_delete_inode(struct inode *inode)
{
    int err;

    svfs_debug(mdc, "svfs delete inode, si %p\n", SVFS_I(inode));
   /* TODO: truncate the inode pagecache */
    truncate_inode_pages(&inode->i_data, 0);

    if (is_bad_inode(inode))
        goto no_delete;
    
    inode->i_size = 0;
    err = svfs_mark_inode_dirty(inode);
    if (err) {
        svfs_warning(mdc, "couldn't mark inode dirty (err %d)\n", err);
        goto no_delete;
    }

    if (inode->i_blocks)
        svfs_truncate(inode);

    SVFS_I(inode)->dtime = get_seconds();
    
    if (svfs_mark_inode_dirty(inode))
        clear_inode(inode);
    else
        svfs_free_inode(inode);

    return;
no_delete:
    clear_inode(inode);
}

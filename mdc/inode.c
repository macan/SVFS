/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-12 22:12:55 macan>
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

#ifdef SVFS_LOCAL_TEST
    /* FIXME: write the path to bse ? */
    {
        svfs_backing_store_commit_bse(inode);
    }
#endif

    return svfs_force_commit(inode->i_sb);
}

int svfs_mark_inode_dirty(struct inode *inode)
{
    struct svfs_inode *si = SVFS_I(inode);

    /* TODO: dirty the disk structure to write */
#ifdef SVFS_LOCAL_TEST
    /* FIXME: setting the internal flags? */
    {
        struct svfs_super_block *ssb = SVFS_SB(inode->i_sb);
        struct backing_store_entry *bse = (ssb->bse + inode->i_no);

        if (si->state & SVFS_STATE_NEW) {
            memset(bse, 0, sizeof(struct backing_store_entry));
            bse->state |= SVFS_BS_NEW;
        }
        svfs_get_inode_flags(si);
        bse->disk_flags = si->flags;
        bse->state = SVFS_BS_DIRTY;
        si->state &= ~SVFS_STATE_NEW;
    }
#endif
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

void svfs_set_inode_flags(struct inode *inode)
{
    unsigned int flags = SVFS_I(inode)->flags;

    inode->i_flags &= ~(S_SYNC | S_APPEND | S_IMMUTABLE |
                        S_NOATIME | S_DIRSYNC);
    if (flags & SVFS_IF_SYNC)
        inode->i_flags |= S_SYNC;
    if (flags & SVFS_IF_APPEND)
        inode->i_flags |= S_APPEND;
    if (flags & SVFS_IF_IMMUTABLE)
        inode->i_flags |= S_IMMUTABLE;
    if (flags & SVFS_IF_NOATIME)
        inode->i_flags |= S_NOATIME;
    if (flags & SVFS_IF_DIRSYNC)
        inode->i_flags |= S_DIRSYNC;
}

void svfs_get_inode_flags(struct svfs_inode *si)
{
	unsigned int flags = si->vfs_inode.i_flags;

	si->flags &= ~(SVFS_IF_SYNC | SVFS_IF_APPEND | SVFS_IF_IMMUTABLE |
                     SVFS_IF_NOATIME | SVFS_IF_DIRSYNC);
	if (flags & S_SYNC)
		si->flags |= SVFS_IF_SYNC;
	if (flags & S_APPEND)
		si->flags |= SVFS_IF_APPEND;
	if (flags & S_IMMUTABLE)
		si->flags |= SVFS_IF_IMMUTABLE;
	if (flags & S_NOATIME)
		si->flags |= SVFS_IF_NOATIME;
	if (flags & S_DIRSYNC)
		si->flags |= SVFS_IF_DIRSYNC;
}

void svfs_set_aops(struct inode *inode)
{
    struct svfs_super_block *ssb = SVFS_SB(inode->i_sb);
    
#ifdef SVFS_LOCAL_TEST
    if (ssb->flags & SVFS_LOCAL_TEST)
        inode->i_mapping->a_ops = &svfs_bs_aops;
    else
        ;
#endif
        /* FIXME */
/*         inode->i_mapping->a_ops = &svfs_aops; */
}

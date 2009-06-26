/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-26 21:36:07 macan>
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

    svfs_debug(mdc, "svfs write inode %ld(0x%lx) now, wait %d\n",
               inode->i_ino, inode->i_state, wait);

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

    if (IS_SVFS_VERBOSE(mdc))
        dump_stack();
    svfs_debug(mdc, "dirty inode %ld\n", inode->i_ino);
    /* TODO: dirty the disk structure to write */
#ifdef SVFS_LOCAL_TEST
    /* FIXME: setting the internal flags? */
    {
        struct svfs_super_block *ssb = SVFS_SB(inode->i_sb);
        struct backing_store_entry *bse = (ssb->bse + inode->i_ino);

        if (si->state & SVFS_STATE_NEW) {
            memset(bse, 0, sizeof(struct backing_store_entry));
            bse->state |= SVFS_BS_NEW;
        }
        bse->state |= SVFS_BS_DIRTY;
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
     * Step 4: relay to LLFS?
     */
    struct svfs_inode *si = SVFS_I(inode);
    int ret;

    /* checking the llfs_md */
    if (!(si->state & SVFS_STATE_CONN)) {
        ret = llfs_lookup(inode);
        if (ret)
            return;
    }
    /* shall we relay the request to LLFS? */
    if (si->disksize > inode->i_size) {
        ret = vmtruncate(si->llfs_md.llfs_filp->f_dentry->d_inode, 
                         inode->i_size);
        if (ret)
            return;
    }
    svfs_debug(mdc, "relay the truncate to LLFS, ino %ld, size %lu\n",
               inode->i_ino, (unsigned long)inode->i_size);
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
    if ((flags & SVFS_IF_DIRSYNC) && S_ISDIR(inode->i_mode))
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

static int svfs_readpage(struct file *file, struct page *page)
{
    return -ENOSYS;
}

static int svfs_readpages(struct file *file, struct address_space *mapping,
                          struct list_head *pages, unsigned nr_pages)
{
    return -ENOSYS;
}

static int svfs_writepage(struct page *page,
                          struct writeback_control *wbc)
{
    return -ENOSYS;
}

static int svfs_writepages(struct address_space *mapping,
                           struct writeback_control *wbc)
{
    return -ENOSYS;
}

static 
int svfs_write_begin(struct file *file, struct address_space *mapping,
                     loff_t pos, unsigned len, unsigned flags,
                     struct page **pagep, void **fsdata)
{
    return -ENOSYS;
}

static int svfs_write_end(struct file *file, struct address_space *mapping,
                          loff_t pos, unsigned len, unsigned copied,
                          struct page *page, void *fsdata)
{
    return -ENOSYS;
}

static void svfs_invalidatepage(struct page *page, unsigned long offset)
{
    return;
}

static int svfs_releasepage(struct page *page, gfp_t wait)
{
    return -ENOSYS;
}

#ifdef SVFS_LOCAL_TEST
static int svfs_bs_readpage(struct file *file, struct page *page)
{
    return -ENOSYS;
}

static int svfs_bs_readpages(struct file *file, struct address_space *mapping,
                             struct list_head *pages, unsigned nr_pages)
{
    return -ENOSYS;
}

static int svfs_bs_writepage(struct page *page,
                             struct writeback_control *wbc)
{
    return -ENOSYS;
}

static int svfs_bs_writepages(struct address_space *mapping,
                              struct writeback_control *wbc)
{
    return -ENOSYS;
}

static int svfs_bs_write_begin(struct file *file, struct address_space *mapping,
                               loff_t pos, unsigned len, unsigned flags,
                               struct page **pagep, void **fsdata)
{
    return -ENOSYS;
}

static int svfs_bs_write_end(struct file *file, struct address_space *mapping,
                             loff_t pos, unsigned len, unsigned copied,
                             struct page *page, void *fsdata)
{
    return -ENOSYS;
}

static void svfs_bs_invalidatepage(struct page *page, unsigned long offset)
{
    return;
}

static int svfs_bs_releasepage(struct page *page, gfp_t wait)
{
    return -ENOSYS;
}

static const struct address_space_operations svfs_bs_aops = {
    .readpage = svfs_bs_readpage,
    .readpages = svfs_bs_readpages,
    .set_page_dirty = __set_page_dirty_nobuffers, /* from NFS */
    .writepage = svfs_bs_writepage,
    .writepages = svfs_bs_writepages,
    .write_begin = svfs_bs_write_begin,
    .write_end = svfs_bs_write_end,
    .invalidatepage = svfs_bs_invalidatepage,
    .releasepage = svfs_bs_releasepage,
    /* need .launder_page? */
};
#endif

static const struct address_space_operations svfs_aops = {
    .readpage = svfs_readpage,
    .readpages = svfs_readpages,
    .set_page_dirty = __set_page_dirty_nobuffers, /* from NFS */
    .writepage = svfs_writepage,
    .writepages = svfs_writepages,
    .sync_page = svfs_sync_page,
    .write_begin = svfs_write_begin,
    .write_end = svfs_write_end,
    .invalidatepage = svfs_invalidatepage,
    .releasepage = svfs_releasepage,
};

void svfs_set_aops(struct inode *inode)
{
    struct svfs_super_block *ssb = SVFS_SB(inode->i_sb);
    
#ifdef SVFS_LOCAL_TEST
    if (ssb->flags & SVFS_SB_LOCAL_TEST)
        inode->i_mapping->a_ops = &svfs_bs_aops;
    else
#endif
        inode->i_mapping->a_ops = &svfs_aops;
}

struct inode *svfs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode;
    struct svfs_inode *si;

    svfs_debug(mdc, "iget inode %ld\n", ino);

    inode = iget_locked(sb, ino);
    if (!inode)
        return ERR_PTR(-ENOMEM);
    if (!(inode->i_state & I_NEW))
        return inode;
    si = SVFS_I(inode);
    /* TODO: find the ino entry and fill it in the inode! */

    svfs_debug(mdc, "I_NEW inode %p\n", inode);
    si->version = 0;
    inode->i_ino = ino;
#ifdef SVFS_LOCAL_TEST
    {
        struct svfs_super_block *ssb = SVFS_SB(sb);
        struct backing_store_entry *bse = ssb->bse + ino;
        int err;

        ASSERT(bse->state & SVFS_BS_VALID);
        inode->i_nlink = bse->nlink;
        inode->i_size = bse->disksize;
        inode->i_mode = bse->mode;
        inode->i_uid = bse->uid;
        inode->i_gid = bse->gid;
        inode->i_atime = bse->atime;
        inode->i_ctime = bse->ctime;
        inode->i_mtime = bse->mtime;
        SVFS_I(inode)->flags = bse->disk_flags;
        /* get the ref path */
        SVFS_I(inode)->llfs_md.llfs_type = bse->llfs_type;
        err = svfs_backing_store_get_path(
            ssb, bse, 
            SVFS_I(inode)->llfs_md.llfs_pathname, NAME_MAX - 1);
        if (err) {
            iget_failed(inode);
            return ERR_PTR(err);
        }
        
        svfs_debug(mdc, "ino %ld, size %lu ,ref_path %s\n", 
                   inode->i_ino, (unsigned long)inode->i_size,
                   SVFS_I(inode)->llfs_md.llfs_pathname);

        if (S_ISDIR(inode->i_mode)) {
            ASSERT(bse->state & SVFS_BS_DIR);
            inode->i_op = &svfs_dir_inode_operations;
            inode->i_fop = &svfs_dir_operations;
        } else if (S_ISREG(inode->i_mode)) {
            ASSERT(bse->state & SVFS_BS_FILE);
            inode->i_op = &svfs_file_inode_operations;
            inode->i_fop = &svfs_file_operations;
            svfs_set_aops(inode);
        } else if (S_ISLNK(inode->i_mode)){
            ASSERT(bse->state & SVFS_BS_LINK);
            /* FIXME: symlink operations */
        } else {
            /* FIXME: special operations */
        }
        svfs_debug(mdc, "nlink %d, size %lu, mode 0x%x, "
                   "atime %lx\n",
                   inode->i_nlink,
                   (unsigned long)inode->i_size,
                   inode->i_mode,
                   inode->i_atime.tv_sec);
    }
#endif
    svfs_set_inode_flags(inode);
    unlock_new_inode(inode);
    return inode;
}

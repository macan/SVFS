/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-16 17:26:28 macan>
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

static 
ssize_t __svfs_backing_store_uread(struct file *filp, void *buf, 
                                   size_t count)
{
    ssize_t retval;
    ssize_t br, bl;

    bl = count;
    while (bl) {
        br = vfs_read(filp, buf, bl, &filp->f_pos);
        if (br < 0) {
            retval = br;
            goto out;
        }
        if (br == 0) {
            retval = count - bl;
            goto out;
        }
        bl -= br;
        buf += br;
    }
    retval = count;
out:
    return retval;
}

static
ssize_t __svfs_backing_store_uwrite(struct file *filp, const void *buf, 
                                    size_t count)
{
    ssize_t retval;
    ssize_t bw, bl;
    const char *p = buf;

    bl = count;
    while (bl) {
        bw = vfs_write(filp, p, bl, &filp->f_pos);
        if (bw < 0) {
            retval = bw;
            goto out;
        }
        if (bw == 0) {
            retval = count - bl;
            goto out;
        }
        bl -= bw;
        p += bw;
    }
    retval = count;
out:
    return retval;
}

ssize_t svfs_backing_store_read(struct svfs_super_block *ssb)
{
    ssize_t retval;
    mm_segment_t oldfs = get_fs();
    
    if (!ssb->bse || !ssb->bs_filp)
        return -EINVAL;

    set_fs(KERNEL_DS);
    retval = __svfs_backing_store_uread(ssb->bs_filp, ssb->bse, 
                                        SVFS_BACKING_STORE_SIZE);
    set_fs(oldfs);

    return retval;
}

ssize_t svfs_backing_store_write(struct svfs_super_block *ssb)
{
    ssize_t retval;
    mm_segment_t oldfs = get_fs();
    
    if (!ssb->bse || !ssb->bs_filp)
        return -EINVAL;

    set_fs(KERNEL_DS);
    retval = __svfs_backing_store_uwrite(ssb->bs_filp, ssb->bse,
                                         SVFS_BACKING_STORE_SIZE);
    set_fs(oldfs);

#if 0
    {
        /* Checking */
        u8 *i = (u8 *)ssb->bse;
        while (i < (((u8 *)ssb->bse) + SVFS_BACKING_STORE_SIZE)) {
            if (*i) {
                svfs_info(mdc, "invalid offset %ld\n", i - (u8 *)ssb->bse);
            }
            i++;
        }
    }
#endif

    return retval;
}

unsigned long svfs_backing_store_lookup(struct svfs_super_block *ssb,
                                        unsigned long dir_ino, 
                                        const char *name)
{
    unsigned long ino = 0;
    struct backing_store_entry *bse = ssb->bse;

    while (ino < SVFS_BACKING_STORE_SIZE / 
           sizeof(struct backing_store_entry)) {
        if (bse->parent_offset == dir_ino &&
            !strcmp(bse->relative_path, name)) {
            return ino;
        }
        ino++;
        bse++;
    }
    
    return -1UL;
}

void svfs_backing_store_mark_new_inode(struct svfs_super_block *ssb, 
                                       int ino)
{
    return;
}

int svfs_backing_store_update_bse(struct svfs_super_block *ssb,
                               struct dentry *dentry, struct inode *inode)
{
    struct backing_store_entry *bse;
    struct inode *dir = dentry->d_parent->d_inode;
    

    if (!(ssb->flags & SVFS_SB_LOCAL_TEST))
        return -EINVAL;

    bse = (ssb->bse + inode->i_ino);
    bse->parent_offset = (u32)dir->i_ino;
    bse->state &= ~SVFS_BS_NEW;
    sprintf(bse->relative_path, "%s", dentry->d_name.name);
    /* FIXME: setting up the entry automatically */
    sprintf(bse->ref_path, "ino_%ld", inode->i_ino);
    bse->state |= SVFS_BS_VALID;

    svfs_info(mdc, "Update the bse %ld: po %d, state 0x%x, "
              "rel path %s, ref path %s\n", inode->i_ino, 
              bse->parent_offset,
              bse->state, bse->relative_path, bse->ref_path);

    return 0;
}

void svfs_backing_store_commit_bse(struct inode *inode)
{
    struct backing_store_entry *bse;
    struct svfs_inode *si = SVFS_I(inode);

    bse = (SVFS_SB(inode->i_sb)->bse + inode->i_ino);
    
    svfs_get_inode_flags(si);
    bse->disk_flags = si->flags;
    bse->disksize = inode->i_size;
    bse->nlink = inode->i_nlink;
    bse->mode = inode->i_mode;
    bse->uid = inode->i_uid;
    bse->gid = inode->i_gid;
    bse->atime = inode->i_atime;
    bse->ctime = inode->i_ctime;
    bse->mtime = inode->i_mtime;
    bse->llfs_type = si->llfs_md.llfs_type;
    /* FIXME: should copy the llfs_path to bse! */

    svfs_debug(mdc, "bse %ld nlink %d, size %lu, mode 0x%x\n",
               inode->i_ino,
               bse->nlink, (unsigned long)bse->disksize, bse->mode);
    if (bse->state & SVFS_BS_DIRTY)
        bse->state &= ~SVFS_BS_DIRTY;
    if (bse->state & SVFS_BS_NEW)
        bse->state &= ~SVFS_BS_NEW;
    
    svfs_get_inode_flags(si);
    bse->disk_flags = si->flags;

    if (S_ISDIR(inode->i_mode))
        bse->state |= SVFS_BS_DIR;
    else if (S_ISREG(inode->i_mode))
        bse->state |= SVFS_BS_FILE;
    else if (S_ISLNK(inode->i_mode))
        bse->state |= SVFS_BS_LINK;
    else
        bse->state |= SVFS_BS_FREE; /* FIXME */

    if (!(bse->state & SVFS_BS_VALID)) {
        svfs_warning(mdc, "This bse %ld is INVALID.\n", inode->i_ino);
    }
}

unsigned long svfs_backing_store_find_mark_ino(struct svfs_super_block *ssb)
{
    unsigned long ino = 0;
    struct backing_store_entry *bse = ssb->bse;

    while (ino < ssb->bs_size) {
        bse = ssb->bse + ino;
        if (!bse->state) {
            bse->state = SVFS_BS_NEW;
            break;
        }
        ino++;
    }
    if (ino >= ssb->bs_size) {
        ino = -1UL;
    }
    svfs_info(mdc, "find new bse %ld\n", ino);
    return ino;
}

void svfs_backing_store_set_root(struct svfs_super_block *ssb)
{
    struct backing_store_entry *root = ssb->bse;

    root->parent_offset = 0;
    root->state = SVFS_BS_VALID | SVFS_BS_DIR;
    root->disksize = 0xff;
    root->nlink = 2;
    root->mode = S_IFDIR;
    root->uid = 0;
    root->gid = 0;
    sprintf(root->relative_path, "/");
    sprintf(root->ref_path, "/");
}

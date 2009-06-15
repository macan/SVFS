/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-15 20:51:11 macan>
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

    return retval;
}

unsigned long svfs_backing_store_lookup(struct svfs_super_block *ssb,
                                        unsigned long dir_ino, char *name)
{
    unsigned long ino = 0;
    struct backing_store_entry *bse = ssb->bse;

    while (ino < SVFS_BACKING_STORE_SIZE / 
           sizeof(struct backing_store_entry)) {
        if (bse->parent_offset == dir_ino &&
            !strcmp(base->relative_path, name)) {
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
    sprintf(bse->ref_path, "ino_%ld", inode->i_ino);
    /* FIXME: print sth. */
    svfs_info(mdc, "Update the bse %ld: po %d, state 0x%x,"
              "rel path %s, ref path %s\n", inode->i_ino, 
              bse->parent_offset,
              bse->state, bse->relative_path, bse->ref_path);
    return 0;
}

void svfs_backing_store_commit_bse(struct inode *inode)
{
    struct backing_store_entry *bse;

    bse = (SVFS_SB(inode->i_sb)->bse + inode->i_ino);
    
    if (bse->state & SVFS_BS_DIRTY)
        bse->state &= ~SVFS_BS_DIRTY;
    if (bse->state & SVFS_BS_NEW)
        bse->state &= ~SVFS_BS_NEW;
    
    if (S_ISDIR(inode->i_mode))
        bse->state |= SVFS_BS_DIR;
    else if (S_ISREG(inode->i_mode))
        bse->state |= SVFS_BS_FILE;
    else if (S_ISLNK(inode->i_mode))
        bse->state |= SVFS_BS_LINK;
    else
        bse->state |= SVFS_BS_FREE; /* FIXME */
    bse->state |= SVFS_BS_VALID;
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
    return ino;
}

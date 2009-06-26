/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-26 15:28:33 macan>
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

    ssb->bs_filp->f_pos = 0;
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

    /* seek to head */
    ssb->bs_filp->f_pos = 0;
    set_fs(KERNEL_DS);
    retval = __svfs_backing_store_uwrite(ssb->bs_filp, ssb->bse,
                                         SVFS_BACKING_STORE_SIZE);
    set_fs(oldfs);

#if 1
    {
        /* Checking */
        int i;
        struct backing_store_entry *bse = ssb->bse;
        for (i = 0; i < ssb->bs_size; i++, bse++) {
            if (bse->state & SVFS_BS_VALID) {
                svfs_debug(mdc, "ino %d, size %lu, "
                           "state 0x%x, atime %lx\n", 
                           i, (unsigned long)bse->disksize, bse->state,
                           bse->atime.tv_sec);
            }
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
            !strcmp(bse->relative_path, name) && 
                    bse->state) {
            return ino;
        }
        ino++;
        bse++;
    }
    
    return -1UL;
}

int svfs_backing_store_delete(struct svfs_super_block *ssb,
                              unsigned long dir_ino,
                              unsigned long ino,
                              const char *name)
{
    struct backing_store_entry *bse = ssb->bse + ino;

    if (bse->state & SVFS_BS_VALID) {
        /* checking it */
        ASSERT(dir_ino == bse->parent_offset);
        ASSERT(!strncmp(bse->relative_path, name, strlen(name)));
        bse->state |= SVFS_BS_DELETING;
    } else {
        svfs_warning(mdc, "delete invalid bse entry %lu %s @ %lu\n",
                     ino, name, dir_ino);
    }
    return 0;
}

unsigned long svfs_backing_store_find_child(struct svfs_super_block *ssb,
                                            unsigned long parent_ino,
                                            unsigned long offset)
{
    unsigned long ino;
    struct backing_store_entry *bse = ssb->bse + offset + 1;

    for (ino = offset + 1; ino < ssb->bs_size; ino++, bse++) {
        if (!(bse->state & SVFS_BS_VALID))
            continue;
        if (parent_ino == ino)
            continue;
        if (bse->parent_offset == parent_ino) {
            return ino;
        }
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
    struct backing_store_entry *bse, *parent;
    struct inode *dir = dentry->d_parent->d_inode;
    

    if (!(ssb->flags & SVFS_SB_LOCAL_TEST))
        return -EINVAL;

    bse = (ssb->bse + inode->i_ino);
    parent = (ssb->bse + dir->i_ino);
    bse->parent_offset = (u32)dir->i_ino;
    bse->depth = parent->depth + 1;
    bse->state &= ~SVFS_BS_NEW;
    sprintf(bse->relative_path, "%s", dentry->d_name.name);
    /* FIXME: setting up the entry automatically */
    sprintf(bse->ref_path, "ino_%ld", inode->i_ino);
    bse->state |= SVFS_BS_VALID;

    svfs_info(mdc, "Update the bse %ld: po %d, state 0x%x, "
              "rel path %s, ref path %s, depth %d\n", inode->i_ino, 
              bse->parent_offset,
              bse->state, bse->relative_path, bse->ref_path, bse->depth);

    return 0;
}

void svfs_backing_store_commit_bse(struct inode *inode)
{
    struct backing_store_entry *bse;
    struct svfs_inode *si = SVFS_I(inode);

    bse = (SVFS_SB(inode->i_sb)->bse + inode->i_ino);
    
    /* checking the freeing flags */
    if (bse->state & SVFS_BS_DELETING) {
        ASSERT(inode->i_state & I_FREEING);
        bse->state = 0;
        return;
    }

    svfs_get_inode_flags(si);
    bse->disk_flags = si->flags;
    if (S_ISDIR(inode->i_mode))
        bse->disksize = inode->i_size = SVFS_SB(inode->i_sb)->bs_size;
    else
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

    svfs_debug(mdc, "bse %ld nlink %d, size %lu, mode 0x%x, "
               "atime %lx\n",
               inode->i_ino,
               bse->nlink, 
               (unsigned long)bse->disksize, 
               bse->mode, inode->i_atime.tv_sec);
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
    root->depth = 0;
    root->state = SVFS_BS_VALID | SVFS_BS_DIR;
    root->disksize = 0xff;
    root->nlink = 2;
    root->mode = S_IFDIR;
    root->uid = 0;
    root->gid = 0;
    sprintf(root->relative_path, "/");
    sprintf(root->ref_path, "/");
}

int svfs_backing_store_get_path(struct svfs_super_block *ssb,
                                struct backing_store_entry *bse,
                                char *buf, size_t len)
{
    struct backing_store_entry *pos = bse;
    int depth = bse->depth, bp = 0;
    char *cursor[depth], *p;

    memset(buf, 0, len);
    if (len < 0 || !(bse->state & SVFS_BS_VALID))
        return -EINVAL;
    if (!depth) {
        snprintf(buf, len, "/");
        return 0;
    }
        
    while (depth > 0) {
        cursor[--depth] = pos->relative_path;
        pos = ssb->bse + pos->parent_offset;
    }

    p = buf;
    snprintf(buf, len, "/");
    p++;
    len--;
    while (depth < bse->depth) {
        bp = snprintf(p, len, ".%s", cursor[depth++]);
        p += bp;
        len -= bp;
    }
    return 0;
}

void svfs_backing_store_write_dirty(struct svfs_super_block *ssb)
{
    int i = 0;
    struct backing_store_entry *bse = ssb->bse;
    struct inode *inode;

    for (; i < ssb->bs_size; i++, bse++) {
        if (bse->state & SVFS_BS_DIRTY) {
            inode = ilookup(ssb->sb, i);
            if (inode) {
                /* this is the valid inode, do the data commit */
                svfs_backing_store_commit_bse(inode);
            } else
                continue;
            iput(inode);
        }
    }    
}

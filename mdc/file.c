/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-08-12 15:49:24 macan>
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

/* 
 * @inode:  svfs inode
 */
int llfs_lookup(struct inode *inode)
{
    struct svfs_datastore *sd;
    struct dentry *llfs_dentry;
    const struct cred *cred = current_cred();
    struct nameidata llfs_nd;
    char *ref_path;
    int err = 0;
    
    if (S_ISDIR(inode->i_mode))
        goto out;
    if (SVFS_I(inode)->state & SVFS_STATE_DA)
        goto out;
    if (SVFS_I(inode)->state & SVFS_STATE_CONN)
        goto out;

    err = -ENOMEM;
    ref_path = __getname();
    if (!ref_path)
        goto out;

    err = -EINVAL;
    sd = svfs_datastore_get(SVFS_I(inode)->llfs_md.llfs_type,
                            SVFS_I(inode)->llfs_md.llfs_fsid);
    if (!sd)
        goto out_release_name;
    sprintf(ref_path, "%s%s", sd->pathname,
            SVFS_I(inode)->llfs_md.llfs_pathname);
    svfs_debug(mdc, "fully ref_path %s\n", ref_path);

    err = path_lookup(ref_path, LOOKUP_FOLLOW, &llfs_nd);
    if (err)
        goto fail_lookup;
    SVFS_I(inode)->llfs_md.llfs_filp = dentry_open(llfs_nd.path.dentry,
                                                   llfs_nd.path.mnt,
                                                   O_RDWR,
                                                   cred);
    if (IS_ERR(SVFS_I(inode)->llfs_md.llfs_filp))
        goto out_put_path;

    llfs_dentry = svfs_relay(lookup,
                             SVFS_I(inode)->llfs_md.llfs_type, inode);
    if (IS_ERR(llfs_dentry)) {
        SVFS_I(inode)->state |= SVFS_STATE_DISC;
        svfs_err(mdc, "svfs_relay 'lookup' failed %ld\n",
                 PTR_ERR(llfs_dentry));
        goto out_put_filp;
    }
    SVFS_I(inode)->state |= SVFS_STATE_CONN;
    err = 0;
out:
    return err;

out_put_filp:
    fput(SVFS_I(inode)->llfs_md.llfs_filp);
    goto out;
out_put_path:
    /* path_put(&llfs_nd.path) */
    {
        svfs_debug(mdc, "llfs_nd.path->dentry %d, vfsmount %d\n",
                   atomic_read(&llfs_nd.path.dentry->d_count),
                   atomic_read(&llfs_nd.path.mnt->mnt_count));
    }
    goto out;
fail_lookup:
out_release_name:
    __putname(ref_path);
    goto out;
}

/* This function is ONLY designed for SVFS_STATE_DA */
int llfs_create(struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    struct svfs_inode *si = SVFS_I(inode);
    struct svfs_datastore *sd;
    struct file *llfs_file;
    char *ref_path;
    int ret;

    sd = svfs_datastore_get(LLFS_TYPE_ANY, 0);
    if (!sd) {
        ret = PTR_ERR(sd);
        goto out;
    }
    si->llfs_md.llfs_type = sd->type;
    si->llfs_md.llfs_fsid = svfs_datastore_fsid(sd->pathname);
    ret = -ENOMEM;
    ref_path = __getname();
    if (!ref_path)
        goto out;
    ret = svfs_backing_store_get_path2(SVFS_SB(inode->i_sb),
                                       SVFS_SB(inode->i_sb)->bse + 
                                       inode->i_ino,
                                       si->llfs_md.llfs_pathname,
                                       NAME_MAX - 1);
    if (ret)
        goto out_putname;
    snprintf(ref_path, PATH_MAX - 1, "%s%s", sd->pathname,
             si->llfs_md.llfs_pathname);
    svfs_debug(mdc, "New LLFS path %s, state 0x%x\n", ref_path, si->state);
    llfs_file = filp_open(ref_path, O_RDWR | O_CREAT,
                          S_IRUGO | S_IWUSR);
    ret = PTR_ERR(llfs_file);
    if (IS_ERR(llfs_file))
        goto out_putname;
    si->llfs_md.llfs_filp = llfs_file;
    si->state |= SVFS_STATE_CONN;
    si->state &= ~SVFS_STATE_DA;
    ret = 0;
    
out_putname:
    __putname(ref_path);
out:
    svfs_exit(mdc, "err %d. [NOTE]: if you get error here,"
              " you should check the LLFS permissions!\n", ret);
    return ret;
}

static ssize_t
svfs_file_aio_read(struct kiocb *iocb, const struct iovec *iov,
                   unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct file *llfs_filp;
    struct inode *inode = filp->f_dentry->d_inode;
    struct svfs_inode *si = SVFS_I(inode);
    char __user *buf = iov->iov_base;
    size_t count = iov->iov_len;
    ssize_t ret = 0, br;
    int seg;

    if (si->state & SVFS_STATE_DA) {
        /* create it now */
        ASSERT(!(si->state & SVFS_STATE_CONN));
        ret = llfs_create(filp->f_dentry);
        if (ret)
            goto out;
    }
    
    if (!(si->state & SVFS_STATE_CONN)) {
        /* open it? */
        ret = llfs_lookup(inode);
        if (ret)
            goto out;
    }

    llfs_filp = si->llfs_md.llfs_filp;
    llfs_filp->f_pos = pos;
    if (!(llfs_filp->f_mode & FMODE_READ))
        return -EBADF;
    if (!llfs_filp->f_op || 
        (!llfs_filp->f_op->read && !llfs_filp->f_op->aio_read))
        return -EINVAL;

    for (seg = 0; seg < nr_segs; seg++) {
        buf = iov[seg].iov_base;
        count = iov[seg].iov_len;
        if (llfs_filp->f_op->read)
            br = llfs_filp->f_op->read(llfs_filp, buf, count, 
                                       &llfs_filp->f_pos);
        else
            br = do_sync_read(llfs_filp, buf, count, &llfs_filp->f_pos);
        ret += br;
        svfs_debug(mdc, "buf %p, len %ld: \n", buf, count);
    }
    if (ret > 0)
        fsnotify_access(llfs_filp->f_dentry);
    iocb->ki_pos += ret;
out:
    return ret;
}

static ssize_t
svfs_file_aio_write(struct kiocb *iocb, const struct iovec *iov,
                    unsigned long nr_segs, loff_t pos)
{
    struct file *filp = iocb->ki_filp;
    struct file *llfs_filp;
    struct inode *inode = filp->f_dentry->d_inode;
    struct svfs_inode *si = SVFS_I(inode);
    const char __user *buf;
    size_t count;
    ssize_t ret = 0, bw;
    int seg;

    svfs_entry(mdc, "f_mode 0x%x, pos %lu, check 0x%x\n",
               filp->f_mode,
               (unsigned long)pos,
               (si->state & SVFS_STATE_CONN));
    if (si->state & SVFS_STATE_DA) {
        /* create it now */
        ASSERT(!(si->state & SVFS_STATE_CONN));
        ret = llfs_create(filp->f_dentry);
        if (ret)
            goto out;
    }

    if (!(si->state & SVFS_STATE_CONN)) {
        /* open it? */
        ret = llfs_lookup(inode);
        if (ret)
            goto out;
    }

    BUG_ON(iocb->ki_pos != pos);
    ASSERT(llfs_filp->f_dentry);
    ASSERT(llfs_filp->f_dentry->d_inode);

    /* adjusting the offset */
    if (filp->f_flags & O_APPEND)
        pos = i_size_read(inode);
    
    llfs_filp = si->llfs_md.llfs_filp;
    llfs_filp->f_pos = pos;
    if (!(llfs_filp->f_mode & FMODE_WRITE))
        return -EBADF;
    if (!llfs_filp->f_op ||
        (!llfs_filp->f_op->write && !llfs_filp->f_op->aio_write))
        return -EINVAL;

    for (seg = 0; seg < nr_segs; seg++) {
        buf = iov[seg].iov_base;
        count = iov[seg].iov_len;
        svfs_debug(mdc, "buf %p, len %ld: \n", buf, count);
        if (llfs_filp->f_op->write)
            bw = llfs_filp->f_op->write(llfs_filp, buf, count, 
                                        &llfs_filp->f_pos);
        else
            bw = do_sync_write(llfs_filp, buf, count, &llfs_filp->f_pos);
        if (bw < 0) {
            ret = bw;
            goto out;
        }
        ret += bw;
    }
    
    if (ret > 0)
        fsnotify_modify(llfs_filp->f_dentry);
    
    if (ret > 0 && ((filp->f_flags & O_SYNC) || IS_SYNC(inode))) {
        ssize_t err;
        err = sync_page_range(llfs_filp->f_dentry->d_inode,
                              llfs_filp->f_mapping,
                              pos, ret);
        if (err < 0)
            ret = err;
    }
    iocb->ki_pos += ret;
    ASSERT(llfs_filp->f_pos == iocb->ki_ops);
    /* should update the file info */
    file_update_time(filp);
    if (pos + ret > inode->i_size) {
        svfs_debug(mdc, "update with pos %lu count %ld, "
                   "original i_size %lu\n",
                   (unsigned long)pos, ret, 
                   (unsigned long)inode->i_size);
        i_size_write(inode, pos + ret);
        mark_inode_dirty(inode);
    }
out:
    return ret;
}

static
loff_t svfs_file_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t n;
    svfs_entry(mdc, "offset %ld, origin %d\n", 
               (unsigned long)offset, origin);
	mutex_lock(&file->f_dentry->d_inode->i_mutex);
	n = generic_file_llseek_unlocked(file, offset, origin);
	mutex_unlock(&file->f_dentry->d_inode->i_mutex);
	return n;
}

static 
int svfs_file_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct address_space *mapping = file->f_mapping;
    struct address_space *llfs_mapping;
    struct inode *inode = mapping->host;
    struct file *llfs_filp;
    int ret;

    ret = -EINVAL;
    if (!(SVFS_I(inode)->state & SVFS_STATE_CONN)) {
        /* open it? */
        ret = llfs_lookup(inode);
        if (ret)
            goto out;
    }
    llfs_filp = SVFS_I(inode)->llfs_md.llfs_filp;
    llfs_mapping = llfs_filp->f_mapping;
    
    file_accessed(file);
    /* DANGEOUS: redirect to llfs file ? */
    vma->vm_file = llfs_filp;
    
    if (llfs_filp->f_op && llfs_filp->f_op->mmap)
        ret = llfs_filp->f_op->mmap(llfs_filp, vma);
    else
        ret = -ENOEXEC;
    if (!ret) {
        fput(file);
        get_file(llfs_filp);
    }
out:
	return ret;
}

static 
ssize_t svfs_file_splice_read(struct file *in, loff_t *ppos,
                              struct pipe_inode_info *pipe, size_t len,
                              unsigned int flags)
{
    int ret;
    struct file *llfs_file;
    struct svfs_inode *si;

    svfs_entry(mdc, "pos %lu, len %ld, flags 0x%x\n", (unsigned long)*ppos,
               (long)len, flags);

    /* we should change the file struct here! */
    si = SVFS_I(in->f_dentry->d_inode);
    if (si->state & SVFS_STATE_DA) {
        /* create it now */
        ASSERT(!(si->state & SVFS_STATE_CONN));
        ret = llfs_create(in->f_dentry);
        if (ret)
            goto out;
    }
    if (!(si->state & SVFS_STATE_CONN)) {
        /* open it ? */
        ret = llfs_lookup(in->f_dentry->d_inode);
        if (ret)
            goto out;
    }
    
    llfs_file = si->llfs_md.llfs_filp;
    ASSERT(llfs_filp);
    ret = generic_file_splice_read(llfs_file, ppos, pipe, len, flags);
    
out:
    return ret;
}

static
ssize_t svfs_file_splice_write(struct pipe_inode_info *pipe, 
                               struct file *out, loff_t *ppos, size_t len,
                               unsigned int flags)
{
    struct address_space *mapping;
    struct inode *inode;
    struct file *llfs_filp;
    struct svfs_inode *si;
    struct splice_desc sd = {0,};
    ssize_t ret;

    svfs_entry(mdc, "pos %lu, len %ld, flags 0x%x\n", (unsigned long)*ppos,
               (long)len, flags);

    si = SVFS_I(out->f_dentry->d_inode);
    if (si->state & SVFS_STATE_DA) {
        /* create it now */
        ASSERT(!(si->state & SVFS_STATE_CONN));
        ret = llfs_create(out->f_dentry);
        if (ret)
            goto out;
    }
    if (!(si->state & SVFS_STATE_CONN)) {
        /* open it ? */
        ret = llfs_lookup(out->f_dentry->d_inode);
        if (ret)
            goto out;
    }
    llfs_filp = si->llfs_md.llfs_filp;
    ASSERT(llfs_filp);
    mapping = llfs_filp->f_mapping;
    inode = mapping->host;
    sd.total_len = len;
    sd.flags = flags;
    sd.pos = *ppos;
    sd.u.file = llfs_filp;

    pipe_lock(pipe);

    splice_from_pipe_begin(&sd);
    do {
        ret = splice_from_pipe_next(pipe, &sd);
        if (ret <= 0)
            break;

        mutex_lock_nested(&inode->i_mutex, I_MUTEX_CHILD);
        ret = file_remove_suid(out);
        if (!ret)
            ret = splice_from_pipe_feed(pipe, &sd, pipe_to_file);
        mutex_unlock(&inode->i_mutex);
    } while (ret > 0);
    splice_from_pipe_end(pipe, &sd);

    pipe_unlock(pipe);

    if (sd.num_spliced)
        ret = sd.num_spliced;

    if (ret > 0) {
        unsigned long nr_pages;

        *ppos += ret;
        nr_pages = (ret + PAGE_CACHE_SIZE - 1) >> PAGE_CACHE_SHIFT;

		/*
		 * If file or inode is SYNC and we actually wrote some data,
		 * sync it.
		 */
        if (unlikely((out->f_flags & O_SYNC) || IS_SYNC(inode))) {
            int err;

            mutex_lock(&inode->i_mutex);
            err = generic_osync_inode(inode, mapping,
                                      OSYNC_METADATA|OSYNC_DATA);
            mutex_unlock(&inode->i_mutex);

            if (err)
                ret = err;
        }
        balance_dirty_pages_ratelimited_nr(mapping, nr_pages);
    }

out:
    return ret;
}

const struct file_operations svfs_file_operations = {
    .llseek = svfs_file_llseek,
    .open = generic_file_open,
    .aio_read = svfs_file_aio_read,
    .aio_write = svfs_file_aio_write,
    .mmap = svfs_file_mmap,
    .fsync = svfs_sync_file,
    .splice_read = svfs_file_splice_read,
    .splice_write = svfs_file_splice_write,
};

const struct inode_operations svfs_file_inode_operations = {
    /* FIXME */
    .truncate = svfs_truncate,
    .setattr = NULL,
    .getattr = NULL,
};

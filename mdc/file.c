/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-23 10:18:08 macan>
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
    if (SVFS_I(inode)->state & SVFS_STATE_CONN)
        goto out;

    err = -ENOMEM;
    ref_path = __getname();
    if (!ref_path)
        goto out;

    err = -EINVAL;
    sd = svfs_datastore_get(SVFS_I(inode)->llfs_md.llfs_type);
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

    svfs_debug(mdc, "entering svfs_file_aio_write, pos %lu, check 0x%x\n",
               (unsigned long)pos,
               (si->state & SVFS_STATE_CONN));
    if (!(si->state & SVFS_STATE_CONN)) {
        /* open it? */
        ret = llfs_lookup(inode);
        if (ret)
            goto out;
    }

    BUG_ON(iocb->ki_pos != pos);
    ASSERT(llfs_filp->f_dentry);
    ASSERT(llfs_filp->f_dentry->d_inode);
    
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
        svfs_info(mdc, "update with pos %lu count %ld, "
                  "original i_size %lu\n",
                  (unsigned long)pos, ret, 
                  (unsigned long)inode->i_size);
        i_size_write(inode, pos + ret);
        mark_inode_dirty(inode);
    }
out:
    return ret;
}

const struct file_operations svfs_file_operations = {
    /* FIXME */
    .llseek = generic_file_llseek,
    .open = generic_file_open,
    .aio_read = svfs_file_aio_read,
    .aio_write = svfs_file_aio_write,
    .splice_read = generic_file_splice_read,
    .splice_write = generic_file_splice_write,
};

const struct inode_operations svfs_file_inode_operations = {
    /* FIXME */
    .truncate = svfs_truncate,
    .setattr = NULL,
    .getattr = NULL,
};

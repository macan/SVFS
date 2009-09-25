/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-07-22 17:21:52 macan>
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

/* Adding one dentry in the dir, filesystem special opeartions */
static int svfs_add_entry(struct dentry *dentry, struct inode *inode)
{
    struct inode *dir = dentry->d_parent->d_inode;
    struct super_block *sb = dir->i_sb;
    struct svfs_inode *si = SVFS_I(inode);
    struct svfs_datastore *sd;
    struct file *llfs_file;
    char *ref_path;
    int retval;
    
#ifdef SVFS_LOCAL_TEST
    /* setting up the relationship */
    retval = svfs_backing_store_update_bse(SVFS_SB(sb), dentry, inode);
    if (retval)
        return retval;
#endif

    ASSERT(si->state & SVFS_STATE_NEW);
    /* TODO: alloc the llfs inode? */
    if (S_ISDIR(inode->i_mode))
        goto out;
    if (si->state & SVFS_STATE_CONN)
        goto out;
    /* OK, should we do delay allocation here? */
    if (si->flags & SVFS_IF_DA) {
        svfs_debug(mdc, "do delay allocation '%s' here\n", 
                   dentry->d_name.name);
        si->state |= SVFS_STATE_DA;
        goto out;
    }
    
    sd = svfs_datastore_get(LLFS_TYPE_ANY, 0);
    if (!sd) {
        retval = PTR_ERR(sd);
        goto out_dsget;
    }
    si->llfs_md.llfs_type = sd->type;
    si->llfs_md.llfs_fsid = svfs_datastore_fsid(sd->pathname);
    retval = -ENOMEM;
    ref_path = __getname();
    if (!ref_path)
        goto out_dsget;
    retval = svfs_backing_store_get_path2(SVFS_SB(sb), 
                                          SVFS_SB(sb)->bse + inode->i_ino,
                                          si->llfs_md.llfs_pathname,
                                          NAME_MAX - 1);
    if (retval)
        goto out_putname;
    snprintf(ref_path, PATH_MAX - 1, "%s%s", sd->pathname,
             si->llfs_md.llfs_pathname);
    svfs_debug(mdc, "New LLFS path %s, state 0x%x\n", ref_path, si->state);
    llfs_file = filp_open(ref_path, O_RDWR | O_CREAT, 
                          S_IRUGO | S_IWUSR);
    retval = PTR_ERR(llfs_file);
    if (IS_ERR(llfs_file))
        goto out_putname;
    si->llfs_md.llfs_filp = llfs_file;
    si->state |= SVFS_STATE_CONN;
    retval = 0;

out_putname:
    __putname(ref_path);
out_dsget:
out:
    return retval;
}

static int svfs_delete_entry(struct inode *dir ,struct dentry *dentry)
{
    struct super_block *sb = dir->i_sb;
    struct inode *inode = dentry->d_inode;
    int ret;

#ifdef SVFS_LOCAL_TEST
    ret = svfs_backing_store_delete(SVFS_SB(sb), dir->i_ino, inode->i_ino,
                                    dentry->d_name.name);
    if (ret) {
        svfs_debug(mdc, "delete entry '%s' failed with %d\n",
                   dentry->d_name.name, ret);
    }
#endif
    return ret;
}

static int svfs_add_nondir(struct dentry *dentry, struct inode *inode)
{
    int err = svfs_add_entry(dentry, inode);
    if (!err) {
        svfs_mark_inode_dirty(inode);
        d_instantiate(dentry, inode);
        return 0;
    }
    drop_nlink(inode);
    iput(inode);
    return err;
    
}

static int svfs_create(struct inode *dir, struct dentry *dentry, int mode,
                       struct nameidata *nd)
{
    struct inode *inode;
    int err;

    svfs_entry(mdc, "begin create a new entry %s\n",
               dentry->d_name.name);
    /* TODO: redirect the request to ext4 file system */
    inode = svfs_new_inode(dir, mode);
    err = PTR_ERR(inode);
    if (!IS_ERR(inode)) {
        inode->i_op = &svfs_file_inode_operations;
        inode->i_fop = &svfs_file_operations;
        svfs_set_aops(inode);
        err = svfs_add_nondir(dentry, inode);
    }

    svfs_debug(mdc, "create a new entry %s(%ld) in %ld with %d\n",
               dentry->d_name.name, inode->i_ino, dir->i_ino, err);
    svfs_debug(mdc, "mode 0x%x, flags 0x%x\n", 
               inode->i_mode, inode->i_flags);
    return err;
}

unsigned long svfs_find_entry(struct dentry *dentry)
{
    struct super_block *sb;
    struct inode *dir = dentry->d_parent->d_inode;
    unsigned long ino = -1UL;

    sb = dir->i_sb;
    if (IS_SVFS_VERBOSE(mdc))
        dump_stack();
#ifdef SVFS_LOCAL_TEST
    ino = svfs_backing_store_lookup(SVFS_SB(sb), dir->i_ino, 
                                    dentry->d_name.name);
    if (ino == -1UL) {
        svfs_debug(mdc, "svfs_find_dentry can not find the entry %s\n",
                     dentry->d_name.name);
        goto out;
    } else {
        svfs_debug(mdc, "svfs_find_dentry find the entry %s @ %ld\n",
                   dentry->d_name.name, ino);
    }
#endif
out:
    return ino;
}

static struct dentry *svfs_lookup(struct inode *dir, struct dentry *dentry,
                                  struct nameidata *nd)
{
    struct inode *inode;
    struct dentry *retval, *llfs_dentry;
    unsigned long ino;
    char *ref_path;
    struct svfs_datastore *sd;
    const struct cred *cred = current_cred();
    struct nameidata llfs_nd;
    int err;

    if (dentry->d_name.len > SVFS_NAME_LEN)
        return ERR_PTR(-ENAMETOOLONG);
    
    ref_path = __getname();
    if (!ref_path)
        return ERR_PTR(-ENOMEM);
    /* Step 1: find the svfs inode */
    ino = svfs_find_entry(dentry);
    if (ino == -1UL)
        return NULL;
    /* Step 2: get the svfs inode */
    inode = NULL;
    inode = svfs_iget(dir->i_sb, ino);
    if (IS_ERR(inode))
        return ERR_CAST(inode);
    /* Step 3: d_add(inode, dentry) */
    retval = d_splice_alias(inode, dentry);
    if (IS_ERR(retval))
        goto out;

    /* Step 4: lookup the llfs inode */
    /* if it is a dir, then no need to propagate to llfs */
    if (S_ISDIR(inode->i_mode))
        goto out;
    if (SVFS_I(inode)->state & SVFS_STATE_DA)
        goto out;
    if (SVFS_I(inode)->state & SVFS_STATE_CONN)
        goto out;

    retval = ERR_PTR(-EINVAL);
    sd = svfs_datastore_get(SVFS_I(inode)->llfs_md.llfs_type,
                            SVFS_I(inode)->llfs_md.llfs_fsid);
    if (!sd)
        goto out;
    sprintf(ref_path, "%s%s", sd->pathname, 
            SVFS_I(inode)->llfs_md.llfs_pathname);
    
    svfs_debug(mdc, "fully ref_path %s\n", ref_path);
    
    err = path_lookup(ref_path, LOOKUP_FOLLOW, &llfs_nd);
    if (err)
        goto fail_lookup;
    /* we get the ref dentry, get the filp next */
    SVFS_I(inode)->llfs_md.llfs_filp = dentry_open(llfs_nd.path.dentry,
                                                   llfs_nd.path.mnt,
                                                   O_RDWR, cred);
    if (IS_ERR(SVFS_I(inode)->llfs_md.llfs_filp))
        goto out_put_path;
    
    llfs_dentry = svfs_relay(lookup, 
                             SVFS_I(inode)->llfs_md.llfs_type, inode);
    if (IS_ERR(llfs_dentry)) {
        retval = llfs_dentry;
        SVFS_I(inode)->state |= SVFS_STATE_DISC;
        svfs_err(mdc, "svfs_relay 'lookup' failed %ld\n", 
                 PTR_ERR(llfs_dentry));
        goto out_put_filp;
    }
    SVFS_I(inode)->state |= SVFS_STATE_CONN;
    retval = NULL;

out:
    __putname(ref_path);
    return retval;
out_put_path:
    /* path_put(&llfs_nd.path); */
    {
        svfs_debug(mdc, "llfs_nd.path->dentry %d, vfsmount %d\n",
                   atomic_read(&llfs_nd.path.dentry->d_count),
                   atomic_read(&llfs_nd.path.mnt->mnt_count));
    }
    goto out;
out_put_filp:
    fput(SVFS_I(inode)->llfs_md.llfs_filp);
    goto out;
fail_lookup:
    retval = ERR_PTR(err);
    goto out;
}

struct dentry *svfs_get_parent(struct dentry *child)
{
    struct inode *inode;
    unsigned long ino;
    
#ifdef SVFS_LOCAL_TEST
    ino = svfs_backing_store_lookup_parent(SVFS_SB(child->d_inode->i_sb), 
                                           child->d_inode->i_ino);
    if (ino == -1UL) {
        svfs_err(mdc, "get parent failed! What happened?\n");
        return ERR_PTR(-ENOENT);
    }
#endif
    inode = svfs_iget(child->d_inode->i_sb, ino);
    if (IS_ERR(inode))
        return ERR_CAST(inode);

    return d_obtain_alias(inode);
}

static int svfs_link(struct dentry *old_dentry, struct inode *dir,
                     struct dentry *dentry)
{
    svfs_entry(mdc, "link not implemented.\n");
    return -ENOSYS;
}

static int svfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    struct file *llfs_filp;
    int ret = 0;
    
    svfs_entry(mdc, "unlink the LLFS dentry first\n");
    /* first, we should relay the unlink to LLFS now */
    if (S_ISDIR(inode->i_mode))
        goto bypass;
    if (SVFS_I(inode)->state & SVFS_STATE_DA) {
        goto bypass;
    }
    if (!(SVFS_I(inode)->state & SVFS_STATE_CONN)) {
        /* open it? */
        ret = llfs_lookup(inode);
        if (ret)
            goto out;
    }
    llfs_filp = SVFS_I(inode)->llfs_md.llfs_filp;
    /* do path get here? */
    svfs_debug(mdc, "1 dentry->d_count %d, inode->i_count %d\n",
               atomic_read(&llfs_filp->f_dentry->d_count),
               atomic_read(&llfs_filp->f_dentry->d_inode->i_count));
    dget(llfs_filp->f_dentry);
    mutex_lock(&llfs_filp->f_dentry->d_parent->d_inode->i_mutex);
    atomic_inc(&llfs_filp->f_dentry->d_inode->i_count);
    ret = mnt_want_write(llfs_filp->f_vfsmnt);
    if (ret)
        goto out_free;

    ret = vfs_unlink(llfs_filp->f_dentry->d_parent->d_inode,
                     llfs_filp->f_dentry);
    mnt_drop_write(llfs_filp->f_vfsmnt);
out_free:
    mutex_unlock(&llfs_filp->f_dentry->d_parent->d_inode->i_mutex);
    dput(llfs_filp->f_dentry);
    iput(llfs_filp->f_dentry->d_inode);
    svfs_debug(mdc, "2 dentry->d_count %d, inode->i_count %d, ret %d\n",
               atomic_read(&llfs_filp->f_dentry->d_count),
               atomic_read(&llfs_filp->f_dentry->d_inode->i_count), ret);
    if (ret)
        goto out;

bypass:
    ret = -ENOENT;
    ASSERT(inode);
    if (!inode->i_nlink) {
        svfs_warning(mdc, "svfs_unlink deleting nonexistent file "
                     "(%lu), %d\n", inode->i_ino, inode->i_nlink);
        inode->i_nlink = 1;
    }

    /* delete the entry */
    ret = svfs_delete_entry(dir, dentry);
    if (ret)
        goto out;

    dir->i_ctime = dir->i_mtime = CURRENT_TIME;
    svfs_mark_inode_dirty(dir);
    drop_nlink(inode);
    inode->i_ctime = CURRENT_TIME;
    svfs_mark_inode_dirty(inode);
    
    ret = 0;
    svfs_debug(mdc, "unlink ino %ld %s in %ld.\n", inode->i_ino,
               dentry->d_name.name, dir->i_ino);
out:
    return ret;
}

static int svfs_symlink(struct inode *dir, struct dentry *dentry, 
                        const char *symname)
{
    struct inode *inode;
    int l, err;

    l = strlen(symname) + 1;
    if (l > dir->i_sb->s_blocksize)
        return -ENAMETOOLONG;

    inode = svfs_new_inode(dir, S_IFLNK | S_IRWXUGO);
    err = PTR_ERR(inode);
    if (IS_ERR(inode))
        goto out_stop;

#ifdef SVFS_LOCAL_TEST
    {
        struct super_block *sb = dir->i_sb;
        struct backing_store_entry *bse = SVFS_SB(sb)->bse + inode->i_ino;
        
        if (l > sizeof(bse->ref_path)) {
            /* could no filling in the metadata store */
            err = -ENAMETOOLONG;
            goto out_stop;
        } else {
            inode->i_op = &svfs_fast_symlink_inode_operations;
            memcpy((char *)&(bse->ref_path), symname, l);
            inode->i_size = l - 1;
        }
    }
#endif
    SVFS_I(inode)->disksize = inode->i_size;
    err = svfs_add_nondir(dentry, inode);
out_stop:
    return err;
}

static int svfs_mkdir(struct inode *dir, struct dentry *dentry,
                      int mode)
{
    struct inode *inode;
    int err;

    inode = svfs_new_inode(dir, mode | S_IFDIR);
    err = PTR_ERR(inode);
    if (IS_ERR(inode))
        goto out;
    inode->i_op = &svfs_dir_inode_operations;
    inode->i_fop = &svfs_dir_operations;
    inode->i_nlink = 2;
    inode->i_size = 0;          /* default value: 0? */
    err = svfs_add_entry(dentry, inode);
    if (!err) {
        svfs_mark_inode_dirty(inode);
        d_instantiate(dentry, inode);
        inc_nlink(dir);
        svfs_mark_inode_dirty(dir);
    } else {
        clear_nlink(inode);
        iput(inode);
    }

    svfs_debug(mdc, "mkdir a new entry %s(%ld) in %ld with %d\n",
               dentry->d_name.name, inode->i_ino, dir->i_ino, err);
    
out:
    return err;
}

static int empty_dir(struct inode *inode)
{
    int ret = 0;
    
    /* find if there are any dentrys in this inode */
#ifdef SVFS_LOCAL_TEST
    unsigned long ino;
    ino = svfs_backing_store_find_child(SVFS_SB(inode->i_sb), inode->i_ino,
                                        0);
    if (ino == -1ULL)
        ret = 1;
    else 
        ret = 0;
#endif
    return ret;
}

static int svfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode;
    unsigned long ino = -1ULL;
    int ret;

    ret = -ENOENT;
    ino = svfs_find_entry(dentry);
    if (ino == -1ULL) {
        goto end_rmdir;
    }

    inode = dentry->d_inode;
    ret = -ENOTEMPTY;
    if (!empty_dir(inode))
        goto end_rmdir;

    ret = svfs_delete_entry(dir, dentry);
    if (ret)
        goto end_rmdir;
    
    if (!(inode->i_nlink == 1 || inode->i_nlink == 2)) {
        svfs_warning(mdc, "svfs_rmdir empty dir has too many links "
                     "(%lu), %d\n", inode->i_ino, inode->i_nlink);
    }
    inode->i_version++;
    clear_nlink(inode);
    inode->i_size = 0;
    inode->i_ctime = dir->i_ctime = dir->i_mtime = CURRENT_TIME;
    svfs_mark_inode_dirty(inode);
    drop_nlink(dir);
    svfs_mark_inode_dirty(dir);
    ret = 0;
    
end_rmdir:
    return ret;
}

static int svfs_mknod(struct inode *dir, struct dentry *dentry,
                      int mode, dev_t rdev)
{
    svfs_entry(mdc, "mknod not implemented.\n");
    return -ENOSYS;
}

static int svfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                       struct inode *new_dir, struct dentry *new_dentry)
{
    svfs_entry(mdc, "rename not implemented.\n");
    return -ENOSYS;
}

static int svfs_setattr(struct dentry *dentry, struct iattr *attr)
{
    struct inode *inode = dentry->d_inode;
    const unsigned int ia_valid = attr->ia_valid;
    int err, rc;

    err = inode_change_ok(inode, attr);
    if (err)
        return err;

    if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
        (ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid)) {
        /* quota transfer? */
        if (attr->ia_valid & ATTR_UID)
            inode->i_uid = attr->ia_uid;
        if (attr->ia_valid & ATTR_GID)
            inode->i_gid = attr->ia_gid;
        err = svfs_mark_inode_dirty(inode);
    }
    if (S_ISREG(inode->i_mode) &&
        (attr->ia_valid & ATTR_SIZE) && attr->ia_size < inode->i_size) {
        SVFS_I(inode)->disksize = attr->ia_size;
        rc = svfs_mark_inode_dirty(inode);
        if (!err)
            err = rc;
        /* begin truncate the llfs inode now? what should I do */
    }
    rc = inode_setattr(inode, attr);
    if (!err)
        err = rc;
    
    return err;
}

const struct inode_operations svfs_dir_inode_operations = {
    .create = svfs_create,
    .lookup = svfs_lookup,
    .link = svfs_link,
    .unlink = svfs_unlink,
    .symlink = svfs_symlink,
    .mkdir = svfs_mkdir,
    .rmdir = svfs_rmdir,
    .mknod = svfs_mknod,
    .rename = svfs_rename,
    .setattr = svfs_setattr,
};


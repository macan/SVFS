/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-30 14:18:57 macan>
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
#include "svfs_dep.h"

struct list_head svfs_datastore_list;
static int svfs_datastore_count = 0;

void svfs_datastore_init()
{
    INIT_LIST_HEAD(&svfs_datastore_list);
}

int svfs_datastore_adding(char *conf_filename)
{
    char line[256];
    char type[12], pathname[128];
    struct svfs_datastore *sd;
    int ret, flag = 1, rc = -EINVAL;

    memset(line, 0, 256);
    ret = svfs_lib_config_open(conf_filename);
    if (ret) {
        svfs_err(dstore, "open config file '%s' err %d\n", 
                 conf_filename, ret);
        return ret;
    }

    while (1) {
        ret = svfs_lib_get_value("datastore", "fstype", line, flag);
        if (ret) {
            break;
        }
        flag = 0;
        ret = svfs_lib_k2v(line, "fstype", type);
        if (ret < 0)
            svfs_err(dstore, "k2v failed %d\n", ret);
        else
            svfs_err(dstore, "Got fstype value: %s\n", type);
        
        ret = svfs_lib_k2v(line, "mountpoint", pathname);
        if (ret < 0)
            svfs_err(dstore, "k2v failed %d\n", ret);
        else
            svfs_err(dstore, "Got mountpoint value: %s\n", pathname);
        
        sd = svfs_datastore_add_new(svfs_type_revert(type), pathname);
        rc = PTR_ERR(sd);
        if (IS_ERR(sd))
            goto out;
        rc = 0;
    }
    out:
    svfs_lib_config_close();
    return rc;
}

/*
 * This function hash the input string 'name' using the ELF hash
 * function for strings.
 */

u32 svfs_datastore_fsid(char *name)
{
    __u32 h = 0;
	__u32 g;

	while(*name) {
		h = (h<<4) + *name++;
		if ((g = (h & 0xf0000000)))
			h ^=g>>24;
		h &=~g;
	}
    
	return h;
}

struct svfs_datastore *svfs_datastore_add_new(int type, char *pathname)
{
    struct file_system_type *fstype;
    struct svfs_datastore *sd;
    struct super_block *sb, *pos;
    struct nameidata nd;
    int err, found = 0;

    svfs_info(dstore, "add new type %s\n", svfs_type_convert(type));
    /* pre-checking */
    if (type == LLFS_TYPE_ERR)
        return ERR_PTR(-EINVAL);
    
    /* Step 1.1: lookup the pathname? */
    err = path_lookup(pathname, LOOKUP_FOLLOW, &nd);
    if (err)
        goto fail_lookup;
    /* Step 1.2: get the super block */
    sb = nd.path.mnt->mnt_sb;
    err = -ENODEV;
    fstype = get_fs_type(svfs_type_convert(type));
    if (!fstype) {
        goto fail_getfstype;
    }
    list_for_each_entry(pos, &fstype->fs_supers, s_instances) {
        if (sb == pos) {
            found = 1;
            break;
        }
    }
    svfs_info(dstore, "found %d\n", found);
    err = -EINVAL;
    if (!found) {
        svfs_info(mdc, "No '%s' filesystem found @ %s\n",
                  svfs_type_convert(type), pathname);
        goto fail_drop;
    }
    
    /* Step 2: alloc and init the datastore */

    err = -ENOMEM;
    sd = kmalloc(sizeof(struct svfs_datastore), GFP_KERNEL);
    if (!sd)
        goto fail_drop;

    list_add_tail(&sd->list, &svfs_datastore_list);
    sd->type = type;
    strncpy(sd->pathname, pathname, NAME_MAX - 1);
    sd->state = SVFS_DSTORE_VALID;
    sd->root_path = nd.path;
    sd->sb = sb;

    svfs_info(dstore, "init the dstore: type %s, pathname %s, sb %p\n",
              svfs_type_convert(type), sd->pathname, sd->sb);
    svfs_datastore_count++;
    return 0;

fail_drop:
    module_put(fstype->owner);
fail_getfstype:
    path_put(&nd.path);
fail_lookup:
    return ERR_PTR(err);
}

struct svfs_datastore *svfs_datastore_get(int type, u32 fsid)
{
    struct svfs_datastore *pos;
    int select = 0, cur = 0;

    if (!svfs_datastore_count)
        return NULL;

    if (type & LLFS_TYPE_ANY)
        select = random32() % svfs_datastore_count;
    
    list_for_each_entry(pos, &svfs_datastore_list, list) {
        if ((type == pos->type && 
             fsid == svfs_datastore_fsid(pos->pathname))|| 
            ((type & LLFS_TYPE_ANY) && select == cur))
            return pos;
        cur++;
    }
    return NULL;
}

void svfs_datastore_statfs(struct kstatfs *buf)
{
    struct kstatfs st;
    struct svfs_datastore *pos;
    int ret = 0;

    /* init it first */
    buf->f_blocks = buf->f_bfree = buf->f_bavail = 0;

    list_for_each_entry(pos, &svfs_datastore_list, list) {
        ret = vfs_statfs(pos->root_path.dentry, &st);
        if (!ret) {
            buf->f_blocks += st.f_blocks;
            buf->f_bfree += st.f_bfree;
            buf->f_bavail += st.f_bavail;
        }
    }
}

void svfs_datastore_free(struct svfs_datastore *sd)
{
    list_del(&sd->list);
    /* FIXME: free it */
    if (sd->state & SVFS_DSTORE_VALID)
        path_put(&sd->root_path);
    svfs_debug(dstore, "d_count %d, mnt_count %d\n",
              atomic_read(&sd->root_path.dentry->d_count),
              atomic_read(&sd->root_path.mnt->mnt_count));
    kfree(sd);
    svfs_datastore_count--;
}

void svfs_datastore_exit()
{
    struct list_head *pos, *n;
    list_for_each_safe(pos, n, &svfs_datastore_list) {
        struct svfs_datastore *sd = list_entry(pos, 
                                               struct svfs_datastore,
                                               list);
        svfs_info(dstore, "freeing datastore object: %s\n",
                  sd->pathname);
        svfs_datastore_free(sd);
    }
}

        

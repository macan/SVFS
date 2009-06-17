/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-17 11:35:43 macan>
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

struct list_head svfs_datastore_list;

void svfs_datastore_init()
{
    INIT_LIST_HEAD(&svfs_datastore_list);
}


struct svfs_datastore *svfs_datastore_add_new(int type, char *pathname)
{
    struct svfs_datastore *sd;
    struct nameidata nd;
    char *dpath, *p;
    int err;

    svfs_info(dstore, "add new type %s\n", svfs_type_convert(type));
    /* Step 1: checking */
    /* lookup the pathname? */
    err = path_lookup(pathname, LOOKUP_FOLLOW, &nd);
    if (err)
        goto fail_lookup;
    err = -ENOMEM;
    dpath = __getname();
    if (!dpath)
        goto fail_lookup;
    p = d_path(&nd.path, dpath, PATH_MAX);
    svfs_info(dstore, "%s: mountpoint %s, mnt_root %s, path %s\n", 
              pathname, nd.path.mnt->mnt_mountpoint->d_name.name,
              nd.path.mnt->mnt_root->d_name.name,
              p);
    __putname(dpath);

    /* Step 2: alloc and init the datastore */

    err = -ENOMEM;
    sd = kmalloc(sizeof(struct svfs_datastore), GFP_KERNEL);
    if (!sd)
        goto fail_release;

    list_add_tail(&sd->list, &svfs_datastore_list);
    sd->type = type;
    strncpy(sd->pathname, pathname, NAME_MAX - 1);
    sd->root_path = nd.path;
    
    return sd;

fail_release:
    path_put(&nd.path);
fail_lookup:
    return ERR_PTR(err);
}

void svfs_datastore_free(struct svfs_datastore *sd)
{
    list_del(&sd->list);
    /* FIXME: free it */
    path_put(&sd->root_path);
    svfs_info(dstore, "d_count %d, mnt_count %d\n",
              atomic_read(&sd->root_path.dentry->d_count),
              atomic_read(&sd->root_path.mnt->mnt_count));
    kfree(sd);
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

        

/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-18 14:45:09 macan>
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

#define svfs_get_iops(type, target) ({                      \
            struct inode_operations *iops = NULL;           \
            switch (type) {                                 \
            case LLFS_TYPE_EXT4:                            \
                iops = ext4_##target##_inode_operations;    \
                break;                                      \
            case LLFS_TYPE_EXT3:                            \
                iops = ext3_##target##_inode_operations;    \
                break;                                      \
            default:                                        \
                ;                                           \
            }                                               \
            iops;                                           \
        })

#define svfs_get_fops(type, target) ({                      \
            struct file_operations *fops = NULL;            \
            switch (type) {                                 \
            case LLFS_TYPE_EXT4:                            \
                iops = ext4_##target##_operations;          \
                break;                                      \
            case LLFS_TYPE_EXT3:                            \
                iops = ext3_##target##_operations;          \
                break;                                      \
            default:                                        \
                ;                                           \
            }                                               \
            fops;                                           \
        })

#define svfs_get_ops(type, ops_type, target) ({                         \
            void *ops;                                                  \
            if (!strcmp("inode_operations", #ops_type)) {               \
                ops = svfs_get_iops(type, target);                      \
            } else if (!strcmp("file_operations", #ops_type)) {         \
                ops = svfs_get_iops(type, target);                      \
            }                                                           \
            ops;                                                        \
        })

struct svfs_datastore *svfs_datastore_add_new(int type, char *pathname)
{
    struct file_system_type *fstype;
    struct svfs_datastore *sd;
    struct super_block *sb, *pos;
    struct nameidata nd;
    int err, found = 0;

    svfs_info(dstore, "add new type %s\n", svfs_type_convert(type));
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
    if (!found)
        goto fail_drop;
    
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

struct svfs_datastore *svfs_datastore_get(int type)
{
    struct svfs_datastore *pos;
    int select, cur = 0;

    if (type & LLFS_TYPE_ANY)
        select = random32() % svfs_datastore_count;
    
    list_for_each_entry(pos, &svfs_datastore_list, list) {
        if (type == pos->type || 
            ((type & LLFS_TYPE_ANY) && select == cur))
            return pos;
        cur++;
    }
    return NULL;
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

        

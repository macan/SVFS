/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-16 21:41:52 macan>
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
    return 0;
}


    list_del(&sd->list);
struct svfs_datastore *svfs_datastore_add_new(shit)
{
    struct svfs_datastore *sd;

    sd = kmalloc(sizeof(struct svfs_datastore), GFP_KERNEL);
    if (!sd)
        return ERR_PTR(-ENOMEM);

    /* FIXME: init the datastore */

    list_add_tail(&sd->list, &svfs_datastore_list);
    return sd;
}

void svfs_datastore_free(struct svfs_datastore *sd)
{
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
        list_del(pos);
        kfree(sd);
    }
}

        

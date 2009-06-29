/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
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

static LIST_HEAD(svfs_tf_list);

void svfs_lib_tracing_exit(void)
{
    struct tracing_flag *pos, *n;
    
    list_for_each_entry_safe(pos, n, &svfs_tf_list, list) {
        svfs_info(lib, "freeing tracing flag: %s\n", pos->name);
        list_del(&pos->list);
        kfree(pos);
    }
}

int svfs_lib_tracing_add(char *name, unsigned int *loc)
{
    struct tracing_flag *tf;

    tf = kmalloc(sizeof(struct tracing_flag), GFP_KERNEL);
    if (!tf)
        return -ENOMEM;
    strncpy(tf->name, name, 63);
    tf->loc = loc;
    list_add_tail(&tf->list, &svfs_tf_list);

    return 0;
}


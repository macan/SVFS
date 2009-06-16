/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-16 19:04:57 macan>
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

#define svfs_relay(name, inode) ({                                   \
            void *retval;                                            \
            retval = svfs_relay_##name(SVFS_I(inode)->               \
                                       llfs_md.llfs_type, inode);    \
            retval;                                                  \
        })

/**
 * lookup:
 *
 * find the llfs dentry, return NULL if succeed
 */
struct dentry *svfs_relay_lookup(u32 type, struct inode *inode)

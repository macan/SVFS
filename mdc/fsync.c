/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-27 13:55:00 macan>
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

int svfs_sync_file(struct file *file, struct dentry *dentry, int datasync)
{
    struct inode *inode = dentry->d_inode;
    int ret = 0;
    
    svfs_entry(mdc, "sync inode %ld, datasync %d\n", inode->i_ino,
               datasync);

    if (datasync && !(inode->i_state & I_DIRTY_DATASYNC))
		goto out;

    if (inode->i_state & (I_DIRTY_SYNC | I_DIRTY_DATASYNC)) {
		struct writeback_control wbc = {
			.sync_mode = WB_SYNC_ALL,
			.nr_to_write = 0, /* sys_fsync did this */
		};
		ret = sync_inode(inode, &wbc);
	}
out:
    return ret;
}

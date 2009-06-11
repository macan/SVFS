/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-10 15:09:35 macan>
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

static int svfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
    return -ENOTSUPP;
}

static int svfs_release_dir(struct inode *inode, struct file *filp)
{
    /* TODO: should we notify MDS? */
    return 0;
}

const struct file_operations svfs_dir_operations = {
    .llseek = generic_file_llseek,
    .read = generic_read_dir,
    .readdir = svfs_readdir,
    .fsync = svfs_sync_file,
    .release = svfs_release_dir,
};


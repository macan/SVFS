/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * klagent supply the interface between BLCR and LAGENT(user space)
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

#ifndef __SVFS_DEP_H__
#define __SVFS_DEP_H__

/* ext4 depends */
/* dir.c */
extern const struct file_operations ext4_dir_operations;

/* file.c */
extern const struct inode_operations ext4_file_inode_operations;
extern const struct file_operations ext4_file_operations;

/* namei.c */
extern const struct inode_operations ext4_dir_inode_operations;
extern const struct inode_operations ext4_special_inode_operations;

/* symlink.c */
extern const struct inode_operations ext4_symlink_inode_operations;
extern const struct inode_operations ext4_fast_symlink_inode_operations;

/* ext3 depends */
/* dir.c */
extern const struct file_operations ext3_dir_operations;

/* file.c */
extern const struct inode_operations ext3_file_inode_operations;
extern const struct file_operations ext3_file_operations;

/* namei.c */
extern const struct inode_operations ext3_dir_inode_operations;
extern const struct inode_operations ext3_special_inode_operations;

/* symlink.c */
extern const struct inode_operations ext3_symlink_inode_operations;
extern const struct inode_operations ext3_fast_symlink_inode_operations;

#endif

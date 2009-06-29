/**
 * Copyright (c) 2009 Ma Can <ml.macana@gmail.com>
 *                           <macan@ncic.ac.cn>
 *
 * Time-stamp: <2009-06-29 15:27:20 macan>
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
#include "lib.h"

static struct proc_dir_entry *svfs_root = NULL;

struct proc_dir_entry *svfs_lib_proc_init(void)
{
    /* create /proc/fs/svfs */
    svfs_root = proc_mkdir("fs/svfs", NULL);
    if (!svfs_root)
        svfs_err(lib, "svfs: Unable to create fs/svfs\n");
    return svfs_root;
}

void svfs_lib_proc_exit(void)
{
    if (svfs_root)
        remove_proc_entry("fs/svfs", NULL);
}

#define	PROC_HANDLER(pde, name, fops)                               \
    do {                                                            \
        proc = proc_create_data(name, mode, pde,                    \
                                fops, NULL);                        \
        if (proc == NULL) {                                         \
            svfs_err(lib, "svfs: can't to create %s\n", name);      \
            goto err_out;                                           \
        }                                                           \
    } while (0)

int svfs_lib_proc_add_entry(struct proc_dir_entry *dir, char *name, 
                            const struct file_operations *fops)
{
    struct proc_dir_entry *proc;
    mode_t mode = S_IFREG | S_IRUGO | S_IWUSR;

    /* fallback to root entry */
    if (!dir)
        dir = svfs_root;

    PROC_HANDLER(dir, name, fops);
    return 0;
err_out:
    svfs_err(lib, "svfs: Unable to create %s\n", name);
    remove_proc_entry(name, dir);
    return -ENOMEM;
}

void svfs_lib_proc_remove_entry(struct proc_dir_entry *dir, char *name)
{
    if (!dir)
        dir = svfs_root;
    remove_proc_entry(name, dir);
}

/* using svfs_lib_proc_remove_entry to remove the DIR */
struct proc_dir_entry *svfs_lib_proc_add_dir(struct proc_dir_entry *dir, 
                                             char *name)
{
    struct proc_dir_entry *new_dir;
    
    if (!dir)
        dir = svfs_root;

    new_dir = proc_mkdir(name, dir);
    if (!new_dir) {
        svfs_err(lib, "svfs: add dir %s failed\n", name);
        return ERR_PTR(-ENOMEM);
    }
    return new_dir;
}

void svfs_lib_proc_test(void)
{
    struct proc_dir_entry *proc;
    
    proc = svfs_lib_proc_init();
    if (!proc) {
        svfs_err(lib, "svfs: init failed\n");
    }
    svfs_lib_proc_exit();
}

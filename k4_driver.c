/*
 * k4_driver.c
 * Universal Loadable Kernel Module — Android Kernel 4.14.86 / ARM64
 *
 * Features:
 *   - /dev/k4drv  : character device (read / write / ioctl)
 *   - /proc/k4_info : module info via proc fs
 *   - dmesg logging on every lifecycle event
 *
 * LICENSE: GPL v2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ioctl.h>
#include <linux/utsname.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("K4-Universal-Driver");
MODULE_DESCRIPTION("Universal LKM skeleton for Android Kernel 4.14.x");
MODULE_VERSION("1.0.0");

#define DEVICE_NAME     "k4drv"
#define PROC_ENTRY_NAME "k4_info"
#define BUF_SIZE        256

#define K4_MAGIC          'K'
#define K4_IOCTL_PING     _IO(K4_MAGIC,  0)
#define K4_IOCTL_KVER     _IOR(K4_MAGIC, 1, char[64])

static atomic_t open_count = ATOMIC_INIT(0);
static char     last_write[BUF_SIZE];
static size_t   last_write_len = 0;

/* /proc/k4_info */
static int k4_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "================================\n");
    seq_printf(m, "  K4 Universal Driver  v1.0.0\n");
    seq_printf(m, "================================\n");
    seq_printf(m, "Kernel  : %s\n", utsname()->release);
    seq_printf(m, "Machine : %s\n", utsname()->machine);
    seq_printf(m, "Node    : %s\n", utsname()->nodename);
    seq_printf(m, "Opens   : %d\n", atomic_read(&open_count));
    if (last_write_len > 0)
        seq_printf(m, "LastMsg : %.*s\n", (int)last_write_len, last_write);
    return 0;
}

static int k4_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, k4_proc_show, NULL);
}

static const struct file_operations k4_proc_fops = {
    .owner   = THIS_MODULE,
    .open    = k4_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

/* char device */
static int k4_dev_open(struct inode *inode, struct file *filp)
{
    atomic_inc(&open_count);
    pr_info("[K4DRV] open() pid=%d comm=%s opens=%d\n",
            current->pid, current->comm, atomic_read(&open_count));
    return 0;
}

static int k4_dev_release(struct inode *inode, struct file *filp)
{
    pr_info("[K4DRV] release() pid=%d\n", current->pid);
    return 0;
}

static ssize_t k4_dev_read(struct file *filp, char __user *ubuf,
                            size_t count, loff_t *ppos)
{
    char resp[BUF_SIZE];
    int  len;

    len = snprintf(resp, sizeof(resp),
                   "K4DRV OK | kernel=%s | opens=%d\n",
                   utsname()->release, atomic_read(&open_count));

    if (*ppos >= len) return 0;
    if (count > (size_t)(len - *ppos)) count = len - *ppos;
    if (copy_to_user(ubuf, resp + *ppos, count)) return -EFAULT;
    *ppos += count;
    return count;
}

static ssize_t k4_dev_write(struct file *filp, const char __user *ubuf,
                             size_t count, loff_t *ppos)
{
    size_t to_copy = count < BUF_SIZE - 1 ? count : BUF_SIZE - 1;
    if (copy_from_user(last_write, ubuf, to_copy)) return -EFAULT;
    last_write[to_copy] = '\0';
    last_write_len = to_copy;
    pr_info("[K4DRV] write() pid=%d msg=%s\n", current->pid, last_write);
    return count;
}

static long k4_dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    char kver[64];
    switch (cmd) {
    case K4_IOCTL_PING:
        pr_info("[K4DRV] ioctl PING pid=%d\n", current->pid);
        return 0xDEADBEEF;
    case K4_IOCTL_KVER:
        strlcpy(kver, utsname()->release, sizeof(kver));
        if (copy_to_user((void __user *)arg, kver, sizeof(kver))) return -EFAULT;
        return 0;
    default:
        return -ENOTTY;
    }
}

static const struct file_operations k4_fops = {
    .owner          = THIS_MODULE,
    .open           = k4_dev_open,
    .release        = k4_dev_release,
    .read           = k4_dev_read,
    .write          = k4_dev_write,
    .unlocked_ioctl = k4_dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl   = k4_dev_ioctl,
#endif
};

static struct miscdevice k4_misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = DEVICE_NAME,
    .fops  = &k4_fops,
    .mode  = 0666,
};

static int __init k4_driver_init(void)
{
    int ret;
    struct proc_dir_entry *pde;

    pr_info("[K4DRV] init on kernel %s (%s)\n", utsname()->release, utsname()->machine);

    ret = misc_register(&k4_misc_dev);
    if (ret < 0) { pr_err("[K4DRV] misc_register failed (%d)\n", ret); return ret; }
    pr_info("[K4DRV] /dev/%s created minor=%d\n", DEVICE_NAME, k4_misc_dev.minor);

    pde = proc_create(PROC_ENTRY_NAME, 0444, NULL, &k4_proc_fops);
    if (!pde) pr_warn("[K4DRV] proc_create failed\n");
    else      pr_info("[K4DRV] /proc/%s created\n", PROC_ENTRY_NAME);

    pr_info("[K4DRV] *** DRIVER LOADED SUCCESSFULLY ***\n");
    return 0;
}

static void __exit k4_driver_exit(void)
{
    remove_proc_entry(PROC_ENTRY_NAME, NULL);
    misc_deregister(&k4_misc_dev);
    pr_info("[K4DRV] *** DRIVER UNLOADED ***\n");
}

module_init(k4_driver_init);
module_exit(k4_driver_exit);

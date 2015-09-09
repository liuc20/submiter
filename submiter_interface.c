
#include "submiter.h"


static int submiter_start_test(const char __user *buffer, unsigned long count, 
							   struct block_device *dev)
{
	struct test_bvec *bvec_array = NULL;
	unsigned long lba = 0;
	int ret = 0;
	char *cmd;
	int len;

	cmd = kmalloc(count, GFP_KERNEL);
	if (!cmd) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(cmd, buffer, count)) {
		ret = -EFAULT;
		goto err_out;
	}
	cmd[count - 1] = '\0';

	bvec_array = submiter_generate_bvecs(cmd, &len, &lba);
	if (!bvec_array) {
		ret = -ENOMEM;
		goto err_out;
	}

	ret = write_data_to_dev(dev, data_buffer, len, bvec_array, lba);
	if (ret) {
		goto err_out2;
	}

	ret = read_data_from_dev(dev, data_buffer, len, bvec_array, lba);

err_out2:
	kfree(bvec_array);
err_out:
	kfree(cmd);
out:
	return ret ? ret : count;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)

static int submiter_proc_write(struct file *file, const char __user *buffer, 
							   unsigned long count, void *data)
{
	struct block_device *bdev = (struct block_device *)data;
	return submiter_start_test(buffer, count, bdev);
}

static int submiter_create_proc(struct block_device *dev)
{
	struct proc_dir_entry *entry = NULL;
	int ret = 0;

	submiter_proc = proc_mkdir("submiter", NULL);
	if (!submiter_proc) {
		ret = -ENOMEM;
		goto out;
	}

	entry = create_proc_entry(TEST_INTERFACE, 0644, submiter_proc);
	if (!entry) {
		ret = -ENOMEM;
		goto err_out;
	}

	entry->read_proc = NULL;
	entry->write_proc = submiter_proc_write;
	entry->data = dev;
	return ret;

err_out:
	remove_proc_entry(submiter_proc->name, submiter_proc->parent);
out:
	return ret;
}

void submiter_destroy_proc(struct proc_dir_entry *dir)
{
	remove_proc_entry(TEST_INTERFACE, dir);
	remove_proc_entry(dir->name, dir->parent);
}

#else

static ssize_t submiter_proc_write(struct file *file, 
								  const char __user *buffer, size_t count,
								  loff_t *data)
{
	struct block_device *bdev = 
		(struct block_device *)PDE_DATA(file_inode(file));
	return submiter_start_test(buffer, count, bdev);
}

static int submiter_proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int submiter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, submiter_proc_show, PDE_DATA(inode));
}

static struct file_operations submiter_proc_ops = {
	.owner		= 	THIS_MODULE,
	.open		= 	submiter_proc_open,
	.read		= 	seq_read,
	.write		= 	submiter_proc_write,
	.llseek		= 	seq_lseek,
	.release	= 	single_release,
};

int submiter_create_proc(struct block_device *dev)
{
	struct proc_dir_entry *entry = NULL;
	int ret = 0;

	g_submiter.sbm_proc = proc_mkdir(SUBMITER_PROC_DIR, NULL);
	if (!g_submiter.sbm_proc) {
		ret = -ENOMEM;
		goto err_out0;
	}

	entry = proc_create_data(TEST_INTERFACE, 0644, g_submiter.sbm_proc, 
							 &submiter_proc_ops, dev);
	if (!entry) {
		printk(KERN_ALERT "proc create failed!\n");
		goto err_out1;
	}
	return 0;

err_out1:
	remove_proc_subtree(SUBMITER_PROC_DIR, NULL);
err_out0:
	return ret;
}

void submiter_destroy_proc(struct proc_dir_entry *dir)
{
	remove_proc_subtree(SUBMITER_PROC_DIR, NULL);
}

#endif

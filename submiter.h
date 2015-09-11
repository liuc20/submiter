#ifndef	__SUBMITER_H__
#define __SUBMITER_H__


#include <linux/init.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kmod.h>
#include <linux/version.h>
#include <linux/seq_file.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)

#define pb_bio_start(bio)               ((bio)->bi_sector)
#define pb_bio_iovec(bio)               (bio_iovec(bio))
#define pb_bio_size(bio)                ((bio)->bi_size)
#define pb_bio_iovec_idx(bio, idx)      (bio_iovec_idx(bio, idx))
#define pb_bio_idx(bio)                 (bio->bi_idx)

#else

#define pb_bio_start(bio)               (bio->bi_iter.bi_sector)
#define pb_bio_iovec(bio)               (bio->bi_io_vec)
#define pb_bio_size(bio)                (bio->bi_iter.bi_size)
#define pb_bio_iovec_idx(bio, idx)      (&bio->bi_io_vec[idx])
#define pb_bio_idx(bio)                 (bio->bi_iter.bi_idx)

#endif

#define SUBMITER_PROC_DIR	"submiter"
#define TEST_INTERFACE 		"submiter_entry"
#define	PBLAZE_DISK_PATH	"/dev/memdiska"
#define TEST_PAGE_SIZE		(4096)

#define DEV_FLAG	(FMODE_READ | FMODE_WRITE)

enum {
	BIO_SUBMIT,
	BIO_ENDIO
};

struct test_bvec {
	unsigned offset;
	int length;
};

struct test_context {
	struct block_device *dev;
	char *data_buffer;
	int error;
	unsigned int bio_status;
	wait_queue_head_t endio_queue;		
};

struct submiter {
	struct block_device *sbm_dev;
	struct proc_dir_entry *sbm_proc;
};

MODULE_LICENSE("Dual BSD/GPL");

extern char *data_buffer;
extern struct submiter g_submiter;

/* export functions */

int submiter_create_proc(struct block_device *dev);
void submiter_destroy_proc(struct proc_dir_entry *dir);

int read_data_from_dev(struct block_device *dev, char* data_buffer,
			       int len, struct test_bvec *array, sector_t lba);

int write_data_to_dev(struct block_device *dev, char *data_buffer, 
			      int len, struct test_bvec *array, sector_t lba);

struct test_bvec *submiter_generate_bvecs(char *cmd, int *len, unsigned long *lba);
#endif

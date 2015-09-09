
#include "submiter.h"

char *data_buffer;

struct submiter g_submiter;

int test_lba = 0;
module_param_named(lba, test_lba, int, S_IRUGO | S_IWUSR);

static struct bio *test_create_bio(struct block_device *dev, int len, 
				   struct test_bvec *array)
{
	struct bio *bio = NULL;
	struct bio_vec *bio_vec = NULL;
	struct page *page;
	unsigned long bi_size = 0;
	int i = 0;

	bio = bio_alloc(GFP_KERNEL, len);
	if (!bio) {
		goto out;
	}

	memset(bio->bi_io_vec, 0, sizeof(struct bio_vec) * bio->bi_vcnt);

	for (i = 0; i < len; i++) {
		page = virt_to_page(__get_free_page(GFP_KERNEL));
		if (!page) {
			goto err_out;
		}
		bio_vec = bio->bi_io_vec + i;
		bio_vec->bv_page = page;
		bio_vec->bv_offset = array[i].offset;
		bio_vec->bv_len = array[i].length;
		bi_size += array[i].length;
	}

	bio->bi_vcnt = len;
	pb_bio_idx(bio) = 0;
	pb_bio_size(bio) = bi_size;

	return bio;
err_out:
	bio_put(bio);
out:
	return NULL;
}

static void test_memcpy(char *dst, char *src, int len)
{
	int i = 0;
	for (i = 0; i < len; i++) {
		dst[i] = src[i];
	}
}

static void test_fill_in_data(struct bio *bio, char *data_buffer)
{
	struct bio_vec *bio_vec = NULL;
	char *page_addr = 0;
	int offset = 0;
	int len = 0;
	int i;

	for (i = 0; i < bio->bi_vcnt; i++) {
		bio_vec = bio->bi_io_vec + i;
		page_addr = page_address(bio_vec->bv_page);
		offset = bio_vec->bv_offset;
		len = bio_vec->bv_len;

		page_addr += offset;
		test_memcpy(page_addr, data_buffer + offset, len);
	}
}

static void destory_test_bio(struct bio *bio)
{
	struct bio_vec *bio_vec;
	int i;

	for (i = 0; i < bio->bi_vcnt; i++) {
		bio_vec = bio->bi_io_vec + i;
		__free_page(bio_vec->bv_page);
	}

	bio_put(bio);
}

void test_write_bio_endio(struct bio *bio, int error)
{
	struct test_context *context = (struct test_context *)bio->bi_private;

	context->error = error;
	destory_test_bio(bio);
	context->bio_status = BIO_ENDIO;
	wake_up(&context->endio_queue);
	return;
}

static int compare_data(char *addr1, char *addr2, int length)
{
	int i = 0;
	char *p1 = addr1;
	char *p2 = addr2;
	for (i = 0; i < length; i++) {
		if (*p1 != *p2) {
			printk("i = %d, p1 = 0x%x, p2 = 0x%x\n", i, *p1, *p2);
			return -1;
		}
		p1++;
		p2++;
	}

	return 0;
}

void test_read_bio_endio(struct bio *bio, int error)
{
	struct test_context *context = (struct test_context *)bio->bi_private;
	char *data_buffer = context->data_buffer;
	struct bio_vec *bio_vec;
	char *page_addr = 0;
	int offset = 0;
	int len = 0;
	int i;
	
	context->error = error;
	if (error) {
		goto end;
	}


	for (i = 0; i < bio->bi_vcnt; i++) {
		bio_vec = bio->bi_io_vec + i;

		page_addr = page_address(bio_vec->bv_page);
		offset = bio_vec->bv_offset;
		len = bio_vec->bv_len;

		page_addr += offset;
		if (compare_data(page_addr, data_buffer + offset, len)) {
			printk("ERROR: data dismatch. page = 0x%p, offset = %d, len = %d\n", 
				page_addr, offset, len);
			context->error = -1;
		} 
	}

end:
	destory_test_bio(bio);
	context->bio_status = BIO_ENDIO;
	wake_up(&context->endio_queue);
	return;
}

static struct test_context *alloc_test_context(struct block_device *dev, 
											   char* data_buffer)
{
	struct test_context *context;

	context = kmalloc(sizeof(struct test_context), GFP_KERNEL);
	if (!context) {
		return NULL;
	}

	context->dev = dev;
	context->data_buffer = data_buffer;
	context->bio_status = BIO_SUBMIT;
	init_waitqueue_head(&context->endio_queue);

	return context;
}

int write_data_to_dev(struct block_device *dev, char *data_buffer, 
			      int len, struct test_bvec *array, sector_t lba)
{
	struct bio *bio = NULL;
	struct test_context *context = NULL;
	int error;

	context = alloc_test_context(dev, data_buffer);
	if (!context) {
		error = -ENOMEM;
		goto out;
	}

	bio = test_create_bio(dev, len, array);
	if (!bio) {
		error = -ENOMEM;
		goto err_out;
	}

	bio->bi_bdev = dev;
	bio->bi_rw = WRITE;
	pb_bio_start(bio) = lba;
	bio->bi_end_io = test_write_bio_endio;
	bio->bi_private = context;

	test_fill_in_data(bio, data_buffer);
	generic_make_request(bio);

	wait_event(context->endio_queue, (context->bio_status == BIO_ENDIO));

	error = context->error;
err_out:
	kfree(context);
out:
	return error;
}

int read_data_from_dev(struct block_device *dev, char* data_buffer,
			       int len, struct test_bvec *array, sector_t lba)
{
	struct test_context *context = NULL;
	struct bio *bio = NULL;
	int error = 0;

	context = alloc_test_context(dev, data_buffer);
	if (!context) {
		error = -ENOMEM;
		goto out;
	}

	bio = test_create_bio(dev, len, array);
	if (!bio) {
		error = -ENOMEM;
		goto err_out;
	}

	bio->bi_bdev = dev;
	bio->bi_rw = READ;
	pb_bio_start(bio) = lba;
	bio->bi_end_io = test_read_bio_endio;
	bio->bi_private = context;

	generic_make_request(bio);
	wait_event(context->endio_queue, (context->bio_status == BIO_ENDIO));
	error = context->error;

err_out:
	kfree(context);
out:
	return error;
}


static char *generate_test_data(void)
{
	char *data_buffer = NULL;
	int i;

	data_buffer = kmalloc(TEST_PAGE_SIZE, GFP_KERNEL);
	if (!data_buffer) {
		return NULL;
	}

	for (i = 0; i < TEST_PAGE_SIZE; i++) {
		data_buffer[i] = 'a';
	}

	return data_buffer;
}

static void destory_test_data(char *data_buffer)
{
	kfree(data_buffer);
	return;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)

struct block_device *get_pblaze_disk(int flag)
{
	struct block_device *dev = NULL;
	dev = open_bdev_exclusive(PBLAZE_DISK_PATH, flag, NULL);
	return dev;
}

#else

struct block_device *get_pblaze_disk(int flag)
{
	struct block_device *dev = NULL;
	dev = blkdev_get_by_path(PBLAZE_DISK_PATH, flag, NULL);
	return dev;
}

#endif

static int submiter_get_next_number(char *buffer, unsigned long *ret)
{
	char *start = buffer;
	char *end = start;
	unsigned long sum = 0;

	while ((*end < '0') || (*end > '9')) end++;

	while ((*end >= '0') && (*end <= '9')) {
		sum = (sum * 10) + (*end - '0');
		end++;
	}

	*ret = sum;
	return (end - start);
}

struct test_bvec *submiter_generate_bvecs(char *cmd, int *len, 
												 unsigned long *lba)
{
	struct test_bvec *bvec_array = NULL;
	int ret = 0;
	int i;

	ret = submiter_get_next_number(cmd, lba);
	ret += submiter_get_next_number(cmd + ret, (unsigned long *)len);
	
	bvec_array = kmalloc((*len) * sizeof(struct test_bvec), GFP_KERNEL);
	if (!bvec_array) {
		goto out;
	}

	for (i = 0; i < *len; i++) {
		ret += submiter_get_next_number(cmd + ret, 
										(unsigned long *)&bvec_array[i].offset);
		ret += submiter_get_next_number(cmd + ret, 
										(unsigned long *)&bvec_array[i].length);
		printk("offset = %d, length = %d\n", 
			   bvec_array[i].offset, bvec_array[i].length);
	}

	return bvec_array;

out:
	return NULL;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static void 
submiter_close_bdev(struct block_device *bdev, int dev_flag)
{
	close_bdev_exclusive(g_submiter.sbm_dev, DEV_FLAG);
}
#else
static void 
submiter_close_bdev(struct block_device *bdev, int dev_flag)
{
	blkdev_put(bdev, dev_flag);
}
#endif

static __init int submiter_init(void)
{
	int ret = 0;

	data_buffer = generate_test_data();
	if (!data_buffer) {
		ret = -ENOMEM;
		goto end;
	}

	g_submiter.sbm_dev = get_pblaze_disk(DEV_FLAG);
	if (IS_ERR(g_submiter.sbm_dev)) {
		printk("can't get pblaze disk!\n");
		ret = -1;
		goto err_out;
	}

	ret = submiter_create_proc(g_submiter.sbm_dev);
	if (ret) {
		goto err_out2;
	}

	return ret;

err_out2:
	submiter_close_bdev(g_submiter.sbm_dev, DEV_FLAG);
err_out:
	kfree(data_buffer);
end:
	return ret;
}

static __exit void submiter_exit(void)
{
	submiter_destroy_proc(g_submiter.sbm_proc);
	submiter_close_bdev(g_submiter.sbm_dev, DEV_FLAG);
	destory_test_data(data_buffer);
}

module_init(submiter_init);
module_exit(submiter_exit);


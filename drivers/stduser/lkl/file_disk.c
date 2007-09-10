#include <linux/kernel.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* invalidate_bdev */
#include <linux/bio.h>

#include "file_disk.h"

/*
 * The internal representation of our device.
 */
struct file_disk_dev {
	void *f;
        int size;                       /* Device size in sectors */
        spinlock_t lock;                /* For mutual exclusion */
        struct request_queue *queue;    /* The device request queue */
        struct gendisk *gd;             /* The gendisk structure */
} file_disk_dev;


static void file_disk_request(request_queue_t *q)
{
	struct request *req;

	while ((req = elv_next_request(q)) != NULL) {
		struct file_disk_dev *dev = req->rq_disk->private_data;
		if (! blk_fs_request(req)) {
			printk (KERN_NOTICE "Skip non-fs request\n");
			end_request(req, 0);
			continue;
		}
		_file_rw(dev->f, req->sector, req->current_nr_sectors,
				req->buffer, rq_data_dir(req));
		end_request(req, 1);
	}
}


static int file_disk_open(struct inode *inode, struct file *filp)
{
	struct file_disk_dev *dev = inode->i_bdev->bd_disk->private_data;

        dev->f=_file_open();
	filp->private_data = dev;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations file_disk_ops = {
	.owner           = THIS_MODULE,
	.open 	         = file_disk_open,
};

/*
 * Set up our internal device.
 */
void setup_device(struct file_disk_dev *dev, int which)
{
        unsigned long nsectors=_file_sectors();

	memset (dev, 0, sizeof (struct file_disk_dev));
	dev->size = nsectors*512;
	spin_lock_init(&dev->lock);
	
        dev->queue = blk_init_queue(file_disk_request, &dev->lock);
        if (dev->queue == NULL)
                return;

	blk_queue_hardsect_size(dev->queue, 512);
	dev->queue->queuedata = dev;
	/*
	 * And the gendisk structure.
	 */
	dev->gd = alloc_disk(1);
	if (! dev->gd) {
		printk (KERN_NOTICE "alloc_disk failure\n");
                return;
	}
	dev->gd->major = FILE_DISK_MAJOR;
	dev->gd->first_minor = which*1;
	dev->gd->fops = &file_disk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf (dev->gd->disk_name, 32, "file_disk%c", which + 'a');
	set_capacity(dev->gd, nsectors);
	add_disk(dev->gd);

	printk("initialized %s with major=%d\n", dev->gd->disk_name, dev->gd->major);
	return;
}


int __init file_disk_init(void)
{
	int err;

	err = register_blkdev(FILE_DISK_MAJOR, "file_disk");
	if (err < 0) {
		printk(KERN_ERR "file_disk: unable to register_blkdev major %d: %d\n", FILE_DISK_MAJOR, err);
		return -EBUSY;
	}

        setup_device(&file_disk_dev, 0);
    
	return 0;
}

late_initcall(file_disk_init);



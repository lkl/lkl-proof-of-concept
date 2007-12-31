#include <apr.h>
#include <apr_file_io.h>
#include <assert.h>

extern void *f;

apr_pool_t *root_pool;

void apr_init(const char *disk_image)
{
	int rc;

	apr_app_initialize(NULL, NULL, NULL);

	rc = apr_pool_create(&root_pool, NULL);
	assert(rc == APR_SUCCESS);
}

void* file_open(const char *name)
{
	apr_file_t *file;
	int rc;

	rc=apr_file_open(&file, name, 
			 APR_FOPEN_READ| APR_FOPEN_WRITE|
			 APR_FOPEN_BINARY, APR_OS_DEFAULT,
			 root_pool);
	assert(rc == APR_SUCCESS);
	return file;
}

void file_close(void *f)
{
	apr_file_close(f);
}


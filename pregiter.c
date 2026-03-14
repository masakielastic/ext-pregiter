#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"

#include "php_pregiter.h"
#include "pregiter_iterator.h"
#include "pregiter_arginfo.h"

static const zend_module_dep pregiter_deps[] = {
	ZEND_MOD_REQUIRED("pcre")
	ZEND_MOD_END
};

PHP_MINIT_FUNCTION(pregiter)
{
	return pregiter_register_iterator_class();
}

PHP_MINFO_FUNCTION(pregiter)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "pregiter support", "enabled");
	php_info_print_table_row(2, "version", PHP_PREGITER_VERSION);
	php_info_print_table_end();
}

zend_module_entry pregiter_module_entry = {
	STANDARD_MODULE_HEADER_EX,
	NULL,
	pregiter_deps,
	"pregiter",
	pregiter_functions,
	PHP_MINIT(pregiter),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(pregiter),
	PHP_PREGITER_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PREGITER
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(pregiter)
#endif

#ifndef PHP_PREGITER_ITERATOR_H
#define PHP_PREGITER_ITERATOR_H

#include "php.h"
#include "Zend/zend_interfaces.h"
#include "ext/pcre/php_pcre.h"

#ifndef PREG_OFFSET_CAPTURE
#define PREG_OFFSET_CAPTURE 256
#endif

#ifndef PREG_UNMATCHED_AS_NULL
#define PREG_UNMATCHED_AS_NULL 512
#endif

typedef struct _pregiter_object {
	zend_string *pattern;
	zend_string *subject;
	zend_long flags;

	pcre_cache_entry *pce;
	pcre2_code *re;
	pcre2_match_data *match_data;

	PCRE2_SPTR name_table;
	uint32_t capture_count;
	uint32_t name_count;
	uint32_t name_entry_size;
	uint32_t compile_options;

	PCRE2_SIZE search_offset;
	zend_ulong current_key;

	zval current_match;

	bool initialized;
	bool valid;
	bool finished;
	bool current_match_empty;

	zend_object std;
} pregiter_object;

extern zend_class_entry *pregiter_ce;

zend_result pregiter_register_iterator_class(void);
zend_object *pregiter_create_iterator_object(zend_class_entry *class_type);

static zend_always_inline pregiter_object *pregiter_from_obj(zend_object *obj)
{
	return (pregiter_object *) ((char *) obj - XtOffsetOf(pregiter_object, std));
}

#define Z_PREGITER_P(zv) pregiter_from_obj(Z_OBJ_P((zv)))

#endif /* PHP_PREGITER_ITERATOR_H */

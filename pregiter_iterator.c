#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/html.h"

#include "pregiter_iterator.h"
#include "pregiter_arginfo.h"

zend_class_entry *pregiter_ce;

static zend_object_handlers pregiter_object_handlers;

static void pregiter_reset_state(pregiter_object *intern)
{
	if (!Z_ISUNDEF(intern->current_match)) {
		zval_ptr_dtor(&intern->current_match);
		ZVAL_UNDEF(&intern->current_match);
	}

	intern->search_offset = 0;
	intern->current_key = 0;
	intern->initialized = false;
	intern->valid = false;
	intern->finished = false;
	intern->current_match_empty = false;
}

static void pregiter_throw_compile_error(void)
{
	if (!EG(exception)) {
		zend_throw_error(NULL, "preg_iter_ex(): failed to compile pattern");
	}
}

static void pregiter_throw_match_error(int error_code)
{
	zend_throw_error(NULL, "preg_iter_ex(): PCRE execution failed with error %d", error_code);
}

static uint32_t pregiter_match_options(const pregiter_object *intern)
{
	(void) intern;
	return 0;
}

static PCRE2_SIZE pregiter_advance_unit(const pregiter_object *intern, PCRE2_SIZE offset)
{
	const unsigned char *subject = (const unsigned char *) ZSTR_VAL(intern->subject);
	size_t subject_len = ZSTR_LEN(intern->subject);
	size_t cursor;
	zend_result status = FAILURE;

	if (offset >= subject_len) {
		return offset;
	}

	if (!(intern->compile_options & PCRE2_UTF)) {
		return offset + 1;
	}

	cursor = offset;
	php_next_utf8_char(subject, subject_len, &cursor, &status);
	if (status == SUCCESS && cursor > offset) {
		return cursor;
	}

	return offset + 1;
}

static void pregiter_init_capture_value(zval *dst, pregiter_object *intern, PCRE2_SIZE start, PCRE2_SIZE end)
{
	bool offset_capture = (intern->flags & PREG_OFFSET_CAPTURE) != 0;
	bool unmatched_as_null = (intern->flags & PREG_UNMATCHED_AS_NULL) != 0;

	if (!offset_capture) {
		if (start == PCRE2_UNSET || end == PCRE2_UNSET) {
			if (unmatched_as_null) {
				ZVAL_NULL(dst);
			} else {
				ZVAL_EMPTY_STRING(dst);
			}
		} else {
			ZVAL_STRINGL(dst, ZSTR_VAL(intern->subject) + start, end - start);
		}
		return;
	}

	array_init_size(dst, 2);
	if (start == PCRE2_UNSET || end == PCRE2_UNSET) {
		if (unmatched_as_null) {
			add_next_index_null(dst);
		} else {
			add_next_index_string(dst, "");
		}
		add_next_index_long(dst, -1);
		return;
	}

	add_next_index_stringl(dst, ZSTR_VAL(intern->subject) + start, end - start);
	add_next_index_long(dst, (zend_long) start);
}

static void pregiter_add_named_captures(zend_array *match, pregiter_object *intern, uint32_t capture_index, zval *value)
{
	uint32_t i;
	PCRE2_SPTR entry;

	if (capture_index == 0 || intern->name_count == 0 || intern->name_table == NULL) {
		return;
	}

	entry = intern->name_table;
	for (i = 0; i < intern->name_count; i++, entry += intern->name_entry_size) {
		uint16_t entry_capture_index = ((uint16_t) entry[0] << 8) | (uint16_t) entry[1];
		if (entry_capture_index == capture_index) {
			zval named_value;
			ZVAL_COPY(&named_value, value);
			zend_hash_str_update(match, (const char *) (entry + 2), strlen((const char *) (entry + 2)), &named_value);
		}
	}
}

static zend_result pregiter_build_current_match(pregiter_object *intern, int rc)
{
	PCRE2_SIZE *ovector;
	uint32_t capture_index;

	if (!Z_ISUNDEF(intern->current_match)) {
		zval_ptr_dtor(&intern->current_match);
		ZVAL_UNDEF(&intern->current_match);
	}

	array_init_size(&intern->current_match, intern->capture_count + intern->name_count + 1);
	ovector = pcre2_get_ovector_pointer(intern->match_data);

	for (capture_index = 0; capture_index <= intern->capture_count; capture_index++) {
		PCRE2_SIZE start = (capture_index < (uint32_t) rc) ? ovector[2 * capture_index] : PCRE2_UNSET;
		PCRE2_SIZE end = (capture_index < (uint32_t) rc) ? ovector[(2 * capture_index) + 1] : PCRE2_UNSET;
		zval value;

		if (start == PCRE2_UNSET || end == PCRE2_UNSET) {
			start = PCRE2_UNSET;
			end = PCRE2_UNSET;
		}

		pregiter_init_capture_value(&value, intern, start, end);
		pregiter_add_named_captures(Z_ARRVAL(intern->current_match), intern, capture_index, &value);
		zend_hash_next_index_insert_new(Z_ARRVAL(intern->current_match), &value);
	}

	intern->search_offset = ovector[1];
	intern->current_match_empty = (ovector[0] == ovector[1]);
	intern->valid = true;
	intern->finished = false;

	return SUCCESS;
}

static int pregiter_run_match(pregiter_object *intern, PCRE2_SIZE offset, uint32_t options)
{
	return pcre2_match(
		intern->re,
		(PCRE2_SPTR) ZSTR_VAL(intern->subject),
		ZSTR_LEN(intern->subject),
		offset,
		options,
		intern->match_data,
		php_pcre_mctx()
	);
}

static zend_result pregiter_finish_no_match(pregiter_object *intern)
{
	if (!Z_ISUNDEF(intern->current_match)) {
		zval_ptr_dtor(&intern->current_match);
		ZVAL_UNDEF(&intern->current_match);
	}

	intern->valid = false;
	intern->finished = true;
	return SUCCESS;
}

static zend_result pregiter_fetch_from_offset(pregiter_object *intern, PCRE2_SIZE offset)
{
	int rc = pregiter_run_match(intern, offset, pregiter_match_options(intern));

	if (rc >= 0) {
		return pregiter_build_current_match(intern, rc);
	}

	if (rc == PCRE2_ERROR_NOMATCH) {
		return pregiter_finish_no_match(intern);
	}

	intern->valid = false;
	intern->finished = true;
	pregiter_throw_match_error(rc);
	return FAILURE;
}

static zend_result pregiter_initialize(pregiter_object *intern)
{
	if (intern->initialized) {
		return SUCCESS;
	}

	intern->initialized = true;
	intern->current_key = 0;
	return pregiter_fetch_from_offset(intern, 0);
}

static zend_result pregiter_move_next(pregiter_object *intern)
{
	int rc;
	PCRE2_SIZE next_offset;

	if (!intern->initialized) {
		return pregiter_initialize(intern);
	}

	if (intern->finished || !intern->valid) {
		return SUCCESS;
	}

	if (intern->current_match_empty) {
		rc = pregiter_run_match(
			intern,
			intern->search_offset,
			pregiter_match_options(intern) | PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED
		);
		if (rc >= 0) {
			intern->current_key++;
			return pregiter_build_current_match(intern, rc);
		}

		if (rc != PCRE2_ERROR_NOMATCH) {
			intern->valid = false;
			intern->finished = true;
			pregiter_throw_match_error(rc);
			return FAILURE;
		}

		if (intern->search_offset >= ZSTR_LEN(intern->subject)) {
			return pregiter_finish_no_match(intern);
		}

		next_offset = pregiter_advance_unit(intern, intern->search_offset);
	} else {
		next_offset = intern->search_offset;
	}

	rc = pregiter_run_match(intern, next_offset, pregiter_match_options(intern));
	if (rc >= 0) {
		intern->current_key++;
		return pregiter_build_current_match(intern, rc);
	}

	if (rc == PCRE2_ERROR_NOMATCH) {
		return pregiter_finish_no_match(intern);
	}

	intern->valid = false;
	intern->finished = true;
	pregiter_throw_match_error(rc);
	return FAILURE;
}

static void pregiter_free_object(zend_object *object)
{
	pregiter_object *intern = pregiter_from_obj(object);

	if (!Z_ISUNDEF(intern->current_match)) {
		zval_ptr_dtor(&intern->current_match);
	}

	if (intern->match_data != NULL) {
		php_pcre_free_match_data(intern->match_data);
	}

	if (intern->pce != NULL) {
		php_pcre_pce_decref(intern->pce);
	}

	if (intern->pattern != NULL) {
		zend_string_release(intern->pattern);
	}

	if (intern->subject != NULL) {
		zend_string_release(intern->subject);
	}

	zend_object_std_dtor(&intern->std);
}

zend_object *pregiter_create_iterator_object(zend_class_entry *class_type)
{
	pregiter_object *intern = zend_object_alloc(sizeof(pregiter_object), class_type);

	intern->pattern = NULL;
	intern->subject = NULL;
	intern->pce = NULL;
	intern->re = NULL;
	intern->match_data = NULL;
	intern->name_table = NULL;
	intern->capture_count = 0;
	intern->name_count = 0;
	intern->name_entry_size = 0;
	intern->compile_options = 0;
	ZVAL_UNDEF(&intern->current_match);

	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);
	intern->std.handlers = &pregiter_object_handlers;

	pregiter_reset_state(intern);

	return &intern->std;
}

zend_result pregiter_register_iterator_class(void)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "PregIterator", pregiterator_methods);
	pregiter_ce = zend_register_internal_class_with_flags(
		&ce,
		NULL,
		ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES | ZEND_ACC_NOT_SERIALIZABLE
	);
	pregiter_ce->create_object = pregiter_create_iterator_object;
	zend_class_implements(pregiter_ce, 1, zend_ce_iterator);

	memcpy(&pregiter_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
	pregiter_object_handlers.offset = XtOffsetOf(pregiter_object, std);
	pregiter_object_handlers.free_obj = pregiter_free_object;
	pregiter_object_handlers.clone_obj = NULL;

	return SUCCESS;
}

ZEND_METHOD(PregIterator, __construct)
{
	ZEND_PARSE_PARAMETERS_NONE();
}

ZEND_METHOD(PregIterator, current)
{
	pregiter_object *intern = Z_PREGITER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (!intern->initialized && pregiter_initialize(intern) == FAILURE) {
		RETURN_THROWS();
	}

	if (!intern->valid || Z_ISUNDEF(intern->current_match)) {
		RETURN_NULL();
	}

	RETURN_COPY(&intern->current_match);
}

ZEND_METHOD(PregIterator, key)
{
	pregiter_object *intern = Z_PREGITER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (!intern->initialized && pregiter_initialize(intern) == FAILURE) {
		RETURN_THROWS();
	}

	if (!intern->valid) {
		RETURN_NULL();
	}

	RETURN_LONG(intern->current_key);
}

ZEND_METHOD(PregIterator, next)
{
	pregiter_object *intern = Z_PREGITER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (pregiter_move_next(intern) == FAILURE) {
		RETURN_THROWS();
	}
}

ZEND_METHOD(PregIterator, valid)
{
	pregiter_object *intern = Z_PREGITER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	if (!intern->initialized && pregiter_initialize(intern) == FAILURE) {
		RETURN_THROWS();
	}

	RETURN_BOOL(intern->valid);
}

ZEND_METHOD(PregIterator, rewind)
{
	pregiter_object *intern = Z_PREGITER_P(ZEND_THIS);

	ZEND_PARSE_PARAMETERS_NONE();

	pregiter_reset_state(intern);
	intern->initialized = true;

	if (pregiter_fetch_from_offset(intern, 0) == FAILURE) {
		RETURN_THROWS();
	}
}

ZEND_FUNCTION(preg_iter_ex)
{
	zend_string *pattern;
	zend_string *subject;
	zend_long flags = 0;
	pregiter_object *intern;
	uint32_t supported_flags = PREG_OFFSET_CAPTURE | PREG_UNMATCHED_AS_NULL;

	ZEND_PARSE_PARAMETERS_START(2, 3)
		Z_PARAM_STR(pattern)
		Z_PARAM_STR(subject)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(flags)
	ZEND_PARSE_PARAMETERS_END();

	if ((flags & ~supported_flags) != 0) {
		zend_argument_value_error(
			3,
			"must be a combination of PREG_OFFSET_CAPTURE and PREG_UNMATCHED_AS_NULL"
		);
		RETURN_THROWS();
	}

	object_init_ex(return_value, pregiter_ce);
	intern = Z_PREGITER_P(return_value);

	intern->pattern = zend_string_copy(pattern);
	intern->subject = zend_string_copy(subject);
	intern->flags = flags;

	intern->pce = pcre_get_compiled_regex_cache(pattern);
	if (intern->pce == NULL) {
		pregiter_throw_compile_error();
		zval_ptr_dtor(return_value);
		ZVAL_UNDEF(return_value);
		RETURN_THROWS();
	}

	php_pcre_pce_incref(intern->pce);
	intern->re = php_pcre_pce_re(intern->pce);

	if (pcre2_pattern_info(intern->re, PCRE2_INFO_CAPTURECOUNT, &intern->capture_count) < 0
		|| pcre2_pattern_info(intern->re, PCRE2_INFO_NAMECOUNT, &intern->name_count) < 0
		|| pcre2_pattern_info(intern->re, PCRE2_INFO_NAMEENTRYSIZE, &intern->name_entry_size) < 0
		|| pcre2_pattern_info(intern->re, PCRE2_INFO_NAMETABLE, &intern->name_table) < 0
		|| pcre2_pattern_info(intern->re, PCRE2_INFO_ALLOPTIONS, &intern->compile_options) < 0) {
		zend_throw_error(NULL, "preg_iter_ex(): failed to read compiled pattern metadata");
		zval_ptr_dtor(return_value);
		ZVAL_UNDEF(return_value);
		RETURN_THROWS();
	}

	intern->match_data = php_pcre_create_match_data(0, intern->re);
	if (intern->match_data == NULL) {
		zend_throw_error(NULL, "preg_iter_ex(): failed to allocate PCRE match data");
		zval_ptr_dtor(return_value);
		ZVAL_UNDEF(return_value);
		RETURN_THROWS();
	}
}
